/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "mdf_common.h"
#include "mwifi.h"

// #define MEMORY_DEBUG
#define CONFIG_COMMUNICATE_UNICAST

static const char *TAG = "mwifi_examples";

/**
 *  @brief: Node (any) message processing thread
 *
  */
static void node_read_task(void *arg)
{
    mdf_err_t ret = MDF_OK;

    //disabled for this example
    //cJSON *pJson = NULL;
    //cJSON *pSub  = NULL;

    char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size   = MWIFI_PAYLOAD_LEN;

    mwifi_data_type_t data_type      = {0x0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0x0};

    MDF_LOGI("READER task started");

    for (;;) {

        //wait for being networked
        if (!mwifi_is_connected()) {
            vTaskDelay(500 / portTICK_RATE_MS);
            MDF_LOGD("...waiting for connection...");
            continue;
        }

        MDF_LOGI("READER running");

        size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);

        //wait for incoming message
        ret = mwifi_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "mwifi_read() failed with error %s, exiting task", mdf_err_to_name(ret) );

        MDF_LOGD("READER message received from " MACSTR ", size: %d, data: %s", MAC2STR(src_addr), size, data);

        /* handle the message here
        pJson = cJSON_Parse(data);
        MDF_ERROR_CONTINUE(!pJson, "cJSON_Parse, data format error, data: %s", data);

        pSub = cJSON_GetObjectItem(pJson, "status");

        if (!pSub) {
            MDF_LOGW("cJSON_GetObjectItem, Destination address not set");
            cJSON_Delete(pJson);
            continue;
        }

        cJSON_Delete(pJson);
        */
    }

    MDF_LOGW("Note read task is exit");

    MDF_FREE(data);
    vTaskDelete(NULL);
}

/**
 *  @brief: Group ID multicasting thread
 *
 *  @Note: multicasting node must be a member of all the groups to transmit
 */
static void node_write_task(void *arg)
{
    mdf_err_t ret = MDF_OK;
    int count     = 0;
    size_t size   = 0;
    char *data    = NULL;

    //!!! important !!!
    mwifi_data_type_t data_type = {
        .group       = true,

#ifdef CONFIG_COMMUNICATE_UNICAST
        /**
         * @brief To use unicast, you need to call `mwifi_read()` for forwarding.
         */
        .communicate = MWIFI_COMMUNICATE_UNICAST,
#else
        .communicate = MWIFI_COMMUNICATE_BROADCAST,
#endif /**< CONFIG_COMMUNICATE_UNICAST */
    };

    MDF_LOGI("WRITER task started");

    do {
        //get number of groups the node belongs to
        size_t group_id_num               = 3;
        const uint8_t group_id_list[3][6] = {
            {0x01, 0x00, 0x5e, 0xae, 0xae, 0xae},
            {0x01, 0x00, 0x5e, 0xaa, 0xaa, 0xaa},
            {0x01, 0x00, 0x5e, 0xac, 0xac, 0xac},
        };

        for (;;) {

            //wait for being networked
            if (!mwifi_is_connected()) {
                vTaskDelay(500 / portTICK_RATE_MS);
                MDF_LOGD("...waiting for connection...");
                continue;
            }

            MDF_LOGI("WRITER running");

            //format data to multicast
            int nRes = asprintf(&data, "{\"seq\":%d,\"layer\":%d}", count++, esp_mesh_get_layer());
            ret = nRes <= 0 ? MDF_ERR_INVALID_ARG : MDF_OK;
            MDF_ERROR_CONTINUE( ret != MDF_OK, "asprintf() failed to format data -> no message sent, continue" );

            size = (size_t)nRes;
            MDF_LOGD("WRITER message sent: size %d, data %s", size, data);

            //multicasting (unicast transfer to each group id)
            for (int i = 0; i < group_id_num; ++i) {
                ret = mwifi_write(group_id_list[i], &data_type, data, size, true);
                MDF_ERROR_CONTINUE(ret != MDF_OK, "mwifi_write() failed to send data to group id " MACSTR ", error (%s)", MAC2STR(group_id_list[i]), mdf_err_to_name(ret));
                MDF_LOGW("\tsent to group_id: " MACSTR, MAC2STR(group_id_list[i]));
            }

            MDF_FREE(data);

            //sleep for 2 seconds
            vTaskDelay(2000 / portTICK_RATE_MS);
        }

        // MDF_FREE(group_id_list);
        break;

    } while (true);

    MDF_LOGW("WRITER task finished");
    vTaskDelete(NULL);
}


/**
 * @brief System information rpint callback
 */
static void print_system_info_timercb(void *timer)
{
    uint8_t primary                 = 0;
    wifi_second_chan_t second       = 0;
    mesh_addr_t parent_bssid        = {0};
    uint8_t sta_mac[MWIFI_ADDR_LEN] = {0};
    mesh_assoc_t mesh_assoc         = {0x0};
    wifi_sta_list_t wifi_sta_list   = {0x0};

    esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    esp_wifi_get_channel(&primary, &second);
    esp_wifi_vnd_mesh_get(&mesh_assoc);
    esp_mesh_get_parent_bssid(&parent_bssid);

    MDF_LOGI("System information, channel: %d, layer: %d, self mac: " MACSTR ", parent bssid: " MACSTR
             ", parent rssi: %d, node num: %d, free heap: %u", primary,
             esp_mesh_get_layer(), MAC2STR(sta_mac), MAC2STR(parent_bssid.addr),
             mesh_assoc.rssi, esp_mesh_get_total_node_num(), esp_get_free_heap_size());

    for (int i = 0; i < wifi_sta_list.num; i++) {
        MDF_LOGI("Child mac: " MACSTR, MAC2STR(wifi_sta_list.sta[i].mac));
    }

#ifdef MEMORY_DEBUG
    if (!heap_caps_check_integrity_all(true)) {
        MDF_LOGE("At least one heap is corrupt");
    }

    mdf_mem_print_heap();
    mdf_mem_print_record();
#endif /**< MEMORY_DEBUG */
}


