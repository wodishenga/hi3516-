/******************************************************************************

  Copyright (C), 2017, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : sample_venc.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2017
  Description   :
******************************************************************************/

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "sample_comm.h"
#include <sys/time.h>
#include "sample_md5.h"
#include "sample_cjson.h"
#include "sample_protocol.h"



extern char macAddr[30];



HI_U8 hex2char(HI_U8 hex, HI_U8 flag)
{
	if (hex <= 9)
		return hex + '0';
	if (hex >= 10 && hex <= 15)
	{
		if (flag)
			return (hex - 10 + 'A');
		else
			return (hex - 10 + 'a');
	}
	return '0';
}


HI_VOID hex2string(HI_U8 *string, HI_U8 *hex, HI_U16 len, HI_U8 flag)
{
	for (HI_U8 i = 0; i < len; i++)
	{
		string[i * 2] = hex2char(hex[i] >> 4 & 0x0f, flag);
		string[i * 2 + 1] = hex2char(hex[i] & 0x0f, flag);
	}
	string[len * 2] = '\0';
}


HI_VOID delete_char(HI_U8 str[],HI_U8 target){
	int i,j;
	for(i=j=0;str[i]!='\0';i++){
		if(str[i]!=target){
			str[j++]=str[i];
		}
	}
	str[j]='\0';
}

HI_S32 get_mac_addr(HI_U8 * macAddr)
{
	FILE* f_mac = NULL;
	if((f_mac = fopen("/root/mac.txt","r")) == NULL)
		{
			SAMPLE_PRT("file open error\n");
			return -1;
		}
	fseek(f_mac, 4, SEEK_SET);
	
	fread(macAddr,17,1,f_mac);
	
	
	delete_char(macAddr,':');
	SAMPLE_PRT("macAddr=%s\n", macAddr);
	fclose(f_mac);
	
	return 0;
}


HI_S32 get_mqtt_password(HI_U8 *macAddr, HI_U8 *passwordMd5)
{
	
	HI_U8  password[30] = "0";
	HI_U8 md51[16];
	if (strlen(macAddr) > 0) 
	{
	
		strcpy(password, macAddr);
		strcat((char *)password, "_zdst666");
		md5((const char *)password, strlen((const char *)password), md51);
		hex2string((char *)passwordMd5, md51, 16, 0);
		return 0;
	}
	return -1;
}

HI_S32 get_mqtt_username(HI_U8    * macAddr, HI_U8 *username)
{
	
	if (strlen(macAddr) > 0) 
	{
		strcpy(username, macAddr);
		strcat((char *)username, "_account");
		printf("%s\n", username);
		return 0;
	}
	return -1;
	
}

HI_S32 get_mqtt_clientid(HI_U8    * macAddr, HI_U8 *client)
{
	if (strlen(macAddr) > 0) 
	{
		strcpy(client, macAddr);
		strcat((char *)client, "_client");
		return 0;
	}
	return -1;

}

HI_S32 get_mqtt_pubTopic(HI_U8    * macAddr, HI_U8 *pubTopic)
{
	
	if (strlen(macAddr)> 0) //ti设备，设备ID长度是9个字节
	{
		strcpy((char *)pubTopic, "/BOX/");
		strcat((char *)pubTopic, macAddr);
		strcat((char *)pubTopic, "/pub");
		return 0;
	}
	return -1;
}

HI_S32 get_mqtt_subTopic(HI_U8 *macAddr, HI_U8  *subTopic)	
{
	if (strlen(macAddr)> 0) //ti设备，设备ID长度是9个字节
	{
		strcpy((char *)subTopic, "/BOX/");
		strcat((char *)subTopic, macAddr);
		strcat((char *)subTopic, "/sub");
		return 1;
	}
	return -1;
}


HI_VOID get_pub_Msg(char *pubmsg)
{
	cJSON *PubDTO; char *out;
	PubDTO=cJSON_CreateObject();
	cJSON_AddNumberToObject(PubDTO,"pub_tp",		1);
	
	cJSON_AddNumberToObject(PubDTO,"timestamp",		1566982917306);

	cJSON_AddStringToObject(PubDTO,"sign",		"d326ed20de22b9ffe0c0ebc725a8413b");

	out=cJSON_Print(PubDTO);	cJSON_Delete(PubDTO); printf("%s\n",out);

	//sprintf(pubmsg, "%s", out);
	strcpy(pubmsg, out);

	free(out);
	
}

HI_VOID get_time(char *time_now)
{
 struct tm nowtime;
 struct timeval tv;
 gettimeofday(&tv, NULL);
 localtime_r(&tv.tv_sec,&nowtime);

 sprintf(time_now,"%d-%d-%d %d:%d:%d",
    nowtime.tm_year+1900,
    nowtime.tm_mon+1,
    nowtime.tm_mday,
    nowtime.tm_hour,
    nowtime.tm_min,
    nowtime.tm_sec
 );
 printf("current time is %s\n",time_now);
}

HI_VOID get_pubContent_msg(char *pubContent)
{
	cJSON *PubContentDTO,*AlarmDTO; char *out;
	PubContentDTO=cJSON_CreateObject();
	cJSON_AddItemToObject(PubContentDTO, "alarm", AlarmDTO=cJSON_CreateObject());
	cJSON_AddNumberToObject(AlarmDTO,"statu", 	8);
	char time_now[30];
	get_time(time_now);
	cJSON_AddStringToObject(AlarmDTO,"alarm_time", 	time_now);
	cJSON_AddStringToObject(AlarmDTO,"mac", 	macAddr);
	out=cJSON_Print(PubContentDTO);	cJSON_Delete(PubContentDTO); printf("%s\n",out);
		
			//sprintf(pubmsg, "%s", out);
	strcpy(pubContent, out);
		
	free(out);
}

void get_warning_msg(char *pubmsg)
{
	cJSON *PubDTO,*PubContentDTO,*AlarmDTO; char *out;
	PubDTO=cJSON_CreateObject();
	cJSON_AddNumberToObject(PubDTO,"pub_tp",		4);
	cJSON_AddNumberToObject(PubDTO,"timestamp",		1566982917306);

	
	cJSON_AddItemToObject(PubDTO, "pub_content", PubContentDTO=cJSON_CreateObject());
	cJSON_AddItemToObject(PubContentDTO, "alarm", AlarmDTO=cJSON_CreateObject());
	cJSON_AddNumberToObject(AlarmDTO,"statu", 	8);
	char time_now[30];
	get_time(time_now);
	cJSON_AddStringToObject(AlarmDTO,"alarm_time", 	time_now);
	cJSON_AddStringToObject(AlarmDTO,"mac", 	macAddr);
	cJSON_AddStringToObject(PubDTO,"sign",		"d326ed20de22b9ffe0c0ebc725a8413b");
	out=cJSON_Print(PubDTO);	cJSON_Delete(PubDTO); printf("%s\n",out);

	//sprintf(pubmsg, "%s", out);
	strcpy(pubmsg, out);

	free(out);
	
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */



