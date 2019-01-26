
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


#define BUFFER_LENGTH       PAGE_SIZE
//#define numeros 

struct list_item {
#ifdef numeros
	int data;
#else
	char *word;
#endif
	
	struct list_head links;
};


struct list_head my_list;


static struct proc_dir_entry *proc_entry;



static ssize_t my_write ( struct file *filp, const char __user *buf, size_t len, loff_t *off  ){
	
	char *modlist;
	char command[25];
#ifdef numeros
	int my_num = -1;
#else
	char *my_string;
#endif
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
	
#ifdef numeros
	sscanf(modlist, "%s %i",command,&my_num);
#else
	my_string = (char *) vmalloc( BUFFER_LENGTH );
	sscanf(modlist, "%s",command);
#endif
	
	
	
	if (strcmp(command,"add") == 0){
#ifdef numeros
		elem = (struct list_item *)vmalloc(sizeof(struct list_item));
		elem->data = my_num;
		list_add_tail(&elem->links, &my_list);
#else
		elem = (struct list_item *)vmalloc(sizeof(struct list_item));
		my_string=modlist+4;
		if ((strlen(my_string) > 0) && (my_string[strlen(my_string)-1] == '\n')){//evita que genere salto linea al escribir
        		my_string[strlen(my_string)-1] = '\0';
		}
		elem->word=vmalloc(strlen(my_string)+1);		
		strcpy(elem->word,my_string);
		list_add_tail(&elem->links, &my_list);
#endif
				
	}
	
		
	else if (strcmp(command,"remove") == 0){
			
		list_for_each_safe(elem_act,aux,&my_list) {
		elem = list_entry(elem_act, struct list_item, links);
#ifdef numeros
			if( elem->data == my_num ){
				list_del( &elem->links );
				vfree( elem );
			}
#else
			my_string=modlist+7;
			if ((strlen(my_string) > 0) && (my_string[strlen(my_string)-1] == '\n')){//evita que genere salto linea al escribir
        			my_string[strlen(my_string)-1] = '\0';
			}
			if(strcmp(my_string,elem->word) == 0){
				list_del( &elem->links );
				vfree( elem );
			}
#endif	
		}
	}
	
	else if (strcmp(command,"cleanup") == 0){
		
		list_for_each_safe(elem_act,aux,&my_list) {
			elem = list_entry(elem_act, struct list_item, links);
			list_del( &elem->links );
			vfree( elem );
		}
	}
	
	vfree(modlist);
	vfree(my_string);
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
#ifdef numeros
		list += sprintf(list,"%i\n",elem->data);
#else
		list += sprintf(list,"%s\n",elem->word);
#endif
	}
	
	nr_bytes = strlen( buff_kern );

	printk(KERN_INFO "ACABO DE LEER nr_bytes: %i",nr_bytes);
	

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





