//Alejandro Povedano Atienza
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>


MODULE_LICENSE("GPL");


#define MAX_KBUF  200
#define MAX_CBUFFER_LEN 64

struct kfifo cbuffer;
int prod_count = 0; /*procesos que abrieron proc para escribir*/
int cons_count = 0;
struct semaphore mtx;
struct semaphore sem_prod;
struct semaphore sem_cons;
int nr_prod_waiting=0;/*procesos esperando*/
int nr_cons_waiting=0;
static struct proc_dir_entry *proc_entry;

static int fifoproc_open(struct inode *inode,struct file *file){
if(down_interruptible(&mtx)) return -EINTR;

	if (file->f_mode & FMODE_READ){//consumidor
	 
		cons_count++;
		if(nr_prod_waiting > 0) {	
			up(&sem_prod);
			nr_prod_waiting--;		
		}
		while(prod_count<=0) {
			nr_cons_waiting++;
			up(&mtx);
			if(down_interruptible(&sem_cons)) {
				down(&mtx);
				nr_cons_waiting--;
				up(&mtx);
				return -EINTR;
			}
			if(down_interruptible(&mtx)) return -EINTR;
		}	
		

	} else{ //productor
	
		prod_count++;
		if(nr_cons_waiting > 0) {
			up(&sem_cons);
			nr_cons_waiting--;		
		}

		while(cons_count<=0) {
			nr_prod_waiting++;
			up(&mtx);
			if(down_interruptible(&sem_prod)) {
				down(&mtx);
				nr_prod_waiting--;
				up(&mtx);
				return -EINTR;
			}
			if(down_interruptible(&mtx)) return -EINTR;
		}	


	}

	
	up(&mtx);
	printk(KERN_INFO "OPEN: Productores: %d Consumidores: %d\n",prod_count, cons_count);
return 0;
}

static int fifoproc_release(struct inode *inode,struct file *file){
	
	if(down_interruptible(&mtx)) return -EINTR;
	if(file->f_mode & FMODE_READ) {
		cons_count--;
		if(nr_prod_waiting > 0) {
			up(&sem_prod);
			nr_prod_waiting--;		
		}
	}
	else {
		prod_count--;
		if(nr_cons_waiting > 0) {
			up(&sem_cons);
			nr_cons_waiting--;		
		}
	}
	if((cons_count==0)&&(prod_count==0))
		kfifo_reset(&cbuffer);

	up(&mtx);
	return 0;

	printk(KERN_INFO "RELEASE: Productores: %d Consumidores: %d\n",prod_count, cons_count);


return 0;
}


static ssize_t fifoproc_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
	char kbuffer[MAX_KBUF];

	if( len > MAX_CBUFFER_LEN ){
		return -EFAULT;
	}	

	if( copy_from_user( kbuffer, buf, len ) ){
		return -EFAULT;
	}

	if( down_interruptible( &mtx ) ){
		return -EINTR;
	}

	kbuffer[len] = '\0';
	

	while( kfifo_avail( &cbuffer ) < len && cons_count > 0 ){
		printk(KERN_INFO "Write: Esperando con %d consumidores\n", cons_count);
		nr_prod_waiting ++;
		up( &mtx );
		if(down_interruptible(&sem_prod)){
			down(&mtx);
			nr_prod_waiting--;
			up(&mtx);
			return -EFAULT;		
		}
		if(down_interruptible(&mtx)) return -EINTR;
	}

	
	if( cons_count == 0 ){
		up( &mtx );
		return -EPIPE;
	}

	kfifo_in( &cbuffer, kbuffer, len );

	if( nr_cons_waiting > 0 ){
		up( &sem_cons );
		nr_cons_waiting --;
	}

	up( &mtx );
	printk(KERN_INFO "Write: Elems en kfifo %i\n",kfifo_len(&cbuffer));
	printk(KERN_INFO "Write: Huecos libres en kfifo %i\n",kfifo_avail(&cbuffer));
	return len;
}

static ssize_t fifoproc_read(struct file *file, char __user *buf, size_t len, loff_t *off){
	
	char kbuffer[MAX_KBUF];
	int out;
	if( len > MAX_CBUFFER_LEN ){
		return -EFAULT;
	}
	
	if( down_interruptible( &mtx ) ){
		return -EINTR;
	}
	
	
	while( kfifo_len( &cbuffer ) < len && prod_count > 0 ){
		printk(KERN_INFO "Read: Esperando con %d productores\n", prod_count);
		nr_cons_waiting ++;
		up( &mtx );
		if(down_interruptible(&sem_cons)){
			down(&mtx);
			nr_cons_waiting--;
			up(&mtx);
			return -EFAULT;		
		}
		if(down_interruptible(&mtx)) return -EINTR;
	}
	
	if( prod_count == 0 && kfifo_len( &cbuffer ) == 0 ){
		up( &mtx );
		return 0;
	}
	
	out = kfifo_out(&cbuffer,kbuffer,len);

	if( nr_prod_waiting > 0 ){
		up( &sem_prod );
		nr_prod_waiting --;
	}

	up( &mtx );

	if( copy_to_user( buf, kbuffer, len ) ){
		return -EFAULT;
	}
	return len;
	

}



static const struct file_operations proc_entry_fops = {
	.read = fifoproc_read,
	.write = fifoproc_write,
	.release = fifoproc_release,
	.open = fifoproc_open,
	
};




int start_module(void){
	
	int retval = 0;

	retval = kfifo_alloc(&cbuffer,MAX_CBUFFER_LEN,GFP_KERNEL);

	if (retval)
		return -ENOMEM;

	sema_init(&sem_prod, 0);
	sema_init(&sem_cons,0);
	sema_init(&mtx,1);


	printk(KERN_INFO "ModFifo: Loaded\n");
	proc_entry = proc_create_data("modfifo",0666, NULL, &proc_entry_fops, NULL);
	

    	if( proc_entry == NULL ){
		kfifo_free(&cbuffer);
      	 	printk(KERN_INFO "Cannot create entry\n");
    		retval =-ENOSPC;
   	}
	

return retval;
	
}




void finish_module(void){

	remove_proc_entry("modfifo", NULL);
	kfifo_free(&cbuffer);
	printk(KERN_INFO "ModFifo: Removed\n");
}

module_init(start_module);
module_exit(finish_module);





