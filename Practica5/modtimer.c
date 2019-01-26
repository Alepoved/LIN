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
#include <linux/random.h>
#include <linux/workqueue.h>
#include <linux/random.h>
#include <linux/list.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");


#define BUFFER_LENGTH 200
#define MAX_CBUFFER_LEN 24

struct kfifo cbuffer;
DEFINE_SPINLOCK(sp);
unsigned long flags;
DEFINE_SEMAPHORE(sem);
struct semaphore sem_queue;

struct timer_list timer;
struct work_struct work;

int timer_period_ms=250;
int max_random=300;
int emergency_threshold=80;
int finish;
int nr_threads_waiting;

static struct proc_dir_entry *proc_modtimer;
static struct proc_dir_entry *proc_modconfig;

struct list_item {
	int data;
	struct list_head links;
};

struct list_head my_list;

//----------------------------------------------------------------



static ssize_t modtimer_read(struct file *file, char __user *buf, size_t len, loff_t *off){
	
	
	char kbuffer[BUFFER_LENGTH];
	int nr_bytes;
	struct list_item *item = NULL;
  	struct list_head *cur_node = NULL;
	struct list_head *aux=NULL;
	char *list = kbuffer;
	
	

	if( down_interruptible(&sem)){
		return -EINTR;
	}

	while(list_empty(&my_list)){
		nr_threads_waiting++;
		up(&sem);
		
		if(down_interruptible(&sem_queue)){
			if( down_interruptible(&sem)){
				return -EINTR;
			}
			nr_threads_waiting--;
			up(&sem);
			return -EINTR;
		}

		if( down_interruptible(&sem)){
			return -EINTR;
		}
	}

	
	 list_for_each_safe(cur_node, aux, &my_list) {
		item = list_entry(cur_node, struct list_item, links);
		list_del(cur_node);
		list += sprintf(list, "%d\n", item->data);
		vfree(item);
		 
	 }
	

	up(&sem);

	//nr_bytes = list - kbuffer;
	nr_bytes = strlen(kbuffer);
	
	if (len < nr_bytes)
    		return -ENOSPC;
	
	if( copy_to_user(buf,kbuffer,nr_bytes)){
		return -EFAULT;
	}
		
	

return nr_bytes;
}

	
static void copy_items_into_list(struct work_struct *work){
	
	
	struct list_item *item = NULL;
	int cont = 0,cont2 = 0, out;
	int elem;
	int buf_aux[MAX_CBUFFER_LEN];

	
	
	if (down_interruptible(&sem)){
		return;	
	}

	spin_lock_irqsave(&sp,flags);
	while(!kfifo_is_empty(&cbuffer)){
		out = kfifo_out(&cbuffer,&elem,sizeof(int));
		buf_aux[cont] = elem;
		cont++;
	}
	spin_unlock_irqrestore(&sp,flags);

	for(cont2 = 0; cont2 < cont; cont2++){
		item = (struct list_item *) vmalloc(sizeof(struct list_item));
		elem = buf_aux[cont2];
		item->data = elem;
		list_add_tail(&item->links, &my_list);
	}
	

	printk(KERN_INFO "%d elements moved from the buffer to the list\n",cont);

	if(nr_threads_waiting > 0){
		up(&sem_queue);
		nr_threads_waiting--;
	}
   	 


	up(&sem);


}

static void fire_timer(unsigned long data){

	int random,size,cpu;
	
	/*srand(time(NULL));
	random = rand() % max_random;*/
	random = get_random_int() % max_random;
	
	printk(KERN_INFO "Generated number: %i\n",random);

	spin_lock_irqsave(&sp,flags);

	if(kfifo_len(&cbuffer) < MAX_CBUFFER_LEN-1)
		kfifo_in(&cbuffer,&random,sizeof(int));

	size = kfifo_len(&cbuffer);
	//printk(KERN_INFO "ocupadas %i pos\n",size/4);

	spin_unlock_irqrestore(&sp,flags);

	//printk(KERN_INFO "size: %i\n",size);
	//printk(KERN_INFO "threshold: %i\n",(emergency_threshold*MAX_CBUFFER_LEN)/100);

	if ( size >= (emergency_threshold*MAX_CBUFFER_LEN)/100){
		cpu = smp_processor_id();

		INIT_WORK(&work, copy_items_into_list);// work + function

		if (cpu == 0)
			schedule_work_on(cpu+1, &work);
		else
			schedule_work_on(cpu-1, &work);
	}

 
	mod_timer( &timer, jiffies + timer_period_ms);
}

