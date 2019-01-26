//Alejandro Povedano Atienza
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/vmalloc.h>
#include <linux/ftrace.h> //COMPILACION
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/string.h>


MODULE_LICENSE("GPL");

DEFINE_SPINLOCK(sp);
#define BUFFER_LENGTH PAGE_SIZE


struct list_item {
	int data;
	struct list_head links;
};



struct list_head my_list;


static struct proc_dir_entry *proc_entry;


static ssize_t my_write ( struct file *filp, const char __user *buf, size_t len, loff_t *off  ){
	

	 int available_space = BUFFER_LENGTH-1;
  int num;
  char kbuf[BUFFER_LENGTH];

  struct list_item *item = NULL;
  struct list_head *cur_node = NULL;
 struct list_head *aux=NULL;

  if ((*off) > 0) /* The application can write in this entry just once !! */
    return 0;

  if (len > available_space) {
    printk(KERN_INFO "modlist: not enough space!!\n");
    return -ENOSPC; // No queda espacio en el dispositivo
  }
  
  if (copy_from_user( kbuf, buf, len ))
    return -EFAULT; // Direccion incorrecta

  kbuf[len]='\0'; // añadimos el fin de la cadena al copiar los datos from user space.
  *off+=len;            /* Update the file pointer */

  	if( sscanf(kbuf,"add %d",&num) == 1) {
	    item=(struct list_item *) vmalloc(sizeof (struct list_item));
	    item->data = num;
	    
		spin_lock(&sp);
	
	       list_add_tail(&item->links, &my_list);
	    spin_unlock(&sp);

	  }
	else if( sscanf(kbuf,"remove %d",&num) == 1) {
	    struct list_head *aux = NULL;
	    
		spin_lock(&sp);
	       
	       list_for_each_safe(cur_node, aux, &my_list) {
		
		 item = list_entry(cur_node, struct list_item, links);
		 if((item->data) == num){
		   list_del(cur_node);
		   vfree(item);
		 }
	       }
	    spin_unlock(&sp);

	  }
	else if ( strcmp(kbuf,"cleanup\n") == 0){ 
   		 spin_lock(&sp);//

		list_for_each_safe(cur_node, aux, &my_list){
			 item=list_entry(cur_node, struct list_item, links);
			 list_del(cur_node);
	    	    	 vfree(item);
    		 }
		spin_unlock(&sp);//
	}
		
	else{
   	 printk(KERN_INFO "ERROR: comando no valido!!!\n");
  	}
  return len;

}


static ssize_t my_read ( struct file *filp, char __user *buf, size_t len,loff_t *off ){
	
	int nr_bytes=0;//,cont=0;
  struct list_item *item = NULL;
  struct list_head *cur_node = NULL;
  int count=0;

  char kbuf[BUFFER_LENGTH] = "";
  char aux[10];
  char *list_string = kbuf;


  if ((*off) > 0) /* Tell the application that there is nothing left to read  "Para no copiar basura si llamas otra vez" */
      return 0;

  spin_lock(&sp);
     list_for_each(cur_node, &my_list) {
      /
       item = list_entry(cur_node, struct list_item, links);
	if(sprintf(aux,"%d\n", item->data)+nr_bytes >= BUFFER_LENGTH-1)
		break;

        count= sprintf(list_string, "%d\n", item->data);
	list_string+=count;
	nr_bytes+=count;
     }
  spin_unlock(&sp);

  if (len<nr_bytes) {
    //vfree(kbuf);
    return -ENOSPC; //No queda espacio en el dispositivo
  }

   
  if (copy_to_user(buf, kbuf, nr_bytes)) {
    
    return -EINVAL; //Argumento invalido
  }

  (*off)+=len;  /* Update the file pointer */
  
  return nr_bytes;

}

static const struct file_operations proc_entry_fops = {
	.read = my_read,
	.write = my_write,
};

int createEntry(void){

    int ret = 0;
	
	proc_entry = proc_create( "modlist", 0666, NULL, &proc_entry_fops );
	if( proc_entry == NULL ){
		ret = -ENOMEM;
		printk(KERN_INFO "Modlist: Can't create /proc/modlist entry\n");
	}else{
		printk(KERN_INFO "Modlist: Module loaded\n");
	} 
	
	return ret;
}


static int __init list_module_init(void){
	
	int ret = 0;
	printk(KERN_INFO "ListMod: Loaded\n");
	
    	if( createEntry() != 0 ){
      	 	printk(KERN_INFO "Cannot create entry\n");
    		ret =-ENOSPC;
   	 }

	else{
    	INIT_LIST_HEAD(&my_list);
	    
   	 }

	return ret;
	
}




static void __exit list_module_exit(void){

	remove_proc_entry("modlist", NULL);
	printk(KERN_INFO "ListMod: Removed\n");
}

module_init(list_module_init);
module_exit(list_module_exit);





