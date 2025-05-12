#include <stdio.h>
#include <pthread.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bh_platform.h"
#include "esp_log.h"
#include "wasm_export.h"
#include "rust_drowsiness_trucker.h"
#include "esp_camera.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"

#define TAG_CAPTURE "capture_process"

// support IDF 5.x
#ifndef portTICK_RATE_MS
    #define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define LOG_TAG "wamr"
#define TAG "esp-camera"

#define WIFI_TAG "wi-fi"

#define WIFI_SSID "RAFAEL_2G"
#define WIFI_PASS "@Netcabo4321#"

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

#include "esp_wifi.h"

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(LOG_TAG, "Tentando reconectar ao Wi-Fi...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(LOG_TAG, "Conectado ao Wi-Fi. IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta() {
    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Criar o grupo de eventos
    wifi_event_group = xEventGroupCreate();

    // Inicializar a pilha de eventos do Wi-Fi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Criar uma interface de rede Wi-Fi
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Configurar o handler de eventos
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // Configurar Wi-Fi no modo STA
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(LOG_TAG, "Wi-Fi inicializado");
}

static const char *TAG_HTTP = "HTTP_SERVER";

#define MAX_IMAGE_SIZE 1024 * 20 // 20KB de buffer para imagem processada

static wasm_module_inst_t module_inst = NULL; // Adicionando a variável global

// -------------------------------------------------
// Função auxiliar para processar o frame via WASM em uma thread pthread
typedef struct {
    camera_fb_t *fb;
    uint32_t expected_len;
    uint32_t wasm_buf_offset;
    uint8_t *result;    // Ponteiro para o buffer com a imagem processada (será alocado aqui)
    int error;          // Código de erro (0 = sucesso)
} wasm_thread_arg_t;

void *wasm_process_frame(void *arg) {
    wasm_thread_arg_t *targ = (wasm_thread_arg_t *)arg;

    // Obter ponteiro para a área de memória WASM (assumindo offset fixo)
    uint8_t *wasm_buf = wasm_runtime_addr_app_to_native(module_inst, targ->wasm_buf_offset);
    if (!wasm_buf) {
        targ->error = -1;
        return NULL;
    }

    // Copia o frame capturado para a memória do WASM
    memcpy(wasm_buf, targ->fb->buf, targ->expected_len);

    // Prepara os argumentos para a função WASM 'analisando'
    uint32_t args[5];
    args[0] = 0;  // Slot de retorno
    args[1] = targ->wasm_buf_offset;
    args[2] = targ->expected_len;
    args[3] = targ->fb->width;
    args[4] = targ->fb->height;

    // Cria um ambiente de execução para a função WASM (32KB de pilha)
    wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, 32 * 1024);
    if (!exec_env) {
        targ->error = -2;
        return NULL;
    }

    wasm_function_inst_t wasm_func = wasm_runtime_lookup_function(module_inst, "analisando", NULL);
    if (!wasm_func) {
        targ->error = -3;
        wasm_runtime_destroy_exec_env(exec_env);
        return NULL;
    }

    if (!wasm_runtime_call_wasm(exec_env, wasm_func, 5, args)) {
        targ->error = -4;
        wasm_runtime_destroy_exec_env(exec_env);
        return NULL;
    }
    wasm_runtime_destroy_exec_env(exec_env);

    // Aloca buffer para copiar o resultado processado
    targ->result = malloc(targ->expected_len);
    if (!targ->result) {
        targ->error = -5;
        return NULL;
    }
    memcpy(targ->result, wasm_buf, targ->expected_len);

    // Libera a área de memória WASM (se necessário)
    wasm_runtime_module_free(module_inst, targ->wasm_buf_offset);

    targ->error = 0;
    return NULL;
}

void configure_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    //config.pixel_format = PIXFORMAT_JPEG;
    //config.pixel_format = PIXFORMAT_GRAYSCALE;

    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size   = FRAMESIZE_QQVGA; // QQVGA geralmente é 128x128
    //config.frame_size   = FRAMESIZE_QVGA; 
    //config.frame_size = FRAMESIZE_VGA;   // 640x480
    //config.frame_size = FRAMESIZE_SVGA;  // 800x600
    //config.frame_size = FRAMESIZE_XGA;   // 1024x768
    //config.frame_size = FRAMESIZE_SXGA;  // 1280x1024
    //config.frame_size = FRAMESIZE_UXGA;  // 1600x1200
    
    config.jpeg_quality = 10;            // Não usado para RGB565
    config.fb_count     = 2;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error");
    } else {
        ESP_LOGI(TAG, "Camera initialized successfully!");
    }
}

