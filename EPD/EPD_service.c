/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include <string.h>
#include "sdk_macros.h"
#include "ble_srv_common.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_pwr_mgmt.h"
#include "app_scheduler.h"
#include "EPD_service.h"
#include "nrf_log.h"

#if defined(S112)
//#define EPD_CFG_DEFAULT {0x14, 0x13, 0x06, 0x05, 0x04, 0x03, 0x02, 0x03, 0xFF, 0x12, 0x07} // 52811
#define EPD_CFG_DEFAULT {0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x03, 0xFF, 0x0D, 0x02} // 52810
#else
//#define EPD_CFG_DEFAULT {0x05, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x01, 0x07}
#endif

#ifndef EPD_CFG_DEFAULT
#define EPD_CFG_DEFAULT {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x03, 0x09, 0x03}
#endif

// defined in main.c
extern uint32_t timestamp(void);
extern void set_timestamp(uint32_t timestamp);
extern void sleep_mode_enter(void);

static void epd_gui_update(void * p_event_data, uint16_t event_size)
{
    epd_gui_update_event_t *event = (epd_gui_update_event_t *)p_event_data;
    ble_epd_t *p_epd = event->p_epd;

    EPD_GPIO_Init();
    epd_model_t *epd = epd_init((epd_model_id_t)p_epd->config.model_id);
    gui_data_t data = {
        .bwr             = epd->bwr,
        .width           = epd->width,
        .height          = epd->height,
        .timestamp       = event->timestamp,
        .temperature     = epd->drv->read_temp(),
        .voltage         = EPD_ReadVoltage(),
    };
    DrawGUI(&data, epd->drv->write_image, p_epd->display_mode);
    epd->drv->refresh();
    EPD_GPIO_Uninit();
}

