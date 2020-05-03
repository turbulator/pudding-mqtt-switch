#include <string.h>
#include <stdio.h>

#include <api_os.h>
#include <api_info.h>
#include <api_gps.h>
#include <api_debug.h>
#include <api_mqtt.h>
#include <api_socket.h>

#include <api_hal_watchdog.h>
#include <api_hal_gpio.h>

#include <time.h>

#include "mqtt_task.h"
#include "secret.h"


static HANDLE mqttTaskHandle = NULL;
HANDLE semMqttStart = NULL;


GPIO_config_t switchGpio = {
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GPIO_PIN0,
    .defaultLevel = GPIO_LEVEL_HIGH
};


MQTT_Connect_Info_t ci;
MQTT_Client_t *mqttClient = NULL;

char imei[16] = "";
char mqttBuffer[1024] = "";
bool switchState = false;

MQTT_Status_t mqttStatus = MQTT_STATUS_DISCONNECTED;


void OnMqttConnect(MQTT_Client_t *client, void *arg, MQTT_Connection_Status_t status) {
    Trace(1,"MQTT connection status: %d", status);

    if(status == MQTT_CONNECTION_ACCEPTED) {
        Trace(1,"MQTT succeed connect to broker");
        mqttStatus = MQTT_STATUS_CONNECTED;
    } else {
        mqttStatus = MQTT_STATUS_DISCONNECTED;
    }

    WatchDog_KeepAlive();
}

void MqttConnect(void) {
    Trace(1, "MqttConnect");

    MQTT_Error_t err = MQTT_Connect(mqttClient, BROKER_IP, BROKER_PORT, OnMqttConnect, NULL, &ci);

    if(err != MQTT_ERROR_NONE) {
        Trace(1, "MQTT connect fail, error code: %d", err);
    }

    mqttStatus = MQTT_STATUS_CONNECTING;
}


void OnPublishAvailable(void* arg, MQTT_Error_t err) {
    if(err == MQTT_ERROR_NONE) {
        Trace(1,"MQTT publish available success");
        mqttStatus = MQTT_STATUS_ONLINE;
    } else {
        Trace(1,"MQTT publish available error, error code: %d", err);
        mqttStatus = MQTT_STATUS_CONNECTED;
    }

    WatchDog_KeepAlive();
}

