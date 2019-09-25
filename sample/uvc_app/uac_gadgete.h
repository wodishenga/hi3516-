/*
 * uac interface
 */

#ifndef __UAC_GADGETE_H__
#define __UAC_GADGETE_H__

int open_uac_device(const char *devpath);
int close_uac_device();
int run_uac_device();

#endif //__UAC_GADGETE_H__
