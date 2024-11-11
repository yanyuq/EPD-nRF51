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

/**@file
 *
 * @defgroup ble_sdk_srv_nus Nordic UART Service
 * @{
 * @ingroup  ble_sdk_srv
 * @brief    Nordic UART Service implementation.
 *
 * @details The Nordic UART Service is a simple GATT-based service with TX and RX characteristics.
 *          Data received from the peer is passed to the application, and the data received
 *          from the application of this service is sent to the peer as Handle Value
 *          Notifications. This module demonstrates how to implement a custom GATT-based
 *          service and characteristics using the S110 SoftDevice. The service
 *          is used by the application to send and receive ASCII text strings to and from the
 *          peer.
 *
 * @note The application must propagate S110 SoftDevice events to the Nordic UART Service module
 *       by calling the ble_nus_on_ble_evt() function from the ble_stack_handler callback.
 */

#ifndef EPD_BLE_H__
#define EPD_BLE_H__

#include "ble.h"
#include "ble_srv_common.h"
#include <stdint.h>
#include <stdbool.h>
#include "DEV_Config.h"

#define BLE_EPD_MAX_DATA_LEN (GATT_MTU_SIZE_DEFAULT - 3) /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */

enum EPD_CMDS
{
		EPD_CMD_SET_PINS,
		EPD_CMD_INIT,
		EPD_CMD_CLEAR,
		EPD_CMD_SEND_COMMAND,
		EPD_CMD_SEND_DATA,
		EPD_CMD_DISPLAY,
		EPD_CMD_SLEEP,
};

/* Forward declaration of the epd_driver_t type. */
typedef struct epd_driver_s epd_driver_t;

/**< epd driver DIs. */
enum EPD_DRIVER_IDS
{
		EPD_DRIVER_4IN2 = 1,
		EPD_DRIVER_4IN2_V2,
		EPD_DRIVER_4IN2B_V2,
};

/**@brief EPD driver structure.
 *
 * @details This structure contains epd driver functions.
 */
struct epd_driver_s
{
		uint8_t id;                                       /**< driver ID. */
		void (*init)(void);                               /**< Initialize the e-Paper register */
		void (*clear)(void);                              /**< Clear screen */
		void (*send_command)(UBYTE Reg);                  /**< send command */
		void (*send_data)(UBYTE Data);                    /**< send data */
		void (*display)(void);                            /**< Sends the image buffer in RAM to e-Paper and displays */
		void (*sleep)(void);                              /**< Enter sleep mode */
};

/* Forward declaration of the ble_epd_t type. */
typedef struct ble_epd_s ble_epd_t;

/**@brief EPD Service structure.
 *
 * @details This structure contains status information related to the service.
 */
struct ble_epd_s
{
    uint8_t                  uuid_type;               /**< UUID type for EPD Service Base UUID. */
    uint16_t                 service_handle;          /**< Handle of EPD Service (as provided by the S110 SoftDevice). */
    ble_gatts_char_handles_t char_handles;            /**< Handles related to the EPD characteristic (as provided by the S110 SoftDevice). */
    uint16_t                 conn_handle;             /**< Handle of the current connection (as provided by the S110 SoftDevice). BLE_CONN_HANDLE_INVALID if not in a connection. */
    bool                     is_notification_enabled; /**< Variable to indicate if the peer has enabled notification of the RX characteristic.*/
		epd_driver_t             *driver;                 /**< current EPD driver */
};

/**@brief Function for initializing the EPD Service.
 *
 * @param[out] p_epd      EPD Service structure. This structure must be supplied
 *                        by the application. It is initialized by this function and will
 *                        later be used to identify this particular service instance.
 * @param[in] p_epd_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was successfully initialized. Otherwise, an error code is returned.
 * @retval NRF_ERROR_NULL If either of the pointers p_epd or p_epd_init is NULL.
 */
uint32_t ble_epd_init(ble_epd_t * p_epd);

/**@brief Function for handling the EPD Service's BLE events.
 *
 * @details The EPD Service expects the application to call this function each time an
 * event is received from the S110 SoftDevice. This function processes the event if it
 * is relevant and calls the EPD Service event handler of the
 * application if necessary.
 *
 * @param[in] p_epd       EPD Service structure.
 * @param[in] p_ble_evt   Event received from the S110 SoftDevice.
 */
void ble_epd_on_ble_evt(ble_epd_t * p_epd, ble_evt_t * p_ble_evt);

/**@brief Function for sending a string to the peer.
 *
 * @details This function sends the input string as an RX characteristic notification to the
 *          peer.
 *
 * @param[in] p_epd       Pointer to the EPD Service structure.
 * @param[in] p_string    String to be sent.
 * @param[in] length      Length of the string.
 *
 * @retval NRF_SUCCESS If the string was sent successfully. Otherwise, an error code is returned.
 */
uint32_t ble_epd_string_send(ble_epd_t * p_epd, uint8_t * p_string, uint16_t length);

#endif // EPD_BLE_H__

/** @} */
