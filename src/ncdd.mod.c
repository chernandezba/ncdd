#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__attribute_used__
__attribute__((section("__versions"))) = {
	{ 0x8f78fa6f, "struct_module" },
	{ 0x8ecc3ceb, "wake_up_process" },
	{ 0xa03d6a57, "__get_user_4" },
	{ 0x932da67e, "kill_proc" },
	{ 0x3f5a71d, "kmem_cache_alloc" },
	{ 0xaa87002a, "malloc_sizes" },
	{ 0xbf8c5785, "cdev_add" },
	{ 0x55da4577, "cdev_init" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0xe9a525c5, "cdev_del" },
	{ 0x37a0cba, "kfree" },
	{ 0x2f287f0d, "copy_to_user" },
	{ 0xd6c963c, "copy_from_user" },
	{ 0x4292364c, "schedule" },
	{ 0x534c289, "per_cpu__current_task" },
	{ 0x1b7d4074, "printk" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "F096FE59BE042771987CCA8");
