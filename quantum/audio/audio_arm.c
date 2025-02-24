/* Copyright 2016-2019 Jack Humbert
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "audio.h"
#include "ch.h"
#include "hal.h"

#include <string.h>
#include "print.h"
#include "keymap.h"

#include "eeconfig.h"

// -----------------------------------------------------------------------------

uint8_t dac_voices = 0;
float dac_frequencies[8] = { 0.0 };

bool     playing_notes = false;
bool     playing_note = false;
float    note_frequency = 0;
float    note_length = 0;
uint8_t  note_tempo = TEMPO_DEFAULT;
float    note_timbre = TIMBRE_DEFAULT;
uint32_t note_position = 0;
float (* notes_pointer)[][2];
uint16_t notes_count;
bool     notes_repeat;

uint16_t current_note = 0;

#ifdef VIBRATO_ENABLE
float vibrato_counter = 0;
float vibrato_strength = .5;
float vibrato_rate = 0.125;
#endif

float polyphony_rate = 0;

static bool audio_initialized = false;

audio_config_t audio_config;

uint16_t envelope_index = 0;
bool glissando = true;

#ifndef STARTUP_SONG
    #define STARTUP_SONG SONG(STARTUP_SOUND)
#endif
float startup_song[][2] = STARTUP_SONG;

static const dacsample_t dac_buffer_sine[DAC_BUFFER_SIZE] = {
  // 256 values, max 4095
  0x800,0x832,0x864,0x896,0x8c8,0x8fa,0x92c,0x95e,
  0x98f,0x9c0,0x9f1,0xa22,0xa52,0xa82,0xab1,0xae0,
  0xb0f,0xb3d,0xb6b,0xb98,0xbc5,0xbf1,0xc1c,0xc47,
  0xc71,0xc9a,0xcc3,0xceb,0xd12,0xd39,0xd5f,0xd83,
  0xda7,0xdca,0xded,0xe0e,0xe2e,0xe4e,0xe6c,0xe8a,
  0xea6,0xec1,0xedc,0xef5,0xf0d,0xf24,0xf3a,0xf4f,
  0xf63,0xf76,0xf87,0xf98,0xfa7,0xfb5,0xfc2,0xfcd,
  0xfd8,0xfe1,0xfe9,0xff0,0xff5,0xff9,0xffd,0xffe,
  0xfff,0xffe,0xffd,0xff9,0xff5,0xff0,0xfe9,0xfe1,
  0xfd8,0xfcd,0xfc2,0xfb5,0xfa7,0xf98,0xf87,0xf76,
  0xf63,0xf4f,0xf3a,0xf24,0xf0d,0xef5,0xedc,0xec1,
  0xea6,0xe8a,0xe6c,0xe4e,0xe2e,0xe0e,0xded,0xdca,
  0xda7,0xd83,0xd5f,0xd39,0xd12,0xceb,0xcc3,0xc9a,
  0xc71,0xc47,0xc1c,0xbf1,0xbc5,0xb98,0xb6b,0xb3d,
  0xb0f,0xae0,0xab1,0xa82,0xa52,0xa22,0x9f1,0x9c0,
  0x98f,0x95e,0x92c,0x8fa,0x8c8,0x896,0x864,0x832,
  0x800,0x7cd,0x79b,0x769,0x737,0x705,0x6d3,0x6a1,
  0x670,0x63f,0x60e,0x5dd,0x5ad,0x57d,0x54e,0x51f,
  0x4f0,0x4c2,0x494,0x467,0x43a,0x40e,0x3e3,0x3b8,
  0x38e,0x365,0x33c,0x314,0x2ed,0x2c6,0x2a0,0x27c,
  0x258,0x235,0x212,0x1f1,0x1d1,0x1b1,0x193,0x175,
  0x159,0x13e,0x123,0x10a,0xf2, 0xdb, 0xc5, 0xb0,
  0x9c, 0x89, 0x78, 0x67, 0x58, 0x4a, 0x3d, 0x32,
  0x27, 0x1e, 0x16, 0xf,  0xa,  0x6,  0x2,  0x1,
  0x0,  0x1,  0x2,  0x6,  0xa,  0xf,  0x16, 0x1e,
  0x27, 0x32, 0x3d, 0x4a, 0x58, 0x67, 0x78, 0x89,
  0x9c, 0xb0, 0xc5, 0xdb, 0xf2, 0x10a,0x123,0x13e,
  0x159,0x175,0x193,0x1b1,0x1d1,0x1f1,0x212,0x235,
  0x258,0x27c,0x2a0,0x2c6,0x2ed,0x314,0x33c,0x365,
  0x38e,0x3b8,0x3e3,0x40e,0x43a,0x467,0x494,0x4c2,
  0x4f0,0x51f,0x54e,0x57d,0x5ad,0x5dd,0x60e,0x63f,
  0x670,0x6a1,0x6d3,0x705,0x737,0x769,0x79b,0x7cd
};

static const dacsample_t dac_buffer_triangle[DAC_BUFFER_SIZE] = {
  // 256 values, max 4095
  0x20, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xe0, 0x100,
  0x120,0x140,0x160,0x180,0x1a0,0x1c0,0x1e0,0x200,
  0x220,0x240,0x260,0x280,0x2a0,0x2c0,0x2e0,0x300,
  0x320,0x340,0x360,0x380,0x3a0,0x3c0,0x3e0,0x400,
  0x420,0x440,0x460,0x480,0x4a0,0x4c0,0x4e0,0x500,
  0x520,0x540,0x560,0x580,0x5a0,0x5c0,0x5e0,0x600,
  0x620,0x640,0x660,0x680,0x6a0,0x6c0,0x6e0,0x700,
  0x720,0x740,0x760,0x780,0x7a0,0x7c0,0x7e0,0x800,
  0x81f,0x83f,0x85f,0x87f,0x89f,0x8bf,0x8df,0x8ff,
  0x91f,0x93f,0x95f,0x97f,0x99f,0x9bf,0x9df,0x9ff,
  0xa1f,0xa3f,0xa5f,0xa7f,0xa9f,0xabf,0xadf,0xaff,
  0xb1f,0xb3f,0xb5f,0xb7f,0xb9f,0xbbf,0xbdf,0xbff,
  0xc1f,0xc3f,0xc5f,0xc7f,0xc9f,0xcbf,0xcdf,0xcff,
  0xd1f,0xd3f,0xd5f,0xd7f,0xd9f,0xdbf,0xddf,0xdff,
  0xe1f,0xe3f,0xe5f,0xe7f,0xe9f,0xebf,0xedf,0xeff,
  0xf1f,0xf3f,0xf5f,0xf7f,0xf9f,0xfbf,0xfdf,0xfff,
  0xfdf,0xfbf,0xf9f,0xf7f,0xf5f,0xf3f,0xf1f,0xeff,
  0xedf,0xebf,0xe9f,0xe7f,0xe5f,0xe3f,0xe1f,0xdff,
  0xddf,0xdbf,0xd9f,0xd7f,0xd5f,0xd3f,0xd1f,0xcff,
  0xcdf,0xcbf,0xc9f,0xc7f,0xc5f,0xc3f,0xc1f,0xbff,
  0xbdf,0xbbf,0xb9f,0xb7f,0xb5f,0xb3f,0xb1f,0xaff,
  0xadf,0xabf,0xa9f,0xa7f,0xa5f,0xa3f,0xa1f,0x9ff,
  0x9df,0x9bf,0x99f,0x97f,0x95f,0x93f,0x91f,0x8ff,
  0x8df,0x8bf,0x89f,0x87f,0x85f,0x83f,0x81f,0x800,
  0x7e0,0x7c0,0x7a0,0x780,0x760,0x740,0x720,0x700,
  0x6e0,0x6c0,0x6a0,0x680,0x660,0x640,0x620,0x600,
  0x5e0,0x5c0,0x5a0,0x580,0x560,0x540,0x520,0x500,
  0x4e0,0x4c0,0x4a0,0x480,0x460,0x440,0x420,0x400,
  0x3e0,0x3c0,0x3a0,0x380,0x360,0x340,0x320,0x300,
  0x2e0,0x2c0,0x2a0,0x280,0x260,0x240,0x220,0x200,
  0x1e0,0x1c0,0x1a0,0x180,0x160,0x140,0x120,0x100,
  0xe0, 0xc0, 0xa0, 0x80, 0x60, 0x40, 0x20, 0x0
};

static const dacsample_t dac_buffer_square[DAC_BUFFER_SIZE] = {
  // First half is max, second half is 0
  [0                 ... DAC_BUFFER_SIZE/2-1] = DAC_SAMPLE_MAX,
  [DAC_BUFFER_SIZE/2 ... DAC_BUFFER_SIZE  -1] = 0,
};

static dacsample_t dac_buffer_empty[DAC_BUFFER_SIZE] = { DAC_OFF_VALUE };

static float dac_if[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

/**
 * Generation of the waveform being passed to the callback. Declared weak so users
 * can override it with their own waveforms/noises.
 */
