/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include "sample_comm.h"

extern int check_endian();
extern HI_U16 byte2hex(char *pData);
extern HI_U16 hex2ascii(HI_U8 data_hex);
extern HI_U8 ascii2hex(char ascii);
extern HI_U16 get_len(HI_U8 *pData);
extern void convert_len_to_assi(HI_U16 len, char *assi_len);
extern HI_U16 check_sum(HI_U8 *data, HI_U32 len);
extern void hex2byte(HI_U16 hex, HI_U8 *bytes);
extern char hex2char(HI_U8 hex, HI_U8 flag);
extern void hex2string(char *string, char *hex, HI_U16 len, HI_U8 flag);

extern int get_mac_addr(char * macAddr);
extern int get_mqtt_password(char *macAddr, char *passwordMd5);
extern int get_mqtt_username(char    * macAddr, char *username);
extern int get_mqtt_clientid(char    * macAddr, char *client);
extern int get_mqtt_pubTopic(char    * macAddr, char *pubTopic);
extern int get_mqtt_subTopic(char *macAddr, char *subTopic);

extern void get_warning_msg(char *pubmsg);



extern void get_pub_Msg(char *pubmsg);

















#endif /* PROJECT_CONF_H_ */
