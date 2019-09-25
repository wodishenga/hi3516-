/******************************************************************************

  Copyright (C), 2017, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : vedu_init.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2017
  Description   :
******************************************************************************/


#include <linux/module.h>
#include <linux/of_platform.h>
#include "hi_type.h"
#include "hi_defines.h"
#include "hi_osal.h"


#define VEDU_DEV_NAME_LENGTH 10

extern void *s_apVeduAddr[VEDU_IP_NUM];
extern unsigned int vedu_irq[VEDU_IP_NUM];
extern void * g_pstJpgeCHIP_Reg[VEDU_IP_NUM];
extern unsigned int  jpeg_irq[VEDU_IP_NUM];

extern int VPU_ModInit(void);
extern void VPU_ModExit(void);


static int hi35xx_vedu_probe(struct platform_device *pdev)
{
    HI_U32 i;
    struct resource *mem;
    HI_CHAR VeduDevName[VEDU_DEV_NAME_LENGTH] = {'\0'};


    /* if vedu num more than 1, use "platform_get_resource_byname" function to replace */
    for(i=0; i< VEDU_IP_NUM; i++)
    {
        snprintf(VeduDevName, VEDU_DEV_NAME_LENGTH, "vedu%d", i);
        mem = osal_platform_get_resource_byname(pdev, IORESOURCE_MEM, VeduDevName);

        s_apVeduAddr[i] = (void *)devm_ioremap_resource(&pdev->dev, mem);
        if (IS_ERR(s_apVeduAddr[i]))
        {
            osal_printk("%s,%d,remap vedu(%d) IRQ!!\n",__FUNCTION__,__LINE__,i);
            return PTR_ERR(s_apVeduAddr[i]);
        }

        vedu_irq[i] = (unsigned int)osal_platform_get_irq_byname(pdev, VeduDevName);
        if (vedu_irq[i] <= 0) {
            osal_printk("%s,%d,cannot find vedu(%d) IRQ!!\n",__FUNCTION__,__LINE__,i);
                dev_err(&pdev->dev, "cannot find vedu IRQ\n");
                return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}


static int hi35xx_jpge_probe(struct platform_device *pdev)
{
    struct resource *mem;
    HI_CHAR JpgeDevName[VEDU_DEV_NAME_LENGTH] = {'\0'};

    snprintf(JpgeDevName, VEDU_DEV_NAME_LENGTH, "jpge");

    mem = osal_platform_get_resource_byname(pdev, IORESOURCE_MEM, JpgeDevName);

    g_pstJpgeCHIP_Reg[0] = (void *)devm_ioremap_resource(&pdev->dev, mem);
    if (IS_ERR(g_pstJpgeCHIP_Reg[0]))
    {
        osal_printk("%s,%d,remap jpge err!!\n",__FUNCTION__,__LINE__);
        return PTR_ERR(g_pstJpgeCHIP_Reg[0]);
    }

    jpeg_irq[0] = osal_platform_get_irq_byname(pdev, JpgeDevName);
    if (jpeg_irq[0] <= 0) {
            osal_printk("%s,%d,cannot find jpge IRQ!!\n",__FUNCTION__,__LINE__);
            dev_err(&pdev->dev, "cannot find jpge IRQ\n");
            return HI_FAILURE;
    }

    return HI_SUCCESS;
}


static int hi35xx_vpu_probe(struct platform_device *pdev)
{
    HI_S32 s32Ret;

    s32Ret = hi35xx_vedu_probe(pdev);
    if(HI_SUCCESS != s32Ret)
    {
        osal_printk("%s,%d,vedu probe err!!\n",__FUNCTION__,__LINE__);
        return s32Ret;
    }

    s32Ret = hi35xx_jpge_probe(pdev);
    if(HI_SUCCESS != s32Ret)
    {
        osal_printk("%s,%d,jpge probe err!!\n",__FUNCTION__,__LINE__);
        return s32Ret;
    }

    s32Ret = VPU_ModInit();
    if(HI_SUCCESS != s32Ret)
    {
        osal_printk("%s,%d,VPU ModInit err!!\n",__FUNCTION__,__LINE__);
        return s32Ret;
    }

    return s32Ret;
}

static int hi35xx_vpu_remove(struct platform_device *pdev)
{
    VPU_ModExit();
    return 0;
}

static const struct of_device_id hi35xx_vpu_match[] = {
        { .compatible = "hisilicon,hisi-vedu" },
        {},
};
MODULE_DEVICE_TABLE(of, hi35xx_vpu_match);

static struct platform_driver hi35xx_vpu_driver = {
        .probe          = hi35xx_vpu_probe,
        .remove         = hi35xx_vpu_remove,
        .driver         = {
                .name   = "hi35xx_vedu",
                .of_match_table = hi35xx_vpu_match,
        }
};

osal_module_platform_driver(hi35xx_vpu_driver);
MODULE_LICENSE("Proprietary");


