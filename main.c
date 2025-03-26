/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
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

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#if defined(S112)
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#else
#include "fstorage.h"
#include "softdevice_handler.h"
#endif
#include "nrf_power.h"
#include "app_error.h"
#include "app_timer.h"
#include "app_scheduler.h"
#include "nrf_drv_gpiote.h"
#include "nrf_pwr_mgmt.h"
#include "EPD_service.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#if defined(S112)
#include "nrf_log_default_backends.h"
#endif

#define CENTRAL_LINK_COUNT              0                                               /**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1                                               /**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define DEVICE_NAME                      "NRF_EPD"                                      /**< Name of device. Will be included in the advertising data. */
#define APP_ADV_INTERVAL                 320                                            /**< The advertising interval (in units of 0.625 ms. This value corresponds to 200 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS       120                                            /**< The advertising timeout (in units of seconds). */
#define APP_TIMER_PRESCALER              0                                              /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE          4                                              /**< Size of timer operation queues. */

#if defined(S112)
    #define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */
    #define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */
    #define TIMER_TICKS(MS) APP_TIMER_TICKS(MS)
#else
    #define TIMER_TICKS(MS) APP_TIMER_TICKS(MS, APP_TIMER_PRESCALER)
    // Low frequency clock source to be used by the SoftDevice
    #define NRF_CLOCK_LFCLKSRC      {.source        = NRF_CLOCK_LF_SRC_SYNTH,           \
                                     .rc_ctiv       = 0,                                \
                                     .rc_temp_ctiv  = 0,                                \
                                     .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM}
#endif

#define MIN_CONN_INTERVAL                MSEC_TO_UNITS(7.5, UNIT_1_25_MS)               /**< Minimum connection interval (7.5 ms) */
#define MAX_CONN_INTERVAL                MSEC_TO_UNITS(30, UNIT_1_25_MS)                /**< Maximum connection interval (30 ms). */
#define SLAVE_LATENCY                    6                                              /**< Slave latency. */
#define CONN_SUP_TIMEOUT                 MSEC_TO_UNITS(430, UNIT_10_MS)                 /**< Connection supervisory timeout (430 ms). */
#define FIRST_CONN_PARAMS_UPDATE_DELAY   TIMER_TICKS(5000)                              /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY    TIMER_TICKS(30000)                             /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT     3                                              /**< Number of attempts before giving up the connection parameter negotiation. */

#define SCHED_MAX_EVENT_DATA_SIZE       EPD_GUI_SCHD_EVENT_DATA_SIZE                    /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE                10                                              /**< Maximum number of events in the scheduler queue. */

#define CLOCK_TIMER_INTERVAL             TIMER_TICKS(1000)                              /**< Clock timer interval (ticks). */

#define DEAD_BEEF                        0xDEADBEEF                                     /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#if defined(S112)
NRF_BLE_GATT_DEF(m_gatt);                                                               /**< GATT module instance. */
BLE_ADVERTISING_DEF(m_advertising);                                                     /**< Advertising module instance. */
#endif
static uint16_t                          m_conn_handle = BLE_CONN_HANDLE_INVALID;       /**< Handle of the current connection. */
static ble_uuid_t                        m_adv_uuids[] = {{BLE_UUID_EPD_SERVICE, \
                                                           EPD_SERVICE_UUID_TYPE}};     /**< Universally unique service identifier. */

BLE_EPD_DEF(m_epd);                                                                     /**< Structure to identify the EPD Service. */
static uint32_t                          m_timestamp = 1735689600;                      /**< Current timestamp. */
APP_TIMER_DEF(m_clock_timer_id);                                                        /**< Clock timer. */

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void clock_timer_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    m_timestamp++;

    ble_epd_on_timer(&m_epd, m_timestamp, false);
}

/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    // Initialize timer module.
#if defined(S112)
    APP_ERROR_CHECK(app_timer_init());
#else
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
#endif
    // Create timers.
    APP_ERROR_CHECK(app_timer_create(&m_clock_timer_id,
                                     APP_TIMER_MODE_REPEATED,
                                     clock_timer_timeout_handler));
}

/**@brief Function for starting application timers.
 */
static void application_timers_start(void)
{
    // Start application timers.
    APP_ERROR_CHECK(app_timer_start(m_clock_timer_id, CLOCK_TIMER_INTERVAL, NULL));
}

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    NRF_LOG_DEBUG("Entering deep sleep mode\n");

    ble_epd_sleep_prepare(&m_epd);
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
}

