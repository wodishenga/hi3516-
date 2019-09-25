#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/version.h>

#include "hi_common.h"
#include "hi_osal.h"

#define DIS_DEV_NAME_LENGTH 10

extern int DIS_ModInit(void);
extern void DIS_ModExit(void);

static int __init dis_mod_init(void)
{
    return DIS_ModInit();
}
static void __exit dis_mod_exit(void)
{
    DIS_ModExit();
}
module_init(dis_mod_init);
module_exit(dis_mod_exit);


MODULE_LICENSE("Proprietary");