__attribute__ ((weak))
uint16_t dac_value_generate(void) {
  uint16_t value = DAC_OFF_VALUE;
  uint8_t working_voices = dac_voices;
  if (working_voices > DAC_VOICES_MAX)
    working_voices = DAC_VOICES_MAX;

  if (working_voices > 0) {
    uint16_t value_avg = 0;
    for (uint8_t i = 0; i < working_voices; i++) {
      dac_if[i] = dac_if[i] + ((dac_frequencies[i] * DAC_BUFFER_SIZE) / DAC_SAMPLE_RATE);

      // Needed because % doesn't work with floats
      while (dac_if[i] >= (DAC_BUFFER_SIZE))
        dac_if[i] = dac_if[i] - DAC_BUFFER_SIZE;

      // Wavetable generation/lookup
      uint16_t dac_i = (uint16_t)dac_if[i];
      // SINE
      value_avg += dac_buffer_sine[dac_i] / working_voices / 3;
      // TRIANGLE
      value_avg += dac_buffer_triangle[dac_i] / working_voices / 3;
      // SQUARE
      value_avg += dac_buffer_square[dac_i] / working_voices / 3;
    }
    value = value_avg;
  }
  return value;
}

/**
 * DAC streaming callback. Does all of the main computing for playing songs.
 */
