#include "speaker.hpp"

bool Speaker::begin() {
    logger->info("SPEAKER", "Initializing ES8311 + I2S...");

    // Configure I2S in master TX+RX mode with MCLK
    const i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate          = SPEAKER_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 64,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = SPEAKER_SAMPLE_RATE * 256,
        .mclk_multiple        = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan         = I2S_BITS_PER_CHAN_DEFAULT,
    };

    const i2s_pin_config_t pin_config = {
        .mck_io_num   = SPEAKER_MCLK,
        .bck_io_num   = SPEAKER_SCLK,
        .ws_io_num    = MIC_LRCK,
        .data_out_num = MIC_DSIN,
        .data_in_num  = MIC_ASDOUT,
    };

    esp_err_t err = i2s_driver_install(SPEAKER_I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        logger->failure("SPEAKER", "I2S driver install failed");
        return false;
    }

    err = i2s_set_pin(SPEAKER_I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        logger->failure("SPEAKER", "I2S set pin failed");
        i2s_driver_uninstall(SPEAKER_I2S_PORT);
        return false;
    }

    // Enable NS4150B power amplifier
    pinMode(MIC_PA_CTRL, OUTPUT);
    digitalWrite(MIC_PA_CTRL, HIGH);

    // Initialize ES8311 codec via I2C port 0 (Wire)
    es_handle = es8311_create(0, ES8311_ADDRESS_0);
    if (!es_handle) {
        logger->failure("SPEAKER", "es8311_create failed");
        i2s_driver_uninstall(SPEAKER_I2S_PORT);
        return false;
    }

    const es8311_clock_config_t es_clk = {
        .mclk_inverted      = false,
        .sclk_inverted      = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency     = SPEAKER_SAMPLE_RATE * 256,
        .sample_frequency   = SPEAKER_SAMPLE_RATE
    };

    err = es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    if (err != ESP_OK) {
        logger->failure("SPEAKER", "ES8311 init failed");
        return false;
    }

    es8311_microphone_config(es_handle, false);
    es8311_voice_volume_set(es_handle, SPEAKER_VOLUME_DEFAULT, NULL);

    initialized = true;
    logger->success("SPEAKER", "ES8311 ready");
    return true;
}

void Speaker::beep(uint16_t freq, uint16_t durationMs) {
    if (!initialized) return;

    // Square wave: half-period in samples at SPEAKER_SAMPLE_RATE
    const int16_t amplitude = 8000;
    const uint32_t totalSamples = (uint32_t)SPEAKER_SAMPLE_RATE * durationMs / 1000;
    const uint32_t halfPeriod = SPEAKER_SAMPLE_RATE / (2 * freq);

    // Write in small chunks to avoid large stack allocation
    const size_t chunkSamples = 256;
    int16_t buf[chunkSamples * 2]; // stereo
    uint32_t written = 0;

    while (written < totalSamples) {
        size_t n = min((uint32_t)chunkSamples, totalSamples - written);
        for (size_t i = 0; i < n; i++) {
            int16_t val = ((written + i) / halfPeriod) % 2 == 0 ? amplitude : -amplitude;
            buf[i * 2]     = val; // L
            buf[i * 2 + 1] = val; // R
        }
        size_t bytes_written = 0;
        i2s_write(SPEAKER_I2S_PORT, buf, n * 4, &bytes_written, portMAX_DELAY);
        written += n;
    }
}

size_t Speaker::play(const uint8_t* data, size_t len) {
    if (!initialized) return 0;
    size_t written = 0;
    i2s_write(SPEAKER_I2S_PORT, data, len, &written, portMAX_DELAY);
    return written;
}

void Speaker::setVolume(int volume) {
    if (!initialized) return;
    es8311_voice_volume_set(es_handle, volume, NULL);
}

void Speaker::mute(bool enable) {
    if (!initialized) return;
    es8311_voice_mute(es_handle, enable);
}
