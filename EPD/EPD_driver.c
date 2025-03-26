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
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_drv_spi.h"
#include "EPD_driver.h"
#include "nrf_log.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// GPIO Pins
static uint32_t EPD_MOSI_PIN = 5;
static uint32_t EPD_SCLK_PIN = 8;
static uint32_t EPD_CS_PIN = 9;
static uint32_t EPD_DC_PIN = 10;
static uint32_t EPD_RST_PIN = 11;
static uint32_t EPD_BUSY_PIN = 12;
static uint32_t EPD_BS_PIN = 13;
static uint32_t EPD_EN_PIN = 0xFF;
static uint32_t EPD_LED_PIN = 0xFF;

// EPD model
static epd_model_t *EPD = NULL;

// Arduino like function wrappers

void pinMode(uint32_t pin, uint32_t mode)
{
    switch (mode)
    {
        case INPUT:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
            break;
        case INPUT_PULLUP:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
            break;
        case INPUT_PULLDOWN:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
            break;
        case OUTPUT:
            nrf_gpio_cfg_output(pin);
            break;
        case DEFAULT:
        default:
            nrf_gpio_cfg_default(pin);
            break;
    }
}

void digitalWrite(uint32_t pin, uint32_t value)
{
    if (value == LOW)
        nrf_gpio_pin_clear(pin);
    else
        nrf_gpio_pin_set(pin);
}

uint32_t digitalRead(uint32_t pin)
{
#if defined(S112)
    nrf_gpio_pin_dir_t dir = nrf_gpio_pin_dir_get(pin);
#else
    NRF_GPIO_Type * reg = nrf_gpio_pin_port_decode(&pin);
    nrf_gpio_pin_dir_t dir = (nrf_gpio_pin_dir_t)((reg->PIN_CNF[pin] &
                                                   GPIO_PIN_CNF_DIR_Msk) >> GPIO_PIN_CNF_DIR_Pos);
#endif
    if (dir == NRF_GPIO_PIN_DIR_INPUT)
        return nrf_gpio_pin_read(pin);
    else
        return nrf_gpio_pin_out_read(pin);
}

void delay(uint32_t ms)
{
    nrf_delay_ms(ms);
}

// Hardware SPI (write only)

#define SPI_INSTANCE  0 /**< SPI instance index. */
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);  /**< SPI instance. */
static bool spi_initialized = false;

static void EPD_SPI_Init(void)
{
    if (spi_initialized) return;
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin = EPD_SCLK_PIN;
    spi_config.mosi_pin = EPD_MOSI_PIN;
    spi_config.ss_pin = EPD_CS_PIN;
#if defined(S112)
    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, NULL, NULL));
#else
    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, NULL));
#endif
    spi_initialized = true;
}

static void EPD_SPI_Uninit(void)
{
    if (!spi_initialized) return;
    nrf_drv_spi_uninit(&spi);
    spi_initialized = false;
}

void EPD_SPI_WriteByte(uint8_t value)
{
    EPD_SPI_Init();
    APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, &value, 1, NULL, 0));
}

void EPD_SPI_WriteBytes(uint8_t *value, uint8_t len)
{
    EPD_SPI_Init();
    APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, value, len, NULL, 0));
}


// Software SPI (read / write)
void EPD_SPI_WriteByte_SW(uint8_t data)
{
    EPD_SPI_Uninit();
    pinMode(EPD_MOSI_PIN, OUTPUT);
    digitalWrite(EPD_CS_PIN, LOW);
    for (int i = 0; i < 8; i++)
    {
        if ((data & 0x80) == 0) digitalWrite(EPD_MOSI_PIN, LOW); 
        else                    digitalWrite(EPD_MOSI_PIN, HIGH);

        data <<= 1;
        digitalWrite(EPD_SCLK_PIN, HIGH);     
        digitalWrite(EPD_SCLK_PIN, LOW);
    }
    digitalWrite(EPD_CS_PIN, HIGH);
}

uint8_t EPD_SPI_ReadByte_SW(void)
{
    EPD_SPI_Uninit();
    uint8_t j = 0xff;
    pinMode(EPD_MOSI_PIN, INPUT);
    digitalWrite(EPD_CS_PIN, LOW);
    for (int i = 0; i < 8; i++)
    {
        j = j << 1;
        if (digitalRead(EPD_MOSI_PIN))  j = j | 0x01;
        else                            j = j & 0xfe;
        
        digitalWrite(EPD_SCLK_PIN, HIGH);     
        digitalWrite(EPD_SCLK_PIN, LOW);
    }
    digitalWrite(EPD_CS_PIN, HIGH);
    pinMode(EPD_MOSI_PIN, 1);
    return j;
}

void EPD_WriteCommand_SW(uint8_t Reg)
{
    digitalWrite(EPD_DC_PIN, LOW);
    digitalWrite(EPD_CS_PIN, LOW);
    EPD_SPI_WriteByte_SW(Reg);
    digitalWrite(EPD_CS_PIN, HIGH);
}

