#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/interp.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include <array>

#include "blink.pio.h"

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
  blink_program_init(pio, sm, offset, pin);
  pio_sm_set_enabled(pio, sm, true);

  printf("Blinking pin %d at %d Hz\n", pin, freq);

  // PIO counter program takes 3 more cycles in total than we pass as
  // input (wait for n + 1; mov; jmp)
  pio->txf[sm] = (150000000 / (2 * freq)) - 3;
}

int64_t alarm_callback(alarm_id_t id, void *user_data) {
  // Put your timeout handler code in here
  return 0;
}

// Data will be copied from src to dst
std::array<char, 32 * 1024> src;
char dst[count_of(src)];

int main() {
  stdio_init_all();

  const auto hz = clock_get_hz(clk_sys);
  printf("System Clock Frequency is %d Hz\n", hz);
  printf("USB Clock Frequency is %d Hz\n", clock_get_hz(clk_usb));

  for (int i = 0; i < src.size(); i++) {
  }

  // Get a free channel, panic() if there are none
  int chan = dma_claim_unused_channel(true);

  // 8 bit transfers. Both read and write address increment after each
  // transfer (each pointing to a location in src or dst respectively).
  // No DREQ is selected, so the DMA transfers as fast as it can.

  dma_channel_config c = dma_channel_get_default_config(chan);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
  channel_config_set_read_increment(&c, true);
  channel_config_set_write_increment(&c, true);

  dma_channel_configure(
      chan,          // Channel to be configured
      &c,            // The configuration we just created
      dst,           // The initial write address
      src,           // The initial read address
      count_of(src), // Number of transfers; in this case each is 1 byte.
      true           // Start immediately.
  );

  // We could choose to go and do something else whilst the DMA is doing its
  // thing. In this case the processor has nothing else to do, so we just
  // wait for the DMA to finish.
  dma_channel_wait_for_finish_blocking(chan);

  // The DMA has now copied our text from the transmit buffer (src) to the
  // receive buffer (dst), so we can print it out from there.
  puts(dst);

  // PIO Blinking example
  PIO pio = pio0;
  uint offset = pio_add_program(pio, &blink_program);
  printf("Loaded program at %d\n", offset);

#ifdef PICO_DEFAULT_LED_PIN
  blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 3);
#else
  blink_pin_forever(pio, 0, offset, 6, 3);
#endif
  // For more pio examples see
  // https://github.com/raspberrypi/pico-examples/tree/master/pio

  int x = 0;
  while (true) {
    printf("Hello, world: %d\n", x++);
    sleep_ms(1000);
  }
}
