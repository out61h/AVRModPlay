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

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <cstdint>
#endif

namespace mod8 {

namespace math {

#if defined(ARDUINO_ARCH_AVR)

// TODO: Ensure target platform has little endianess.
// FIXME: Reading from a union field that was not the last field written to is undefined behaviour!

// -----------------------------------------------------------------------------
union Int32 {
  uint32_t u32;
  int32_t s32;

  struct {
    uint16_t w0;
    uint16_t w1;
  } w32;

  struct {
    uint8_t b0;
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;
  } b32;
};

static_assert(sizeof(Int32) == 4, "Unexpected type size");

// -----------------------------------------------------------------------------
union Int16 {
  int16_t s16;
  uint16_t u16;

  struct {
    uint8_t b0;
    uint8_t b1;
  } b16;
};

static_assert(sizeof(Int16) == 2, "Unexpected type size");

#endif  // defined(ARDUINO_ARCH_AVR)

////////////////////////////////////////////////////////////////////////////////
/// @brief Makes a byte out of two nibbles.
////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t make_byte(uint8_t hi_nibble, uint8_t lo_nibble) {
  return ((hi_nibble & 0xFU) << 4U) | (lo_nibble & 0xFU);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extracts the most significant nibble from a byte.
////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t hi_nibble(uint8_t value) {
  return (value & 0xF0U) >> 4U;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extracts the less significant nibble from a byte.
////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t lo_nibble(uint8_t value) {
  return value & 0xFU;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Makes a word out of two bytes.
////////////////////////////////////////////////////////////////////////////////
constexpr uint16_t make_word(uint8_t hi_byte, uint8_t lo_byte) {
  return ((unsigned)(hi_byte) << 8U) | (unsigned)(lo_byte);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extracts the most significant byte from a word.
////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t hi_byte(uint16_t value) {
  return static_cast<uint8_t>(value >> 8U);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extracts the less significant byte from a word.
////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t lo_byte(uint16_t value) {
  return static_cast<uint8_t>(value & 0xffU);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Converts an unsigned 8-bit integer to a signed one.
/// @note Uses implementation-defined behaviour, but we don't care about that,
///       since we have compile-time checks.
////////////////////////////////////////////////////////////////////////////////
constexpr int8_t u8_to_s8(uint8_t value) {
  return static_cast<int8_t>(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a fixed-point number from an integer and a fractional part.
////////////////////////////////////////////////////////////////////////////////
template<typename T, uint8_t FRACTIONAL_BITS>
constexpr T make_fixp(T integer, T fractional) {
  return static_cast<T>((integer << FRACTIONAL_BITS) | fractional);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a fixed-point number from a simple fraction.
////////////////////////////////////////////////////////////////////////////////
template<typename T, uint8_t FRACTIONAL_BITS>
constexpr T make_fixp_fraction(T numerator, T denominator) {
  return make_fixp<T, FRACTIONAL_BITS>(
    (numerator / denominator),
    (numerator % denominator) * (1UL << FRACTIONAL_BITS) / denominator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Limits the value to the range [min; max].
////////////////////////////////////////////////////////////////////////////////
template<typename T>
constexpr T clamp(const T& value, const T& min, const T& max) {
  return value < min ? value : (value > max ? max : value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Finds the maximum value of two values.
////////////////////////////////////////////////////////////////////////////////
template<typename T>
constexpr T maximum(const T& value1, const T& value2) {
  return value1 > value2 ? value1 : value2;
}

////////////////////////////////////////////////////////////////////////////////
/// @name Unit tests at compile time, huh.
//@{
static_assert(u8_to_s8(0U) == 0, "Test failed: u8_to_s8");
static_assert(u8_to_s8(1U) == 1, "Test failed: u8_to_s8");
static_assert(u8_to_s8(127U) == 127, "Test failed: u8_to_s8");
static_assert(u8_to_s8(255U) == -1, "Test failed: u8_to_s8");
static_assert(u8_to_s8(128U) == -128, "Test failed: u8_to_s8");
static_assert(make_byte(0xAU, 0xBU) == 0xABU, "Test failed: make_byte");
static_assert(hi_nibble(0xABU) == 0xAU, "Test failed: hi_nibble");
static_assert(lo_nibble(0xABU) == 0xBU, "Test failed: lo_nibble");
static_assert(make_word(0xA0U, 0xB0U) == 0xA0B0U, "Test failed: make_word");
static_assert(make_word(0xFFU, 0xFFU) == 0xFFFFU, "Test failed: make_word");
static_assert(hi_byte(0xABCDU) == 0xABU, "Test failed: hi_byte");
static_assert(lo_byte(0xABCDU) == 0xCDU, "Test failed: lo_byte");
static_assert(maximum(5, -3) == 5, "Test failed: maximum");
static_assert(maximum(5, 5) == 5, "Test failed: maximum");
static_assert(clamp(5, -3, 3) == 3, "Test failed: clamp");
static_assert(clamp(0, -3, 3) == 0, "Test failed: clamp");
//@}
////////////////////////////////////////////////////////////////////////////////

}  // namespace math

}  // namespace mod8