/**@brief Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(ble_epd_t * p_epd, ble_evt_t * p_ble_evt)
{
    p_epd->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    EPD_GPIO_Init();
}

/**@brief Function for handling the @ref BLE_GAP_EVT_DISCONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_disconnect(ble_epd_t * p_epd, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_epd->conn_handle = BLE_CONN_HANDLE_INVALID;
    EPD_GPIO_Uninit();
}

static void epd_service_on_write(ble_epd_t * p_epd, uint8_t * p_data, uint16_t length)
{
    NRF_LOG_DEBUG("[EPD]: on_write LEN=%d\n", length);
    NRF_LOG_HEXDUMP_DEBUG(p_data, length);
    if (p_data == NULL || length <= 0) return;

    switch (p_data[0])
    {
      case EPD_CMD_SET_PINS:
          if (length < 8) return;

          p_epd->config.mosi_pin = p_data[1];
          p_epd->config.sclk_pin = p_data[2];
          p_epd->config.cs_pin = p_data[3];
          p_epd->config.dc_pin = p_data[4];
          p_epd->config.rst_pin = p_data[5];
          p_epd->config.busy_pin = p_data[6];
          p_epd->config.bs_pin = p_data[7];
          if (length > 8)
              p_epd->config.en_pin = p_data[8];
          epd_config_write(&p_epd->config);

          EPD_GPIO_Uninit();
          EPD_GPIO_Load(&p_epd->config);
          EPD_GPIO_Init();
          break;

      case EPD_CMD_INIT: {
          uint8_t id = length > 1 ? p_data[1] : p_epd->config.model_id;
          if (id != p_epd->config.model_id) {
              p_epd->config.model_id = id;
              epd_config_write(&p_epd->config);
          }
          p_epd->epd = epd_init((epd_model_id_t)id);
        } break;

      case EPD_CMD_CLEAR:
          p_epd->display_mode = MODE_NONE;
          p_epd->epd->drv->clear();
          break;

      case EPD_CMD_SEND_COMMAND:
          if (length < 2) return;
          EPD_WriteCommand(p_data[1]);
          break;

      case EPD_CMD_SEND_DATA:
          EPD_WriteData(&p_data[1], length - 1);
          break;

      case EPD_CMD_REFRESH:
          p_epd->display_mode = MODE_NONE;
          p_epd->epd->drv->refresh();
          break;

      case EPD_CMD_SLEEP:
          p_epd->epd->drv->sleep();
          break;

      case EPD_CMD_SET_TIME: {
          if (length < 5) return;

          NRF_LOG_DEBUG("time: %02x %02x %02x %02x\n", p_data[1], p_data[2], p_data[3], p_data[4]);
          if (length > 5) NRF_LOG_DEBUG("timezone: %d\n", (int8_t)p_data[5]);

          uint32_t timestamp = (p_data[1] << 24) | (p_data[2] << 16) | (p_data[3] << 8) | p_data[4];
          timestamp += (length > 5 ? (int8_t)p_data[5] : 8) * 60 * 60; // timezone
          set_timestamp(timestamp);
          p_epd->display_mode = length > 6 ? (display_mode_t)p_data[6] : MODE_CALENDAR;
          ble_epd_on_timer(p_epd, timestamp, true);
      } break;

      case EPD_CMD_WRITE_IMAGE: // MSB=0000: ram begin, LSB=1111: black
          if (length < 3) return;
          if ((p_data[1] >> 4) == 0x00) {
              bool black = (p_data[1] & 0x0F) == 0x0F;
              EPD_WriteCommand(black ? p_epd->epd->drv->cmd_write_ram1 : p_epd->epd->drv->cmd_write_ram2);
          }
          EPD_WriteData(&p_data[2], length - 2);
          break;

      case EPD_CMD_SET_CONFIG:
          if (length < 2) return;
          memcpy(&p_epd->config, &p_data[1], (length - 1 > EPD_CONFIG_SIZE) ? EPD_CONFIG_SIZE : length - 1);
          epd_config_write(&p_epd->config);
          break;

      case EPD_CMD_SYS_SLEEP:
          sleep_mode_enter();
          break;

        case EPD_CMD_SYS_RESET:
#if defined(S112)
            nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_RESET);
#else
            NVIC_SystemReset();
#endif
        break;

      case EPD_CMD_CFG_ERASE:
          epd_config_clear(&p_epd->config);
          nrf_delay_ms(100); // required
          NVIC_SystemReset();
          break;

      default:
        break;
    }
}

/**@brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(ble_epd_t * p_epd, ble_evt_t * p_ble_evt)
{
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (
        (p_evt_write->handle == p_epd->char_handles.cccd_handle)
        &&
        (p_evt_write->len == 2)
       )
    {
        if (ble_srv_is_notification_enabled(p_evt_write->data))
        {
            NRF_LOG_DEBUG("notification enabled\n");
            p_epd->is_notification_enabled = true;
            static uint16_t length = sizeof(epd_config_t);
            NRF_LOG_DEBUG("send epd config\n");
            uint32_t err_code = ble_epd_string_send(p_epd, (uint8_t *)&p_epd->config, length);
            if (err_code != NRF_ERROR_INVALID_STATE)
                APP_ERROR_CHECK(err_code);
        }
        else
        {
            p_epd->is_notification_enabled = false;
        }
    }
    else if (p_evt_write->handle == p_epd->char_handles.value_handle)
    {
        epd_service_on_write(p_epd, p_evt_write->data, p_evt_write->len);
    }
    else
    {
        // Do Nothing. This event is not relevant for this service.
    }
}

#if defined(S112)
void ble_epd_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    if (p_context == NULL || p_ble_evt == NULL) return;
    
    ble_epd_t *p_epd = (ble_epd_t *)p_context;
    ble_epd_on_ble_evt(p_epd, (ble_evt_t *)p_ble_evt);
}
#endif

void ble_epd_on_ble_evt(ble_epd_t * p_epd, ble_evt_t * p_ble_evt)
{
    if ((p_epd == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_epd, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_epd, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_epd, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}


static uint32_t epd_service_init(ble_epd_t * p_epd)
{
    ble_uuid_t            ble_uuid = {0};
    ble_uuid128_t         base_uuid = BLE_UUID_EPD_SVC_BASE;
    ble_add_char_params_t add_char_params;
    uint8_t               app_version = APP_VERSION;
 
    VERIFY_SUCCESS(sd_ble_uuid_vs_add(&base_uuid, &ble_uuid.type));

    ble_uuid.type = ble_uuid.type;
    ble_uuid.uuid = BLE_UUID_EPD_SVC;
    VERIFY_SUCCESS(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                            &ble_uuid,
                                            &p_epd->service_handle));

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid                     = BLE_UUID_EPD_CHAR;
    add_char_params.uuid_type                = ble_uuid.type;
    add_char_params.max_len                  = BLE_EPD_MAX_DATA_LEN;
    add_char_params.init_len                 = sizeof(uint8_t);
    add_char_params.is_var_len               = true;
    add_char_params.char_props.notify        = 1;
    add_char_params.char_props.write         = 1;
    add_char_params.char_props.write_wo_resp = 1;
    add_char_params.read_access              = SEC_OPEN;
    add_char_params.write_access             = SEC_OPEN;
    add_char_params.cccd_write_access        = SEC_OPEN;

    VERIFY_SUCCESS(characteristic_add(p_epd->service_handle, &add_char_params, &p_epd->char_handles));

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid                     = BLE_UUID_APP_VER;
    add_char_params.uuid_type                = ble_uuid.type;
    add_char_params.max_len                  = sizeof(uint8_t);
    add_char_params.init_len                 = sizeof(uint8_t);
    add_char_params.p_init_value             = &app_version;
    add_char_params.char_props.read          = 1;
    add_char_params.read_access              = SEC_OPEN;

    return characteristic_add(p_epd->service_handle, &add_char_params, &p_epd->app_ver_handles);
}

void ble_epd_sleep_prepare(ble_epd_t * p_epd)
{
    // Turn off led
    EPD_LED_OFF();
    // Prepare wakeup pin
    if (p_epd->config.wakeup_pin != 0xFF)
    {
        nrf_gpio_cfg_sense_input(p_epd->config.wakeup_pin, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_HIGH);
    }
}

uint32_t ble_epd_init(ble_epd_t * p_epd)
{
    if (p_epd == NULL) return NRF_ERROR_NULL;

    // Initialize the service structure.
    p_epd->max_data_len = BLE_EPD_MAX_DATA_LEN;
    p_epd->conn_handle             = BLE_CONN_HANDLE_INVALID;
    p_epd->is_notification_enabled = false;

    epd_config_init(&p_epd->config);
    epd_config_read(&p_epd->config);
    
    // write default config
    if (epd_config_empty(&p_epd->config))
    {
        uint8_t cfg[] = EPD_CFG_DEFAULT;
        memcpy(&p_epd->config, cfg, sizeof(cfg));
        epd_config_write(&p_epd->config);
    }

    // load config
    EPD_GPIO_Load(&p_epd->config);

    // blink LED on start
    EPD_LED_BLINK();

    // Add the service.
    return epd_service_init(p_epd);
}

uint32_t ble_epd_string_send(ble_epd_t * p_epd, uint8_t * p_string, uint16_t length)
{
    if ((p_epd->conn_handle == BLE_CONN_HANDLE_INVALID) || (!p_epd->is_notification_enabled))
        return NRF_ERROR_INVALID_STATE;
    if (length > p_epd->max_data_len)
        return NRF_ERROR_INVALID_PARAM;

    ble_gatts_hvx_params_t hvx_params;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_epd->char_handles.value_handle;
    hvx_params.p_data = p_string;
    hvx_params.p_len  = &length;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;

    return sd_ble_gatts_hvx(p_epd->conn_handle, &hvx_params);
}

void ble_epd_on_timer(ble_epd_t * p_epd, uint32_t timestamp, bool force_update)
{
    // Update calendar on 00:00:00, clock on every minute
    if (force_update || 
        (p_epd->display_mode == MODE_CALENDAR && timestamp % 86400 == 0) ||
        (p_epd->display_mode == MODE_CLOCK && timestamp % 60 == 0)) {
        epd_gui_update_event_t event = { p_epd, timestamp };
        app_sched_event_put(&event, sizeof(epd_gui_update_event_t), epd_gui_update);
    }
}
