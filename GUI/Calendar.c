#include "Adafruit_GFX.h"
#include "fonts.h"
#include "Lunar.h"
#include "Calendar.h"
#include "nrf_log.h"

#if defined(S112)
#define PAGE_HEIGHT 150
#else
#define PAGE_HEIGHT 64
#endif

#define GFX_printf_styled(gfx, fg, bg, font, ...) \
            GFX_setTextColor(gfx, fg, bg);        \
            GFX_setFont(gfx, font);               \
            GFX_printf(gfx, __VA_ARGS__);

static void DrawDateHeader(Adafruit_GFX *gfx, int16_t x, int16_t y, tm_t *tm, struct Lunar_Date *Lunar)
{
    GFX_setCursor(gfx, x, y);
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%d", tm->tm_year + YEAR0);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "年");
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%02d", tm->tm_mon + 1);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "月");
    GFX_printf_styled(gfx, GFX_RED, GFX_WHITE, u8g2_font_helvB18_tn, "%02d", tm->tm_mday);
    GFX_printf_styled(gfx, GFX_BLACK, GFX_WHITE, u8g2_font_wqy12_t_lunar, "日 ");

    GFX_setFont(gfx, u8g2_font_wqy9_t_lunar);
    GFX_printf(gfx, "星期%s", Lunar_DayString[tm->tm_wday]);

    LUNAR_SolarToLunar(Lunar, tm->tm_year + YEAR0, tm->tm_mon + 1, tm->tm_mday);

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

void DrawCalendar(epd_model_t *epd, uint32_t timestamp)
{
    tm_t tm = {0};
    struct Lunar_Date Lunar;

    transformTime(timestamp, &tm);

    Adafruit_GFX gfx;

    if (epd->bwr)
      GFX_begin_3c(&gfx, epd->width, epd->height, PAGE_HEIGHT);
    else
      GFX_begin(&gfx, epd->width, epd->height, PAGE_HEIGHT);

    GFX_firstPage(&gfx);
    do {
        NRF_LOG_DEBUG("page %d\n", gfx.current_page);
        GFX_fillScreen(&gfx, GFX_WHITE);

        DrawDateHeader(&gfx, 10, 28, &tm, &Lunar);
        DrawWeekHeader(&gfx, 10, 32);
        DrawMonthDays(&gfx, &tm, &Lunar);
    } while(GFX_nextPage(&gfx, epd->drv->write_image));

    GFX_end(&gfx);

    NRF_LOG_DEBUG("display start\n");
    epd->drv->refresh();
    NRF_LOG_DEBUG("display end\n");
}
