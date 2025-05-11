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
#include "Sampler.hpp"

namespace mod8 {

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// NOTE: Fixed point 0.16
constexpr uint16_t ARPEGGIO_TABLE[] MOD8_ATTR_CONST_ARRAY{
  61857,  // +1 halftone
  58385,  // +2 halftones
  55108,  // +3 halftones
  52015,  // +4 halftones
  49096,  // +5 halftones
  46340,  // +6 halftones
  43740,  // +7 halftones
  41285,  // +8 halftones
  38967,  // +9 halftones
  36780,  // +10 halftones
  34716,  // +11 halftones
  32768,  // +12 halftones
  30928,  // +13 halftones
  29192,  // +14 halftones
  27554   // +15 halftones
};

////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t SINE_TABLE_LENGTH = 32U;
constexpr uint8_t SINE_TABLE_LENGTH_MASK = SINE_TABLE_LENGTH - 1U;
constexpr uint8_t OSC_PERIOD = SINE_TABLE_LENGTH * 2U;

////////////////////////////////////////////////////////////////////////////////
constexpr uint8_t SINE_TABLE[SINE_TABLE_LENGTH] MOD8_ATTR_CONST_ARRAY{
  0,   24,  49,  74,  97,  120, 141, 161, 180, 197, 212,
  224, 235, 244, 250, 253, 255, 253, 250, 244, 235, 224,
  212, 197, 180, 161, 141, 120, 97,  74,  49,  24
};

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
class Channel {
public:
  //////////////////////////////////////////////////////////////////////////////
  Channel() = default;
  ~Channel() = default;
  Channel(const Channel &) = delete;
  Channel &operator=(const Channel &) = delete;
  Channel(Channel &&) = delete;
  Channel &operator=(Channel &&) = delete;

  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE void fetch_sample() /* called from interrupt */ {
    m_sampler.fetch_sample();
  }

  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE const Sampler &sampler() const /* called from interrupt */ {
    return m_sampler;
  }

  //////////////////////////////////////////////////////////////////////////////
  void init() {
    m_sampler.init();
    reset_row();

    m_state = {};
    m_input = {};
  }

  //////////////////////////////////////////////////////////////////////////////
  void reset() {
    m_sampler.reset();
    init();
  }

  //////////////////////////////////////////////////////////////////////////////
  void reset_row() {
    m_row_state = {};
    m_row_effects.reset();
    m_tick_state.actions = ACTION_NONE;
  }

