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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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
    void (*send_command)(uint8_t Reg);                /**< send command */
	void (*send_byte)(uint8_t Reg);                   /**< send byte */
    void (*send_data)(uint8_t *Data, uint8_t Len);    /**< send data */
    void (*write_image)(uint8_t *black, uint8_t *color, uint16_t x, uint16_t y, uint16_t w, uint16_t h); /**< write image */
    void (*refresh)(void);                            /**< Sends the image buffer in RAM to e-Paper and displays */
    void (*sleep)(void);                              /**< Enter sleep mode */
    int8_t (*read_temp)(void);                        /**< Read temperature from driver chip */
    void (*force_temp)(int8_t value);                 /**< Force temperature (will trigger OTP LUT switch) */
} epd_driver_t;

extern uint32_t EPD_MOSI_PIN;
extern uint32_t EPD_SCLK_PIN;
extern uint32_t EPD_CS_PIN;
extern uint32_t EPD_DC_PIN;
extern uint32_t EPD_RST_PIN;
extern uint32_t EPD_BUSY_PIN;
extern uint32_t EPD_BS_PIN;
extern uint32_t EPD_EN_PIN;
extern uint32_t EPD_LED_PIN;

#define LOW             (0x0)
#define HIGH            (0x1)

#define INPUT           (0x0)
#define OUTPUT          (0x1)
#define INPUT_PULLUP    (0x2)
#define INPUT_PULLDOWN  (0x3)

// Arduino like function wrappers
void pinMode(uint32_t pin, uint32_t mode);
void digitalWrite(uint32_t pin, uint32_t value);
uint32_t digitalRead(uint32_t pin);
void delay(uint32_t ms);

// GPIO
void DEV_Module_Init(void);
void DEV_Module_Exit(void);

// Software SPI (read / write)
void DEV_SPI_WriteByte_SW(uint8_t data);
uint8_t DEV_SPI_ReadByte_SW(void);
void EPD_WriteCommand_SW(uint8_t Reg);
void EPD_WriteByte_SW(uint8_t Data);
uint8_t EPD_ReadByte_SW(void);

// Hardware SPI (write only)
void DEV_SPI_Init(void);
void DEV_SPI_Exit(void);
void DEV_SPI_WriteByte(uint8_t value);
void DEV_SPI_WriteBytes(uint8_t *value, uint8_t len);
void EPD_WriteCommand(uint8_t Reg);
void EPD_WriteByte(uint8_t Data);
void EPD_WriteData(uint8_t *Data, uint8_t Len);

void EPD_Reset(uint32_t value, uint16_t duration);
void EPD_WaitBusy(uint32_t value, uint16_t timeout);

// lED
void EPD_LED_ON(void);
void EPD_LED_OFF(void);
void EPD_LED_TOGGLE(void);

epd_driver_t *epd_driver_get(void);
epd_driver_t *epd_driver_by_id(uint8_t id);
bool epd_driver_set(uint8_t id);

#endif
