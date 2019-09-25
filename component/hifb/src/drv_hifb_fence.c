/*
* Copyright (c) 2018 HiSilicon Technologies Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
*/
#ifdef CONFIG_HIFB_FENCE_SUPPORT
#include <linux/file.h>
#include "drv_hifb_fence.h"

/********************** Global Variable declaration **********************************************/

HIFB_SYNC_INFO_S gs_SyncInfo;

HI_S32 DRV_HIFB_FENCE_Init(HIFB_PAR_S *par)
{
    if (NULL == par)
    {
        return HI_FAILURE;
    }

    if (!(IS_HD_LAYER(par->u32LayerID)))
    {
        return HI_SUCCESS;
    }

    gs_SyncInfo.FenceValue     = 0;
    gs_SyncInfo.ReleaseFenceFd = -1;

    gs_SyncInfo.pstTimeline = hi_sw_sync_timeline_create(HIFB_SYNC_NAME);
    par->pFenceRefreshWorkqueue = create_singlethread_workqueue("HIFB_REFRESH_WorkQueque");
    if (NULL == par->pFenceRefreshWorkqueue)
    {
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_VOID DRV_HIFB_FENCE_DeInit(HIFB_PAR_S *par)
{
    if (NULL == par)
    {
        return ;
    }

    if (NULL != gs_SyncInfo.pstTimeline)
    {
        hi_sync_timeline_destroy((struct sync_timeline*)gs_SyncInfo.pstTimeline);
    }
    gs_SyncInfo.pstTimeline = NULL;
    if (NULL != par->pFenceRefreshWorkqueue)
    {
        destroy_workqueue(par->pFenceRefreshWorkqueue);
    }
    par->pFenceRefreshWorkqueue = NULL;
    return ;
}

HI_S32 DRV_HIFB_FENCE_Create(HI_VOID)
{
    HI_S32 FenceFd   = -1;

    HI_U32 FenceValue = 0;
    struct hi_sync_fence *fence = NULL;
    struct sync_pt *pt = NULL;

    if (NULL == gs_SyncInfo.pstTimeline)
    {
        return HI_FAILURE;
    }

    FenceFd = get_unused_fd_flags(0);
    if (FenceFd < 0)
    {
        return FenceFd;
    }
    gs_SyncInfo.ReleaseFenceFd = FenceFd;

    FenceValue = ++(gs_SyncInfo.FenceValue);
    pt = hi_sw_sync_pt_create(gs_SyncInfo.pstTimeline, FenceValue);
    if (NULL == pt)
    {
        return HI_FAILURE;
    }

    gs_SyncInfo.pt = pt;

    fence = hi_sync_fence_create(pt);
    if (NULL == fence)
    {
        printk("------------%s:%d\n", __FUNCTION__, __LINE__);
        hi_sync_pt_free(pt);
        return -ENOMEM;
    }
    hi_sync_fence_install(fence, FenceFd);

    return FenceFd;
}

HI_VOID DRV_HIFB_FENCE_IncRefreshTime()
{
    hi_sw_sync_timeline_inc(gs_SyncInfo.pstTimeline, 1);
    return;
}

#endif