void EPD_WriteByte_SW(uint8_t Data)
{
    digitalWrite(EPD_DC_PIN, HIGH);
    digitalWrite(EPD_CS_PIN, LOW);
    EPD_SPI_WriteByte_SW(Data);
    digitalWrite(EPD_CS_PIN, HIGH);
}

uint8_t EPD_ReadByte_SW(void)
{
    digitalWrite(EPD_DC_PIN, HIGH);
    return EPD_SPI_ReadByte_SW();
}


// Hardware SPI
void EPD_WriteCommand(uint8_t Reg)
{
    digitalWrite(EPD_DC_PIN, LOW);
    EPD_SPI_WriteByte(Reg);
}

void EPD_WriteByte(uint8_t Data)
{
    digitalWrite(EPD_DC_PIN, HIGH);
    EPD_SPI_WriteByte(Data);
}

void EPD_WriteData(uint8_t *Data, uint8_t Len)
{
    digitalWrite(EPD_DC_PIN, HIGH);
    EPD_SPI_WriteBytes(Data, Len);
}

void EPD_Reset(uint32_t value, uint16_t duration)
{
    digitalWrite(EPD_RST_PIN, value);
    delay(10);
    digitalWrite(EPD_RST_PIN, (value == LOW) ? HIGH : LOW);
    delay(duration);
    digitalWrite(EPD_RST_PIN, value);
    delay(duration);
}

void EPD_WaitBusy(uint32_t value, uint16_t timeout)
{
    uint32_t led_status = digitalRead(EPD_LED_PIN);

    NRF_LOG_DEBUG("[EPD]: check busy\n");
    while (digitalRead(EPD_BUSY_PIN) == value) {
        if (timeout % 100 == 0) EPD_LED_Toggle();
        delay(1);
        timeout--;
        if (timeout == 0) {
            NRF_LOG_DEBUG("[EPD]: busy timeout!\n");
            break;
        }
    }
    NRF_LOG_DEBUG("[EPD]: busy release\n");

    // restore led status
    if (led_status == LOW)
        EPD_LED_ON();
    else
        EPD_LED_OFF();
}

// GPIO
void EPD_GPIO_Load(epd_config_t *cfg)
{
    if (cfg == NULL) return;
    EPD_MOSI_PIN = cfg->mosi_pin;
    EPD_SCLK_PIN = cfg->sclk_pin;
    EPD_CS_PIN = cfg->cs_pin;
    EPD_DC_PIN = cfg->dc_pin;
    EPD_RST_PIN = cfg->rst_pin;
    EPD_BUSY_PIN = cfg->busy_pin;
    EPD_BS_PIN = cfg->bs_pin;
    EPD_EN_PIN = cfg->en_pin;
    EPD_LED_PIN = cfg->led_pin;
}

void EPD_GPIO_Init(void)
{
    pinMode(EPD_CS_PIN, OUTPUT);
    pinMode(EPD_DC_PIN, OUTPUT);
    pinMode(EPD_RST_PIN, OUTPUT);
    pinMode(EPD_BUSY_PIN, INPUT);

    if (EPD_EN_PIN != 0xFF) {
        pinMode(EPD_EN_PIN, OUTPUT);
        digitalWrite(EPD_EN_PIN, HIGH);
    }

    pinMode(EPD_BS_PIN, OUTPUT);
    digitalWrite(EPD_BS_PIN, LOW);

    digitalWrite(EPD_DC_PIN, LOW);
    digitalWrite(EPD_CS_PIN, LOW);
    digitalWrite(EPD_RST_PIN, HIGH);

    if (EPD_LED_PIN != 0xFF)
        pinMode(EPD_LED_PIN, OUTPUT);
}

void EPD_GPIO_Uninit(void)
{
    EPD_LED_OFF();

    digitalWrite(EPD_DC_PIN, LOW);
    digitalWrite(EPD_CS_PIN, LOW);
    digitalWrite(EPD_RST_PIN, LOW);
    if (EPD_EN_PIN != 0xFF) {
        digitalWrite(EPD_EN_PIN, LOW);
    }

    EPD_SPI_Uninit();

    // reset pin state
    pinMode(EPD_MOSI_PIN, DEFAULT);
    pinMode(EPD_SCLK_PIN, DEFAULT);
    pinMode(EPD_CS_PIN, DEFAULT);
    pinMode(EPD_DC_PIN, DEFAULT);
    pinMode(EPD_RST_PIN, DEFAULT);
    pinMode(EPD_BUSY_PIN, DEFAULT);
    pinMode(EPD_BS_PIN, DEFAULT);
    pinMode(EPD_EN_PIN, DEFAULT);
    pinMode(EPD_LED_PIN, DEFAULT);
}

// lED
void EPD_LED_ON(void)
{
    if (EPD_LED_PIN != 0xFF)
        digitalWrite(EPD_LED_PIN, LOW);
}

void EPD_LED_OFF(void)
{
    if (EPD_LED_PIN != 0xFF)
        digitalWrite(EPD_LED_PIN, HIGH);
}