static void dac_end(DACDriver * dacp, dacsample_t * sample_p, size_t sample_count) {

  (void)dacp;

  for (uint8_t s = 0; s < sample_count; s++) {
    sample_p[s] = dac_value_generate();
  }

  if (playing_notes) {
    note_position += sample_count;

    // End of the note - 35 is arbitary here, but gets us close to AVR's timing
    if ((note_position >= (note_length*DAC_SAMPLE_RATE/35))) {
      stop_note((*notes_pointer)[current_note][0]);
      current_note++;
      if (current_note >= notes_count) {
        if (notes_repeat) {
          current_note = 0;
        } else {
          playing_notes = false;
          return;
        }
      }
      play_note((*notes_pointer)[current_note][0], 15);
      envelope_index = 0;
      note_length = ((*notes_pointer)[current_note][1] / 4) * (((float)note_tempo) / 100);

      // Skip forward in the next note's length if we've over shot the last, so
      // the overall length of the song is the same
      note_position = note_position - (note_length*DAC_SAMPLE_RATE/35);
    }
  }
}

static void dac_error(DACDriver *dacp, dacerror_t err) {

  (void)dacp;
  (void)err;

  chSysHalt("DAC failure. halp");
}

static const GPTConfig gpt6cfg1 = {
  .frequency    = DAC_SAMPLE_RATE * 3,
  .callback     = NULL,
  .cr2          = TIM_CR2_MMS_1,       /* MMS = 010 = TRGO on Update Event.  */
  .dier         = 0U
};

