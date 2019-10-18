#ifndef SAMPLE_MQTT_H_
#define SAMPLE_MQTT_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "MQTTClient.h"

#include "sample_protocol.h"



extern int mqtt_publish_data(char *data, int len);
extern int mqtt_subscribing_process(void);

#endif 





