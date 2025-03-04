/*****************************************************************************
* | File        : DEV_Config.cpp
* | Author      : Waveshare team
* | Function    :
* | Info        :
*   Image scanning
*      Please use progressive scanning to generate images or fonts
*----------------
* | This version:   V1.0
* | Date        :   2018-01-11
* | Info        :   Basic version
*
******************************************************************************/

#include "app_error.h"
#include "nrf_drv_spi.h"
#include "EPD_driver.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

uint32_t EPD_MOSI_PIN = 5;
uint32_t EPD_SCLK_PIN = 8;
uint32_t EPD_CS_PIN = 9;
uint32_t EPD_DC_PIN = 10;
uint32_t EPD_RST_PIN = 11;
uint32_t EPD_BUSY_PIN = 12;
uint32_t EPD_BS_PIN = 13;
uint32_t EPD_EN_PIN = 0xFF;

#define SPI_INSTANCE  0 /**< SPI instance index. */
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);  /**< SPI instance. */

extern epd_driver_t epd_driver_4in2;
extern epd_driver_t epd_driver_4in2bv2;

/** EPD drivers */
static epd_driver_t *epd_drivers[] = {
    &epd_driver_4in2,
    &epd_driver_4in2bv2,
};

/**< current EPD driver */
static epd_driver_t *m_driver = NULL;

epd_driver_t *epd_driver_get(void)
{
    if (m_driver == NULL)
        m_driver = epd_drivers[0];
    return m_driver;
}

epd_driver_t *epd_driver_by_id(uint8_t id)
{
    for (uint8_t i = 0; i < ARRAY_SIZE(epd_drivers); i++)
    {
      if (epd_drivers[i]->id == id)
      {
          return epd_drivers[i];
      }
    }
    return NULL;
}

bool epd_driver_set(uint8_t id)
{
    epd_driver_t *driver = epd_driver_by_id(id);
    if (driver )
    {
        m_driver = driver;
        return true;
    }
    return false;
}

void DEV_Module_Init(void)
{
    nrf_gpio_cfg_output(EPD_CS_PIN);
    nrf_gpio_cfg_output(EPD_DC_PIN);
    nrf_gpio_cfg_output(EPD_RST_PIN);
    nrf_gpio_cfg_input(EPD_BUSY_PIN, NRF_GPIO_PIN_NOPULL);

    if (EPD_EN_PIN != 0xFF) {
        nrf_gpio_cfg_output(EPD_EN_PIN);
        DEV_Digital_Write(EPD_EN_PIN, 1);
    }

    nrf_gpio_cfg_output(EPD_BS_PIN);
    DEV_Digital_Write(EPD_BS_PIN, 0);

    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin = EPD_SCLK_PIN;
    spi_config.mosi_pin = EPD_MOSI_PIN;
    spi_config.ss_pin = EPD_CS_PIN;
#if defined(S112)
    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, NULL, NULL));
#else
    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, NULL));
#endif

    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_Digital_Write(EPD_RST_PIN, 1);
}

void DEV_Module_Exit(void)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_Digital_Write(EPD_RST_PIN, 0);

    nrf_drv_spi_uninit(&spi);
}

void DEV_SPI_WriteByte(uint8_t value)
{
    APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, &value, 1, NULL, 0));
}

void DEV_SPI_WriteBytes(uint8_t *value, uint8_t len)
{
    APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, value, len, NULL, 0));
}

void EPD_WriteCommand(uint8_t Reg)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_SPI_WriteByte(Reg);
}

void EPD_WriteByte(uint8_t Data)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_SPI_WriteByte(Data);
}

void EPD_WriteData(uint8_t *Data, uint8_t Len)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_SPI_WriteBytes(Data, Len);
}
