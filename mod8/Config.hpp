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

#include "Math.hpp"

#if defined(ARDUINO)
#include <Arduino.h>
static_assert(sizeof(size_t) == 2, "Unexpected type size");
#else  // defined(ARDUINO)
#include <cstdint>
#include <cstring>
#endif  // defined(ARDUINO)

static_assert(sizeof(long) == sizeof(uint32_t), "Unexpected type size");

////////////////////////////////////////////////////////////////////////////////
// Default configuration
////////////////////////////////////////////////////////////////////////////////
#if !defined(MOD8_PARAM_MIXING_FREQ)
/// @brief Mixing frequency (in Hertz).
/// Default value is phase correct PWM frequency for Atmega clocked by 16 MHz.
#define MOD8_PARAM_MIXING_FREQ (16000000UL / 256UL / 2UL)
#endif

#if !defined(MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2)
/// @brief Binary logarithm of downsamling factor.
/// Values 0 or 1 are OK.
#define MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 0
#endif

#if !defined(MOD8_PARAM_VOLUME_ATTENUATION_LOG2)
/// @brief Binary logarithm of volume attenuation factor.
#define MOD8_PARAM_VOLUME_ATTENUATION_LOG2 0
#endif

#if !defined(MOD8_OPTION_PLAYER_EVENTS)
/// @brief If enabled, the user application must define event callbacks to receive player events.
#define MOD8_OPTION_PLAYER_EVENTS false
#endif

#if !defined(MOD8_OPTION_DOWNSAMPLING_WITH_LERP)
/// @brief Whether to use linear interpolation when downsampling is enabled.
#define MOD8_OPTION_DOWNSAMPLING_WITH_LERP true
#endif

#if !defined(MOD8_OPTION_AMIGA_PERIODS)
/// @brief Whether to clamp period values to Amiga Paula chip limits.
#define MOD8_OPTION_AMIGA_PERIODS false
#endif

#if !defined(MOD8_OPTION_STOP_ON_F00_CMD)
/// @brief If enabled, the F00 command stops playback.
#define MOD8_OPTION_STOP_ON_F00_CMD false
#endif

////////////////////////////////////////////////////////////////////////////////
// Misc. attributes of functions and variables.
////////////////////////////////////////////////////////////////////////////////
#if defined(ARDUINO)
#define MOD8_ATTR_CONST_ARRAY PROGMEM
#define MOD8_ATTR_INLINE __attribute__((always_inline)) inline
#define MOD8_ATTR_UNUSED __attribute__((unused))
#else  // defined(ARDUINO)
#define MOD8_ATTR_CONST_ARRAY
#define MOD8_ATTR_INLINE inline
#define MOD8_ATTR_UNUSED [[maybe_unused]]
#endif  // defined(ARDUINO)

////////////////////////////////////////////////////////////////////////////////
// Stuff for debugging.
////////////////////////////////////////////////////////////////////////////////
/// @brief Enables printing of some information during compilation.
#define MOD8_INTERNAL_ENABLE_CONSTEXPR_PRINT false

#if MOD8_INTERNAL_ENABLE_CONSTEXPR_PRINT
#define MOD8_INTERNAL_CONCAT(x, y) x##y
#define MOD8_INTERNAL_CAT(x, y) MOD8_INTERNAL_CONCAT(x, y)
#define MOD8_INTERNAL_CONSTEXPR_PRINT(x) \
  template<uint32_t> \
  struct MOD8_INTERNAL_CAT(MOD8_INTERNAL_CAT(value_of_, x), _is); \
  static_assert(MOD8_INTERNAL_CAT(MOD8_INTERNAL_CAT(value_of_, x), _is) < x > ::x, "");
#else  // MOD8_INTERNAL_ENABLE_CONSTEXPR_PRINT
#define MOD8_INTERNAL_CONSTEXPR_PRINT(x)
#endif  // MOD8_INTERNAL_ENABLE_CONSTEXPR_PRINT

////////////////////////////////////////////////////////////////////////////////
namespace mod8 {

namespace config {

////////////////////////////////////////////////////////////////////////////////
/// @brief Mixing frequency in Hertz.
constexpr uint16_t MIXING_FREQ = MOD8_PARAM_MIXING_FREQ;
/// @brief Integer binary logarithm of volume attenuation coefficient.
constexpr uint8_t VOLUME_ATTENNUATION_LOG2 = MOD8_PARAM_VOLUME_ATTENUATION_LOG2;
/// @brief Downsapling factor.
constexpr int8_t DOWNSAMPLING_FACTOR = 1 << MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2;

static_assert(DOWNSAMPLING_FACTOR >= 1 && DOWNSAMPLING_FACTOR <= 2, "Unsupported downsampling factor");

/// @brief Amiga Paula chip clock frequency (PAL version, in Hertz).
constexpr uint32_t AMIGA_PAULA_CLOCK_FREQ = 3546894UL;
/// @brief Amiga VBLANK interrupt frequency (PAL version, in Hertz).
constexpr uint16_t AMIGA_VBLANK_INT_FREQ = 50;
/// @brief Player sampling frequence, in Hertz
constexpr uint16_t SAMPLING_FREQ = MIXING_FREQ / DOWNSAMPLING_FACTOR;
/// @brief How many samples will the player read per 1 tick (i.e. Amiga VBLANK interrupt period)
constexpr uint16_t SAMPLES_PER_AMIGA_VBLANK = SAMPLING_FREQ / AMIGA_VBLANK_INT_FREQ;

}  // namespace config

////////////////////////////////////////////////////////////////////////////////
// Utility functions for working with memory.
////////////////////////////////////////////////////////////////////////////////
namespace memory {

#if defined(ARDUINO)

MOD8_ATTR_INLINE uint8_t read_song_byte(const uint8_t *addr) {
  return pgm_read_byte(addr);
}

MOD8_ATTR_INLINE uint8_t read_table_byte(const uint8_t *addr) {
  return pgm_read_byte(addr);
}

MOD8_ATTR_INLINE uint16_t read_table_word(const uint16_t *addr) {
  return pgm_read_word(addr);
}

MOD8_ATTR_INLINE uint32_t read_table_dword(const uint32_t *addr) {
  return pgm_read_dword(addr);
}

#else  // defined(ARDUINO)

MOD8_ATTR_INLINE uint8_t read_song_byte(const uint8_t *addr) {
  return *addr;
}

MOD8_ATTR_INLINE uint8_t read_table_byte(const uint8_t *addr) {
  return *addr;
}

MOD8_ATTR_INLINE uint16_t read_table_word(const uint16_t *addr) {
  return *addr;
}

MOD8_ATTR_INLINE uint32_t read_table_dword(const uint32_t *addr) {
  return *addr;
}

#endif  // defined(ARDUINO)

}  // namespace memory
}  // namespace mod8
