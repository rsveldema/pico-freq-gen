#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/interp.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include <math.h>
#include <stdio.h>

#include <array>

#include "blink.pio.h"

// Data will be copied from src to dst
const int sm = 0;

// this is a raw helper function for use by the user which sets up the GPIO output, and configures the SM to output on a
// particular pin
void blink_program_init(PIO pio, uint sm, uint offset, uint pin)
{
    pio_gpio_init(pio, pin);
    gpio_set_drive_strength(pin, gpio_drive_strength::GPIO_DRIVE_STRENGTH_8MA);

    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_config pio_config = blink_program_get_default_config(offset);
    // sm_config_set_set_pins(&pio_config, pin, 1);

    sm_config_set_set_pins(&pio_config, pin, 1);
    sm_config_set_out_pins(&pio_config, pin, 1);
    sm_config_set_fifo_join(&pio_config, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&pio_config, true, false, 32);

    pio_sm_init(pio, sm, offset, &pio_config);
    pio_sm_set_enabled(pio, sm, true);
}

int main()
{
    stdio_init_all();

    constexpr size_t sys_hz = 150 * 1000 * 1000;
    if (clock_get_hz(clk_sys) != sys_hz)
    {
        // constexpr size_t sys_khz = sys_hz / 1000;
        // set_sys_clock_khz(sys_khz, true);
        printf("error: clk-hz != sys clk\n");
        abort();
    }
    printf("System Clock Frequency is %d Hz\n", sys_hz);
    printf("USB Clock Frequency is %d Hz\n", clock_get_hz(clk_usb));

    constexpr float freq = (48 * 1000.0) + 0;
    constexpr float wave_len = sys_hz / (freq * 2);

    PIO pio = pio0;
    const auto offset = pio_add_program(pio, &blink_program);

    blink_program_init(pio, sm, offset, 6);
    pio_sm_set_enabled(pio, sm, true);

    printf("Loaded program at %d\n", offset);

    constexpr int rounded_down = static_cast<int>(wave_len);
    constexpr float after_comma = wave_len - rounded_down;

    static_assert(after_comma >= 0);
    static_assert(after_comma < 1);


    printf("error value = %f, wave len = %f\n", after_comma, wave_len);

    uint32_t wave_ix = 0;

    // once error_sum exceeds '1' we extend our wave for one tick more
    float error_sum = 0;
    while (true)
    {
        int len_ones = wave_len;
        int len_zeros = wave_len;

        error_sum += after_comma;
        if (error_sum >= 1)
        {
            error_sum -= 1;
            len_zeros++;
        }

        if ((wave_ix % 10000) == 0)
        {
          //  printf("ix = %d, len = %d error = %f\n", wave_ix, len, error_sum);
        }
        constexpr auto WAVE_SETUP_COST = 4; // 6 insns spread over ones and zeros
        pio_sm_put_blocking(pio, sm, len_ones - WAVE_SETUP_COST);
        pio_sm_put_blocking(pio, sm, len_zeros - WAVE_SETUP_COST);

        wave_ix++;
    }
}
