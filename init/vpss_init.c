#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <linux/of_platform.h>

#include "hi_common.h"
#include "hi_osal.h"
#include "hi_defines.h"

extern int VPSS_ModInit(void);
extern void VPSS_ModExit(void);

extern void * pVpssReg[VPSS_IP_NUM];
extern unsigned int vpss_irq[VPSS_IP_NUM];

static int hi35xx_vpss_probe(struct platform_device *pdev)
{
    struct resource* mem;

    mem = osal_platform_get_resource(pdev, IORESOURCE_MEM, 0);
    pVpssReg[0] = devm_ioremap_resource(&pdev->dev, mem);

    if (IS_ERR(pVpssReg[0]))
    { return PTR_ERR(pVpssReg[0]); }

    vpss_irq[0] = osal_platform_get_irq(pdev, 0);

    //printk("++++++++++ pVpssReg[0] = %p vpss_irq[0] = %d\n",pVpssReg[0], vpss_irq[0]);

    if (vpss_irq[0] <= 0)
    {
        dev_err(&pdev->dev, "cannot find vpss IRQ\n");
    }

    VPSS_ModInit();

    return 0;
}

static int hi35xx_vpss_remove(struct platform_device *pdev)
{
    VPSS_ModExit();

    pVpssReg[0] = HI_NULL;

    return 0;
}


static const struct of_device_id hi35xx_vpss_match[] = {
        { .compatible = "hisilicon,hisi-vpss" },
        {},
};
MODULE_DEVICE_TABLE(of, hi35xx_vpss_match);

static struct platform_driver hi35xx_vpss_driver = {
        .probe          = hi35xx_vpss_probe,
        .remove         = hi35xx_vpss_remove,
        .driver         = {
                .name   = "hi35xx_vpss",
                .of_match_table = hi35xx_vpss_match,
        },
};

osal_module_platform_driver(hi35xx_vpss_driver);

MODULE_LICENSE("Proprietary");
