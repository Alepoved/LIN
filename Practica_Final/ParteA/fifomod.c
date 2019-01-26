//Alejandro Povedano Atienza
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/ftrace.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");

#define MAX_KBUF  200
#define MAX_NAME_LEN 64

typedef struct {
	char *name;
	struct kfifo cbuffer;
	int prod_count;
	int cons_count;
	struct semaphore mtx;
	struct semaphore sem_prod;
	struct semaphore sem_cons;
	int nr_prod_waiting;
	int nr_cons_waiting;
	struct list_head links;
}elem_data;

struct semaphore lock_list;

static int max_entries = 4;
module_param(max_entries,int,0660);

static int max_cbuffer_size= 64;
module_param(max_cbuffer_size,int,0660);

struct list_head fifolist;
int count_list;

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;
static struct proc_dir_entry *proc_entry_control;

int addNewFifo(char * name);
int deleteAllFifo(char * name);
int deleteFifo(char * name);
static ssize_t control_write(struct file *file, const char __user *buf, size_t len, loff_t *off);
static int fifoproc_open(struct inode *inode,struct file *file);
static int fifoproc_release(struct inode *inode,struct file *file);
static ssize_t fifoproc_read(struct file *file, char __user *buf, size_t len, loff_t *off);
static ssize_t fifoproc_write(struct file *file, const char __user *buf, size_t len, loff_t *off);


static const struct file_operations proc_entry_fops = {
	.read = fifoproc_read,
	.write = fifoproc_write,
	.release = fifoproc_release,
	.open = fifoproc_open,
	
};

static const struct file_operations proc_entry_control_fops = {
    	.write = control_write
};



