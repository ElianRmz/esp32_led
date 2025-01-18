
#include <stdio.h>
#include "string.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_http_server.h"
#include "driver/ledc.h" 



extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");
extern const char chroma_start[] asm("_binary_chroma_png_start");
extern const char chroma_end[] asm("_binary_chroma_png_end");
// Variables globales para los valores RGB
int32_t led_r = 0, led_g = 0, led_b = 0;

// Definiciones para el LED PWM
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_HIGH_SPEED_MODE
#define LEDC_OUTPUT_IO_R        (12) // Pin GPIO para LED Rojo
#define LEDC_OUTPUT_IO_G        (14) // Pin GPIO para LED Verde
#define LEDC_OUTPUT_IO_B        (27) // Pin GPIO para LED Azul
#define LEDC_CHANNEL_R          LEDC_CHANNEL_0
#define LEDC_CHANNEL_G          LEDC_CHANNEL_1
#define LEDC_CHANNEL_B          LEDC_CHANNEL_2
#define LEDC_DUTY_RES          LEDC_TIMER_8_BIT // 8-bit resolution
#define LEDC_FREQUENCY          5000
#define LEDC_DUTY              0
#define LEDC_HPOINT            0


// Manejador de peticiones HTTP GET
static esp_err_t api_get_handler(httpd_req_t *req) {
    char* buf;
    size_t buf_len;

    // Obtener longitud de la query
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (buf == NULL) {
            return ESP_ERR_NO_MEM;
        }

        // Obtener la query string una sola vez
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[32];
            
            // Procesar parámetro R
            if (httpd_query_key_value(buf, "r", param, sizeof(param)) == ESP_OK) {
                led_r = atoi(param);
                printf("url r: %d\n", led_r);
            }
            
            // Procesar parámetro G
            if (httpd_query_key_value(buf, "g", param, sizeof(param)) == ESP_OK) {
                led_g = atoi(param);
                printf("url g: %d\n", led_g);
            }
            
            // Procesar parámetro B
            if (httpd_query_key_value(buf, "b", param, sizeof(param)) == ESP_OK) {
                led_b = atoi(param);
                printf("url b: %d\n", led_b);
            }
        }
        free(buf);
    }

    // Actualizar valores LED
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_R, led_r);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_R);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_G, led_g);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_G);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_B, led_b);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_B);
    
    // Enviar respuesta JSON
    httpd_resp_set_hdr(req, "Content-Type", "application/json");
    char res[64];
    sprintf(res, "{\"r\": %d, \"g\": %d, \"b\": %d}", led_r, led_g, led_b);
    httpd_resp_send(req, res, HTTPD_RESP_USE_STRLEN);
    
    return ESP_OK;
}
static esp_err_t home_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req,"text/html");
    
    const uint32_t index_len = index_end - index_start;

    httpd_resp_send(req,index_start, index_len);

    return ESP_OK;
}
static esp_err_t chroma_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req,"image/png");
    
    const uint32_t chroma_len =  chroma_end- chroma_start;
    httpd_resp_send(req,chroma_start, chroma_len);

    return ESP_OK;
}
static const httpd_uri_t api = {
    .uri = "/api",  // Ruta del endpoint
    .method = HTTP_GET,
    .handler = api_get_handler
};
static const httpd_uri_t home = {
    .uri = "/",  // Ruta del endpoint
    .method = HTTP_GET,
    .handler = home_get_handler
};
static const httpd_uri_t chroma = {
    .uri = "/chroma.png",  // Ruta del endpoint
    .method = HTTP_GET,
    .handler = chroma_get_handler
};
// Configuración del URI handler

// Inicialización del servidor web
void web_server() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &api);
        httpd_register_uri_handler(server, &home);
        httpd_register_uri_handler(server, &chroma);
        printf("Servidor web iniciado correctamente\n");
        return;
    }
    printf("Error al iniciar el servidor\n");
}

void app_main(void) {
    // Inicializar NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Inicializar TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Crear el event loop por defecto
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Conectar WiFi
    ESP_ERROR_CHECK(example_connect());
    
    // Obtener información IP
    esp_netif_ip_info_t ip;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    
    if (netif == NULL) {
        printf("No hay interfaz de red\n");
        return;
    }
    
    // Mostrar IP asignada
    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));
    printf("IP: %d.%d.%d.%d\n", IP2STR(&ip.ip));
    
    // Configurar timer LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    
    // Configurar canal R
    ledc_channel_config_t ledc_channel_r = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_R,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_OUTPUT_IO_R,
        .duty = LEDC_DUTY,
        .hpoint = LEDC_HPOINT
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_r));
    
    // Configurar canal G
    ledc_channel_config_t ledc_channel_g = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_G,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_OUTPUT_IO_G,
        .duty = LEDC_DUTY,
        .hpoint = LEDC_HPOINT
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_g));
    
    // Configurar canal B
    ledc_channel_config_t ledc_channel_b = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_B,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_OUTPUT_IO_B,
        .duty = LEDC_DUTY,
        .hpoint = LEDC_HPOINT
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_b));
    
    // Inicializar valores LED
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_R, led_r);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_R);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_G, led_g);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_G);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_B, led_b);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_B);
    
    // Iniciar servidor web
    web_server();
}