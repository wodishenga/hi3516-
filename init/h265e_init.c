/******************************************************************************

  Copyright (C), 2017, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : h265e_init.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2017
  Description   :
******************************************************************************/


#include <linux/module.h>


extern int H265E_ModInit(void);
extern void H265E_ModExit(void);

static int __init h265e_mod_init(void){

    return H265E_ModInit();
}
static void __exit h265e_mod_exit(void){
    H265E_ModExit();
}

module_init(h265e_mod_init);
module_exit(h265e_mod_exit);

MODULE_LICENSE("Proprietary");



