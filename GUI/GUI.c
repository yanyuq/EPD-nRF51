#include "fonts.h"
#include "Lunar.h"
#include "GUI.h"
#include <stdio.h>

#define GFX_printf_styled(gfx, fg, bg, font, ...) \
            GFX_setTextColor(gfx, fg, bg);        \
            GFX_setFont(gfx, font);               \
            GFX_printf(gfx, __VA_ARGS__);

static void DrawBattery(Adafruit_GFX *gfx, int16_t x, int16_t y, float voltage)
{
    uint8_t level = (uint8_t)(voltage * 100 / 4.2);
    GFX_setCursor(gfx, x - 26, y + 9);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "%.1fV", voltage);
    GFX_fillRect(gfx, x, y, 20, 10, GFX_WHITE);
    GFX_drawRect(gfx, x, y, 20, 10, GFX_BLACK);
    GFX_fillRect(gfx, x + 20, y + 4, 2, 2, GFX_BLACK);
    GFX_fillRect(gfx, x + 2, y + 2, 16 * level / 100, 6, GFX_BLACK);
}

static void DrawTemperature(Adafruit_GFX *gfx, int16_t x, int16_t y, int8_t temp)
{
    GFX_setCursor(gfx, x, y);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "%d℃", temp);
}

static void DrawDate(Adafruit_GFX *gfx, int16_t x, int16_t y, tm_t *tm)
{
    GFX_setCursor(gfx, x, y);
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%d", tm->tm_year + YEAR0);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "年");
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%02d", tm->tm_mon + 1);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "月");
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%02d", tm->tm_mday);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "日 ");
}

static void DrawDateHeader(Adafruit_GFX *gfx, int16_t x, int16_t y, tm_t *tm, struct Lunar_Date *Lunar, gui_data_t *data)
{
    DrawDate(gfx, x, y, tm);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "星期%s", Lunar_DayString[tm->tm_wday]);

    DrawBattery(gfx, 365, 4, data->voltage);

    GFX_setCursor(gfx, x + 270, y);
    GFX_printf(gfx, "%s%s%s %s%s", Lunar_MonthLeapString[Lunar->IsLeap], Lunar_MonthString[Lunar->Month],
                     Lunar_DateString[Lunar->Date], Lunar_StemStrig[LUNAR_GetStem(Lunar)],
                     Lunar_BranchStrig[LUNAR_GetBranch(Lunar)]);
    GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
    GFX_printf(gfx, "%s", Lunar_ZodiacString[LUNAR_GetZodiac(Lunar)]);
    GFX_setTextColor(gfx, GFX_BLACK, GFX_WHITE);
    GFX_printf(gfx, "年");
}

static void DrawWeekHeader(Adafruit_GFX *gfx, int16_t x, int16_t y)
{
    GFX_fillRect(gfx, x, y, 380, 24, GFX_RED);
    GFX_fillRect(gfx, x + 50, y, 280, 24, GFX_BLACK);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    for (int i = 0; i < 7; i++) {
        GFX_setTextColor(gfx, GFX_WHITE, (i > 0 && i < 6) ? GFX_BLACK : GFX_RED);
        GFX_setCursor(gfx, x + 15 + i * 55, y + 16);
        GFX_printf(gfx, "%s", Lunar_DayString[i]);
    }
}

