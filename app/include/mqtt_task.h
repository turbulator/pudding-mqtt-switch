#ifndef __MQTT_TASK_H_
#define __MQTT_TASK_H_

#define MQTT_TASK_STACK_SIZE       (2048 * 4)
#define MQTT_TASK_PRIORITY         1
#define MQTT_TASK_NAME             "MQTT Task"

#define MQTT_INTERVAL              1000
#define MQTT_WATCHDOG_INTERVAL     2 * 60

#define MQTT_TOPIC_STATE           "dacha/switch"
#define MQTT_TOPIC_SET             "dacha/switch/set"
#define MQTT_TOPIC_AVAILABILITY    "dacha/switch/available"

#define MQTT_PAYLOAD_AVAILABLE     "online"
#define MQTT_PAYLOAD_UNAVAILABLE   "offline"
#define MQTT_PAYLOAD_ON            "ON"
#define MQTT_PAYLOAD_OFF           "OFF"


typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,
    MQTT_STATUS_CONNECTING,
    MQTT_STATUS_CONNECTED,
    MQTT_STATUS_PUBLISHING_ONLINE,
    MQTT_STATUS_ONLINE,
    MQTT_STATUS_SUBSCRIBING,
    MQTT_STATUS_READY,
    MQTT_STATUS_PUBLISH_STATE,
    MQTT_STATUS_PUBLISHING_STATE,
    MQTT_STATUS_MAX
} MQTT_Status_t;


extern HANDLE semMqttStart;

void MqttTaskInit(void);


#endif /* __MQTT_TASK_H_ */
