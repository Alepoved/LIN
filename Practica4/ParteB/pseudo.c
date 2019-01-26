

static int fifoproc_open(struct inode *inode,struct file *file){
	lock(mtx);
	if (modo lectura){
		/* Un consumidor abrió el FIFO*/
		
		cons_count++;
		
		cond_signal(prod);	
		
		while(prod_count<=0) {
			cond_wait(prod,mtx);
		}	
	} 
	else{
		/* Un productor abrió el FIFO*/
		prod_count++;
		
		cond_signal(cons);		

		while(cons_count<=0) {
			cond_wait(cons,mtx);
		}	
unlock(mtx);
return 0;
}

int fifoproc_release(struct inode *inode,struct file *file){
	
	lock(mtx);
	if(modo_lectura){
		cons_count--;
		cond_signal(prod);			
	}
	else {
		prod_count--;
		cond_signal(cons);	
	}

	if(prod_count == 0 && cons_count == 0){
		kfifo_reset(&cbuffer);
	}

	unlock(mtx);
}


int fifoproc_write(char* buff,int len) {
	char kbuffer[MAX_KBUF];

	if (len> MAX_CBUFFER_LEN || len> MAX_KBUF) {
		return -EFAULT;
	}

	if (copy_from_user(kbuffer,buff,len)) {
		return -EFAULT;
	}
	
	
	lock(&mtx); 
		

	/* Esperar hasta que haya hueco para insertar(debe haber consumidores) */
	while(kfifo_avail(&cbuffer) < len && cons_count > 0){ 
		cond_wait(prod,mtx);
	}

	/* Detectar fin de comunicación por error(consumidorcierraFIFOantes) */
	if (cons_count==0) {
		unlock(mtx);
		return -EPIPE;

	}

	kfifo_in(&cbuffer,kbuffer,len);/*insertar elementos*/

	/* Despertara posible consumidor bloqueado*/
	
	cond_signal(cons);
	
	unlock(mtx);
	

return len;
}

int fifoproc_read(char* buff,int len){
	char kbuffer[MAX_KBUF];

	if (len> MAX_CBUFFER_LEN || len> MAX_KBUF) {
		return -EFAULT;
	}

	if (copy_from_user(kbuffer,buff,len)) {
		return -EFAULT;
	}
	
	
	lock(&mtx); 
		
	
	while(kfifo_len(&cbuffer) < len && cons_count > 0){ 
		cond_wait(cons,mtx);
	}

	/* Detectar fin de comunicación por error(productor cierra FIFO antes) */
	if (prod_count==0 && kfifo_is_empty(&cbuffer)) {
		unlock(mtx);
		return 0;
	}
	
	
	out = kfifo_out(&cbuffer,kbuffer,len);/*sacar elementos*/
	
	
	cond_signal(prod);
		

	up( &mtx );

	if( copy_to_user( buf, kbuffer, len ) ){
		return -EFAULT;
	}
	
}

