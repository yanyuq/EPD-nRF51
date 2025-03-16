/**
 * Based on GDEH042Z96 driver from Good Display
 * https://www.good-display.com/product/214.html
 */
#include "EPD_driver.h"
#include "nrf_log.h"

// commands used by this driver
#define CMD_DRIVER_CTRL           0x01        // Driver Output control
#define CMD_BOOSTER_CTRL          0x0C        // Booster Soft start Control
#define CMD_DEEP_SLEEP            0x10        // Deep Sleep mode 
#define CMD_DATA_MODE             0x11        // Data Entry mode setting
#define CMD_SW_RESET              0x12        // SW RESET
#define CMD_TSENSOR_CTRL          0x18        // Temperature Sensor Control
#define CMD_TSENSOR_WRITE         0x1A        // Temperature Sensor Control (Write to temperature register)
#define CMD_TSENSOR_READ          0x1B        // Temperature Sensor Control (Read from temperature register)
#define CMD_MASTER_ACTIVATE       0x20        // Master Activation 
#define CMD_DISP_CTRL1            0x21        // Display Update Control 1
#define CMD_DISP_CTRL2            0x22        // Display Update Control 2
#define CMD_WRITE_RAM1            0x24        // Write RAM (BW)
#define CMD_WRITE_RAM2            0x26        // Write RAM (RED)
#define CMD_VCOM_CTRL             0x2B        // Write Register for VCOM Control
#define CMD_BORDER_CTRL           0x3C        // Border Waveform Control
#define CMD_RAM_XPOS              0x44        // Set RAM X - address Start / End position
#define CMD_RAM_YPOS              0x45        // Set Ram Y- address Start / End position
#define CMD_RAM_XCOUNT            0x4E        // Set RAM X address counter
#define CMD_RAM_YCOUNT            0x4F        // Set RAM Y address counter
#define CMD_ANALOG_BLOCK_CTRL     0x74        // Set Analog Block Control
#define CMD_DIGITAL_BLOCK_CTRL    0x7E        // Set Digital Block Control

int8_t SSD1619_Read_Temp(void)
{
    EPD_WriteCommand_SW(CMD_TSENSOR_READ);
    return (int8_t) EPD_ReadByte_SW();
}

void SSD1619_Force_Temp(int8_t value)
{
    EPD_WriteCommand(CMD_TSENSOR_WRITE);
    EPD_WriteByte(value);
}

static void _setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    EPD_WriteCommand(CMD_DATA_MODE); // set ram entry mode
    EPD_WriteByte(0x03);    // x increase, y increase
    EPD_WriteCommand(CMD_RAM_XPOS);
    EPD_WriteByte(x / 8);
    EPD_WriteByte((x + w - 1) / 8);
    EPD_WriteCommand(CMD_RAM_YPOS);
    EPD_WriteByte(y % 256);
    EPD_WriteByte(y / 256);
    EPD_WriteByte((y + h - 1) % 256);
    EPD_WriteByte((y + h - 1) / 256);
    EPD_WriteCommand(CMD_RAM_XCOUNT);
    EPD_WriteByte(x / 8);
    EPD_WriteCommand(CMD_RAM_YCOUNT);
    EPD_WriteByte(y % 256);
    EPD_WriteByte(y / 256);
}

void SSD1619_Init()
{
    epd_model_t *EPD = epd_get();

    EPD_Reset(HIGH, 10);

    EPD_WriteCommand(CMD_SW_RESET);
    EPD_WaitBusy(HIGH, 200);

    EPD_WriteCommand(CMD_ANALOG_BLOCK_CTRL);
    EPD_WriteByte(0x54);
    EPD_WriteCommand(CMD_DIGITAL_BLOCK_CTRL);
    EPD_WriteByte(0x3B);

    EPD_WriteCommand(CMD_DRIVER_CTRL);
    EPD_WriteByte((EPD->height - 1) % 256);
    EPD_WriteByte((EPD->height - 1) / 256);
    EPD_WriteByte(0x00);

    _setPartialRamArea(0, 0, EPD->width, EPD->height);

    EPD_WriteCommand(CMD_BORDER_CTRL);
    EPD_WriteByte(0x01);

    EPD_WriteCommand(CMD_TSENSOR_CTRL);
    EPD_WriteByte(0x80);
    EPD_WriteCommand(CMD_DISP_CTRL2);
    EPD_WriteByte(0xB1);
    EPD_WriteCommand(CMD_MASTER_ACTIVATE);
    EPD_WaitBusy(HIGH, 200);
}

