/******************************************************************************

  Copyright (C), 2017, Hisilicon. Co., Ltd.

 ******************************************************************************
 * File Name     : hdmi_init.c
 * Version       : Initial Draft
 * Author        : Hisilicon multimedia software group
 * Created       : 2017/8/15
 * Description   :
 * History       :
 * Date          : 2017/8/15
 * Author        :
 * Modification  :
 *******************************************************************************
*/
#include <linux/module.h>
#include <linux/of_platform.h>

#include "hi_defines.h"
#include "hi_type.h"
#include "hi_osal.h"

#define HDMI_DEV_NAME_LENGTH    16
extern HI_CHAR *g_pstHdmiReg;

extern int HDMI_DRV_ModInit(void);
extern void HDMI_DRV_ModExit(void);

static int hi35xx_hdmi_probe(struct platform_device *pdev)
{
    struct resource* mem = NULL;
    HI_CHAR acHdmiDevName[HDMI_DEV_NAME_LENGTH] = "hdmi0";

    mem = osal_platform_get_resource_byname(pdev, IORESOURCE_MEM, acHdmiDevName);
    g_pstHdmiReg = devm_ioremap_resource(&pdev->dev, mem);

    if (IS_ERR(g_pstHdmiReg))
    {
        return PTR_ERR(g_pstHdmiReg);
    }

    HDMI_DRV_ModInit();

    return 0;
}

static int hi35xx_hdmi_remove(struct platform_device* pdev)
{
    HDMI_DRV_ModExit();
    g_pstHdmiReg = NULL;

    return 0;
}

static const struct of_device_id hi35xx_hdmi_match[] =
{
    {.compatible = "hisilicon,hisi-hdmi"},
    {},
};

MODULE_DEVICE_TABLE(of, hi35xx_hdmi_match);

static struct platform_driver hi35xx_hdmi_driver =
{
    .probe      = hi35xx_hdmi_probe,
    .remove     = hi35xx_hdmi_remove,
    .driver     = {
        .name           = "hi35xx_hdmi",
        .of_match_table = hi35xx_hdmi_match,
    },
};

osal_module_platform_driver(hi35xx_hdmi_driver);

MODULE_LICENSE("Proprietary");