bool epd_cmd_callback(uint8_t cmd, uint8_t *data, uint16_t len)
{
    switch (cmd)
    {
        case EPD_CMD_SET_TIME:
            if (len < 4) {
                NRF_LOG_DEBUG("invalid time data!\n");
                return false;
            }

            NRF_LOG_DEBUG("time: %02x %02x %02x %02x\n", data[0], data[1], data[2], data[3]);
            if (len > 4) NRF_LOG_DEBUG("timezone: %d\n", (int8_t)data[4]);

            app_timer_stop(m_clock_timer_id);
            m_timestamp = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            m_timestamp += (len > 4 ? (int8_t)data[4] : 8) * 60 * 60; // timezone
            app_timer_start(m_clock_timer_id, CLOCK_TIMER_INTERVAL, NULL);
            m_epd.display_mode = len > 5 ? (display_mode_t)data[5] : MODE_CALENDAR;
            ble_epd_on_timer(&m_epd, m_timestamp, true);
            return true;

        case EPD_CMD_SYS_SLEEP:
            sleep_mode_enter();
            return true;

        case EPD_CMD_SYS_RESET:
#if defined(S112)
            nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_RESET);
#else
            NVIC_SystemReset();
#endif
            return true;
            
        default:
            break;
    }
    return false;
}

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    memset(&m_epd, 0, sizeof(ble_epd_t));
    APP_ERROR_CHECK(ble_epd_init(&m_epd, epd_cmd_callback));
}

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    char                    device_name[20];
    ble_gap_addr_t          addr;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
#if defined(S112)
    APP_ERROR_CHECK(sd_ble_gap_addr_get(&addr));
#else
    APP_ERROR_CHECK(sd_ble_gap_address_get(&addr));
#endif

    NRF_LOG_INFO("Bluetooth MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   addr.addr[5], addr.addr[4], addr.addr[3],
                   addr.addr[2], addr.addr[1], addr.addr[0]);

    snprintf(device_name, 20, "%s_%02X%02X", DEVICE_NAME, addr.addr[1],addr.addr[0]);
    APP_ERROR_CHECK(sd_ble_gap_device_name_set(&sec_mode,
                                               (const uint8_t *)device_name,
                                               strlen(device_name)));

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    APP_ERROR_CHECK(sd_ble_gap_ppcp_set(&gap_conn_params));
}

/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        APP_ERROR_CHECK(sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE));
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    APP_ERROR_CHECK(ble_conn_params_init(&cp_init));
}


static void advertising_start(void)
{
    NRF_LOG_INFO("advertising start\n");
#if defined(S112)
    APP_ERROR_CHECK(ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST));
#else
    APP_ERROR_CHECK(ble_advertising_start(BLE_ADV_MODE_FAST));
#endif
}

void gpiote_evt_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    NRF_LOG_DEBUG("pin: %d, event: %d\n", pin, action);

    nrf_drv_gpiote_in_event_disable(pin);
    nrf_drv_gpiote_in_uninit(pin);
    nrf_drv_gpiote_uninit();

    advertising_start();
}

static void setup_wakeup_pin(nrf_drv_gpiote_pin_t pin) {
    NRF_LOG_DEBUG("Setting up wakeup pin\n");

    APP_ERROR_CHECK(nrf_drv_gpiote_init());
    nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_LOTOHI(false);
    APP_ERROR_CHECK(nrf_drv_gpiote_in_init(pin, &config, gpiote_evt_handler));
    nrf_drv_gpiote_in_event_enable(pin, true);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            break;
        case BLE_ADV_EVT_IDLE:
            NRF_LOG_INFO("advertising timeout\n");
            if (m_epd.config.wakeup_pin != 0xFF) {
                if (m_epd.display_mode != MODE_NONE)
                    setup_wakeup_pin(m_epd.config.wakeup_pin);
                else
                    sleep_mode_enter();
            } else {
                advertising_start();
            }
            break;
        default:
            break;
    }
}