static void SSD1619_Refresh(void)
{
    epd_model_t *EPD = epd_get();

    NRF_LOG_DEBUG("[EPD]: refresh begin\n");
    NRF_LOG_DEBUG("[EPD]: temperature: %d\n", SSD1619_Read_Temp());
    EPD_WriteCommand(CMD_DISP_CTRL2);
    EPD_WriteByte(0xC7);
    EPD_WriteCommand(CMD_MASTER_ACTIVATE);
    EPD_WaitBusy(HIGH, 30000);
    NRF_LOG_DEBUG("[EPD]: refresh end\n");

    _setPartialRamArea(0, 0, EPD->width, EPD->height); // DO NOT REMOVE!
}

void SSD1619_Clear(void)
{
    epd_model_t *EPD = epd_get();
    uint16_t Width = (EPD->width + 7) / 8;
    uint16_t Height = EPD->height;

    _setPartialRamArea(0, 0, EPD->width, EPD->height);
    EPD_WriteCommand(CMD_WRITE_RAM1);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            EPD_WriteByte(0xFF);
        }
    }
    EPD_WriteCommand(CMD_WRITE_RAM2);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            EPD_WriteByte(EPD->invert_color ? 0x00 : 0xFF);
        }
    }

    SSD1619_Refresh();
}

void SSD1619_Write_Image(uint8_t *black, uint8_t *color, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    epd_model_t *EPD = epd_get();
    uint16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
    x -= x % 8; // byte boundary
    w = wb * 8; // byte boundary
    if (x + w > EPD->width || y + h > EPD->height) return;
    _setPartialRamArea(x, y, w, h);
    EPD_WriteCommand(CMD_WRITE_RAM1);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            EPD_WriteByte(black ? black[j + i * wb] : 0xFF);
        }
    }
    EPD_WriteCommand(CMD_WRITE_RAM2);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            if (EPD->bwr) {
                uint8_t data = color ? color[j + i * wb] : 0xFF;
                EPD_WriteByte(EPD->invert_color ? ~data : data);
            } else {
                EPD_WriteByte(black[j + i * wb]);
            }
        }
    }
}

void SSD1619_Sleep(void)
{
    EPD_WriteCommand(CMD_DEEP_SLEEP);
    EPD_WriteByte(0x01);
    delay(100);
}

static epd_driver_t epd_drv_ssd1619 = {
    .init = SSD1619_Init,
    .clear = SSD1619_Clear,
    .write_image = SSD1619_Write_Image,
    .refresh = SSD1619_Refresh,
    .sleep = SSD1619_Sleep,
    .read_temp = SSD1619_Read_Temp,
    .force_temp = SSD1619_Force_Temp,
};

// SSD1619 400x300 Black/White/Red
const epd_model_t epd_ssd1619_420_bwr = {
    .id = EPD_SSD1619_420_BWR,
    .drv = &epd_drv_ssd1619,
    .width = 400,
    .height = 300,
    .bwr = true,
    .invert_color = true,
};

// SSD1619 400x300 Black/White
const epd_model_t epd_ssd1619_420_bw = {
    .id = EPD_SSD1619_420_BW,
    .drv = &epd_drv_ssd1619,
    .width = 400,
    .height = 300,
    .bwr = false,
    .invert_color = false,
};
