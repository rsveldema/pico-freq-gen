#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/interp.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include <stdio.h>
#include <math.h>

#include <array>

#include "blink.pio.h"

template <int num_bytes> class BitVec
{
  public:
    using elt_type_t = int32_t;
    static constexpr auto BITS_PER_ELT = sizeof(elt_type_t) * 8;

    BitVec()
    {
    }

    elt_type_t *data()
    {
        return m_data.data();
    }

    size_t num_bits() const
    {
        return m_data.size() * BITS_PER_ELT;
    }

    bool get_bit(size_t bit) const
    {
        const auto off = bit / BITS_PER_ELT;
        const auto ix = bit % BITS_PER_ELT;
        return (m_data[off] & (1 << ix)) != 0;
    }

    void set_bit(size_t bit, bool value)
    {
        const auto off = bit / BITS_PER_ELT;
        const auto ix = bit % BITS_PER_ELT;
        m_data[off] |= (value << ix);
    }

    size_t num_elts() const
    {
        return m_data.size();
    }

  private:
    std::array<elt_type_t, num_bytes> m_data;
};

// Data will be copied from src to dst
BitVec<48 * 1024> src;
int dma_chan;
const int sm = 0;
dma_channel_config dma_config;

// this is a raw helper function for use by the user which sets up the GPIO output, and configures the SM to output on a
// particular pin
void blink_program_init(PIO pio, uint sm, uint offset, uint pin)
{
    pio_gpio_init(pio, pin);
    gpio_set_drive_strength(pin, gpio_drive_strength::GPIO_DRIVE_STRENGTH_8MA);

    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_config pio_config = blink_program_get_default_config(offset);
    //sm_config_set_set_pins(&pio_config, pin, 1);

    sm_config_set_out_pins(&pio_config, pin, 1);
    sm_config_set_fifo_join(&pio_config, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&pio_config, true, true, src.BITS_PER_ELT);

    pio_sm_init(pio, sm, offset, &pio_config);
    pio_sm_set_enabled(pio, sm, true);
}

void dma_handler()
{
    // Clear the interrupt request.
    dma_hw->ints0 = 1u << dma_chan;
    // Give the channel a new wave table entry to read from, and re-trigger it
    dma_channel_set_read_addr(dma_chan, src.data(), true);
}

int main()
{
    stdio_init_all();

    constexpr size_t sys_hz = 150 * 1000 * 1000;
    if (clock_get_hz(clk_sys) != sys_hz) {
        //constexpr size_t sys_khz = sys_hz / 1000;
        //set_sys_clock_khz(sys_khz, true);
        printf("error: clk-hz != sys clk\n");
        abort();
    }
    printf("System Clock Frequency is %d Hz\n", sys_hz);
    printf("USB Clock Frequency is %d Hz\n", clock_get_hz(clk_usb));

    constexpr double freq = (48 * 1000.0) + 0;
    constexpr double wave_len = sys_hz / freq;
    constexpr double wave_half = wave_len / 2;
    if (wave_len > src.num_bits())
    {
        printf("ERROR: wave len %g > %g\n", wave_len, (int)src.num_bits());
        abort();
    }
    printf("NOTE: wave len %g <= %d\n", wave_len, (int)src.num_bits());

    for (size_t i = 0; i < src.num_bits(); i++)
    {
        const auto wave_ix = floor((double)i / wave_len);
        const auto start_range = wave_ix * wave_len;
        const auto end_range = (wave_ix + 1) * wave_len;
        const auto range_len = end_range - start_range;

        const auto value = (i - start_range) > wave_half;
        src.set_bit(i, value);
    }

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);

    blink_program_init(pio, sm, offset, 6);
    pio_sm_set_enabled(pio, sm, true);

    printf("Loaded program at %d\n", offset);

    dma_chan = dma_claim_unused_channel(true);
    dma_config = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, DREQ_PIO0_TX0);

    dma_channel_configure(dma_chan,       // Channel to be configured
                          &dma_config,    // The configuration we just created
                          &pio->txf[sm],  // The initial write address
                          NULL, //src.data(),     // The initial read address
                          src.num_elts(), // Number of transfers; in this case each is 1 byte.
                          false           // Start immediately.
    );
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // start running now that the dma handler is setup
    dma_handler();

    int x = 0;
    while (true)
    {
        tight_loop_contents();
        //printf("Hello, world: %d\n", x++);
        //sleep_ms(100000);
    }
}
