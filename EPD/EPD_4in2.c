/*****************************************************************************
* | File      	:   EPD_4in2.c
* | Author      :   Waveshare team
* | Function    :   4.2inch e-paper
* | Info        :
*----------------
* |	This version:   V3.0
* | Date        :   2019-06-13
* | Info        :
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
#include "EPD_4in2.h"

static const unsigned char EPD_4IN2_lut_vcom0[] = {
    0x00, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0x00, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 					
	};
static const unsigned char EPD_4IN2_lut_ww[] = {
	0x50, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0xA0, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
static const unsigned char EPD_4IN2_lut_bw[] = {
	0x50, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0xA0, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
static const unsigned char EPD_4IN2_lut_wb[] = {
	0xA0, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0x50, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	};
static const unsigned char EPD_4IN2_lut_bb[] = {
	0x20, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01,	
	0x10, 0x08, 0x08, 0x00, 0x00, 0x02,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	};

/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_4IN2_Reset(void)
{
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(10);
}

/******************************************************************************
function :	send command
parameter:
     Reg : Command register
******************************************************************************/
void EPD_4IN2_SendCommand(UBYTE Reg)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_SPI_WriteByte(Reg);
    DEV_Digital_Write(EPD_CS_PIN, 1);
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
void EPD_4IN2_SendData(UBYTE Data)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_SPI_WriteByte(Data);
    DEV_Digital_Write(EPD_CS_PIN, 1);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
void EPD_4IN2_ReadBusy(void)
{
    while(DEV_Digital_Read(EPD_BUSY_PIN) == 0) {      //LOW: idle, HIGH: busy
        DEV_Delay_ms(100);
    }
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
void EPD_4IN2_TurnOnDisplay(void)
{
    EPD_4IN2_SendCommand(0x12);
    DEV_Delay_ms(100);
    EPD_4IN2_ReadBusy();
}

/******************************************************************************
function :	set the look-up tables
parameter:
******************************************************************************/
static void EPD_4IN2_SetLut(void)
{
	unsigned int count;
	EPD_4IN2_SendCommand(0x20);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_vcom0[count]);}

	EPD_4IN2_SendCommand(0x21);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_ww[count]);}   
	
	EPD_4IN2_SendCommand(0x22);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_bw[count]);} 

	EPD_4IN2_SendCommand(0x23);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_wb[count]);} 

	EPD_4IN2_SendCommand(0x24);
	for(count=0;count<36;count++)	     
		{EPD_4IN2_SendData(EPD_4IN2_lut_bb[count]);}  
}	

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
void EPD_4IN2_Init(void)
{
	EPD_4IN2_Reset();
	EPD_4IN2_SendCommand(0x01);			// POWER SETTING 
	EPD_4IN2_SendData (0x03);	        // VDS_EN, VDG_EN internal  
	EPD_4IN2_SendData (0x00);           // VCOM_HV, VGHL_LV=16V
	EPD_4IN2_SendData (0x2b);           // VDH=11V
	EPD_4IN2_SendData (0x2b);           // VDL=11V

	EPD_4IN2_SendCommand(0x06);         // boost soft start
	EPD_4IN2_SendData (0x17);		    // A
	EPD_4IN2_SendData (0x17);		    // B
	EPD_4IN2_SendData (0x17);		    // C

	EPD_4IN2_SendCommand(0x04);         // POWER ON
	EPD_4IN2_ReadBusy();

	EPD_4IN2_SendCommand(0x00);			// panel setting
	EPD_4IN2_SendData(0x3f);		    // 300x400 B/W mode, LUT set by register


	EPD_4IN2_SendCommand(0x30);			// PLL setting (clock frequency)
	EPD_4IN2_SendData (0x3c);      	    // 3c=50HZ 3a=100HZ 29=150Hz 39=200HZ 31=171HZ

	EPD_4IN2_SendCommand(0x61);			// resolution setting
	EPD_4IN2_SendData (EPD_4IN2_WIDTH / 256);
	EPD_4IN2_SendData (EPD_4IN2_WIDTH % 256);
	EPD_4IN2_SendData (EPD_4IN2_HEIGHT / 256);
	EPD_4IN2_SendData (EPD_4IN2_HEIGHT % 256);

	EPD_4IN2_SendCommand(0x82);			// vcom_DC setting  	
	EPD_4IN2_SendData (0x12);	        // -0.1 + 18 * -0.05 = -1.0V

	EPD_4IN2_SendCommand(0x50);         // VCOM AND DATA INTERVAL SETTING
	EPD_4IN2_SendData(0x97);            // LUTB=0 LUTW=1 interval=10

	EPD_4IN2_SetLut();
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_4IN2_Clear(void)
{
    UWORD Width, Height;
    Width = (EPD_4IN2_WIDTH % 8 == 0)? (EPD_4IN2_WIDTH / 8 ): (EPD_4IN2_WIDTH / 8 + 1);
    Height = EPD_4IN2_HEIGHT;

    EPD_4IN2_SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2_SendData(0xFF);
        }
    }

    EPD_4IN2_SendCommand(0x13);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2_SendData(0xFF);
        }
    }

    EPD_4IN2_TurnOnDisplay();
}

/******************************************************************************
function :	Sends the image buffer in RAM to e-Paper and displays
parameter:
******************************************************************************/
void EPD_4IN2_Display(UBYTE *Image)
{
    UWORD Width, Height;
    Width = (EPD_4IN2_WIDTH % 8 == 0)? (EPD_4IN2_WIDTH / 8 ): (EPD_4IN2_WIDTH / 8 + 1);
    Height = EPD_4IN2_HEIGHT;

	EPD_4IN2_SendCommand(0x10);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2_SendData(0x00);
        }
    }

    EPD_4IN2_SendCommand(0x13);
    for (UWORD j = 0; j < Height; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_4IN2_SendData(Image[i + j * Width]);
        }
    }

    EPD_4IN2_TurnOnDisplay();
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_4IN2_Sleep(void)
{
	EPD_4IN2_SendCommand(0x50); // DEEP_SLEEP
	EPD_4IN2_SendData(0XF7);

	EPD_4IN2_SendCommand(0x02); // POWER_OFF
	EPD_4IN2_ReadBusy();

	EPD_4IN2_SendCommand(0x07); // DEEP_SLEEP
	EPD_4IN2_SendData(0XA5);
}