int addNewFifo(char * name){

	elem_data *elem = (elem_data *)vmalloc(sizeof(elem_data));
	elem_data *elem_aux;
	struct list_head *item = NULL;
	struct list_head *aux = NULL;
	int fifo_exist = 0;

	/*Check number fifos is lower than max_entries*/
	if(down_interruptible(&lock_list)){
		return -ENAVAIL;
	}
	if(count_list >= max_entries)goto end;

	up(&lock_list);

	/*Check fifo's name is not already taken*/
	if(down_interruptible(&lock_list)){
		return -ENAVAIL;
	}
	list_for_each_safe(item, aux, &fifolist) {
		elem_aux = list_entry(item,elem_data, links);
		if(strcmp(elem_aux->name, name) == 0) {
			fifo_exist = 1;
		}
	}
	up(&lock_list);
		
	if(fifo_exist == 1){
		printk(KERN_INFO "Fifo's name already exists\n");
		vfree(elem);
		return -EINVAL;
	}
		

	/*Creation fifo entry*/
	
	if(kfifo_alloc(&elem->cbuffer,max_cbuffer_size,GFP_KERNEL)!=0){
		vfree(elem);
		printk(KERN_INFO "Cannot allocate memory for fifo\n");
		return -ENOMEM;
	}
	 
	sema_init(&elem->mtx, 1);
	sema_init(&elem->sem_cons, 0);
	sema_init(&elem->sem_prod, 0);

	elem->prod_count = 0;
	elem->cons_count = 0;
	elem->name = (char *)vmalloc(sizeof(MAX_NAME_LEN));
	strcpy(elem->name, name);
	elem->nr_prod_waiting = 0;
	elem->nr_cons_waiting = 0;
	 
	    
	if(down_interruptible(&lock_list)){
		return -ENAVAIL;
	}

	list_add_tail(&elem->links, &fifolist);
	count_list++;

	up(&lock_list);

	proc_entry = proc_create_data(name, 0666, proc_dir, &proc_entry_fops, (void *) elem);
	if (proc_entry == NULL) {
		deleteFifo(name);
		printk(KERN_INFO "fifoproc: Cannot create entry\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "Created new fifo: %s\n",elem->name);

return 0;

end:
	vfree(elem);
	up(&lock_list);
	printk(KERN_INFO "Cannot create new entry. max_entires=%i\n",max_entries);
return -ENOMEM;
}

void deleteAllFifos(void){
	
	struct list_head *item = NULL;
    	struct list_head* aux=NULL;
	elem_data *elem;
	
	if(down_interruptible(&lock_list)){
		goto end;
	}
	
	list_for_each_safe(item, aux, &fifolist) {
		count_list--;
		elem = list_entry(item,elem_data, links);
		list_del(item);
		kfifo_free(&elem->cbuffer);
		remove_proc_entry(elem->name, proc_dir);
		vfree(elem->name);
		vfree(elem);
	}
	

	up(&lock_list);

	printk(KERN_INFO "Deleted all fifos");

end:
return;
} 

int deleteFifo(char * name){

	struct list_head *item = NULL;
    	struct list_head* aux=NULL;
    	elem_data *elem;
	int flag = -1;
   	
	if(down_interruptible(&lock_list)){
		return -ENAVAIL;
	}

	list_for_each_safe(item, aux, &fifolist) {
		elem = list_entry(item,elem_data, links);
		if(strcmp(elem->name, name) == 0) {
			count_list--; flag = 0;
			list_del(item);
			strcpy(name,elem->name);
			kfifo_reset(&elem->cbuffer);
			vfree(elem->name);
			vfree(elem);
			
		}
	}

	up(&lock_list);
	    
	if(flag == 0){
		remove_proc_entry(name, proc_dir);
		printk(KERN_INFO "Deleted fifo: %s\n",name);
	}
	else{
		printk(KERN_INFO "Not found fifo with name: %s\n",name);
		return -ENOMEM;
	}

return 0;

}

//--------------------------------------------------------------------------------

static ssize_t control_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
	
	char kbuffer[MAX_KBUF];
	char name[50];

	if( len > max_cbuffer_size ){
		return -EFAULT;
	}	

	if( copy_from_user( kbuffer, buf, len ) ){
		return -EFAULT;
	}

	kbuffer[len] = '\0';

	if(sscanf(kbuffer,"create %s",name)){
		if(strcmp(name,"control") != 0){
			if(addNewFifo(name) != 0){
				return -ENOMEM;
			}		
		}
		else return -EINVAL;

	}

	else if(sscanf(kbuffer,"delete %s",name)){
		if(strcmp(name,"control") != 0){
			if(deleteFifo(name) !=0 ){
				return -ENOMEM;
			}		
		}
		else return -ENOMEM;

	}

return len;
}

//----------------------------------------------------------------------------------

static int fifoproc_open(struct inode *inode,struct file *file){

	elem_data *elem = (elem_data *)PDE_DATA(file->f_inode);

	int i;

	if(down_interruptible(&elem->mtx)) return -EINTR;
	
	if(file->f_mode & FMODE_WRITE){
		elem->prod_count++;
		if(elem->cons_count == 0) {	
			up(&elem->mtx);
			if(down_interruptible(&elem->sem_prod)){
		        	down(&(elem->mtx));
		      		  elem->prod_count--;//cambio
		      		 up(&elem->mtx);
		       		return -EPIPE;
			}
			if(down_interruptible(&elem->mtx)) return -EPIPE;		
		}
		if(elem->cons_count > 0 && elem->prod_count > 0){
			for(i = 0; i < elem->cons_count; i++)
				up(&elem->sem_cons);
		}
	}
	else if(file->f_mode & FMODE_READ){
		elem->cons_count++;
		if(elem->prod_count == 0) {	
			up(&elem->mtx);
			if(down_interruptible(&elem->sem_cons)){
		        	down(&(elem->mtx));
		      		  elem->cons_count--;
		      		 up(&elem->mtx);
		       		return -EPIPE;
			}
			if(down_interruptible(&elem->mtx)) return -EPIPE;		
			}
		if(elem->cons_count > 0 && elem->prod_count > 0){
			for(i = 0; i < elem->prod_count; i++)
				up(&elem->sem_prod);
		}
	}
	
	
	up(&elem->mtx);	
	printk(KERN_INFO "OPEN: Productores: %d Consumidores: %d\n",elem->prod_count, elem->cons_count);

return 0;
}

static int fifoproc_release(struct inode *inode,struct file *file){
	
	elem_data *elem = (elem_data *)PDE_DATA(file->f_inode);
	if(down_interruptible(&elem->mtx)) return -EINTR;	
	
	if(file->f_mode & FMODE_WRITE){
		elem->prod_count--;
		if(elem->prod_count == 0 && elem->nr_cons_waiting > 0){//no hay productores
			int i = 0, cons_waiting = elem->nr_cons_waiting;
			while(i < cons_waiting){
				elem->nr_cons_waiting--;
				up(&elem->sem_cons);
				i++;				
			}
		    }	
	}
	
	else if(file->f_mode & FMODE_READ){
		elem->cons_count--;
		if(elem->cons_count == 0 && elem->nr_prod_waiting > 0){//no hay consumidores
			int i = 0, prod_waiting = elem->nr_prod_waiting;
			while(i < prod_waiting){
				elem->nr_prod_waiting--;
				up(&elem->sem_prod);
				i++;				
			}
		}	
	}


    	if((elem->cons_count == 0) && (elem->prod_count == 0))
		kfifo_reset(&elem->cbuffer);
	
	up(&elem->mtx);

	printk(KERN_INFO "RELEASE: Productores: %d Consumidores: %d\n",elem->prod_count, elem->cons_count);


return 0;

}

static ssize_t fifoproc_read(struct file *file, char __user *buf, size_t len, loff_t *off){

	char kbuffer[MAX_KBUF];
	int out;
	elem_data *elem = (elem_data *)PDE_DATA(file->f_inode);

	if(len > max_cbuffer_size) return -EFAULT;
	
	if(down_interruptible(&elem->mtx)) return -EINTR;

	while(kfifo_len(&elem->cbuffer) < len && elem->prod_count > 0 ){
		printk(KERN_INFO "Read: Esperando con %d productores\n", elem->prod_count);
		elem->nr_cons_waiting ++;
		up( &elem->mtx );
		if(down_interruptible(&elem->sem_cons)){
			down(&elem->mtx);
			elem->nr_cons_waiting--;
			up(&elem->mtx);
			return -EFAULT;		
		}
		if(down_interruptible(&elem->mtx)) return -EINTR;
	}

	if(elem->prod_count == 0 && kfifo_len(&elem->cbuffer) == 0){
		up(&elem->mtx);
		return 0;
	}

	out = kfifo_out(&elem->cbuffer,kbuffer,len);

	if(elem->nr_prod_waiting > 0){
		up(&elem->sem_prod);
		elem->nr_prod_waiting--;
	}

	up(&elem->mtx);

	if(copy_to_user(buf,kbuffer,len)){
		return -EFAULT;
	}
	

return len;
}

static ssize_t fifoproc_write(struct file *file, const char __user *buf, size_t len, loff_t *off){     

	char kbuffer[MAX_KBUF];
	elem_data *elem = (elem_data *)PDE_DATA(file->f_inode);

	if( len > max_cbuffer_size ){
		return -EFAULT;
	}	

	if( copy_from_user( kbuffer, buf, len )){
		return -EFAULT;
	}

	if( down_interruptible(&elem->mtx)){
		return -EINTR;
	}

	kbuffer[len] = '\0';
	

	while(kfifo_avail(&elem->cbuffer) < len && elem->cons_count > 0 ){
		printk(KERN_INFO "Write: Esperando con %d consumidores\n", elem->cons_count);
		elem->nr_prod_waiting ++;
		up(&elem->mtx);
		if(down_interruptible(&elem->sem_prod)){
			down(&elem->mtx);
			elem->nr_prod_waiting--;
			up(&elem->mtx);
			return -EFAULT;		
		}
		if(down_interruptible(&elem->mtx)) return -EINTR;
	}

	
	if( elem->cons_count == 0 ){
		up(&elem->mtx);
		return -EPIPE;
	}

	kfifo_in(&elem->cbuffer,kbuffer,len);

	if(elem->nr_cons_waiting > 0 ){
		up(&elem->sem_cons);
		elem->nr_cons_waiting--;
	}

	up(&elem->mtx);
	printk(KERN_INFO "Write: Elems en kfifo %i\n",kfifo_len(&elem->cbuffer));
	printk(KERN_INFO "Write: Huecos libres en kfifo %i\n",kfifo_avail(&elem->cbuffer));
	

return len;
}


//-----------------------------------------------------------------------------------
int start_module(void){
	
	int retval=0;

	proc_dir = proc_mkdir("fifo", NULL);

    	if( proc_dir == NULL ){
      	 	printk(KERN_INFO "Can't create dir\n");
		return -ENOMEM;
   	}

	printk(KERN_INFO "Fifo: Loaded\n");
	proc_entry_control = proc_create("control",0666,proc_dir,&proc_entry_control_fops);

    	if( proc_entry_control == NULL ){
		remove_proc_entry("fifo", NULL);
        	printk(KERN_INFO "fifo: Can't create /proc entry control\n");
	return -ENOMEM;
   	}
	
	sema_init(&lock_list, 1);
    	INIT_LIST_HEAD(&fifolist);
	count_list = 0;

	retval = addNewFifo("default");
	

return retval;
	
}




void finish_module(void){

	deleteAllFifos();
	remove_proc_entry("control", proc_dir);
	remove_proc_entry("fifo", NULL);
	printk(KERN_INFO "Fifo: Removed\n");
}

module_init(start_module);
module_exit(finish_module);