static const DACConfig dac_conf = {
  .init         = DAC_SAMPLE_MAX,
  .datamode     = DAC_DHRM_12BIT_RIGHT
};

/**
 * @note The DAC_TRG(0) here selects the Timer 6 TRGO event, which is triggered
 * on the rising edge after 3 APB1 clock cycles, causing our gpt6cfg1.frequency
 * to be a third of what we expect.
 *
 * Here are all the values for DAC_TRG (TSEL in the ref manual)
 * TIM15_TRGO 0b011
 * TIM2_TRGO  0b100
 * TIM3_TRGO  0b001
 * TIM6_TRGO  0b000
 * TIM7_TRGO  0b010
 * EXTI9      0b110
 * SWTRIG     0b111
 */
static const DACConversionGroup dac_conv_cfg = {
  .num_channels = 1U,
  .end_cb       = dac_end,
  .error_cb     = dac_error,
  .trigger      = DAC_TRG(0b000)
};

void audio_init() {

  if (audio_initialized) {
    return;
  }

  // Check EEPROM
  #if defined(STM32_EEPROM_ENABLE) || defined(PROTOCOL_ARM_ATSAM) || defined(EEPROM_SIZE)
    if (!eeconfig_is_enabled()) {
      eeconfig_init();
    }
    audio_config.raw = eeconfig_read_audio();
#else // ARM EEPROM
    audio_config.enable = true;
  #ifdef AUDIO_CLICKY_ON
    audio_config.clicky_enable = true;
  #endif
#endif // ARM EEPROM


#if defined(A4_AUDIO)
  palSetPadMode(GPIOA, 4, PAL_MODE_INPUT_ANALOG );
  dacStart(&DACD1, &dac_conf);
  dacStartConversion(&DACD1, &dac_conv_cfg, dac_buffer_empty, DAC_BUFFER_SIZE);
#endif
#if defined(A5_AUDIO)
  palSetPadMode(GPIOA, 5, PAL_MODE_INPUT_ANALOG );
  dacStart(&DACD2, &dac_conf);
  dacStartConversion(&DACD2, &dac_conv_cfg, dac_buffer_empty, DAC_BUFFER_SIZE);
#endif

  gptStart(&GPTD6, &gpt6cfg1);
  gptStartContinuous(&GPTD6, 2U);

  audio_initialized = true;

  if (audio_config.enable) {
    PLAY_SONG(startup_song);
  } else {
    stop_all_notes();
  }

}

void stop_all_notes() {
    dprintf("audio stop all notes");

    if (!audio_initialized) {
        audio_init();
    }
    dac_voices = 0;

    playing_notes = false;
    playing_note = false;

    for (uint8_t i = 0; i < 8; i++) {
        dac_frequencies[i] = 0;
    }
}

void stop_note(float freq) {
  dprintf("audio stop note freq=%d", (int)freq);

  if (playing_note) {
    if (!audio_initialized) {
      audio_init();
    }
    for (int i = 7; i >= 0; i--) {
      if (dac_frequencies[i] == freq) {
        dac_frequencies[i] = 0;
        for (int j = i; (j < 7); j++) {
          dac_frequencies[j] = dac_frequencies[j+1];
          dac_frequencies[j+1] = 0;
        }
        break;
      }
    }
    dac_voices--;
    if (dac_voices < 0) {
      dac_voices = 0;
    }
    if (dac_voices == 0) {
      playing_note = false;
    }
  }
}

#ifdef VIBRATO_ENABLE

float mod(float a, int b) {
  float r = fmod(a, b);
  return r < 0 ? r + b : r;
}

float vibrato(float average_freq) {
  #ifdef VIBRATO_STRENGTH_ENABLE
    float vibrated_freq = average_freq * pow(vibrato_lut[(int)vibrato_counter], vibrato_strength);
  #else
    float vibrated_freq = average_freq * vibrato_lut[(int)vibrato_counter];
  #endif
  vibrato_counter = mod((vibrato_counter + vibrato_rate * (1.0 + 440.0/average_freq)), VIBRATO_LUT_LENGTH);
  return vibrated_freq;
}

#endif


void play_note(float freq, int vol) {

  dprintf("audio play note freq=%d vol=%d", (int)freq, vol);

  if (!audio_initialized) {
      audio_init();
  }

  if (audio_config.enable && dac_voices < 8) {
    playing_note = true;
    if (freq > 0) {
      envelope_index = 0;
      dac_frequencies[dac_voices] = freq;
      dac_voices++;
    }
  }
}

__attribute__ ((weak))
void dac_setup_note(void) {
  dac_if[dac_voices] = 0.0f;
}

void play_notes(float (*np)[][2], uint16_t n_count, bool n_repeat) {

  if (!audio_initialized) {
    audio_init();
  }

  if (audio_config.enable) {

    playing_notes = true;

    notes_pointer = np;
    notes_count = n_count;
    notes_repeat = n_repeat;

    current_note = 0;

    note_length = ((*notes_pointer)[current_note][1] / 4) * (((float)note_tempo) / 100);
    note_position = 0;

    play_note((*notes_pointer)[current_note][0], 15);

  }
}

bool is_playing_note(void) {
  return playing_note;
}

bool is_playing_notes(void) {
  return playing_notes;
}

bool is_audio_on(void) {
  return (audio_config.enable != 0);
}

void audio_toggle(void) {
  audio_config.enable ^= 1;
  eeconfig_update_audio(audio_config.raw);
  if (audio_config.enable) {
    audio_on_user();
  }
}

void audio_on(void) {
  audio_config.enable = 1;
  eeconfig_update_audio(audio_config.raw);
  audio_on_user();
}

void audio_off(void) {
  stop_all_notes();
  audio_config.enable = 0;
  eeconfig_update_audio(audio_config.raw);
}

#ifdef VIBRATO_ENABLE

// Vibrato rate functions

void set_vibrato_rate(float rate) {
  vibrato_rate = rate;
}

void increase_vibrato_rate(float change) {
  vibrato_rate *= change;
}

void decrease_vibrato_rate(float change) {
  vibrato_rate /= change;
}

#ifdef VIBRATO_STRENGTH_ENABLE

void set_vibrato_strength(float strength) {
  vibrato_strength = strength;
}

void increase_vibrato_strength(float change) {
  vibrato_strength *= change;
}

void decrease_vibrato_strength(float change) {
  vibrato_strength /= change;
}

#endif  /* VIBRATO_STRENGTH_ENABLE */

#endif /* VIBRATO_ENABLE */

// Polyphony functions

void set_polyphony_rate(float rate) {
  polyphony_rate = rate;
}

void enable_polyphony() {
  polyphony_rate = 5;
}

void disable_polyphony() {
  polyphony_rate = 0;
}

void increase_polyphony_rate(float change) {
  polyphony_rate *= change;
}

void decrease_polyphony_rate(float change) {
  polyphony_rate /= change;
}

// Timbre function

void set_timbre(float timbre) {
  note_timbre = timbre;
}

// Tempo functions

void set_tempo(uint8_t tempo) {
  note_tempo = tempo;
}

void decrease_tempo(uint8_t tempo_change) {
  note_tempo += tempo_change;
}

void increase_tempo(uint8_t tempo_change) {
  if (note_tempo - tempo_change < 10) {
    note_tempo = 10;
  } else {
    note_tempo -= tempo_change;
  }
}

uint8_t dac_number_of_voices(void) {
  return dac_voices;
}

float dac_get_frequency(uint8_t index) {
  return dac_frequencies[index];
}
