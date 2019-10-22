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

static HI_U8 
hex2char(HI_U8 hex, HI_U8 flag)
{
	if (hex <= 9)
		return hex + '0';
	if (hex >= 10 && hex <= 15) {
		if (flag)
			return (hex - 10 + 'A');
		else
			return (hex - 10 + 'a');
	}
	return '0';
}


static HI_VOID 
hex2string(char *string, char *hex, HI_U16 len, char flag)
{
	for (HI_U8 i = 0; i < len; i++) {
		string[i * 2] = hex2char(hex[i] >> 4 & 0x0f, flag);
		string[i * 2 + 1] = hex2char(hex[i] & 0x0f, flag);
	}
	string[len * 2] = '\0';
}


static HI_VOID
delete_char(char str[],char target)
{
	int i,j;
	for (i = j = 0; str[i] != '\0'; i++) {
		if(str[i] != target) {
			str[j++]=str[i];
		}
	}
	str[j]='\0';
}


HI_S32 
get_mac_addr(char * macAddr)
{
	FILE* f_mac = NULL;
	if((f_mac = fopen("/root/mac.txt","r")) == NULL) {
		printf("Failed to open mac.txt\n");
		return -1;
	}
	
	if (fseek(f_mac, 4, SEEK_SET) != 0) {
		printf("Failed to fseek\n");
		return -1;
	};

	if (fread(macAddr,17,1,f_mac) == 0) {
		printf("Failed to fread\n");
		return -1;
	};
	
	delete_char(macAddr,':');
	printf("macAddr=%s\n", macAddr);
	fclose(f_mac);
	
	return 0;
}


HI_S32
get_mqtt_password(char *macAddr, char *passwordMd5)
{
	
	char  password[30] = "0";
	char md51[16];
	if (strlen((char *)macAddr) > 0) {
		strcpy(password, macAddr);
		strcat((char *)password, "_zdst666");
		md5((const char *)password, strlen((const char *)password), md51);
		hex2string((char *)passwordMd5, md51, 16, 0);
		return 0;
	}
	return -1;
}


HI_S32
get_mqtt_username(char *macAddr, char *username)
{
	if (strlen(macAddr) > 0) {
		strcpy(username, macAddr);
		strcat((char *)username, "_account");
		printf("%s\n", username);
		return 0;
	}
	return -1;
}


HI_S32
get_mqtt_clientid(char    * macAddr, char *client)
{
	if (strlen(macAddr) > 0) {
		strcpy(client, macAddr);
		strcat((char *)client, "_client");
		return 0;
	}
	return -1;
}


HI_S32
get_mqtt_pubTopic(char    * macAddr, char *pubTopic)
{
	if (strlen(macAddr)> 0) {
		strcpy((char *)pubTopic, "/SMARTCAMERA/");
		strcat((char *)pubTopic, macAddr);
		strcat((char *)pubTopic, "/pub");
		return 0;
	}
	return -1;
}


HI_S32 
get_mqtt_subTopic(char *macAddr, char  *subTopic)	
{
	if (strlen(macAddr)> 0) {
		strcpy((char *)subTopic, "/SMARTCAMERA/");
		strcat((char *)subTopic, macAddr);
		strcat((char *)subTopic, "/sub");
		return 0;
	}
	return -1;
}


HI_VOID
get_time(char *time_now)
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
}

static void
get_pub_content(char *pub_content)
{
	char date[30] = "0";
	char *msg;
	cJSON *ContentDTO,*alarmDTO; 
	memset(pub_content, 0 , 100);
	get_time(date);
	
	ContentDTO=cJSON_CreateObject();
	cJSON_AddItemToObject(ContentDTO, "alarm", alarmDTO=cJSON_CreateObject());
	
	cJSON_AddStringToObject(alarmDTO,"alarm_time", 	date);
	cJSON_AddNumberToObject(alarmDTO,"channel", 	0);
	cJSON_AddStringToObject(alarmDTO,"mac", 	macAddr);
	cJSON_AddNumberToObject(alarmDTO,"oneself", 1);       //是否带分析功能
	cJSON_AddNumberToObject(alarmDTO,"statu",	8);

	msg=cJSON_Print(ContentDTO);	cJSON_Delete(ContentDTO); 

	strcpy(pub_content, msg);
	free(msg);
}


/*去掉字符串中的回车以及制表符*/
static void
process(char *str)
{
	int len = strlen(str);
	char buff[len+1];
	int count = 0;
	char *p = str;
	while(*p != '\0')
	{
        if(*p == '\r' || *p == '\t' || *p == '\n') {
            p++;
            continue;
        } else {
            buff[count] = *p;
            count++;
            p++;
        }
    }
    buff[count] = '\0';
	strcpy(str,buff);
}


HI_S32
get_pub_info(int pub_id, char *pub_msg)
{
	struct timeval tv;
	cJSON *PubDTO; 
	char *out;
	char md51[16] = "0";
	char sign[32] = "0";
	char md5buf[200] = "0";
	char date[30] = "0";
	char pubContentDTO[120] = "0";
	/*获取时间戳*/
	gettimeofday(&tv,NULL);
	long long timestamp = tv.tv_sec;

	PubDTO=cJSON_CreateObject();
	cJSON_AddNumberToObject(PubDTO,"pub_tp",		pub_id);

	switch (pub_id) {
		case 100:
			sprintf(md5buf, "%d%lld%s", pub_id, timestamp, "_zdst666");
			break;
		
		case 4:
			
			cJSON *PubContentDTO,*AlarmDTO;
			/*获取现在的时间*/
			get_time(date);
			
			cJSON_AddItemToObject(PubDTO, "pub_content", PubContentDTO=cJSON_CreateObject());
			cJSON_AddItemToObject(PubContentDTO, "alarm", AlarmDTO=cJSON_CreateObject());
			
			cJSON_AddStringToObject(AlarmDTO,"alarm_time", 	date);
			cJSON_AddNumberToObject(AlarmDTO,"channel", 	0);
			cJSON_AddStringToObject(AlarmDTO,"mac", 	macAddr);
			cJSON_AddNumberToObject(AlarmDTO,"oneself", 1);       //是否带分析功能
			cJSON_AddNumberToObject(AlarmDTO,"statu", 	8);
			/*获取pubcontent 字符串*/
			get_pub_content(pubContentDTO);
			/*去掉字符串中的回车以及换行符*/
			process(pubContentDTO);
			
			sprintf(md5buf, "%d%s%lld%s",4 ,pubContentDTO, timestamp, "_zdst666");
			printf("md5buf = %s\n", md5buf);
			break;
			
		default:
			printf("not support yet\n");
			return -1;
			break;
		}
	cJSON_AddNumberToObject(PubDTO,"timestamp", 	timestamp);
	
	md5(md5buf, strlen((const char *)md5buf), md51);
	hex2string(sign, md51, 16, 0);
	cJSON_AddStringToObject(PubDTO,"sign",		sign);
	
	out=cJSON_Print(PubDTO);	cJSON_Delete(PubDTO); 
	
	strcpy(pub_msg, out);
	free(out);
	
	return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */



