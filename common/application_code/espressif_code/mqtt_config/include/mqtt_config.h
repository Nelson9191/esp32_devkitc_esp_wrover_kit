#ifndef _MQTT_CONFIG_H_
#define _MQTT_CONFIG_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "aws_mqtt_agent.h"


/**
 * Estructura para almacenar los mensajes del gpio que cambia de estado.
 * Estos mensajes son encolados desde el gpio_task; y desencolados
 * desde el mqtt_task donde son reportados al broker.
 * 
*/
struct MqttMsg {
    uint32_t gpio;
    int status;
};

/**
 * Cola para reporte de estado de los gpios
 * 
*/
extern QueueHandle_t mqtt_queue;

void mqtt_config_init();

/**
 * Tarea mqtt
 * @Params: - pvParameters: puntero void requerido para la creación de la tarea (No se usa)
 * @Return:
 * 
*/
void mqtt_config_task(void * pvParameters);

/**
 * Función llamada al recibir información en la cola (mqtt_queue).
 * @Params: - mqtt_msg: mensaje con el gpio y estado
 * @Return:
*/
void mqtt_config_report_status(struct MqttMsg mqtt_msg);

/**
 * Retorna handler de Mqtt (handler static). Requerido en el OTA agent.
 * @Params: 
 * @Return: Mqtt handler
*/
MQTTAgentHandle_t mqtt_config_get_handler();


#endif