#include "sample_mqtt.h"

#define true 1
#define false 0

#define QOS         1
#define TIMEOUT     10000L
#define DISCONNECT "out"

volatile MQTTClient_deliveryToken deliveredtoken;
//声明一个MQTTClient
MQTTClient client = NULL;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
//声明消息token
MQTTClient_deliveryToken token;
char macAddr[30] = "0";
char username[50] = "42BA173CBC6A_account"; //添加的用户名
char password[50] = "6a624c8a835e734c87285aedd9834af8"; //添加的密码
char clientID[50] = "46DBE224D516_client"; //客户端id
char pubTopic[50] = "/SMARTCAMERA/42BA173CBC6A/pub"; //推送的topic
char subTopic[50] = "/SMARTCAMERA/42BA173CBC6A/sub"; //订阅的topic


char globalTopic[50] = "/SMARTCAMERA/global/sub";
char serverIP[50] = "172.16.0.19:18884";
//char serverIP[50] = "192.168.1.69:1883";

//初始化MQTT Client选项
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;


/*mqtt推送消息到服务器*/
int 
mqtt_publish_data(char *data, int len)
{
	if (MQTTClient_isConnected(client) != true) {
		printf("mqtt 连接断开\n");
		return -1;
	}
	
	pubmsg.payload = data;
	pubmsg.payloadlen = len;
	pubmsg.qos = QOS;
	pubmsg.retained = 0;
	
	if (MQTTClient_publishMessage(client, pubTopic, &pubmsg, &token) != MQTTCLIENT_SUCCESS) {
		printf("发送失败\n");
		return -1;
	}
	
	printf("Waiting for up to %d seconds for publication of %s\n"
            "on topic %s for client with ClientID: %s\n",
            (int)(TIMEOUT/1000), data, pubTopic, clientID);
	MQTTClient_waitForCompletion(client, token, TIMEOUT);
	printf("Message with delivery token %d delivered\n", token);
	
	return 0;
}


static void
delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}


static int
msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
    char* payloadptr;

    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = (char *)message->payload;
    if (strcmp(payloadptr, DISCONNECT) == 0) {
        printf(" \n out!!");
    }
    
    for (i = 0; i < message->payloadlen; i++) {
        putchar(*payloadptr++);
    }
    printf("\n");
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
	
    return 1;
}


void
connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
	do
	{

			mqtt_subscribing_process();
			sleep(2);
		
	}while(MQTTClient_isConnected(client) != true);
}


static int 
get_mqtt_info(void)
{
	if (get_mac_addr(macAddr) != 0) {
		printf("Failed to get macAddr\n");
		return -1;
	}
	if (get_mqtt_username(macAddr,username) != 0)
		return -1;
	if (get_mqtt_clientid(macAddr,clientID) != 0)
		return -1;
	if (get_mqtt_password(macAddr,password) != 0)
		return -1;
	if (get_mqtt_pubTopic(macAddr,pubTopic) != 0)
		return -1;
	if (get_mqtt_subTopic(macAddr,subTopic) != 0)
		return -1;

	return 0;
}


/*连接服务器，订阅主题*/
int 
mqtt_subscribing_process(void)
{
	char pubMsg[300] = "0";
	int msgLen = 0;
	static int rc;
	
	if (get_mqtt_info() != 0) {
 		printf("Failed to get mqtt info \n");
		return -1;
	}
	
	/*使用参数创建一个client，并将其赋值给之前声明的client*/
	MQTTClient_create(&client, serverIP, clientID,
                    MQTTCLIENT_PERSISTENCE_NONE, NULL);

	conn_opts.keepAliveInterval = 30;
	conn_opts.cleansession = 1;
	conn_opts.username = username; //将用户名写入连接选项中
	conn_opts.password = password; //将密码写入连接选项中

	/*使用MQTTClient_connect将client连接到服务器，使用指定的连接选项。成功则返回MQTTCLIENT_SUCCESS*/
	MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
		printf("Failed to connect, return code %d\n", rc);
		return -1;
	}
	printf("Connected to server addr:%s\n", serverIP);
	printf("Subscribing to topic %s for client %s using QoS%d\n", subTopic, clientID, QOS);
	
	/*订阅私有主题*/
	if ((rc = MQTTClient_subscribe(client, subTopic, QOS)) != MQTTCLIENT_SUCCESS) {
		printf("Failed to subscribe private topic, return code %d\n", rc);
		return -1;
	}
	
	/*订阅全局主题*/
	if ((rc = MQTTClient_subscribe(client, globalTopic, QOS)) != MQTTCLIENT_SUCCESS) {
		printf("Failed to subscribe global topic, return code %d\n", rc);
		return -1;
	}

	/*发送上线消息*/
	if (get_pub_info(100,pubMsg) != 0) {
		printf("Failed to get pubinfo\n");
		return -1;
	} 
	
	/*推送消息*/
  	msgLen = strlen(pubMsg);
	if (mqtt_publish_data(pubMsg, msgLen) != 0 ) {
		printf("Failed to publish data\n");
		return -1;
	};

	return 0;
}



