#include <linux/module.h>
#include <linux/kernel.h>
#include "hi_type.h"

extern int VENC_ModInit(void);
extern void VENC_ModExit(void);


extern HI_U32 VencBufferCache;
extern HI_U32 FrameBufRecycle;
extern HI_U32 VencMaxChnNum;

module_param( VencMaxChnNum, uint, S_IRUGO);
EXPORT_SYMBOL(VencBufferCache);
EXPORT_SYMBOL(FrameBufRecycle);

static int __init venc_mod_init(void){
    VENC_ModInit();
    return 0;
}
static void __exit venc_mod_exit(void){
    VENC_ModExit();
}

module_init(venc_mod_init);
module_exit(venc_mod_exit);

MODULE_LICENSE("Proprietary");