// Função auxiliar para imprimir o buffer em formato hexadecimal
void print_frame_hex(const uint8_t *buffer, size_t len) {
    for (size_t i = 0; i < len; i++) {
        // Imprime cada byte em 2 dígitos hexadecimais
        printf("%02X ", buffer[i]);
        // Adiciona uma quebra de linha a cada 16 bytes para melhor visualização
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n");
}

// Handler do endpoint de streaming
static esp_err_t stream_handler(httpd_req_t *req) {
    ESP_LOGI("HTTP_SERVER", "Cliente conectado ao endpoint de streaming");

    // Define o tipo de resposta para streaming multipart
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");

    while (1) {
        // 1. Captura o frame da câmera (em RGB565)
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE("HTTP_SERVER", "Falha ao capturar frame");
            break;
        }
        // Armazena largura e altura para usar depois na conversão
        uint16_t width = fb->width;
        uint16_t height = fb->height;
        // Calcula o tamanho do frame em RGB565
        uint32_t expected_len = width * height * 2;

        // 2. Cria uma thread pthread para processar o frame via WASM
        pthread_t thread;
        wasm_thread_arg_t arg;
        arg.fb = fb;
        arg.expected_len = expected_len;
        arg.wasm_buf_offset = 1024; // Offset fixo (garanta que não haja conflito)
        arg.result = NULL;
        arg.error = 0;

        if (pthread_create(&thread, NULL, wasm_process_frame, &arg) != 0) {
            ESP_LOGE("HTTP_SERVER", "Erro ao criar pthread para processamento WASM");
            esp_camera_fb_return(fb);
            break;
        }

        // Aguarda a conclusão da thread
        pthread_join(thread, NULL);

        if (arg.error != 0 || arg.result == NULL) {
            ESP_LOGE("HTTP_SERVER", "Erro no processamento WASM (código %d)", arg.error);
            esp_camera_fb_return(fb);
            break;
        }

        uint8_t *processed_img = arg.result;

        // Libera o frame capturado (já usamos os dados e armazenamos width/height)
        esp_camera_fb_return(fb);

        // 3. Converte o buffer processado (RGB565) para JPEG
        // Cria um "fake" camera_fb_t para usar com frame2jpg()
        camera_fb_t fake_fb;
        fake_fb.width = width;
        fake_fb.height = height;
        fake_fb.buf = processed_img;
        fake_fb.len = expected_len; // Tamanho do buffer RGB565
        fake_fb.format = PIXFORMAT_RGB565;

        uint8_t *jpg_buf = NULL;
        size_t jpg_len = 0;
        // Aqui definimos uma qualidade (por exemplo, 80). Ajuste conforme necessário.
        if (!frame2jpg(&fake_fb, 80, &jpg_buf, &jpg_len)) {
            ESP_LOGE("HTTP_SERVER", "Erro ao converter para JPEG");
            free(processed_img);
            break;
        }
        // Após a conversão, liberamos o buffer RAW
        free(processed_img);

        // 4. Envia o frame JPEG via streaming usando multipart
        char part_buf[128];
        int header_len = sprintf(part_buf,
                                 "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                                 (unsigned int)jpg_len);
        if (httpd_resp_send_chunk(req, part_buf, header_len) != ESP_OK) {
            free(jpg_buf);
            break;
        }
        if (httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_len) != ESP_OK) {
            free(jpg_buf);
            break;
        }
        if (httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK) {
            free(jpg_buf);
            break;
        }
        free(jpg_buf);

        // Controle de taxa de frames: delay de 100ms (aprox. 10 fps)
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    return ESP_OK;
}

// Configura e inicia o servidor HTTP com os endpoints
void start_file_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {

        // Endpoint para streaming de frames
        httpd_uri_t stream_uri = {
            .uri       = "/stream",
            .method    = HTTP_GET,
            .handler   = stream_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);

        ESP_LOGI(TAG_HTTP, "Servidor HTTP iniciado. Acesse:");
        ESP_LOGI(TAG_HTTP, "http://<ip_do_esp32>/stream");
    } else {
        ESP_LOGE(TAG_HTTP, "Falha ao iniciar o servidor HTTP");
    }
}

// Função que inicializa o runtime WASM
bool initialize_wasm_runtime() {
    uint8_t *wasm_file_buf = (uint8_t *)rust_drowsiness_trucker;
    uint32_t wasm_file_buf_size = rust_drowsiness_trucker_len;

    char error_buf[128];

    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(RuntimeInitArgs));
    init_args.mem_alloc_type = Alloc_With_System_Allocator;

    if (!wasm_runtime_full_init(&init_args)) {
        ESP_LOGE(TAG, "Falha ao inicializar o runtime WASM");
        return false;
    }

    wasm_module_t module = wasm_runtime_load(wasm_file_buf, wasm_file_buf_size, error_buf, sizeof(error_buf));
    if (!module) {
        ESP_LOGE(TAG, "Falha ao carregar o módulo WASM: %s", error_buf);
        wasm_runtime_destroy();
        return false;
    }

    module_inst = wasm_runtime_instantiate(module, 1024 * 1024, 512 * 1024, error_buf, sizeof(error_buf));
    if (!module_inst) {
        ESP_LOGE(TAG, "Falha ao instanciar o módulo WASM: %s", error_buf);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return false;
    }

    return true;
}

void app_main(void) {
    ESP_LOGI(TAG, "Inicializando a câmera...");

    // Inicializa a câmera
    configure_camera();

    wifi_init_sta();
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(WIFI_TAG, "Wi-Fi conectado.");

    // Inicializa o runtime WASM
    if (!initialize_wasm_runtime()) {
        ESP_LOGE(TAG, "Falha na inicialização do WASM, saindo...");
        return;
    }

    start_file_server();

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}







