#ifndef __CALENDAR_H
#define __CALENDAR_H

#include <stdint.h>
#include "EPD_driver.h"

void DrawCalendar(epd_driver_t *driver, uint32_t timestamp);

#endif
