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

////////////////////////////////////////////////////////////////////////////////
/// @brief
////////////////////////////////////////////////////////////////////////////////
class Timer {
public:
  //////////////////////////////////////////////////////////////////////////////
  Timer() = default;
  ~Timer() = default;
  Timer(const Timer &) = delete;
  Timer &operator=(const Timer &) = delete;
  Timer(Timer &&) = delete;
  Timer &operator=(Timer &&) = delete;

  //////////////////////////////////////////////////////////////////////////////
  void reset(uint16_t period) {
    m_new_period = m_period = m_counter = period;
    m_load_new_period = false;
    m_fire_counter = 0;
    m_fire_counter_last = 0;
  }

  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE uint16_t get_period() const {
    return m_new_period;
  }

  //////////////////////////////////////////////////////////////////////////////
  void set_period(uint16_t new_period) {
    while (m_load_new_period) {}

    m_new_period = new_period;
    m_load_new_period = true;
  }

  //////////////////////////////////////////////////////////////////////////////
  ///
  //////////////////////////////////////////////////////////////////////////////
  // ■■■■■■■■■■■■■■■■
  // ■ 18/43 clocks ■
  // ■■■■■■■■■■■■■■■■
  MOD8_ATTR_INLINE void clock() /* called from interrupt */ {
    // ■■■■■■■■■■■■■■■
    // ■ 5/18 clocks ■
    // ■■■■■■■■■■■■■■■
    if (m_load_new_period) {
      m_period = m_counter = m_new_period;
      m_load_new_period = false;
    }

    // ■■■■■■■■■■■■■■■■
    // ■ 13/25 clocks ■
    // ■■■■■■■■■■■■■■■■
    if (--m_counter == 0) {
      m_counter = m_period;
      m_fire_counter++;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  bool is_fired() {
    const uint8_t counter = m_fire_counter;

    if (counter == m_fire_counter_last) {
      return false;
    }

    m_fire_counter_last = counter;
    return true;
  }

private:
  uint16_t m_counter /* written and read from interrupt */;
  uint16_t m_period /* written and read from interrupt */;
  uint16_t m_new_period;
  volatile bool m_load_new_period /* written and read from interrupt */;
  volatile uint8_t m_fire_counter /* written and read from interrupt */;
  uint8_t m_fire_counter_last;
};

}  // namespace mod8