static void DrawMonthDays(Adafruit_GFX *gfx, tm_t *tm, struct Lunar_Date *Lunar)
{
    uint8_t firstDayWeek = get_first_day_week(tm->tm_year + YEAR0, tm->tm_mon + 1);
    uint8_t monthMaxDays = thisMonthMaxDays(tm->tm_year + YEAR0, tm->tm_mon + 1);
    uint8_t monthDayRows = 1 + (monthMaxDays - (7 - firstDayWeek) + 6) / 7;

    for (uint8_t i = 0; i < monthMaxDays; i++) {
        uint8_t day = i + 1;

        int16_t w = (firstDayWeek + i) % 7;
        bool weekend = (w  == 0) || (w == 6);

        int16_t x = 22 + w * 55;
        int16_t y = (monthDayRows > 5 ? 69 : 72) + (firstDayWeek + i) / 7 * (monthDayRows > 5 ? 39 : 48);

        if (day == tm->tm_mday) {
            GFX_fillCircle(gfx, x + 11, y + (monthDayRows > 5 ? 10 : 12), 20, GFX_RED);
            GFX_setTextColor(gfx, GFX_WHITE, GFX_RED);
        } else {
            GFX_setTextColor(gfx, weekend ? GFX_RED : GFX_BLACK, GFX_WHITE);
        }

        GFX_setFont(gfx, u8g2_font_helvB14_tn);
        GFX_setCursor(gfx, x + (day < 10 ? 6 : 2), y + 10);
        GFX_printf(gfx, "%d", day);

        GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
        GFX_setCursor(gfx, x, y + 24);
        uint8_t JQdate;
        if (GetJieQi(tm->tm_year + YEAR0, tm->tm_mon + 1, day, &JQdate) && JQdate == day) {
            uint8_t JQ = (tm->tm_mon + 1 - 1) * 2;
            if (day >= 15) JQ++;
            if (day != tm->tm_mday) GFX_setTextColor(gfx, GFX_RED, GFX_WHITE);
            GFX_printf(gfx, "%s", JieQiStr[JQ]);
        } else {
            LUNAR_SolarToLunar(Lunar, tm->tm_year + YEAR0, tm->tm_mon + 1, day);
            if (Lunar->Date == 1)
                GFX_printf(gfx, "%s", Lunar_MonthString[Lunar->Month]);
            else
                GFX_printf(gfx, "%s", Lunar_DateString[Lunar->Date]);
        }
    }
}

static void DrawCalendar(Adafruit_GFX *gfx, tm_t *tm, struct Lunar_Date *Lunar, gui_data_t *data)
{
    DrawDateHeader(gfx, 10, 28, tm, Lunar, data);
    DrawWeekHeader(gfx, 10, 32);
    DrawMonthDays(gfx, tm, Lunar);
}

/* Routine to Draw Large 7-Segment formated number
   Contributed by William Zaggle.

   int n - The number to be displayed
   int xLoc = The x location of the upper left corner of the number
   int yLoc = The y location of the upper left corner of the number
   int cS = The size of the number. 
   fC is the foreground color of the number
   bC is the background color of the number (prevents having to clear previous space)
   nD is the number of digit spaces to occupy (must include space for minus sign for numbers < 0).

   width: nD*(11*cS+2)-2*cS
   height: 20*cS+4

   https://forum.arduino.cc/t/fast-7-segment-number-display-for-tft/296619/4
*/
static void Draw7Number(Adafruit_GFX *gfx, int n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC, char nD) {
    unsigned int num=abs(n),i,t,w,col,h,a,b,j=1,d=0,S2=5*cS,S3=2*cS,S4=7*cS,x1=cS+1,x2=S3+S2+1,y1=yLoc+x1,y3=yLoc+S3+S4+1;
    unsigned int seg[7][3]={{x1,yLoc,1},{x2,y1,0},{x2,y3+x1,0},{x1,(2*y3)-yLoc,1},{0,y3+x1,0},{0,y1,0},{x1,y3,1}};
    unsigned char nums[12]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x00,0x40},c=(c=abs(cS))>10?10:(c<1)?1:c,cnt=(cnt=abs(nD))>10?10:(cnt<1)?1:cnt;
    for (xLoc+=cnt*(d=S2+(3*S3)+2);cnt>0;cnt--){
      for (i=(num>9)?num%10:((!cnt)&&(n<0))?11:((nD<0)&&(!num))?10:num,xLoc-=d,num/=10,j=0;j<7;++j){
        col=(nums[i]&(1<<j))?fC:bC;
        if (seg[j][2])for(w=S2,t=seg[j][1]+S3,h=seg[j][1]+cS,a=xLoc+seg[j][0]+cS,b=seg[j][1];b<h;b++,a--,w+=2)GFX_drawFastHLine(gfx,a,b,w,col);
        else for(w=S4,t=xLoc+seg[j][0]+S3,h=xLoc+seg[j][0]+cS,b=xLoc+seg[j][0],a=seg[j][1]+cS;b<h;b++,a--,w+=2)GFX_drawFastVLine(gfx,b,a,w,col);
        for (;b<t;b++,a++,w-=2)seg[j][2]?GFX_drawFastHLine(gfx,a,b,w,col):GFX_drawFastVLine(gfx,b,a,w,col);
        }
    }
}

