#include "mqtt_config.h"
#include "mqtt_config_static.h"
#include "mqtt_info.h"
#include "task_config.h"
#include "flags.h"
#include "queue_conf.h"
#include "gpio_info.h"

#include "string.h"
#include "stdio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "message_buffer.h"
#include "aws_mqtt_agent.h"
#include "aws_hello_world.h"
#include "rtc_config.h"
#include "jsmn.h"


static MQTTAgentHandle_t xMQTTHandle = NULL;
static MessageBufferHandle_t xEchoMessageBuffer = NULL;
static const int JSON_VALUE_LEN = 20;
QueueHandle_t mqtt_queue;


static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

void mqtt_config_init(void * param){
    //mqtt_queue = xQueueCreate(5, sizeof(struct MqttMsg));
}

void mqtt_config_task(void * pvParameters){
    BaseType_t xX;
    BaseType_t xReturned;
    const TickType_t xFiveSeconds = pdMS_TO_TICKS( 5000UL );
    const BaseType_t xIterationsInAMinute = 60 / 5;
    TaskHandle_t xEchoingTask = NULL;
    struct MqttMsg mqtt_msg;

    ( void ) pvParameters;

    for(;;){
        if(flags_is_wifi_connected()){
            break;
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    /* Create the MQTT client object and connect it to the MQTT broker. */
    xReturned = mqtt_config_create();

    if(xReturned != eMQTTAgentSuccess){
        goto end_task;
    }

    xReturned = mqtt_config_connect();

    if(xReturned != eMQTTAgentSuccess){
        mqtt_config_delete();
        goto end_task;
    }


    flags_set_mqtt_connected();
    xReturned = mqtt_config_subcribe();

    for(;;){

        if( !flags_is_mqtt_connected() ){
            printf("MQTT disconnected\n");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            xReturned = mqtt_config_connect();
            if(xReturned != eMQTTAgentSuccess){
                goto br;
            }

            xReturned = mqtt_config_subcribe();
            if(xReturned != eMQTTAgentSuccess){
                mqtt_config_disconnect();
                goto br;
            }

        }
    br:
        if(xQueueReceive(mqtt_queue, &mqtt_msg, 0 )){//Lee si hay items en la cola
            mqtt_config_report_status(mqtt_msg);
        } 
        //mqtt_config_report_status(mqtt_msg);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }


end_task:
    /* Disconnect the client. */
    ( void ) MQTT_AGENT_Disconnect( xMQTTHandle, MQTT_TIMEOUT );

    /* End the demo by deleting all created resources. */
    configPRINTF( ( "MQTT echo demo finished.\r\n" ) );
    configPRINTF( ( "----Demo finished----\r\n" ) );
    vMessageBufferDelete( xEchoMessageBuffer );
    vTaskDelete( xEchoingTask );
    vTaskDelete( NULL ); /* Delete this task. */
}

void mqtt_config_report_status(struct MqttMsg mqtt_msg){
    MQTTAgentReturnCode_t xReturned;
    MQTTAgentPublishParams_t xPublishParameters;
    char cDataBuffer[ MQTT_MAX_DATA_LENGTH ];

    (void)snprintf( cDataBuffer, MQTT_MAX_DATA_LENGTH, "{\"parameter\": \"%s\", \"value\": %d, \"date\": %u, \"connection\":true}", mqtt_msg.name, mqtt_msg.status , mqtt_msg.timestamp);
    printf("%s\n", cDataBuffer);
    memset(&(xPublishParameters), 0x00, sizeof(xPublishParameters));
    xPublishParameters.pucTopic = MQTT_PUBLISH_TOPIC;
    xPublishParameters.pvData = cDataBuffer;
    xPublishParameters.usTopicLength = ( uint16_t ) strlen( ( const char * ) MQTT_PUBLISH_TOPIC );
    xPublishParameters.ulDataLength = ( uint32_t ) strlen( cDataBuffer );
    xPublishParameters.xQoS = eMQTTQoS1;
    xReturned = MQTT_AGENT_Publish( xMQTTHandle, &( xPublishParameters ), MQTT_TIMEOUT );

}

static MQTTAgentReturnCode_t mqtt_config_create( void ){
    MQTTAgentReturnCode_t xReturned;
    BaseType_t xReturn = pdFAIL;
    MQTTAgentConnectParams_t xConnectParameters =
    {
        MQTT_BROKER_ENDPOINT, /* The URL of the MQTT broker to connect to. */
        MQTT_AGENT_CONNECT_FLAGS,             /* Connection flags. */
        pdFALSE,                              /* Deprecated. */
        MQTT_BROKER_PORT,                     /* Port number on which the MQTT broker is listening. Can be overridden by ALPN connection flag. */
        MQTT_CLIENT_ID,                       /* Client Identifier of the MQTT client. It should be unique per broker. */
        0,                                    /* The length of the client Id, filled in later as not const. */
        pdFALSE,                              /* Deprecated. */
        NULL,                                 /* User data supplied to the callback. Can be NULL. */
        mqtt_events_callback,                 /* Callback used to report various events. Can be NULL. */
        NULL,                                 /* Certificate used for secure connection. Can be NULL. */
        0                                     /* Size of certificate used for secure connection. */
    };

    /* Check this function has not already been executed. */
    configASSERT( xMQTTHandle == NULL );

    /* The MQTT client object must be created before it can be used.  The
     * maximum number of MQTT client objects that can exist simultaneously
     * is set by mqttconfigMAX_BROKERS. */
    xReturned = MQTT_AGENT_Create( &xMQTTHandle );

    return xReturned;
}

static MQTTAgentReturnCode_t mqtt_config_connect(){
    MQTTAgentReturnCode_t xReturned;

    MQTTAgentConnectParams_t xConnectParameters =
    {
        MQTT_BROKER_ENDPOINT, /* The URL of the MQTT broker to connect to. */
        MQTT_AGENT_CONNECT_FLAGS,             /* Connection flags. */
        pdFALSE,                              /* Deprecated. */
        MQTT_BROKER_PORT,                     /* Port number on which the MQTT broker is listening. Can be overridden by ALPN connection flag. */
        MQTT_CLIENT_ID,                       /* Client Identifier of the MQTT client. It should be unique per broker. */
        0,                                    /* The length of the client Id, filled in later as not const. */
        pdFALSE,                              /* Deprecated. */
        NULL,                                 /* User data supplied to the callback. Can be NULL. */
        mqtt_events_callback,                 /* Callback used to report various events. Can be NULL. */
        NULL,                                 /* Certificate used for secure connection. Can be NULL. */
        0                                     /* Size of certificate used for secure connection. */
    };

    configPRINTF( ( "MQTT echo attempting to connect to %s.\r\n", MQTT_BROKER_ENDPOINT ) );

    /* Fill in the MQTTAgentConnectParams_t member that is not const,
    * and therefore could not be set in the initializer (where
    * xConnectParameters is declared in this function). */
    
    xConnectParameters.usClientIdLength = ( uint16_t ) strlen( ( const char * ) MQTT_CLIENT_ID );

    xReturned = MQTT_AGENT_Connect( xMQTTHandle,
                                    &xConnectParameters,
                                    MQTT_ECHO_TLS_NEGOTIATION_TIMEOUT );
    

    if(xReturned == eMQTTAgentSuccess){
        flags_set_mqtt_connected();
        printf("MQTT connected\n");
    }
    else{
        printf("MQTT could not connect\n");
    }
    return xReturned;
}

static void mqtt_config_delete(){
    (void)MQTT_AGENT_Delete( xMQTTHandle );
    flags_reset_mqtt_connected();
}

static void mqtt_config_disconnect(){
    (void) MQTT_AGENT_Disconnect( xMQTTHandle, MQTT_TIMEOUT );
    flags_reset_mqtt_connected();
}


static BaseType_t mqtt_config_subcribe(void){

    if(!flags_is_mqtt_connected()){
        return -1;
    }
    MQTTAgentReturnCode_t xReturned;
    BaseType_t xReturn;
    MQTTAgentSubscribeParams_t xSubscribeParams;
    //printf("TOPIC: %s\n", MQTT_SUBSCRIBE_TOPIC);
    /* Setup subscribe parameters to subscribe to MQTT_SUBSCRIBE_TOPIC topic. */
    xSubscribeParams.pucTopic = MQTT_SUBSCRIBE_TOPIC;
    xSubscribeParams.pvPublishCallbackContext = NULL;
    xSubscribeParams.pxPublishCallback = mqtt_config_subs_callback;
    xSubscribeParams.usTopicLength = ( uint16_t ) strlen( ( const char * ) MQTT_SUBSCRIBE_TOPIC );
    xSubscribeParams.xQoS = eMQTTQoS1;

    /* Subscribe to the topic. */
    xReturned = MQTT_AGENT_Subscribe( xMQTTHandle,
                                      &xSubscribeParams,
                                      MQTT_TIMEOUT );

    if( xReturned == eMQTTAgentSuccess )
    {
        configPRINTF( ( "MQTT Echo demo subscribed to %s\r\n", MQTT_SUBSCRIBE_TOPIC ) );
        xReturn = pdPASS;
    }
    else
    {
        configPRINTF( ( "ERROR:  MQTT Echo demo could not subscribe to %s\r\n", MQTT_SUBSCRIBE_TOPIC ) );
        xReturn = pdFAIL;
    }

    return xReturned;
}

static MQTTBool_t mqtt_config_subs_callback(void * pvUserData, const MQTTPublishData_t * const pxCallbackParams){
    char cBuffer[ MQTT_MAX_DATA_LENGTH];
    uint32_t ulBytesToCopy = ( MQTT_MAX_DATA_LENGTH - 1 ); /* Bytes to copy initialized to ensure it
                                                                                   * fits in the buffer. One place is left
                                                                                   * for NULL terminator. */
                                                                                  int i;
	int r;
    jsmn_parser p;
	jsmntok_t t[20]; /* We expect no more than 128 tokens */
    char value[JSON_VALUE_LEN];

    /* Remove warnings about the unused parameters. */
    ( void ) pvUserData;


    /* Don't expect the callback to be invoked for any other topics. */
    //configASSERT( ( size_t ) ( pxCallbackParams->usTopicLength ) == strlen( ( const char * ) MQTT_SUBSCRIBE_TOPIC ) );
    //configASSERT( memcmp( pxCallbackParams->pucTopic, MQTT_SUBSCRIBE_TOPIC, ( size_t ) ( pxCallbackParams->usTopicLength ) ) == 0 );

    //printf("TOPIC recibo: %s\n", pxCallbackParams->pucTopic);
    //memcmp(pxCallbackParams->pucTopic, MQTT_SUBSCRIBE_TOPIC, ( size_t ) ( pxCallbackParams->usTopicLength )) ;

    if( pxCallbackParams->ulDataLength <= ulBytesToCopy ){
        ulBytesToCopy = pxCallbackParams->ulDataLength;
        
        memset( cBuffer, 0x00, sizeof( cBuffer ) );
        memcpy( cBuffer, pxCallbackParams->pvData, ( size_t ) ulBytesToCopy );

        //printf("Data received: %s\n", cBuffer);

        jsmn_init(&p);
	    r = jsmn_parse(&p, cBuffer, strlen(cBuffer), t, sizeof(t)/sizeof(t[0]));
	    if (r < 0) {
		    printf("Failed to parse JSON: %d\n", r);
		    return 1;
	    }

        /* Assume the top-level element is an object */
        if (r < 1 || t[0].type != JSMN_OBJECT) {
            printf("Object expected\n");
            return 1;
        } 

        uint32_t gpio = 0;
        uint32_t status = 0;

        for (int i = 1; i < r; i++) {
            memset( value, 0x00, JSON_VALUE_LEN);
            strncpy(value, cBuffer + t[i+1].start, t[i+1].end-t[i+1].start);
            if (jsoneq(cBuffer, &t[i], DO01_NAME) == 0) {
                gpio = 1;
                i++;                
            } else if (jsoneq(cBuffer, &t[i], DO02_NAME) == 0) {
                gpio = 2;
                i++;
            } else if (jsoneq(cBuffer, &t[i], DO03_NAME) == 0) {            
                gpio = 3;
                i++;
            } else if (jsoneq(cBuffer, &t[i], DO04_NAME) == 0) {
                gpio = 4;
                i++;
            }else if (jsoneq(cBuffer, &t[i], "hora") == 0) {
                gpio = 0;
                i++; 
                rtc_config_set_time_(value, 0);
            }else {
                printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
                        cBuffer + t[i].start);
                i++;                        
            }
	    }

        if(gpio > 0){            
            queue_conf_send_gpio(gpio, value[0] == '0' ? 0 : 1);
        }

    }
    else{
        configPRINTF( ( "[WARN]: Dropping received message as it does not fit in the buffer.\r\n" ) );
    }

    /* The data was copied into the FreeRTOS message buffer, so the buffer
     * containing the data is no longer required.  Returning eMQTTFalse tells the
     * MQTT agent that the ownership of the buffer containing the message lies with
     * the agent and it is responsible for freeing the buffer. */
    return eMQTTFalse;
}

MQTTAgentHandle_t mqtt_config_get_handler(){
    return xMQTTHandle;
}

static BaseType_t mqtt_events_callback (void * pvUserData, const MQTTAgentCallbackParams_t * const pxCallbackParams){
    MQTTAgentEvent_t xMQTTEvent = pxCallbackParams->xMQTTEvent;
    switch(xMQTTEvent){
        case eMQTTAgentPublish:
            printf("eMQTTAgentPublish\n");
            break;
        case eMQTTAgentDisconnect:
            flags_reset_mqtt_connected();
            printf("eMQTTAgentDisconnect\n");
            break;
    }
    return pdTRUE;
}

/*

Callback
You can specify an optional callback that is invoked whenever the MQTT agent is disconnected from the broker or whenever 
a publish message is received from the broker. The received publish message is stored in a buffer taken from the 
central buffer pool. This message is passed to the callback. This callback runs in the context of the MQTT task and 
therefore must be quick. If you need to do longer processing, you must take the ownership of the buffer by returning 
pdTRUE from the callback. You must then return the buffer back to the pool whenever you are done by calling 
FreeRTOS_Agent_ReturnBuffer.

typedef enum
{
    eMQTTAgentPublish,   //< A Publish message was received from the broker. 
    eMQTTAgentDisconnect //< The connection to the broker got disconnected. 
} MQTTAgentEvent_t;
*/


/**
 * @brief Signature of the callback registered by the user to get notified of various events.
 *
 * The user can register an optional callback to get notified of various events.
 *
 * @param[in] pvUserData The user data as provided in the connect parameters while connecting.
 * @param[in] pxCallbackParams The event and related data.
 *
 * @return The return value is ignored in all other cases except publish (i.e. eMQTTAgentPublish
 * event):
 * 1. If pdTRUE is returned - The ownership of the buffer passed in the callback (xBuffer
 * in MQTTPublishData_t) lies with the user.
 * 2. If pdFALSE is returned - The ownership of the buffer passed in the callback (xBuffer
 * in MQTTPublishData_t) remains with the library and it is recycled as soon as
 * the callback returns.<br>
 * The user should take the ownership of the buffer containing the received message from the
 * broker by returning pdTRUE from the callback if the user wants to use the buffer after
 * the callback is over. The user should return the buffer whenever done by calling the
 * MQTT_AGENT_ReturnBuffer API.
 *
 * @see MQTTAgentCallbackParams_t.
 */