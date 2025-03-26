#ifndef __GUI_H
#define __GUI_H

#include <stdint.h>
#include "EPD_driver.h"

typedef enum {
    MODE_NONE = 0,
    MODE_CALENDAR = 1,
    MODE_CLOCK = 2,
} display_mode_t;

void DrawGUI(epd_model_t *epd, uint32_t timestamp, display_mode_t mode);

#endif
