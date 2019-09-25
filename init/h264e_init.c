/******************************************************************************

  Copyright (C), 2017, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : h264e_init.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2017
  Description   :
******************************************************************************/



#include <linux/module.h>

extern int H264E_ModInit(void);
extern void H264E_ModExit(void);


static int __init h264e_mod_init(void)
{
    return H264E_ModInit();
}
static void __exit h264e_mod_exit(void){
    H264E_ModExit();
}

module_init(h264e_mod_init);
module_exit(h264e_mod_exit);

MODULE_LICENSE("Proprietary");


