#ifndef __CALENDAR_H
#define __CALENDAR_H

#include <stdint.h>
#include "EPD_driver.h"

void DrawCalendar(epd_model_t *epd, uint32_t timestamp);

#endif
