#include <linux/module.h>
#include <linux/of_platform.h>

#include "hi_type.h"
#include "hi_defines.h"
#include "hi_osal.h"

extern int JPEGD_ModInit(void);
extern void JPEGD_ModExit(void);
extern HI_VOID* g_pstJpegdCHIP_Reg[JPEGD_IP_NUM];
extern HI_U32 g_JpegdIrq[JPEGD_IP_NUM];

#define JPEGD_IP_NAME_LENGTH 10

static HI_S32 JPEGD_DevInit(struct platform_device* pdev)
{
    HI_U32 i;
    HI_CHAR JpegdIpName[JPEGD_IP_NAME_LENGTH] = {'\0'};
    struct resource* mem;

    for (i = 0; i < JPEGD_IP_NUM; i++)
    {
        //snprintf(JpegdIpName, JPEGD_IP_NAME_LENGTH, "jpegd%d", i);
        snprintf(JpegdIpName, JPEGD_IP_NAME_LENGTH, "jpegd");
        mem = osal_platform_get_resource_byname(pdev, IORESOURCE_MEM, JpegdIpName);
        g_pstJpegdCHIP_Reg[i] = devm_ioremap_resource(&pdev->dev, mem);

        if (IS_ERR(g_pstJpegdCHIP_Reg[i]))
        {
            osal_printk("devm_ioremap_resource fail for %ld\n", PTR_ERR(g_pstJpegdCHIP_Reg[i]));
            return PTR_ERR(g_pstJpegdCHIP_Reg[i]);
        }

        g_JpegdIrq[i] = osal_platform_get_irq_byname(pdev, JpegdIpName);

        if (g_JpegdIrq[i] <= 0)
        {
            dev_err(&pdev->dev, "cannot find jpegd%d IRQ\n", i);
            return HI_FAILURE;
        }
    }

    return 0;
}

static HI_VOID JPEGD_DevExit(HI_VOID)
{
    HI_U32 i;

    for (i = 0; i < JPEGD_IP_NUM; i++)
    {
        g_pstJpegdCHIP_Reg[i] = NULL;
    }
}

static int hi35xx_jpegd_probe(struct platform_device* pdev)
{
    HI_S32 s32Ret = HI_SUCCESS;

    s32Ret = JPEGD_DevInit(pdev);
    if(HI_SUCCESS != s32Ret)
    {
        osal_printk("JPEGD_RegisterDev fail!\n");
        return s32Ret;
    }

    s32Ret = JPEGD_ModInit();

    return s32Ret;
}

static int hi35xx_jpegd_remove(struct platform_device* pdev)
{
    JPEGD_ModExit();
    JPEGD_DevExit();

    return HI_SUCCESS;
}





static const struct of_device_id hi35xx_jpegd_match[] =
{
    { .compatible = "hisilicon,hisi-jpegd" },
    {},
};
MODULE_DEVICE_TABLE(of, hi35xx_jpegd_match);

static struct platform_driver hi35xx_jpegd_driver =
{
    .probe          = hi35xx_jpegd_probe,
    .remove         = hi35xx_jpegd_remove,
    .driver         = {
        .name           = "hi35xx_jpegd",
        .of_match_table = hi35xx_jpegd_match,
    },
};

osal_module_platform_driver(hi35xx_jpegd_driver);









