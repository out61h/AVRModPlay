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

namespace mod8 {

namespace format {

////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t NUM_ORDERS = 128;
constexpr uint8_t NUM_CHANNELS = 4;
constexpr uint8_t NUM_FINETUNES = 16;
constexpr uint8_t NUM_ROWS = 64;
constexpr uint8_t NUM_SAMPLES = 31;

////////////////////////////////////////////////////////////////////////////////
constexpr int8_t MAX_VOLUME = 64;
constexpr uint8_t MAX_FINETUNE = 15;
constexpr uint8_t MAX_TICKS_PER_ROW = 31;

////////////////////////////////////////////////////////////////////////////////
#if MOD8_OPTION_AMIGA_PERIODS
constexpr uint16_t MIN_PERIOD = 113;
constexpr uint16_t MAX_PERIOD = 856;
#else
constexpr uint16_t MIN_PERIOD = 28 * config::DOWNSAMPLING_FACTOR;
constexpr uint16_t MAX_PERIOD = 3424;
#endif

////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t INITIAL_BPM = 125;
constexpr uint8_t INITIAL_SPEED = 6;

////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t ARPEGGIO_PERIOD = 3;

////////////////////////////////////////////////////////////////////////////////
struct Sample {
  uint8_t name[22];
  uint8_t length_hi;
  uint8_t length_lo;
  uint8_t finetune;
  uint8_t volume;
  uint8_t loop_start_hi;
  uint8_t loop_start_lo;
  uint8_t loop_length_hi;
  uint8_t loop_length_lo;
};

static_assert(sizeof(Sample) == 30, "Unexpected sample header struct size");

////////////////////////////////////////////////////////////////////////////////
struct Song {
  uint8_t name[20];
  Sample samples[NUM_SAMPLES];
  // Number of song positions (ie. number of patterns played
  // throughout the song). Legal values are 1..128.
  uint8_t length;
  // Historically set to 127, but can be safely ignored.
  // Noisetracker uses this byte to indicate restart position -
  // this has been made redundant by the 'Position Jump' effect.
  uint8_t loop;
  uint8_t orders[NUM_ORDERS];
  uint8_t format_tag[4];
};

static_assert(sizeof(char) == sizeof(uint8_t), "Unexpected char type size");
static_assert(sizeof(Song) == 1084, "Unexpected song header struct size");

////////////////////////////////////////////////////////////////////////////////
struct Cell {
  uint8_t byte0;
  uint8_t byte1;
  uint8_t byte2;
  uint8_t byte3;
};

static_assert(sizeof(Cell) == 4, "Unexpected cell struct size");

////////////////////////////////////////////////////////////////////////////////
struct Row {
  Cell notes[NUM_CHANNELS];
};

////////////////////////////////////////////////////////////////////////////////
struct Pattern {
  Row rows[NUM_ROWS];
};

static_assert(sizeof(Pattern) == 1024, "Unexpected pattern data struct size");

}  // namespace format
}  // namespace mod8
