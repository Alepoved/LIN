#include <linux/syscalls.h> /* For SYSCALL_DEFINEi() */
#include <linux/kernel.h>
#include <asm-generic/errno.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>
#include <asm-generic/errno-base.h>
#include <linux/errno.h>

struct tty_driver* kbd_driver= NULL;  /* Driver of leds */

/* Get driver handler */
struct tty_driver* get_kbd_driver_handler(void){
   printk(KERN_INFO "modleds: loading\n");
   printk(KERN_INFO "modleds: fgconsole is %x\n", fg_console);
   return vc_cons[fg_console].d->port.tty->driver;
}

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}

SYSCALL_DEFINE1(led_ctl,unsigned int,leds){
   

    unsigned int kernel_leds = 0;

	if((leds<0)||(leds>7))return -EINVAL;

	kbd_driver = get_kbd_driver_handler();

	if(kbd_driver==NULL) return -ENODEV;

	if (leds & 0x1) kernel_leds |= 0x1;
	if (leds & 0x4) kernel_leds |= 0x2;
	if (leds & 0x2) kernel_leds |= 0x4;

return set_leds(kbd_driver,kernel_leds);
}


