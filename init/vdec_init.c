#include <linux/module.h>
#include "hi_type.h"
#include "vdec_exp.h"

extern int VDEC_ModInit(void);
extern void VDEC_ModExit(void);

extern VDEC_EXPORT_SYMBOL_S  g_stVdecExpSymbol;

EXPORT_SYMBOL(g_stVdecExpSymbol);

extern HI_U32 VdecMaxChnNum;
module_param(VdecMaxChnNum, uint, S_IRUGO);

extern HI_U32 VdecDebugMode;
module_param(VdecDebugMode, uint, S_IRUGO);

static int __init vdec_mod_init(void){
    VDEC_ModInit();
    return 0;
}
static void __exit vdec_mod_exit(void){
    VDEC_ModExit();
}

module_init(vdec_mod_init);
module_exit(vdec_mod_exit);

MODULE_LICENSE("Proprietary");