static mdf_err_t wifi_init()
{
    mdf_err_t ret          = nvs_flash_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        MDF_ERROR_ASSERT(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    MDF_ERROR_ASSERT(ret);

    tcpip_adapter_init();
    MDF_ERROR_ASSERT(esp_event_loop_init(NULL, NULL));
    MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
    MDF_ERROR_ASSERT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
    MDF_ERROR_ASSERT(esp_wifi_set_ps(WIFI_PS_NONE));
    MDF_ERROR_ASSERT(esp_mesh_set_6m_rate(false));
    MDF_ERROR_ASSERT(esp_wifi_start());

    return MDF_OK;
}

/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx)
{
    MDF_LOGI("event_loop_cb, event: %d", event);

    switch (event) {
        case MDF_EVENT_MWIFI_STARTED:
            MDF_LOGI("MESH is started");
            break;

        case MDF_EVENT_MWIFI_PARENT_CONNECTED:
            MDF_LOGI("Parent is connected on station interface");
            break;

        case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
            MDF_LOGI("Parent is disconnected on station interface");
            break;

        case MDF_EVENT_MWIFI_ROUTING_TABLE_ADD:
        case MDF_EVENT_MWIFI_ROUTING_TABLE_REMOVE:
            MDF_LOGI("total_num: %d", esp_mesh_get_total_node_num());
            break;

        case MDF_EVENT_MWIFI_ROOT_GOT_IP: {
            MDF_LOGI("Root obtains the IP address. It is posted by LwIP stack automatically");
            break;
        }

        default:
            break;
    }

    return MDF_OK;
}

/**
 * @brief:
 *  ESP-MESH MDF MULTICASTING example
 *
 * @Note:
 *  This example runs either WRITER-mode or READER-mode code (see DEVICE_TYPE in Kconfig settings):
 *      - WRITER node needs to be a member of all groups of interest
 *      - READER node can belong to any group(s)
 *  Current version doesn't support GROUPID configuration in KConfig, sry (=> recompile for each READER node group)
 */

#define DEVICE_TYPE_WRITER  1
#define DEVICE_TYPE_READER  2

void app_main()
{
    mwifi_init_config_t cfg   = MWIFI_INIT_CONFIG_DEFAULT();
    mwifi_config_t config     = {
        .router_ssid     = CONFIG_ROUTER_SSID,
        .router_password = CONFIG_ROUTER_PASSWORD,
        .mesh_id         = CONFIG_MESH_ID,
        .mesh_type       = CONFIG_DEVICE_MESH_TYPE
    };

    /**
     * @brief Set the log level for serial port printing.
     */
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("mwifi", ESP_LOG_DEBUG);

    /**
     * @brief Initialize wifi mesh.
     */
    MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
    MDF_ERROR_ASSERT(wifi_init());
    MDF_ERROR_ASSERT(mwifi_init(&cfg));
    MDF_ERROR_ASSERT(mwifi_set_config(&config));
    MDF_ERROR_ASSERT(mwifi_start());

    if ( CONFIG_DEVICE_TM_TYPE == DEVICE_TYPE_WRITER ) {

        MDF_LOGI("RUNNING AS 'WRITER'");

        /**
         * @brief If you just send data without receiving it, you don't need to
         *        call `esp_mesh_set_group_id()`, otherwise the sending device
         *        will also receive the data, which will be cached in the underlying
         *        protocol stack, causing `queue full`
         */
/*
        //require membership in all groups
        const uint8_t group_id_list[3][6] = {
            {0x01, 0x00, 0x5e, 0xae, 0xae, 0xae},
            {0x01, 0x00, 0x5e, 0xaa, 0xaa, 0xaa},
            {0x01, 0x00, 0x5e, 0xac, 0xac, 0xac},
        };

        MDF_ERROR_ASSERT(esp_mesh_set_group_id((mesh_addr_t *)group_id_list, 3));
*/
        xTaskCreate(node_write_task, "node_write_task", 4 * 1024, NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);

#ifdef CONFIG_COMMUNICATE_UNICAST
        /**
         * @brief If you use unicast to multicast, you need to forward it through `mwifi_read`
         */
        xTaskCreate(node_read_task, "node_read_task", 4 * 1024, NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
#endif /**< CONFIG_COMMUNICATE_UNICAST */
    }
    else {

        MDF_LOGI("RUNNING AS READER");

        //select/extend a group memebership here
        const uint8_t group_id_list[1][6] = { {0x01, 0x00, 0x5e, 0xae, 0xae, 0xae} };
        //const uint8_t group_id_list[1][6] = { {0x01, 0x00, 0x5e, 0xaa, 0xaa, 0xaa} };
        //const uint8_t group_id_list[1][6] = { {0x01, 0x00, 0x5e, 0xac, 0xac, 0xac} };

        MDF_ERROR_ASSERT(esp_mesh_set_group_id((mesh_addr_t *)group_id_list, 1));

        xTaskCreate(node_read_task, "node_read_task", 4 * 1024, NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
    }

    TimerHandle_t timer = xTimerCreate("print_system_info", 10000 / portTICK_RATE_MS, true, NULL, print_system_info_timercb);
    xTimerStart(timer, 0);
}
