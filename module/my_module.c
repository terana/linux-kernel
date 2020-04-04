#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Somebody");
MODULE_DESCRIPTION("My super cool module");
MODULE_VERSION("1.0");


static int my_module_init(void) {
    printk(KERN_INFO "Hello there\n");
    return 0;
}
static void my_module_exit(void) {
    printk(KERN_INFO "Oh no!\n");
}

module_init(my_module_init);
module_exit(my_module_exit);