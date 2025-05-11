/*
 * MIT License
 *
 * Copyright (c) 2025 Konstantin Polevik
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include "Config.hpp"
#include "Format.hpp"
#include "Math.hpp"

namespace mod8 {

namespace internal {

// PAL:   7093789.2 / (period * 2)
// NSTC:  7159090.5 / (period * 2)
// Say if we wanted to find the value in Hz for middle note C-2.
// Looking up the Amiga table we see the value for C-2 is 428. Therefore:
// PAL:   7093789.2 / (428 * 2) = 8287.14hz
// NSTC:  7159090.5 / (428 * 2) = 8363.42hz

// Amiga_NTSC_samplerate = 7,159,090.5 Hz / (Period * 2)
// Arduino_samplerate = 16,000,000 Hz / 256 / 2 = 31,250 Hz
// Playback_speed = Amiga_NTSC_samplerate / Arduino_samplerate
// Playback_speed = (7,159,090.5 Hz / (Period * 2)) / 31,250 Hz
// Playback_speed = 114.545448 / Period
constexpr uint64_t PLAYER_SPEED_CONSTANT = math::make_fixp_fraction<uint32_t, 14>(
  config::AMIGA_PAULA_CLOCK_FREQ, config::SAMPLING_FREQ);  // fixed-point X.14

////////////////////////////////////////////////////////////////////////////////
/// @brief Multiplies the base sample speed by the finetune correction factor.
/// @param num integer part of the correction factor
/// @param den fractional part of the correction factor (in 1/16384 units).
////////////////////////////////////////////////////////////////////////////////
constexpr uint32_t calc_speed(uint16_t intgr, uint16_t fract) {
  // 18.14 x 2.14 / 2^14 = 18.14
  return PLAYER_SPEED_CONSTANT * math::make_fixp<uint16_t, 14>(intgr, fract) / 16384U;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Speed table.
/// Values are fixed-point 18.14 numbers
/// Index is Amiga sample finetune:
/// Index:    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
/// Finetune: 0  +1  +2  +3  +4  +5  +6  +7  -8  -7  -6  -5  -4  -3  -2  -1
////////////////////////////////////////////////////////////////////////////////
constexpr uint32_t SPEED_TABLE[format::NUM_FINETUNES] MOD8_ATTR_CONST_ARRAY{
  calc_speed(1U, 0U),      // =¢0
  calc_speed(1U, 118U),    // +¢12,5
  calc_speed(1U, 238U),    // +¢25,0
  calc_speed(1U, 358U),    // +¢37,5
  calc_speed(1U, 480U),    // +¢50,0
  calc_speed(1U, 602U),    // +¢62,5
  calc_speed(1U, 725U),    // +¢75,0
  calc_speed(1U, 849U),    // +¢87,5
  calc_speed(0U, 15464U),  // -¢100,0
  calc_speed(0U, 15576U),  // -¢87,5
  calc_speed(0U, 15689U),  // -¢75,0
  calc_speed(0U, 15803U),  // -¢62,5
  calc_speed(0U, 15917U),  // -¢50,0
  calc_speed(0U, 16032U),  // -¢37,5
  calc_speed(0U, 16149U),  // -¢25,0
  calc_speed(0U, 16266U)   // -¢12,5
};

constexpr unsigned MAX_SPEED_INDEX = 7;
constexpr unsigned MIN_SPEED_INDEX = 8;

////////////////////////////////////////////////////////////////////////////////
/// @brief Minimal loop length.
/// Shorter loops will be muted 'cause of the huge overhead.
////////////////////////////////////////////////////////////////////////////////
constexpr uint16_t MIN_LOOP_LENGTH = static_cast<uint16_t>(
                                       SPEED_TABLE[MAX_SPEED_INDEX] / format::MIN_PERIOD / 16384U)
                                   + 1U;
static_assert(MIN_LOOP_LENGTH == 5U || config::SAMPLING_FREQ != 31250U, "MIN_LOOP_LENGTH was changed!");

constexpr uint32_t MAX_SPEED = SPEED_TABLE[MAX_SPEED_INDEX];

MOD8_INTERNAL_CONSTEXPR_PRINT(PLAYER_SPEED_CONSTANT);
MOD8_INTERNAL_CONSTEXPR_PRINT(MAX_SPEED);
MOD8_INTERNAL_CONSTEXPR_PRINT(MIN_LOOP_LENGTH);

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
/// @brief Sample data to play.
////////////////////////////////////////////////////////////////////////////////
struct Sample {
  const uint8_t *begin;       // PROGMEM
  const uint8_t *end;         // PROGMEM
  const uint8_t *loop_begin;  // PROGMEM
  const uint8_t *loop_end;    // PROGMEM
  uint8_t finetune;           // ∈ [0; MAX_FINETUNE]
  int8_t volume;              // ∈ [0; MAX_VOLUME]
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Sample player.
////////////////////////////////////////////////////////////////////////////////
class Sampler {
public:
  //////////////////////////////////////////////////////////////////////////////
  //@{
  /// Remove all C++ boilerplate to:
  /// - Keep the class trivially default constructible.
  /// - Avoid creating static initializers for static instances.
  /// - Avoid copying in tiny MCU's RAM.
  /// @note Call the init() method before using the class.
  Sampler() = default;
  ~Sampler() = default;
  Sampler(const Sampler &) = delete;
  Sampler &operator=(const Sampler &) = delete;
  Sampler(Sampler &&) = delete;
  Sampler &operator=(Sampler &&) = delete;
  //@}
  //////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize.
  /// Initializes the smallest possible subset of variables.
  /// Must be called once before starting to work with the sampler.
  //////////////////////////////////////////////////////////////////////////////
  void init() {
    m_active = false;
    m_sampling = false;
    m_sample = 0;
    m_cached_period = 0;
    m_cached_finetune = 0;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return to initial state.
  //////////////////////////////////////////////////////////////////////////////
  void reset() {
    // Activates bypass in the fetch_sample() method to avoid concurrent access.
    if (m_active) {
      m_active = false;
      while (m_sampling) {}
    }

    init();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Start sampling.
  /// @param sample sample to play.
  /// @param period ∈ [MIN_PERIOD; MAX_PERIOD] Amiga period.
  /// @param sample_offset ∈ [0; 255] offset in units. 1 unit = 256 bytes.
  /// @param volume ∈ [0; MAX_VOLUME] playback volume.
  //////////////////////////////////////////////////////////////////////////////
  void retrig(const Sample *sample, uint16_t period, uint8_t sample_offset, int8_t volume) {
    reset();
    set_volume(volume);

    // No sample.
    if (sample == nullptr) {
      return;
    }

    // Nothing to play.
    if (sample->begin == sample->end) {
      return;
    }

    // Update period.
    m_finetune = sample->finetune;
    internal_set_period(period);

#if defined(ARDUINO_ARCH_AVR)
    static_assert(sizeof(void *) == 2, "Only 16-bit pointers are supported");

    // Sample data boundaries.
    m_phase.w32.w1 = reinterpret_cast< uint16_t >(sample->begin);
    m_phase.w32.w0 = 0;
    m_end = reinterpret_cast< uint16_t >(sample->end);
    m_loop_begin = reinterpret_cast< uint16_t >(sample->loop_begin);
    m_loop_end = reinterpret_cast< uint16_t >(sample->loop_end);

    // If the looped section is too short, don't play it.
    // Correct handling of short loops requires too many CPU clocks.
    if (m_loop_end - m_loop_begin < internal::MIN_LOOP_LENGTH) {
      m_loopless = true;
      m_loop_end = m_loop_begin + 1U;
    } else {
      m_loopless = false;
    }

    // Apply sample offset.
    if (sample_offset != 0) {
      if (m_phase.b32.b3 <= 255U - sample_offset) {
        m_phase.b32.b3 += sample_offset;
      } else {
        m_phase.b32.b3 = 255U;
      }

      if (m_phase.w32.w1 > m_end) {
        m_phase.w32.w1 = m_end;
      }
    }

#else  // defined(ARDUINO_ARCH_AVR)
    static_assert(sizeof(intptr_t) >= 4, "Unsupported size of pointer");

    // Sample data boundaries.
    m_phase = 0;
    m_sample_base = sample->begin;
    m_end = sample->end - m_sample_base;
    m_loop_begin = sample->loop_begin - m_sample_base;
    m_loop_end = sample->loop_end - m_sample_base;

    // If the looped section is too short, don't play it.
    // Correct handling of short loops requires too many CPU clocks.
    if (m_loop_end - m_loop_begin < internal::MIN_LOOP_LENGTH) {
      m_loopless = true;
      m_loop_end = m_loop_begin + 1U;
    } else {
      m_loopless = false;
    }

    // Apply sample offset.
    if (sample_offset != 0) {
      m_phase += sample_offset * 256;

      if (m_phase > m_end) {
        m_phase = m_end;
      }
    }

    // convert to fixed point X.16
    m_phase = math::make_fixp<intptr_t, 16>(m_phase, 0);
    m_end = math::make_fixp<intptr_t, 16>(m_end, 0);
    m_loop_begin = math::make_fixp<intptr_t, 16>(m_loop_begin, 0);
    m_loop_end = math::make_fixp<intptr_t, 16>(m_loop_end, 0);

#endif  // defined(ARDUINO_ARCH_AVR)

    m_active = true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set volume.
  /// @param volume ∈ [0; MAX_VOLUME]
  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE void set_volume(int8_t volume) {
    m_volume = volume >> config::VOLUME_ATTENNUATION_LOG2;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set sampling period.
  /// @param period ∈ [MIN_PERIOD; MAX_PERIOD] measured in Amiga values.
  //////////////////////////////////////////////////////////////////////////////
  void set_period(uint16_t period) {
    if (m_active) {
      internal_set_period(period);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Fetch next sample.
  /// Time-critical routine. Called from interrupt.
  /// Estimated max. duration: 86 CPU clocks on ATmega328.
  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE void fetch_sample() /* called from interrupt */ {
    using math::u8_to_s8;
    using memory::read_song_byte;

    // ■■■■■■■■■■■■■
    // ■  5 clocks ■
    // ■■■■■■■■■■■■■
    if (!m_active) {
      return;
    }

    // ■■■■■■■■■■■■■
    // ■  3 clocks ■
    // ■■■■■■■■■■■■■
    m_sampling = true;

#if defined(ARDUINO_ARCH_AVR)
    // ■■■■■■■■■■■■■
    // ■ 16 clocks ■
    // ■■■■■■■■■■■■■
    // sample ∈ [-128; 127]
    const int8_t sample = u8_to_s8(
      read_song_byte(reinterpret_cast<const uint8_t *>(m_phase.w32.w1)));
    // m_volume ∈ [0; 64]
    // m_sample ∈ [-8192; 8128]
    m_sample = sample * m_volume;

    // ■■■■■■■■■■■■■
    // ■ 28 clocks ■
    // ■■■■■■■■■■■■■
    m_phase.u32 += m_phase_increment.u32;

    // ■■■■■■■■■■■■■
    // ■ 11 clocks ■
    // ■■■■■■■■■■■■■
    if (m_phase.w32.w1 >= m_end) {
      // ■■■■■■■■■■■■■■■■
      // ■ 11/13 clocks ■
      // ■■■■■■■■■■■■■■■■
      if (!m_loopless) {
        m_phase.w32.w1 -= (m_end - m_loop_begin);
      } else {
        m_phase.w32.w1 = m_loop_begin;
      }

      // ■■■■■■■■■■■■■
      // ■  8 clocks ■
      // ■■■■■■■■■■■■■
      m_end = m_loop_end;
    }
#else   // defined(ARDUINO_ARCH_AVR)
    // sample ∈ [-128; 127]
    const int8_t sample = u8_to_s8(read_song_byte(m_sample_base + (m_phase >> 16)));
    // m_volume ∈ [0; 64]
    // m_sample ∈ [-8192; 8128]
    m_sample = m_volume * sample;

    m_phase += m_phase_increment;

    if (m_phase >= m_end) {
      if (!m_loopless) {
        m_phase -= (m_end - m_loop_begin);
      } else {
        m_phase = m_loop_begin;
      }

      m_end = m_loop_end;
    }
#endif  // defined(ARDUINO_ARCH_AVR)

    // ■■■■■■■■■■■■■
    // ■  2 clocks ■
    // ■■■■■■■■■■■■■
    m_sampling = false;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Current sample value.
  /// Called from interrupt.
  /// @return ∈ [-8192; 8128]
  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE int16_t get_sample() const /* called from interrupt */ {
    return m_sample;
  }

private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Calc playback speed.
  /// @param period ∈ [MIN_PERIOD; MAX_PERIOD]
  //////////////////////////////////////////////////////////////////////////////
  void internal_set_period(uint16_t period) {
    using memory::read_table_dword;

    if (period < format::MIN_PERIOD) {
      period = format::MIN_PERIOD;
    }

    if (period > format::MAX_PERIOD) {
      period = format::MAX_PERIOD;
    }

    // Skip expensive phase increment calculation if nothing has changed.
    if (period == m_cached_period && m_finetune == m_cached_finetune) {
      return;
    }

    m_cached_period = period;
    m_cached_finetune = m_finetune;

    // Fixed-point 18.14
    const uint32_t speed_constant = read_table_dword(internal::SPEED_TABLE + m_finetune);

    // Fixed-point 18.14 / 16.0 -> 2.14
    const auto speed = static_cast<uint16_t>(speed_constant / period);

#if defined(ARDUINO_ARCH_AVR)
    // Fixed-point 2.14 -> 16.16
    const math::Int32 increment{ (uint32_t)speed << 2U };

    // Update LSB first
    m_phase_increment.b32.b0 = increment.b32.b0;
    m_phase_increment.b32.b1 = increment.b32.b1;
    m_phase_increment.b32.b2 = increment.b32.b2;
    m_phase_increment.b32.b3 = increment.b32.b3;

#else   // defined(ARDUINO_ARCH_AVR)
    // Fixed-point 2.14 -> X.16
    m_phase_increment = static_cast<intptr_t>(speed) << 2;
#endif  // defined(ARDUINO_ARCH_AVR)
  }

  // ---------------------------------------------------------------------------

  // Sync
  bool m_active;
  volatile bool m_sampling;

  // Changeable params
  uint8_t m_finetune;  //  ∈ [0; 15]
  int8_t m_volume;     //  ∈ [0; MAX_VOLUME]

  // Cache
  uint16_t m_cached_period;
  uint8_t m_cached_finetune;

  //
  bool m_loopless;

#if defined(ARDUINO_ARCH_AVR)

  // Optimized for speed.

  // Sample data
  uint16_t m_end;
  uint16_t m_loop_begin;
  uint16_t m_loop_end;

  // State
  math::Int32 m_phase;            // fixed-point 16.16
  math::Int32 m_phase_increment;  // fixed-point 16.16

#else  // defined(ARDUINO_ARCH_AVR)

  // Sample data
  const uint8_t *m_sample_base;

  intptr_t m_end;         // fixed-point X.16
  intptr_t m_loop_begin;  // fixed-point X.16
  intptr_t m_loop_end;    // fixed-point X.16

  // State
  intptr_t m_phase;            // fixed-point X.16
  intptr_t m_phase_increment;  // fixed-point X.16

#endif  // defined(ARDUINO_ARCH_AVR)

  // Output
  int16_t m_sample;  // ∈ [-8192; 8128]
};

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// We fight for every byte of memory on the AVR platform.
////////////////////////////////////////////////////////////////////////////////
#if defined(ARDUINO_ARCH_AVR)

