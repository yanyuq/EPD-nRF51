/*****************************************************************************
* | File      	:   EPD_4in2.c
* | Author      :   Waveshare team
* | Function    :   4.2inch e-paper
* | Info        :
*----------------
* |	This version:   V3.0
* | Date        :   2019-06-13
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "EPD_driver.h"
#include "nrf_log.h"

// Display resolution
#define EPD_4IN2_WIDTH       400
#define EPD_4IN2_HEIGHT      300

static void EPD_4IN2_PowerOn(void)
{
    EPD_WriteCommand(0x04);
	delay(50);
}

static void EPD_4IN2_PowerOff(void)
{
    EPD_WriteCommand(0x02);
	EPD_WaitBusy(LOW, 50);
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
void EPD_4IN2_Refresh(void)
{
    EPD_4IN2_PowerOn();
    EPD_WriteCommand(0x12);
    delay(100);
    EPD_WaitBusy(LOW, 50);
    EPD_4IN2_PowerOff();
}

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
void EPD_4IN2_Init(void)
{
	EPD_Reset(HIGH, 10);

	EPD_WriteCommand(0x00);			// panel setting
	EPD_WriteByte(0x1f);		    // 400x300 B/W mode, LUT from OTP

	EPD_WriteCommand(0x50);         // VCOM AND DATA INTERVAL SETTING
	EPD_WriteByte(0x97);            // LUTB=0 LUTW=1 interval=10
}

void EPD_4IN2B_V2_Init(void)
{
    EPD_Reset(HIGH, 10);

    EPD_WriteCommand(0x00);
    EPD_WriteByte(0x0f);
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_4IN2_Clear(void)
{
    uint16_t Width, Height;
    Width = (EPD_4IN2_WIDTH % 8 == 0)? (EPD_4IN2_WIDTH / 8 ): (EPD_4IN2_WIDTH / 8 + 1);
    Height = EPD_4IN2_HEIGHT;

    EPD_WriteCommand(0x10);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            EPD_WriteByte(0xFF);
        }
    }

    EPD_WriteCommand(0x13);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            EPD_WriteByte(0xFF);
        }
    }

    EPD_4IN2_Refresh();
}

static void _setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t xe = (x + w - 1) | 0x0007; // byte boundary inclusive (last byte)
    uint16_t ye = y + h - 1;
    x &= 0xFFF8; // byte boundary
    xe |= 0x0007; // byte boundary
    EPD_WriteCommand(0x90); // partial window
    EPD_WriteByte(x / 256);
    EPD_WriteByte(x % 256);
    EPD_WriteByte(xe / 256);
    EPD_WriteByte(xe % 256);
    EPD_WriteByte(y / 256);
    EPD_WriteByte(y % 256);
    EPD_WriteByte(ye / 256);
    EPD_WriteByte(ye % 256);
    EPD_WriteByte(0x01);
}

void EPD_4IN2_Write_Image(uint8_t *black, uint8_t *color, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
    x -= x % 8; // byte boundary
    w = wb * 8; // byte boundary
    if (x + w > EPD_4IN2_WIDTH || y + h > EPD_4IN2_HEIGHT) return;
    EPD_WriteCommand(0x91); // partial in
    _setPartialRamArea(x, y, w, h);
    EPD_WriteCommand(0x13);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            EPD_WriteByte(black[j + i * wb]);
        }
    }
    EPD_WriteCommand(0x92); // partial out
}

void EPD_4IN2B_V2_Write_Image(uint8_t *black, uint8_t *color, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
    x -= x % 8; // byte boundary
    w = wb * 8; // byte boundary
    if (x + w > EPD_4IN2_WIDTH || y + h > EPD_4IN2_HEIGHT) return;
    EPD_WriteCommand(0x91); // partial in
    _setPartialRamArea(x, y, w, h);
    EPD_WriteCommand(0x10);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            EPD_WriteByte(black ? black[j + i * wb] : 0xFF);
        }
    }
    EPD_WriteCommand(0x13);
    for (uint16_t i = 0; i < h; i++) {
        for (uint16_t j = 0; j < w / 8; j++) {
            EPD_WriteByte(color ? color[j + i * wb] : 0xFF);
        }
    }
    EPD_WriteCommand(0x92); // partial out
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_4IN2_Sleep(void)
{
	EPD_4IN2_PowerOff();

	EPD_WriteCommand(0x07);
	EPD_WriteByte(0XA5);
}

const epd_driver_t epd_driver_4in2 = {
    .id = EPD_DRIVER_4IN2,
	.width = EPD_4IN2_WIDTH,
	.height = EPD_4IN2_HEIGHT,
    .init = EPD_4IN2_Init,
    .clear = EPD_4IN2_Clear,
    .send_command = EPD_WriteCommand,
	.send_byte = EPD_WriteByte,
    .send_data = EPD_WriteData,
    .write_image = EPD_4IN2_Write_Image,
    .refresh = EPD_4IN2_Refresh,
    .sleep = EPD_4IN2_Sleep,
};

const epd_driver_t epd_driver_4in2bv2 = {
    .id = EPD_DRIVER_4IN2B_V2,
	.width = EPD_4IN2_WIDTH,
	.height = EPD_4IN2_HEIGHT,
    .init = EPD_4IN2B_V2_Init,
    .clear = EPD_4IN2_Clear,
    .send_command = EPD_WriteCommand,
	.send_byte = EPD_WriteByte,
    .send_data = EPD_WriteData,
    .write_image = EPD_4IN2B_V2_Write_Image,
    .refresh = EPD_4IN2_Refresh,
    .sleep = EPD_4IN2_Sleep,
};
