/*****************************************************************************
* | File      	: DEV_Config.h
* | Author      : Waveshare team
* | Function    :	debug with prntf
* | Info        :
*   Image scanning
*      Please use progressive scanning to generate images or fonts
*----------------
* |	This version:   V1.0
* | Date        :   2018-01-11
* | Info        :   Basic version
*
******************************************************************************/

#ifndef __EPD_DRIVER_H
#define __EPD_DRIVER_H

#include <stdint.h>
#include <stdlib.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"

/**< EPD driver IDs. */
enum EPD_DRIVER_IDS
{
    EPD_DRIVER_4IN2 = 1,
    EPD_DRIVER_4IN2B_V2 = 3,
};

/**@brief EPD driver structure.
 *
 * @details This structure contains epd driver functions.
 */
typedef struct
{
    uint8_t id;                                       /**< driver ID. */
	uint16_t width;
	uint16_t height;
    void (*init)(void);                               /**< Initialize the e-Paper register */
    void (*clear)(void);                              /**< Clear screen */
    void (*send_command)(uint8_t Reg);                  /**< send command */
	void (*send_byte)(uint8_t Reg);                     /**< send byte */
    void (*send_data)(uint8_t *Data, uint8_t Len);        /**< send data */
    void (*write_image)(uint8_t *black, uint8_t *color, uint16_t x, uint16_t y, uint16_t w, uint16_t h); /**< write image */
    void (*refresh)(void);                            /**< Sends the image buffer in RAM to e-Paper and displays */
    void (*sleep)(void);                              /**< Enter sleep mode */
} epd_driver_t;

extern uint32_t EPD_MOSI_PIN;
extern uint32_t EPD_SCLK_PIN;
extern uint32_t EPD_CS_PIN;
extern uint32_t EPD_DC_PIN;
extern uint32_t EPD_RST_PIN;
extern uint32_t EPD_BUSY_PIN;
extern uint32_t EPD_BS_PIN;

/**
 * GPIO read and write
**/
#define DEV_Digital_Write(_pin, _value) nrf_gpio_pin_write(_pin, _value)
#define DEV_Digital_Read(_pin) nrf_gpio_pin_read(_pin)

/**
 * delay x ms
**/
#define DEV_Delay_ms(__xms) nrf_delay_ms(__xms);
#define DEV_Delay_us(__xus) nrf_delay_us(__xus);

uint8_t DEV_Module_Init(void);
void DEV_Module_Exit(void);

void DEV_SPI_WriteByte(uint8_t value);
void DEV_SPI_WriteBytes(uint8_t *value, uint8_t len);

void EPD_WriteCommand(uint8_t Reg);
void EPD_WriteByte(uint8_t Data);
void EPD_WriteData(uint8_t *Data, uint8_t Len);

epd_driver_t *epd_driver_get(void);
epd_driver_t *epd_driver_by_id(uint8_t id);
bool epd_driver_set(uint8_t id);

#endif