  //////////////////////////////////////////////////////////////////////////////
  void tick() {
    m_tick_state.period = m_state.period;
    m_tick_state.volume = m_state.volume;

    if (m_row_state.tick_counter != 0) {
      internal_update_volume();
      internal_update_note();
      internal_update_period();
    }

    internal_perform_actions();
    ++m_row_state.tick_counter;
    m_tick_state.actions = ACTION_NONE;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param period ∈ [MIN_PERIOD; MAX_PERIOD]
  void set_period(uint16_t period) {
    if (period != 0) {
      if (period > format::MAX_PERIOD) {
        m_input.period = format::MAX_PERIOD;
      } else if (period < format::MIN_PERIOD) {
        m_input.period = format::MIN_PERIOD;
      } else {
        m_input.period = period;
      }

      m_tick_state.actions |= ACTION_RETRIG;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  void set_sample(const Sample *sample) {
    if (sample != nullptr) {
      m_input.sample = sample;
      m_tick_state.actions |= ACTION_LOAD_SAMPLE;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param volume ∈ [0; MAX_VOLUME]
  void set_volume(uint8_t volume) {
    internal_load_sample();

    if (volume > static_cast<uint8_t>(format::MAX_VOLUME)) {
      m_state.volume = format::MAX_VOLUME;
    } else {
      m_state.volume = math::u8_to_s8(volume);
    }

    m_tick_state.actions |= ACTION_UPDATE_VOLUME;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param delta ∈ [0; MAX_VOLUME]
  void inc_volume(uint8_t delta) {
    internal_load_sample();

    if (delta > static_cast<uint8_t>(format::MAX_VOLUME - m_state.volume)) {
      m_state.volume = format::MAX_VOLUME;
    } else {
      m_state.volume += math::u8_to_s8(delta);
    }

    m_tick_state.actions |= ACTION_UPDATE_VOLUME;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param delta ∈ [0; MAX_VOLUME]
  void dec_volume(uint8_t delta) {
    internal_load_sample();

    if (delta > static_cast<uint8_t>(m_state.volume)) {
      m_state.volume = 0;
    } else {
      m_state.volume -= math::u8_to_s8(delta);
    }

    m_tick_state.actions |= ACTION_UPDATE_VOLUME;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param delta ∈ [0; 15]
  void use_volume_inc(uint8_t delta) {
    if (delta != 0) {
      m_row_effects.volume_effect = VOLUME_EFFECT_INC;
      m_row_effects.volume_param = delta;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param delta ∈ [0; 15]
  void use_volume_dec(uint8_t delta) {
    if (delta != 0) {
      m_row_effects.volume_effect = VOLUME_EFFECT_DEC;
      m_row_effects.volume_param = delta;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Use tremolo effect on row.
  /// Parameters and behaviour as in classic Amiga Protracker 7XX effect.
  /// @param speed ∈ [0; 15] tremolo speed
  /// @param depth ∈ [0; 15] tremolo depth
  //////////////////////////////////////////////////////////////////////////////
  void use_volume_tremolo(uint8_t speed, uint8_t depth) {
    if (speed != 0) {
      m_input.tremolo_speed = speed;
    }

    if (depth != 0) {
      m_input.tremolo_depth = depth;
    }

    m_row_effects.volume_effect = VOLUME_EFFECT_TREMOLO;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param delta ∈ [0; 15]
  void inc_period(uint8_t delta) {
    if (m_state.period < format::MAX_PERIOD - delta) {
      m_state.period += delta;
    } else {
      m_state.period = format::MAX_PERIOD;
    }

    m_tick_state.actions |= ACTION_UPDATE_PERIOD;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param delta ∈ [0; 15]
  void dec_period(uint8_t delta) {
    if (m_state.period > format::MIN_PERIOD + delta) {
      m_state.period -= delta;
    } else {
      m_state.period = format::MIN_PERIOD;
    }

    m_tick_state.actions |= ACTION_UPDATE_PERIOD;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param delta ∈ [0; 255]
  void use_period_inc(uint8_t delta) {
    m_row_effects.period_effect = PERIOD_EFFECT_INC;
    m_row_effects.period_param = delta;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param delta ∈ [0; 255]
  void use_period_dec(uint8_t delta) {
    m_row_effects.period_effect = PERIOD_EFFECT_DEC;
    m_row_effects.period_param = delta;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Use portamento-to-note effect on row.
  /// Parameter and behaviour as in classic Amiga Protracker 3XX effect.
  /// @param slide ∈ [0; 255] period increment
  //////////////////////////////////////////////////////////////////////////////
  void use_period_portamento(uint8_t slide) {
    if (slide != 0) {
      m_input.portamento_slide = slide;
    }

    m_row_effects.period_effect = PERIOD_EFFECT_PORTAMENTO;
    m_tick_state.actions &= ~ACTION_RETRIG;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Use vibrato effect on row.
  /// Parameters and behaviour as in classic Amiga Protracker 4XX effect.
  /// @param speed ∈ [0; 15] vibrato speed
  /// @param depth ∈ [0; 15] vibrato depth
  //////////////////////////////////////////////////////////////////////////////
  void use_period_vibrato(uint8_t speed, uint8_t depth) {
    if (speed != 0) {
      m_input.vibrato_speed = speed;
    }

    if (depth != 0) {
      m_input.vibrato_depth = depth;
    }

    m_row_effects.period_effect = PERIOD_EFFECT_VIBRATO;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param delta ∈ [0; 255]
  void set_sample_offset(uint8_t offset) {
    if (offset != 0) {
      m_input.sample_offset = offset;
    }

    m_tick_state.actions |= ACTION_USE_SAMPLE_OFFSET;
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param ticks ∈ [0; 15]
  void use_note_repeat(uint8_t ticks) {
    if (ticks != 0) {
      m_row_effects.note_effect = NOTE_EFFECT_REPEAT;
      m_row_effects.note_param = ticks;
      m_tick_state.actions |= ACTION_RETRIG;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param ticks ∈ [0; 15]
  void use_note_cut(uint8_t ticks) {
    if (ticks != 0) {
      m_row_effects.note_effect = NOTE_EFFECT_CUT;
      m_row_effects.note_param = ticks;
    } else {
      m_state.volume = 0;
      m_tick_state.actions |= ACTION_UPDATE_VOLUME;
      m_row_effects.volume_effect = VOLUME_EFFECT_NONE;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param ticks ∈ [0; 15]
  void use_note_delay(uint8_t ticks) {
    if (ticks != 0) {
      m_row_effects.note_effect = NOTE_EFFECT_DELAY;
      m_row_effects.note_param = ticks;

      m_row_state.delayed_actions = static_cast<uint8_t>(
        m_tick_state.actions & (ACTION_RETRIG | ACTION_LOAD_SAMPLE));
      m_tick_state.actions &= ~(ACTION_RETRIG | ACTION_LOAD_SAMPLE);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // @param note2 ∈ [0; 15]
  // @param note3 ∈ [0; 15]
  void use_arpeggio(uint8_t note2, uint8_t note3) {
    m_row_effects.arpeggio_effect = ARPEGGIO_EFFECT_ARPEGGIO;
    m_row_effects.arpeggio_params[0] = 0;
    m_row_effects.arpeggio_params[1] = note2;
    m_row_effects.arpeggio_params[2] = note3;
  }

private:
  // ---------------------------------------------------------------------------
  void internal_update_volume() {
    using math::u8_to_s8;
    using memory::read_table_byte;

    switch (m_row_effects.volume_effect) {
      case VOLUME_EFFECT_DEC:
        if (u8_to_s8(m_row_effects.volume_param) > m_state.volume) {
          m_state.volume = 0;
        } else {
          m_state.volume -= u8_to_s8(m_row_effects.volume_param);
        }

        m_tick_state.volume = m_state.volume;
        m_tick_state.actions |= ACTION_UPDATE_VOLUME;
        break;

      case VOLUME_EFFECT_INC:
        if (u8_to_s8(m_row_effects.volume_param) > format::MAX_VOLUME - m_state.volume) {
          m_state.volume = format::MAX_VOLUME;
        } else {
          m_state.volume += u8_to_s8(m_row_effects.volume_param);
        }

        m_tick_state.volume = m_state.volume;
        m_tick_state.actions |= ACTION_UPDATE_VOLUME;
        break;

      case VOLUME_EFFECT_TREMOLO:
        {
          // abs value
          const auto index = static_cast<uint8_t>(
            m_state.tremolo_pos & internal::SINE_TABLE_LENGTH_MASK);
          //  [0; 255] x [0; 15] / 64 -> [0; 59]
          const int8_t delta = u8_to_s8(static_cast<uint8_t>(
            read_table_byte(internal::SINE_TABLE + index) * m_input.tremolo_depth / 64));

          if (m_state.tremolo_pos >= 0) {
            if (m_state.volume + delta > format::MAX_VOLUME) {
              m_tick_state.volume = format::MAX_VOLUME;
            } else {
              m_tick_state.volume = m_state.volume + delta;
            }
          } else {
            if (m_state.volume - delta < 0) {
              m_tick_state.volume = 0;
            } else {
              m_tick_state.volume = m_state.volume - delta;
            }
          }

          m_tick_state.actions |= ACTION_UPDATE_VOLUME;

          m_state.tremolo_pos += m_input.tremolo_speed;

          if (m_state.tremolo_pos >= internal::SINE_TABLE_LENGTH) {
            m_state.tremolo_pos -= internal::OSC_PERIOD;
          }
        }

        break;

      case VOLUME_EFFECT_NONE: break;
    }
  }

  // ---------------------------------------------------------------------------
  void internal_update_note() {
    switch (m_row_effects.note_effect) {
      case NOTE_EFFECT_CUT:
        if (m_row_state.tick_counter == m_row_effects.note_param) {
          m_state.volume = 0;
          m_tick_state.volume = 0;
          m_tick_state.actions |= ACTION_UPDATE_VOLUME;
          m_row_effects.reset();
        }

        break;

      case NOTE_EFFECT_DELAY:
        if (m_row_state.tick_counter == m_row_effects.note_param) {
          m_tick_state.actions |= m_row_state.delayed_actions;
          m_row_effects.reset();
        }

        break;

      case NOTE_EFFECT_REPEAT:
        if (m_row_state.tick_counter % m_row_effects.note_param == 0) {
          m_tick_state.actions |= ACTION_RETRIG;
        }
        break;

      case NOTE_EFFECT_NONE: break;
    }
  }

  // ---------------------------------------------------------------------------
  void internal_update_period() {
    using memory::read_table_byte;

    switch (m_row_effects.period_effect) {
      case PERIOD_EFFECT_PORTAMENTO:
        if (m_input.period != 0) {
          if (m_state.period > m_input.period) {
            if (m_state.period >= m_input.portamento_slide) {
              m_state.period -= m_input.portamento_slide;
            } else {
              m_state.period = 0;
            }

            if (m_state.period < m_input.period) {
              m_state.period = m_input.period;
            }

          } else if (m_state.period < m_input.period) {
            if (m_state.period < format::MAX_PERIOD) {
              m_state.period += m_input.portamento_slide;
            } else {
              m_state.period = format::MAX_PERIOD;
            }

            if (m_state.period > m_input.period) {
              m_state.period = m_input.period;
            }
          }

          m_tick_state.period = m_state.period;
          m_tick_state.actions |= ACTION_UPDATE_PERIOD;
        }

        break;

      case PERIOD_EFFECT_DEC:
        if (m_state.period >= m_row_effects.period_param) {
          m_state.period -= m_row_effects.period_param;
        } else {
          m_state.period = 0;
        }

        if (m_state.period < format::MIN_PERIOD) {
          m_state.period = format::MIN_PERIOD;
        }

        m_tick_state.period = m_state.period;
        m_tick_state.actions |= ACTION_UPDATE_PERIOD;
        break;

      case PERIOD_EFFECT_INC:
        if (m_state.period < format::MAX_PERIOD) {
          m_state.period += m_row_effects.period_param;
        } else {
          m_state.period = format::MAX_PERIOD;
        }

        m_tick_state.period = m_state.period;
        m_tick_state.actions |= ACTION_UPDATE_PERIOD;
        break;

      case PERIOD_EFFECT_VIBRATO:
        {
          // abs value
          const auto index = static_cast<uint8_t>(
            m_state.vibrato_pos & internal::SINE_TABLE_LENGTH_MASK);
          //  [0; 255] x [0; 15] / 128 -> [0; 29]
          const auto delta = static_cast<uint16_t>(
            read_table_byte(internal::SINE_TABLE + index) * m_input.vibrato_depth / 128);

          if (m_state.vibrato_pos >= 0) {
            m_tick_state.period = static_cast<uint16_t>(m_state.period + delta);
          } else {
            m_tick_state.period = static_cast<uint16_t>(m_state.period - delta);
          }

          m_tick_state.actions |= ACTION_UPDATE_PERIOD;

          m_state.vibrato_pos += m_input.vibrato_speed;

          if (m_state.vibrato_pos >= internal::SINE_TABLE_LENGTH) {
            m_state.vibrato_pos -= internal::OSC_PERIOD;
          }
        }

        break;

      case PERIOD_EFFECT_NONE: break;
    }

    if (m_row_effects.arpeggio_effect == ARPEGGIO_EFFECT_ARPEGGIO) {
      m_tick_state.actions |= ACTION_UPDATE_PERIOD;
      m_tick_state.actions |= ACTION_USE_ARPEGGIO;
    }
  }

  // ---------------------------------------------------------------------------
  void internal_load_sample() {
    if (m_tick_state.actions & ACTION_LOAD_SAMPLE) {
      m_state.sample = m_input.sample;
      m_state.volume = m_input.sample->volume;
      m_tick_state.volume = m_state.volume;
      m_tick_state.actions &= ~ACTION_LOAD_SAMPLE;
      m_tick_state.actions |= ACTION_UPDATE_VOLUME;
    }
  }

  // ---------------------------------------------------------------------------
  void internal_perform_actions() {
    using memory::read_table_word;

    internal_load_sample();

    if (m_tick_state.actions & ACTION_RETRIG) {
      m_state.period = m_input.period;
      m_state.vibrato_pos = 0;
      m_state.tremolo_pos = 0;

      if (m_tick_state.actions & ACTION_USE_SAMPLE_OFFSET) {
        m_sampler.retrig(
          m_state.sample, m_state.period, m_input.sample_offset, m_state.volume);
      } else {
        m_sampler.retrig(m_state.sample, m_state.period, 0, m_state.volume);
      }
    } else {
      if (m_tick_state.actions & ACTION_UPDATE_VOLUME) {
        m_sampler.set_volume(m_tick_state.volume);
      }

      if (m_tick_state.actions & ACTION_UPDATE_PERIOD) {
        if (m_tick_state.actions & ACTION_USE_ARPEGGIO) {
          const uint8_t arpeggio_shift = m_row_effects
                                           .arpeggio_params[m_row_state.tick_counter % format::ARPEGGIO_PERIOD];

          if (arpeggio_shift) {
            const uint16_t multiplier = read_table_word(
              internal::ARPEGGIO_TABLE + arpeggio_shift - 1U);
            const auto arpeggio_period = static_cast<uint32_t>(m_tick_state.period) * multiplier;
            m_tick_state.period = arpeggio_period >> 16U;
          }
        }

        if (m_tick_state.period < format::MIN_PERIOD) {
          m_tick_state.period = format::MIN_PERIOD;
        } else if (m_tick_state.period > format::MAX_PERIOD) {
          m_tick_state.period = format::MAX_PERIOD;
        }

        m_sampler.set_period(m_tick_state.period);
      }
    }
  }

  // ---------------------------------------------------------------------------
  //
  // ---------------------------------------------------------------------------
  // state
  Sampler m_sampler;

  enum Action : uint8_t {
    ACTION_NONE = 0U,
    ACTION_UPDATE_VOLUME = 1U << 0U,
    ACTION_UPDATE_PERIOD = 1U << 1U,
    ACTION_USE_SAMPLE_OFFSET = 1U << 2U,
    ACTION_RETRIG = 1U << 3U,
    ACTION_USE_ARPEGGIO = 1U << 4U,
    ACTION_LOAD_SAMPLE = 1U << 5U
  };

  struct {
    uint8_t actions;  // ∈ {Action}
    uint16_t period;  // ∈ [MIN_PERIOD; MAX_PERIOD]
    int8_t volume;    // ∈ [0; MAX_VOLUME]
  } m_tick_state;

  struct {
    uint8_t tick_counter;     // ∈ [0; MAX_TICKS_PER_ROW]
    uint8_t delayed_actions;  // ∈ {Action}
  } m_row_state;

  enum ArpeggioEffect : uint8_t {
    ARPEGGIO_EFFECT_NONE = 0U,
    ARPEGGIO_EFFECT_ARPEGGIO = 1U
  };

  enum VolumeEffect : uint8_t {
    VOLUME_EFFECT_NONE = 0U,
    VOLUME_EFFECT_INC = 1U,
    VOLUME_EFFECT_DEC = 2U,
    VOLUME_EFFECT_TREMOLO = 3U
  };

  enum PeriodEffect : uint8_t {
    PERIOD_EFFECT_NONE = 0U,
    PERIOD_EFFECT_INC = 1U,
    PERIOD_EFFECT_DEC = 2U,
    PERIOD_EFFECT_PORTAMENTO = 3U,
    PERIOD_EFFECT_VIBRATO = 4U
  };

  enum NoteEffect : uint8_t {
    NOTE_EFFECT_NONE = 0U,
    NOTE_EFFECT_REPEAT = 1U,
    NOTE_EFFECT_CUT = 2U,
    NOTE_EFFECT_DELAY = 3U
  };

  struct {
    ArpeggioEffect arpeggio_effect;                    // ∈ {ArpeggioEffect}
    uint8_t arpeggio_params[format::ARPEGGIO_PERIOD];  // ∈ [0; 15]
    VolumeEffect volume_effect;                        // ∈ {VolumeEffect}
    uint8_t volume_param;                              // ∈ [0; 15]
    PeriodEffect period_effect;                        // ∈ {PeriodEffect}
    uint8_t period_param;                              // ∈ [0; 255]
    NoteEffect note_effect;                            // ∈ {NoteEffect}
    uint8_t note_param;                                // ∈ [0; 15]

    void reset() {
      arpeggio_effect = ARPEGGIO_EFFECT_NONE;
      note_effect = NOTE_EFFECT_NONE;
      period_effect = PERIOD_EFFECT_NONE;
      volume_effect = VOLUME_EFFECT_NONE;
    }
  } m_row_effects;

  struct {
    const Sample *sample;  //
    uint16_t period;       // ∈ [MIN_PERIOD; MAX_PERIOD]
    int8_t volume;         // ∈ [0; MAX_VOLUME]
    int8_t vibrato_pos;    // ∈ [-32; 31]
    int8_t tremolo_pos;    // ∈ [-32; 31]
  } m_state;

  struct {
    const Sample *sample;
    uint16_t period;           // ∈ [MIN_PERIOD; MAX_PERIOD]
    uint8_t portamento_slide;  // ∈ [0; 255]
    uint8_t vibrato_speed;     // ∈ [0; 15]
    uint8_t vibrato_depth;     // ∈ [0; 15]
    uint8_t tremolo_speed;     // ∈ [0; 15]
    uint8_t tremolo_depth;     // ∈ [0; 15]
    uint8_t sample_offset;     // ∈ [0; 255]
  } m_input;
};

}  // namespace mod8