void EPD_LED_Toggle(void)
{
    if (EPD_LED_PIN != 0xFF)
        nrf_gpio_pin_toggle(EPD_LED_PIN);
}

void EPD_LED_BLINK(void)
{
    if (EPD_LED_PIN != 0xFF) {
        pinMode(EPD_LED_PIN, OUTPUT);
        digitalWrite(EPD_LED_PIN, LOW);
        delay(100);
        digitalWrite(EPD_LED_PIN, HIGH);
        delay(100);
        pinMode(EPD_LED_PIN, DEFAULT);
    }
}

float EPD_ReadVoltage(void)
{
    #if defined(S112)
    volatile int16_t value = 0;
    NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_10bit;
    NRF_SAADC->ENABLE = (SAADC_ENABLE_ENABLE_Enabled << SAADC_ENABLE_ENABLE_Pos);
    NRF_SAADC->CH[0].CONFIG = ((SAADC_CH_CONFIG_RESP_Bypass     << SAADC_CH_CONFIG_RESP_Pos)   & SAADC_CH_CONFIG_RESP_Msk)
                            | ((SAADC_CH_CONFIG_RESP_Bypass     << SAADC_CH_CONFIG_RESN_Pos)   & SAADC_CH_CONFIG_RESN_Msk)
                            | ((SAADC_CH_CONFIG_GAIN_Gain1_6    << SAADC_CH_CONFIG_GAIN_Pos)   & SAADC_CH_CONFIG_GAIN_Msk)
                            | ((SAADC_CH_CONFIG_REFSEL_Internal << SAADC_CH_CONFIG_REFSEL_Pos) & SAADC_CH_CONFIG_REFSEL_Msk)
                            | ((SAADC_CH_CONFIG_TACQ_3us        << SAADC_CH_CONFIG_TACQ_Pos)   & SAADC_CH_CONFIG_TACQ_Msk)
                            | ((SAADC_CH_CONFIG_MODE_SE         << SAADC_CH_CONFIG_MODE_Pos)   & SAADC_CH_CONFIG_MODE_Msk);
    NRF_SAADC->CH[0].PSELN = SAADC_CH_PSELN_PSELN_NC;
    NRF_SAADC->CH[0].PSELP = SAADC_CH_PSELP_PSELP_VDD;
    NRF_SAADC->RESULT.PTR = (uint32_t)&value;
    NRF_SAADC->RESULT.MAXCNT = 1;
    NRF_SAADC->TASKS_START = 0x01UL;
    while (!NRF_SAADC->EVENTS_STARTED);
    NRF_SAADC->EVENTS_STARTED = 0x00UL;
    NRF_SAADC->TASKS_SAMPLE = 0x01UL;
    while (!NRF_SAADC->EVENTS_END);
    NRF_SAADC->EVENTS_END = 0x00UL;
    NRF_SAADC->TASKS_STOP = 0x01UL;
    while (!NRF_SAADC->EVENTS_STOPPED);
    NRF_SAADC->EVENTS_STOPPED = 0x00UL;
    if (value < 0) value = 0;
    NRF_SAADC->ENABLE = (SAADC_ENABLE_ENABLE_Disabled << SAADC_ENABLE_ENABLE_Pos);
#else
    NRF_ADC->ENABLE = 1;
    NRF_ADC->CONFIG = (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos) |
                      (ADC_CONFIG_INPSEL_SupplyOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos) |
                      (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos) |
                      (ADC_CONFIG_PSEL_Disabled << ADC_CONFIG_PSEL_Pos) |
                      (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
    NRF_ADC->TASKS_START = 1;
    while(!NRF_ADC->EVENTS_END);
    uint16_t value = NRF_ADC->RESULT;
    NRF_ADC->TASKS_STOP = 1;
    NRF_ADC->ENABLE = 0;
#endif
    NRF_LOG_DEBUG("ADC value: %d\n", value);
    return (value * 3.6) / (1 << 10);
}

// EPD models
extern epd_model_t epd_uc8176_420_bw;
extern epd_model_t epd_uc8176_420_bwr;
extern epd_model_t epd_ssd1619_420_bwr;
extern epd_model_t epd_ssd1619_420_bw;
extern epd_model_t epd_uc8276_420_bwr;

static epd_model_t *epd_models[] = {
    &epd_uc8176_420_bw,
    &epd_uc8176_420_bwr,
    &epd_ssd1619_420_bwr,
    &epd_ssd1619_420_bw,
    &epd_uc8276_420_bwr,
};

epd_model_t *epd_get(void)
{
    return EPD == NULL ? epd_models[0] : EPD;
}

epd_model_t *epd_init(epd_model_id_t id)
{
    for (uint8_t i = 0; i < ARRAY_SIZE(epd_models); i++) {
        if (epd_models[i]->id == id) {
            EPD = epd_models[i];
        }
    }
    if (EPD == NULL) EPD = epd_models[0];
    EPD->drv->init();
    return EPD;
}
