/**
 * @file lectura_UART_envio_MQTT.c
 * @brief Ejemplo de aplicación para leer mediciones de corriente por UART y publicar en un broker MQTT con TLS.
 *
 * Esta aplicación realiza:
 *  - Inicialización de almacenamiento no volátil (NVS), interfaces de red y conexión a Wi-Fi/Ethernet.
 *  - Configuración de UART para recibir lecturas de corriente desde un sensor u otro microcontrolador.
 *  - Inicialización de un cliente MQTT con TLS usando el certificado del broker HiveMQ.
 *  - Análisis de datos UART entrantes para extraer valores de corriente RMS.
 *  - Publicación de mensajes en formato JSON en el tópico "casa/{fase}/corriente" con QoS 1.
 *  - Manejo de eventos MQTT: conexión, confirmación de publicación, datos entrantes y errores.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_timer.h"
#include "driver/uart.h"

#define UART_PORT      UART_NUM_0     ///< Puerto UART utilizado para leer valores de corriente
#define UART_BUF_SIZE  1024           ///< Tamaño del buffer de lectura UART

// Certificado CA del broker HiveMQ embebido en binario
extern const uint8_t hivemq_root_ca_pem_start[] asm("_binary_hivemq_root_ca_pem_start");
extern const uint8_t hivemq_root_ca_pem_end[]   asm("_binary_hivemq_root_ca_pem_end");

static const char *TAG = "lectura_UART_envio_MQTT";
static esp_mqtt_client_handle_t g_client = NULL; ///< Handle global del cliente MQTT

/**
 * @brief Publica una medición de corriente RMS en el tópico MQTT.
 *
 * Formatea la marca de tiempo y el valor de corriente en un JSON y envía con QoS 1.
 *
 * @param fase Nombre de la fase eléctrica (p.ej. "Cocina") usado en el tópico.
 * @param I_rms Valor de corriente RMS en amperios.
 */
void publish_corriente(const char *fase, float I_rms)
{
    if (!g_client) {
        ESP_LOGW(TAG, "Cliente MQTT no inicializado, omitiendo publicación");
        return;
    }

    // Construir el tópico en formato casa/{fase}/corriente
    char topic[64];
    snprintf(topic, sizeof(topic), "casa/%s/corriente", fase);

    // Construir payload JSON con timestamp y corriente
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"ts\":%lld,\"I\":%.2f}",
             (long long)time(NULL), I_rms);

    // Publicar mensaje: longitud automática, QoS 1, sin retain
    int msg_id = esp_mqtt_client_publish(
        g_client,
        topic,
        payload,
        0,
        1,
        0);

    ESP_LOGI(TAG, "Publicado msg_id=%d Tema=%s Payload=%s", msg_id, topic, payload);
}

/**
 * @brief Manejador de eventos del cliente MQTT.
 *
 * Procesa conexión, desconexión, confirmaciones de publicación, datos entrantes y errores.
 */
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado al broker MQTT");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Desconectado del broker MQTT");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Mensaje publicado (msg_id=%d)", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Mensaje recibido en %.*s: %.*s",
                 event->topic_len, event->topic,
                 event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Error MQTT tipo=%d", event->error_handle->error_type);
        break;
    default:
        ESP_LOGD(TAG, "Evento MQTT no manejado id=%d", event->event_id);
    }
}

/**
 * @brief Inicializa y arranca el cliente MQTT con configuración TLS.
 */
static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = CONFIG_BROKER_URI,
            .verification.certificate = (const char *)hivemq_root_ca_pem_start
        },
        .credentials = {
        .username = CONFIG_MQTT_USERNAME,
        .authentication.password = CONFIG_MQTT_PASSWORD,
        },
        .session = {.keepalive = 30}
    };

    // Crear y arrancar cliente MQTT
    g_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(g_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(g_client);
}

/**
 * @brief Función principal de la aplicación.
 *
 * - Inicializa NVS, redes y UART.
 * - Arranca el cliente MQTT.
 * - En bucle: lee UART, parsea corriente y publica cada 5 segundos.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Arrancando aplicación");
    ESP_LOGI(TAG, "Memoria libre: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Versión ESP-IDF: %s", esp_get_idf_version());

    // Ajustar niveles de log
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);

    // Inicializar NVS, pila de red y loop de eventos
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Conectar a red según configuración (Wi-Fi o Ethernet)
    ESP_ERROR_CHECK(example_connect());

    // Configurar driver UART a 115200 8N1
    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE, 0, 0, NULL, 0));

    // Semilla para generador de números
    srand((unsigned)(esp_timer_get_time()));

    // Iniciar cliente MQTT
    mqtt_app_start();

    uint8_t rx_buf[UART_BUF_SIZE];
    while (1) {
        // Leer UART con timeout de 1 segundo
        int len = uart_read_bytes(UART_PORT, rx_buf, sizeof(rx_buf) - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            rx_buf[len] = '\0'; // asegurar fin de cadena
            float I_rms;
            // Formato esperado: "Current reading: <valor> A"
            if (sscanf((char *)rx_buf, "Current reading: %f A", &I_rms) == 1) {
                publish_corriente("Cocina", I_rms);
                ESP_LOGI(TAG, "Corriente publicada: %.2f A", I_rms);
            } else {
                ESP_LOGW(TAG, "No pude parsear UART: %s", rx_buf);
            }
        } else {
            ESP_LOGW(TAG, "No hubo datos en UART dentro del timeout");
        }
        // Hardcoded/random values for Sala and Garage para la demo
        float sala_val = 1.0f + ((float)rand() / RAND_MAX) * 0.5f;      // 1.0 a 1.5
        float garage_val = 2.6f + ((float)rand() / RAND_MAX) * 0.3f;    // 2.6 a 2.9
        publish_corriente("Sala", sala_val);
        ESP_LOGI(TAG, "Corriente publicada (Sala): %.2f A", sala_val);
        publish_corriente("Garage", garage_val);
        ESP_LOGI(TAG, "Corriente publicada (Garage): %.2f A", garage_val);
        // Esperar 5 segundos antes de la siguiente lectura
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
