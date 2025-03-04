#include <stdint.h>

typedef struct
{
    uint8_t mosi_pin;
    uint8_t sclk_pin;
    uint8_t cs_pin;
    uint8_t dc_pin;
    uint8_t rst_pin;
    uint8_t busy_pin;
    uint8_t bs_pin;
    uint8_t driver_id;
    uint8_t wakeup_pin;
    uint8_t led_pin;
    uint8_t en_pin;
} epd_config_t;

void epd_config_init(epd_config_t *cfg);
void epd_config_load(epd_config_t *cfg);
void epd_config_clear(epd_config_t *cfg);
void epd_config_save(epd_config_t *cfg);