/**@brief Function for the application's SoftDevice event handler.
 *
 * @param[in] p_ble_evt SoftDevice event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("CONNECTED\n");
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("DISCONNECTED\n");
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
#if !defined(S112)
            advertising_start();
#endif
            break;
#if defined(S112)
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            APP_ERROR_CHECK(sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys));
        } break;
#endif

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            APP_ERROR_CHECK(sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL));
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            APP_ERROR_CHECK(sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0));
            break;
        
        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            APP_ERROR_CHECK(sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                                  BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION));
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            APP_ERROR_CHECK(sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                                  BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION));
            break;

        default:
            // No implementation needed.
            break;
    }
}


#if defined(S112)
/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    UNUSED_PARAMETER(p_context);

    on_ble_evt((ble_evt_t *)p_ble_evt);
}

#else
/**@brief Function for dispatching a SoftDevice event to all modules with a SoftDevice
 *        event handler.
 *
 * @details This function is called from the SoftDevice event interrupt handler after a
 *          SoftDevice event has been received.
 *
 * @param[in] p_ble_evt  SoftDevice event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_epd_on_ble_evt(&m_epd, p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in] sys_evt  System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    // Dispatch the system event to the fstorage module, where it will be
    // dispatched to the Flash Data Storage (FDS) module.
    fs_sys_event_handler(sys_evt);

    // Dispatch to the Advertising module last, since it will check if there are any
    // pending flash operations in fstorage. Let fstorage process system events first,
    // so that it can report correctly to the Advertising module.
    ble_advertising_on_sys_evt(sys_evt);
}
#endif

/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
#if defined(S112)
    APP_ERROR_CHECK(nrf_sdh_enable_request());

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    APP_ERROR_CHECK(nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start));

    // Enable BLE stack.
    APP_ERROR_CHECK(nrf_sdh_ble_enable(&ram_start));

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
#else
    nrf_clock_lf_cfg_t  clock_lf_cfg = NRF_CLOCK_LFCLKSRC;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    ble_enable_params_t ble_enable_params;
    APP_ERROR_CHECK(softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                         PERIPHERAL_LINK_COUNT,
                                                         &ble_enable_params));

    // Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);
    
    // Enable BLE stack.
    APP_ERROR_CHECK(softdevice_enable(&ble_enable_params));

    // Subscribe for BLE events.
    APP_ERROR_CHECK(softdevice_ble_evt_handler_set(ble_evt_dispatch));

    // Subscribe for System events.
    APP_ERROR_CHECK(softdevice_sys_evt_handler_set(sys_evt_dispatch));
#endif
}

#if defined(S112)
/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_epd.max_data_len = p_evt->params.att_mtu_effective - 3;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_epd.max_data_len, m_epd.max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    APP_ERROR_CHECK(nrf_ble_gatt_init(&m_gatt, gatt_evt_handler));
    APP_ERROR_CHECK(nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE));
}
#else
// Set BW Config to HIGH.
static void ble_options_set(void)
{
    ble_opt_t ble_opt;

    memset(&ble_opt, 0, sizeof(ble_opt));

    ble_opt.common_opt.conn_bw.role = BLE_GAP_ROLE_PERIPH;
    ble_opt.common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_HIGH;
    ble_opt.common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_HIGH;

    APP_ERROR_CHECK(sd_ble_opt_set(BLE_COMMON_OPT_CONN_BW, &ble_opt));
}
#endif

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
#if defined(S112)
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS * 100;
    init.evt_handler = on_adv_evt;

    APP_ERROR_CHECK(ble_advertising_init(&m_advertising, &init));

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
#else
    ble_advdata_t          advdata;
    ble_advdata_t          scanrsp;
    ble_adv_modes_config_t options;

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = false;
    advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    
    memset(&scanrsp, 0, sizeof(scanrsp));
    scanrsp.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    scanrsp.uuids_complete.p_uuids  = m_adv_uuids;

    memset(&options, 0, sizeof(options));
    options.ble_adv_fast_enabled  = true;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;

    APP_ERROR_CHECK(ble_advertising_init(&advdata, &scanrsp, &options, on_adv_evt, NULL));
#endif
}

// return current timestamp
uint32_t timestamp(void)
{
    return m_timestamp;
}

/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    APP_ERROR_CHECK(NRF_LOG_INIT(timestamp));
#if defined(S112)
    NRF_LOG_DEFAULT_BACKENDS_INIT();
#endif
}

/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
#if defined(S112)
    APP_ERROR_CHECK(nrf_pwr_mgmt_init());
#else
    APP_ERROR_CHECK(nrf_pwr_mgmt_init(APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER)));
#endif
}

/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
        nrf_pwr_mgmt_run();
}

/**@brief Function for application main entry.
 */
int main(void)
{
    log_init();

    NRF_LOG_DEBUG("init..\n");

    timers_init();
    power_management_init();
    ble_stack_init();
    scheduler_init();
    gap_params_init();
#if defined(S112)
    gatt_init();
#else
    ble_options_set();
#endif
    services_init();
    advertising_init();
    conn_params_init();

    NRF_LOG_DEBUG("start..\n");

    // Start execution.
    application_timers_start();

    advertising_start();

    NRF_LOG_DEBUG("done.\n");

    for (;;)
    {
        app_sched_execute();
        idle_state_handle();
    }
}