static int modtimer_open(struct inode *inode,struct file *file){

	timer.data=0;
	timer.function=fire_timer;
	timer.expires=jiffies + timer_period_ms*(HZ/1000);
	add_timer(&timer);//activar el timer


	nr_threads_waiting = 0;

	try_module_get(THIS_MODULE);
	


return 0;
}

static int modtimer_release(struct inode *inode,struct file *file){

	struct list_item *item= NULL;
	struct list_head *cur_node=NULL;
	struct list_head *aux=NULL;

	module_put(THIS_MODULE);

	del_timer_sync(&timer);
	kfifo_reset(&cbuffer);
	flush_scheduled_work();//esperar a los trabajos

	if(down_interruptible(&sem))
		return -EINTR;

	list_for_each_safe(cur_node, aux, &my_list){
    		item=list_entry(cur_node, struct list_item, links);
    		list_del(cur_node);
    		vfree(item);
  	}

	up(&sem);


return 0;
}
//-----------------------------------------------------------------

static ssize_t modconfig_read(struct file *file, char __user *buf, size_t len, loff_t *off){
	
	int nr_bytes = 0;
	char kbuffer[BUFFER_LENGTH];
	char *data = kbuffer;

	if( (*off) > 0 ){
		return 0;
	}

	data += sprintf(data,"timer_period_ms=%i\n",timer_period_ms);
	data += sprintf(data,"max_random=%i\n",max_random);
	data += sprintf(data,"emergency_threshold=%i\n",emergency_threshold);
	
	nr_bytes =  strlen(kbuffer);

	if( len < nr_bytes ){
		return -ENOSPC;
	}
	
	if( copy_to_user( buf, kbuffer, nr_bytes ) ){
		return -EFAULT;//parametros incorrectos(origen o destino)
	}
	
	(*off) += nr_bytes;

return nr_bytes;
}

static ssize_t modconfig_write(struct file *file, const char __user *buf, size_t len, loff_t *off){

	char kbuffer[BUFFER_LENGTH];
	int num;
	//long int long_num;

	if( (*off) > 0 ){
		return 0;
	}

	if( len > BUFFER_LENGTH ){
    		return -ENOSPC;
	}

	if( copy_from_user( kbuffer, buf, len ) ){
		return -EFAULT;
	}

	kbuffer[len]='\0';
	*off+=len;

	if( sscanf(kbuffer,"timer_period_ms %i",&num) == 1) {
		timer_period_ms = num;
	}
	else if(sscanf(kbuffer,"max_random %i",&num) == 1){
		max_random = num;
	}
	else if(sscanf(kbuffer,"emergency_threshold %i",&num) == 1){
		emergency_threshold = num;
	}
	else 
		printk(KERN_INFO "ERROR: comando no valido!!!\n");

return len;
}


//-----------------------------------------------------------------
static const struct file_operations proc_entry_modconfig = {
	.write = modconfig_write,
	.read = modconfig_read,
	
};

static const struct file_operations proc_entry_modtimer = {
	.open = modtimer_open,
	.release = modtimer_release,
	.read = modtimer_read,	
};



int start_module(void){
	
	int retval = 0;

	retval = kfifo_alloc(&cbuffer,MAX_CBUFFER_LEN,GFP_KERNEL);

	if (retval)
		return -ENOMEM;

	

	printk(KERN_INFO "ModTimer: Loaded\n");
	proc_modtimer = proc_create_data("modtimer",0666, NULL, &proc_entry_modtimer, NULL);
	proc_modconfig = proc_create_data("modconfig",0666, NULL, &proc_entry_modconfig ,NULL);
	
    	if( proc_modtimer== NULL || proc_modconfig==NULL){
		kfifo_free(&cbuffer);
      	 	printk(KERN_INFO "Cannot create entry\n");
    		retval =-ENOSPC;
   	}
	INIT_LIST_HEAD(&my_list);
	sema_init(&sem,1);
	sema_init(&sem_queue,0);
	init_timer(&timer);//transparencias

return retval;
	
}




void finish_module(void){

	remove_proc_entry("modtimer", NULL);
	remove_proc_entry("modconfig",NULL);
	printk(KERN_INFO "ModTimer: Removed\n");
}

module_init(start_module);
module_exit(finish_module);