constexpr uint16_t SIZE_OF_SAMPLER = sizeof(Sampler);
constexpr uint16_t SIZE_OF_SAMPLE = sizeof(Sample);

static_assert(SIZE_OF_SAMPLER == 24, "Size of class Sampler was changed!");
static_assert(SIZE_OF_SAMPLE == 10, "Size of struct Sample was changed!");

MOD8_INTERNAL_CONSTEXPR_PRINT(SIZE_OF_SAMPLER);
MOD8_INTERNAL_CONSTEXPR_PRINT(SIZE_OF_SAMPLE);

#endif  // defined(ARDUINO_ARCH_AVR)

// MAX_INCREMENT @ 31250 Hz = 279420 (FP16.16) = 4.264
constexpr uint32_t MAX_INCREMENT = (SPEED_TABLE[MAX_SPEED_INDEX] / format::MIN_PERIOD) << 2U;

// MIN_INCREMENT @ 31250 Hz = 2048 (FP16.16) = 0.031
constexpr uint32_t MIN_INCREMENT = (SPEED_TABLE[MIN_SPEED_INDEX] / format::MAX_PERIOD) << 2U;

MOD8_INTERNAL_CONSTEXPR_PRINT(MAX_INCREMENT);
MOD8_INTERNAL_CONSTEXPR_PRINT(MIN_INCREMENT);

}  // namespace internal

}  // namespace mod8
