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
#ifndef __DRV_HIFB_FENCE_H__
#define __DRV_HIFB_FENCE_H__


#ifdef CONFIG_HIFB_FENCE_SUPPORT
#include <linux/spinlock.h>
#include "hifb_main.h"
#include "hi_type.h"
#include "sw_sync.h"

#ifdef __cplusplus
#if __cplusplus
   extern "C"
{
#endif
#endif /* __cplusplus */

/***************************** Macro Definition **************************************************/
#define HIFB_SYNC_NAME               "hifb_fence"

#define hi_sync_fence                           sync_file
#define hi_sync_fence_install(fence,fence_fd)   fd_install(fence_fd, fence->file)
#define hi_sync_fence_create(sync_pt)           sync_file_create(&sync_pt->base)

/*************************** Structure Definition ************************************************/

typedef struct
{
    HI_U32   FenceValue;
    HI_S32   ReleaseFenceFd;
    struct sw_sync_timeline *pstTimeline;
    struct sync_pt *pt;
}HIFB_SYNC_INFO_S;

/******************************* API declaration *************************************************/

HI_S32 DRV_HIFB_FENCE_Init(HIFB_PAR_S *par);
HI_VOID DRV_HIFB_FENCE_DeInit(HIFB_PAR_S *par);
HI_S32 DRV_HIFB_FENCE_Create(HI_VOID);
HI_VOID DRV_HIFB_FENCE_IncRefreshTime(HI_VOID);

/*************************************************************************************************/

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