void MqttPublishAvailable(void) {
    Trace(1, "MqttPublishAvailable");

    MQTT_Error_t err = MQTT_Publish(mqttClient, MQTT_TOPIC_AVAILABILITY, MQTT_PAYLOAD_AVAILABLE, strlen(MQTT_PAYLOAD_AVAILABLE), 1, 2, 1, OnPublishAvailable, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish available error, error code: %d", err);

    mqttStatus = MQTT_STATUS_PUBLISHING_ONLINE;
}


void OnPublishState(void* arg, MQTT_Error_t err) {
    if(err == MQTT_ERROR_NONE) {
        Trace(1,"MQTT publish state success");
        mqttStatus = MQTT_STATUS_READY;
    } else {
        Trace(1,"MQTT publish state error, error code: %d", err);
        mqttStatus = MQTT_STATUS_PUBLISH_STATE;
    }

    WatchDog_KeepAlive();
}

void MqttPublishState(void) {
    Trace(1, "MqttPublishState");

    if (switchState)
        snprintf(mqttBuffer, sizeof(mqttBuffer), "%s", MQTT_PAYLOAD_ON); 
    else
        snprintf(mqttBuffer, sizeof(mqttBuffer), "%s", MQTT_PAYLOAD_OFF); 

    MQTT_Error_t err = MQTT_Publish(mqttClient, MQTT_TOPIC_STATE, mqttBuffer, strlen(mqttBuffer), 1, 2, 1, OnPublishState, NULL);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT publish state offline error, error code: %d", err);

    mqttStatus = MQTT_STATUS_PUBLISHING_STATE;
}


void OnMqttReceived(void *arg, const char *topic, uint32_t payloadLen) {
    Trace(1,"MQTT received publish data request, topic: %s, payload length: %d", topic, payloadLen);
}

void OnMqttReceiedData(void *arg, const uint8_t *data, uint16_t len, MQTT_Flags_t flags) {
    Trace(1,"MQTT recieved publish data, length: %d, data: %s", len, data);

    if (strncmp(data, MQTT_PAYLOAD_ON, len) == 0) {
        switchState = true;
        GPIO_SetLevel(switchGpio, GPIO_LEVEL_LOW);
    } else if (strncmp(data, MQTT_PAYLOAD_OFF, len) == 0) {
        switchState = false;
        GPIO_SetLevel(switchGpio, GPIO_LEVEL_HIGH);
    }

    if(flags == MQTT_FLAG_DATA_LAST)
        Trace(1,"MQTT data is last frame");

    mqttStatus = MQTT_STATUS_PUBLISH_STATE;
}

void OnMqttSubscribed(void *arg, MQTT_Error_t err)
{
    if(err != MQTT_ERROR_NONE) {
       Trace(1,"MQTT subscribe fail, error code: %d", err);
       mqttStatus = MQTT_STATUS_ONLINE;
    } else {
       Trace(1,"MQTT subscribe success, topic: %s", (const char*)arg);
       mqttStatus = MQTT_STATUS_READY;
    }
}

void MqttSubscribe(void) {
    Trace(1, "MqttSubscribe");
    MQTT_Error_t err;

    MQTT_SetInPubCallback(mqttClient, OnMqttReceived, OnMqttReceiedData, NULL);
    
    err = MQTT_Subscribe(mqttClient, MQTT_TOPIC_SET, 2, OnMqttSubscribed, (void*)MQTT_TOPIC_SET);

    if(err != MQTT_ERROR_NONE)
        Trace(1,"MQTT subscribe error, error code: %d", err);

    mqttStatus = MQTT_STATUS_SUBSCRIBING;
}


void MqttInit(void) {
    Trace(1, "MQTT init");

    GPIO_Init(switchGpio);

    INFO_GetIMEI(imei);
    Trace(1,"IMEI: %s", imei);

    mqttClient = MQTT_ClientNew();
    memset(&ci, 0, sizeof(MQTT_Connect_Info_t));

    ci.client_id = imei;
    ci.client_user = CLIENT_USER;
    ci.client_pass = CLIENT_PASS;
    ci.keep_alive = 60;
    ci.clean_session = 1;
    ci.use_ssl = false;
#if 1  // Set connection status offline
    ci.will_qos = 2;
    ci.will_topic = MQTT_TOPIC_AVAILABILITY;
    ci.will_retain = 1;
    ci.will_msg = MQTT_PAYLOAD_UNAVAILABLE;
#endif
#if 1  // TLS
    ci.use_ssl = true;
    ci.ssl_verify_mode = MQTT_SSL_VERIFY_MODE_OPTIONAL;
    ci.ca_cert = ca_crt;
    ci.ca_crl = NULL;
    ci.client_cert = client_crt;
    ci.client_key  = client_key;
    ci.client_key_passwd = NULL;
    ci.broker_hostname = BROKER_HOSTNAME;
    ci.ssl_min_version = MQTT_SSL_VERSION_TLSv1_1;
    ci.ssl_max_version = MQTT_SSL_VERSION_TLSv1_2;
    ci.entropy_custom = "GSM SWITCH";
#endif
}


void MqttTask(void *pData) {
    WatchDog_Open(WATCHDOG_SECOND_TO_TICK(MQTT_WATCHDOG_INTERVAL));

    semMqttStart = OS_CreateSemaphore(0);
    OS_WaitForSemaphore(semMqttStart, OS_WAIT_FOREVER);
    OS_DeleteSemaphore(semMqttStart);
    semMqttStart = NULL;

    WatchDog_KeepAlive();

    // Randomize the TCP Sequence number
    uint8_t count = clock() & 0x0F; // Semi-random value from 0 - 15
    for(uint8_t i = 0; i < count; i++) {
        Socket_TcpipClose(Socket_TcpipConnect(TCP, "127.0.0.1", 12345 + count));
    }

    MqttInit();
  
    while(1) {
        switch(mqttStatus) {
            case MQTT_STATUS_DISCONNECTED:
                MqttConnect();
                break;

             case MQTT_STATUS_CONNECTING:
                Trace(1,"MQTT connecting...");
                break;

            case MQTT_STATUS_CONNECTED:
                MqttPublishAvailable();
                break;

            case MQTT_STATUS_PUBLISHING_ONLINE:
                Trace(1,"MQTT going online...");
                break;

            case MQTT_STATUS_ONLINE:
                MqttSubscribe();
                break;

            case MQTT_STATUS_SUBSCRIBING:
                Trace(1,"MQTT subscribing...");
                break;

            case MQTT_STATUS_READY:
                WatchDog_KeepAlive();
                break;

            case MQTT_STATUS_PUBLISH_STATE:
                MqttPublishState();
                break;

            case MQTT_STATUS_PUBLISHING_STATE:
                Trace(1,"MQTT publishing state...");
                break;

            default:
                break;
        }

        OS_Sleep(MQTT_INTERVAL);
    }
}


void MqttTaskInit(void) {
    mqttTaskHandle = OS_CreateTask(MqttTask,
                                   NULL, NULL, MQTT_TASK_STACK_SIZE, MQTT_TASK_PRIORITY, 0, 0, MQTT_TASK_NAME);
}