static void DrawTime(Adafruit_GFX *gfx, tm_t *tm, int16_t x, int16_t y, uint16_t cS, uint16_t nD)
{
    Draw7Number(gfx, tm->tm_hour, x, y, cS, GFX_BLACK, GFX_WHITE, nD);
    x += (nD*(11*cS+2)-2*cS) + 2*cS;
    GFX_fillRect(gfx, x, y + 4.5*cS+1, 2*cS, 2*cS, GFX_BLACK);
    GFX_fillRect(gfx, x, y + 13.5*cS+3, 2*cS, 2*cS, GFX_BLACK);
    x += 4*cS;
    Draw7Number(gfx, tm->tm_min, x, y, cS, GFX_BLACK, GFX_WHITE, nD);
}

static void DrawClock(Adafruit_GFX *gfx, tm_t *tm, struct Lunar_Date *Lunar, gui_data_t *data)
{
    DrawDate(gfx, 40, 36, tm);
    GFX_setCursor(gfx, 40, 58);
    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "星期%s", Lunar_DayString[tm->tm_wday]);
    GFX_setCursor(gfx, 138, 58);
    GFX_printf(gfx, "%s%s%s", Lunar_MonthLeapString[Lunar->IsLeap], Lunar_MonthString[Lunar->Month],
        Lunar_DateString[Lunar->Date]);

    DrawBattery(gfx, 330, 25, data->voltage);
    DrawTemperature(gfx, 330, 58, data->temperature);

    GFX_drawFastHLine(gfx, 30, 68, 330, GFX_BLACK);
    DrawTime(gfx, tm, 70, 98, 5, 2);
    GFX_drawFastHLine(gfx, 30, 232, 330, GFX_BLACK);

    GFX_setCursor(gfx, 40, 275);
    GFX_setFont(gfx, u8g2_font_wqy12_t_lunar);
    GFX_printf(gfx, "%s%s%s年", Lunar_StemStrig[LUNAR_GetStem(Lunar)], Lunar_BranchStrig[LUNAR_GetBranch(Lunar)],
        Lunar_ZodiacString[LUNAR_GetZodiac(Lunar)]);

    uint8_t day = 0;
    uint8_t JQday = GetJieQiStr(tm->tm_year + YEAR0, tm->tm_mon + 1, tm->tm_mday, &day);
    if (day == 0) {
        GFX_setCursor(gfx, 320, 275);
        GFX_printf(gfx, "%s", JieQiStr[JQday % 24]);
    } else {
        GFX_setCursor(gfx, 300, 265);
        GFX_printf(gfx, "离%s", JieQiStr[JQday % 24]);
        GFX_setCursor(gfx, 290, 285);
        GFX_printf(gfx, "还有%d天", day);
    }
}

void DrawGUI(gui_data_t *data, buffer_callback draw, display_mode_t mode)
{
    tm_t tm = {0};
    struct Lunar_Date Lunar;

    transformTime(data->timestamp, &tm);

    Adafruit_GFX gfx;

    if (data->bwr)
      GFX_begin_3c(&gfx, data->width, data->height, PAGE_HEIGHT);
    else
      GFX_begin(&gfx, data->width, data->height, PAGE_HEIGHT);

    GFX_firstPage(&gfx);
    do {
        GFX_fillScreen(&gfx, GFX_WHITE);

        LUNAR_SolarToLunar(&Lunar, tm.tm_year + YEAR0, tm.tm_mon + 1, tm.tm_mday);

        switch (mode) {
            case MODE_CALENDAR:
                DrawCalendar(&gfx, &tm, &Lunar, data);
                break;
            case MODE_CLOCK:
                DrawClock(&gfx, &tm, &Lunar, data);
                break;
            default:
                break;
        }
    } while(GFX_nextPage(&gfx, draw));

    GFX_end(&gfx);
}
