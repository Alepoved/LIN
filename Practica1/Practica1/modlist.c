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
#include "list.h"
#include <linux/string.h>


MODULE_LICENSE("GPL");


#define BUFFER_LENGTH PAGE_SIZE


struct list_item {
	int data;
	struct list_head links;
};



struct list_head my_list;


static struct proc_dir_entry *proc_entry;


static ssize_t my_write ( struct file *filp, const char __user *buf, size_t len, loff_t *off  ){
	

	char command[25];
	char *modlist;
	int my_num = -1;
	int available_space = BUFFER_LENGTH - 1;
	

	struct list_item* elem=NULL;//posicion de la lista
	struct list_head* elem_act=NULL;//posicion actual
	struct list_head* aux=NULL;// almacenamiento temporal para iterar en la lista
	modlist = (char *) vmalloc( BUFFER_LENGTH );	

	if( (*off) > 0 ){
		return 0;
	}

	if( len > available_space ){
    	return -ENOSPC;//espacio insuficiente en el dispositivo
	}

	
	if( copy_from_user( &modlist[0], buf, len ) ){
		return -EFAULT;//parametros incorrectos(origen o destino)
	}

	modlist[ len ] = '\0'; /*fin de palabra*/
	
	
	sscanf(modlist, "%s %i",command,&my_num);
	
	
	if (strcmp(command,"add") == 0){
		elem = (struct list_item *)vmalloc(sizeof(struct list_item));
		elem->data = my_num;
		list_add_tail(&elem->links, &my_list);			
	}
																																												
		
	else if (strcmp(command,"remove") == 0){	
		list_for_each_safe(elem_act,aux,&my_list) {
		elem = list_entry(elem_act, struct list_item, links);
			if( elem->data == my_num ){
				list_del( &elem->links );
				vfree(elem);
			}
		}
	}
	
	else if (strcmp(command,"cleanup") == 0){
		list_for_each_safe(elem_act,aux,&my_list) {
			elem = list_entry(elem_act, struct list_item, links);
			list_del( &elem->links );
			vfree(elem);
		}
	}
	
	vfree(modlist);
	off += len;

	return len;

}


static ssize_t my_read ( struct file *filp, char __user *buf, size_t len,loff_t *off ){
	
	int nr_bytes = 0;
	struct list_item* elem=NULL;
	struct list_head* elem_act=NULL;
	struct list_head* aux=NULL;
	char buff_kern[100];
	char *list = buff_kern;
	
	
	if( (*off) > 0 ){
		return 0;
	}
	
	list_for_each_safe(elem_act,aux,&my_list) {
		elem = list_entry(elem_act, struct list_item, links);
		list += sprintf(list,"%i\n",elem->data);
	}
	//list: punteros a cada uno de las cifras de numeros.
	//buff_kern: contiene en cada posicion una cifra(\0 incluido)
	//nr_bytes= contar las posiciones rellenas o restar posiciones memoria (direcciones)
	
	//nr_bytes = list - buff_kern;

	nr_bytes = strlen( buff_kern );

	printk(KERN_INFO "I'm going to read nr_bytes: %i",nr_bytes);
	

	if( len < nr_bytes ){
		return -ENOSPC;
	}

	
	if( copy_to_user( buf, buff_kern, nr_bytes ) ){
		return -EFAULT;
	}

	(*off) += nr_bytes;



	return nr_bytes;//bytes que hay que leer de la entrada modlist

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





