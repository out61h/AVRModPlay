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

#include "Channel.hpp"
#include "Format.hpp"
#include "Math.hpp"
#include "Timer.hpp"

namespace mod8 {

////////////////////////////////////////////////////////////////////////////////
struct Song {
  uint8_t name[32];
  uint8_t tag[4];
  uint8_t order_count;
  uint8_t pattern_count;
};

////////////////////////////////////////////////////////////////////////////////
namespace events {

////////////////////////////////////////////////////////////////////////////////
enum class Message : uint8_t {
  UNSUPPORTED_FORMAT = 0x1U,
  UNSUPPORTED_EFFECT = 0x2U,
  OUT_OF_RANGE_SAMPLE_BOUNDARIES = 0x3U,
  OUT_OF_RANGE_SAMPLE_FINETUNE = 0x4U,
  OUT_OF_RANGE_SAMPLE_VOLUME = 0x5U,
  OUT_OF_RANGE_SAMPLE_LOOP_LENGTH = 0x6U,
  OUT_OF_RANGE_SAMPLE = 0x7U,
  OUT_OF_RANGE_PERIOD = 0x8U,
  OUT_OF_RANGE_PATTERN = 0x9U,
  OUT_OF_RANGE_EFFECT_PARAM = 0xAU,
  SONG_SIZE_TOO_BIG = 0xBU
};

////////////////////////////////////////////////////////////////////////////////
void on_song_load(const mod8::Song &song);

////////////////////////////////////////////////////////////////////////////////
void on_song_load_error(const mod8::Song &song);

////////////////////////////////////////////////////////////////////////////////
void on_sample_load(uint8_t sample_no, const mod8::Sample &sample);

////////////////////////////////////////////////////////////////////////////////
void on_play_pattern(uint8_t song_position, uint8_t pattern);

////////////////////////////////////////////////////////////////////////////////
void on_play_row_begin(uint8_t row);

////////////////////////////////////////////////////////////////////////////////
void on_play_note(
  uint8_t channel, uint16_t period, uint8_t sample, uint8_t effect, uint8_t param);

////////////////////////////////////////////////////////////////////////////////
void on_play_row_end();

////////////////////////////////////////////////////////////////////////////////
void on_play_song_end(const Song &song);

////////////////////////////////////////////////////////////////////////////////
void on_message(bool condition, uint8_t count, ...);

#if !MOD8_OPTION_PLAYER_EVENTS
void on_song_load(const mod8::Song & /*song*/) {}
void on_song_load_error(const mod8::Song & /*song*/) {}
void on_sample_load(uint8_t /*sample_no*/, const mod8::Sample & /*sample*/) {}
void on_play_pattern(uint8_t /*song_position*/, uint8_t /*pattern*/) {}
void on_play_row_begin(uint8_t /*row*/) {}
void on_play_note(
  uint8_t /*channel*/, uint16_t /*period*/, uint8_t /*sample*/, uint8_t /*effect*/, uint8_t /*param*/) {
}
void on_play_row_end() {}
void on_play_song_end(const Song & /*song*/) {}
void on_message(bool /*condition*/, uint8_t /*count*/, ...) {}
#endif

}  // namespace events

////////////////////////////////////////////////////////////////////////////////
/// @brief Player for Amiga Protracker MOD tunes.
/// Limitations:
/// - MOD file size limited to 64KiB.
////////////////////////////////////////////////////////////////////////////////
class Player {
public:
  //////////////////////////////////////////////////////////////////////////////
  /// Remove all C++ boilerplate to:
  /// - Keep the class trivially default constructible.
  /// - Avoid creating static initializers for static instances.
  /// - Avoid copying in tiny MCU's RAM.
  /// @note Call init() method before use the class.
  //////////////////////////////////////////////////////////////////////////////
  Player() = default;
  ~Player() = default;
  Player(const Player &) = delete;
  Player &operator=(const Player &) = delete;
  Player(Player &&) = delete;
  Player &operator=(Player &&) = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize.
  /// Initialize minimum possible subset of vars.
  /// Must be called once before starting to work with the player.
  //////////////////////////////////////////////////////////////////////////////
  void init() {
    for (Channel &channel : m_channels) {
      channel.init();
    }

    m_playing = false;
  }

  //////////////////////////////////////////////////////////////////////////////
  struct Stats {
    uint8_t max_bpm;
    uint32_t playback_duration;
  };

  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE const Stats &get_stats() const {
    return m_stats;
  }

  //////////////////////////////////////////////////////////////////////////////
  bool load(const uint8_t *data, size_t size) {
    using events::Message;
    using events::on_sample_load;
    using events::on_song_load;
    using events::on_song_load_error;
    using events::on_message;
    using memory::read_song_byte;
    using math::make_word;
    using math::clamp;
    using math::u8_to_s8;

    m_playing = false;

    // ---------------------------- Cleanup ------------------------------------
    for (auto &channel : m_channels) {
      channel.reset();
    }

    for (auto &state : m_pattern_state) {
      state.reset();
    }

    m_output_left = m_output_right = { 0 };
#if MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 > 0
    m_mixing_counter = config::DOWNSAMPLING_FACTOR;
#if MOD8_OPTION_DOWNSAMPLING_WITH_LERP
    m_slope_left = m_slope_right = 0;
#endif
#endif

    memset(&m_song_info, 0, sizeof(m_song_info));
    memset(&m_samples[0], 0, sizeof(m_samples));

    // ----------------------------- Parse -------------------------------------
    m_song_data = reinterpret_cast<const format::Song *>(data);

    uint8_t *dst = &m_song_info.name[0];
    for (const auto &byte : m_song_data->name) {
      *dst++ = read_song_byte(&byte);
    }

    dst = &m_song_info.tag[0];
    for (const auto &byte : m_song_data->format_tag) {
      *dst++ = read_song_byte(&byte);
    }

    const uint8_t SUPPORTED_TAGS[][4] = {
      { 'M', '.', 'K', '.' }, { '4', 'C', 'H', 'N' }, { 'F', 'L', 'T', '4' }
    };

    bool supported = false;

    for (const auto &tag : SUPPORTED_TAGS) {
      // NOTE: Lots of hardcode, but the binary code for AVR is more compact.
      if (m_song_info.tag[0] == tag[0] && m_song_info.tag[1] == tag[1]
          && m_song_info.tag[2] == tag[2] && m_song_info.tag[3] == tag[3]) {
        supported = true;
        break;
      }
    }

    if (!supported) {
      on_song_load_error(m_song_info);
      on_message(true,
                 5,
                 (int)Message::UNSUPPORTED_FORMAT,
                 (int)m_song_info.tag[0],
                 (int)m_song_info.tag[1],
                 (int)m_song_info.tag[2],
                 (int)m_song_info.tag[3]);
      return false;
    }

#if !defined(ARDUINO_ARCH_AVR)
    if (size > 65535) {
      on_song_load_error(m_song_info);
      on_message(true, 1, (int)Message::SONG_SIZE_TOO_BIG);
      return false;
    }
#endif  // !defined(ARDUINO_ARCH_AVR)

    // ---------------------------- Patterns -----------------------------------
    m_song_info.order_count = read_song_byte(&m_song_data->length);
    const auto *patterns = reinterpret_cast<const format::Pattern *>(m_song_data + 1);
    {
      uint8_t pattern_count = 0;

      // Some trackers leave unused patterns in song, so we have to scan all orders to find them.
      for (const uint8_t &order : m_song_data->orders) {
        const uint8_t patten_index = read_song_byte(&order);
        if (patten_index > pattern_count) {
          pattern_count = patten_index;
        }
      }

      m_song_info.pattern_count = pattern_count + 1U;
    }

    on_song_load(m_song_info);

    // ---------------------------- Samples ------------------------------------
    const uint8_t *const data_end = data + size;
    const auto *sample_data = reinterpret_cast<const uint8_t *>(
      patterns + m_song_info.pattern_count);
    const format::Sample *sample_header = &m_song_data->samples[0];

    for (uint8_t i = 0;
         i != sizeof(format::Song::samples) / sizeof(format::Song::samples[0]);
         ++i) {
      Sample &sample = m_samples[i];

      const uint8_t byte_b1 = read_song_byte(&sample_header->length_lo);
      const uint8_t byte_b2 = read_song_byte(&sample_header->length_hi);
      const uint16_t length = make_word(byte_b2, byte_b1) * 2U;
      const uint8_t *const sample_end = sample_data + length;

      // some songs may have empty samples outside file boundaries
      if (length > 2U && sample_end <= data_end) {
        sample.begin = sample_data;
        sample.end = sample.begin + length;

        // ........................... Finetune ................................
        uint8_t finetune = read_song_byte(&sample_header->finetune);
        on_message(finetune > format::MAX_FINETUNE,
                   3,
                   (int)Message::OUT_OF_RANGE_SAMPLE_FINETUNE,
                   (int)(i + 1),
                   (int)finetune);
        finetune = clamp<uint8_t>(finetune, 0U, format::MAX_FINETUNE);
        sample.finetune = finetune;

        // ............................ Volume .................................
        uint8_t volume = read_song_byte(&sample_header->volume);
        on_message(volume > format::MAX_VOLUME,
                   3,
                   (int)Message::OUT_OF_RANGE_SAMPLE_VOLUME,
                   (int)(i + 1),
                   (int)volume);
        volume = clamp<uint8_t>(volume, 0U, format::MAX_VOLUME);
        sample.volume = u8_to_s8(volume);

        // .......................... Loop start ...............................
        const uint8_t byte_c1 = read_song_byte(&sample_header->loop_start_lo);
        const uint8_t byte_c2 = read_song_byte(&sample_header->loop_start_hi);
        const uint16_t loop_start = make_word(byte_c2, byte_c1) * 2U;
        sample.loop_begin = sample.begin + loop_start;

        if (sample.loop_begin > data_end) {
          on_message(
            true, 3, (int)Message::OUT_OF_RANGE_SAMPLE_BOUNDARIES, (int)(i + 1), 2);
          return false;
        }

        // ......................... Loop length ...............................
        const uint8_t byte_d1 = read_song_byte(&sample_header->loop_length_lo);
        const uint8_t byte_d2 = read_song_byte(&sample_header->loop_length_hi);
        const uint16_t loop_length = make_word(byte_d2, byte_d1) * 2U;
        sample.loop_end = sample.loop_begin + loop_length;

        if (sample.loop_end > data_end) {
          on_message(
            true, 3, (int)Message::OUT_OF_RANGE_SAMPLE_BOUNDARIES, (int)(i + 1), 3);
          return false;
        }

        if (loop_length < internal::MIN_LOOP_LENGTH && loop_start != 0) {
          on_message(true,
                     4,
                     (int)Message::OUT_OF_RANGE_SAMPLE_LOOP_LENGTH,
                     (int)(i + 1),
                     (int)loop_length,
                     (int)internal::MIN_LOOP_LENGTH);
          return false;
        }

        sample_data = sample.end;
        on_sample_load(i + 1U, sample);
      } else {
        on_message(
          length > 2, 3, (int)Message::OUT_OF_RANGE_SAMPLE_BOUNDARIES, (int)(i + 1), 1);

        sample.end = sample.begin = sample_data;
        sample.loop_end = sample.loop_begin = sample_data;

        uint8_t volume = read_song_byte(&sample_header->volume);
        on_message(volume > format::MAX_VOLUME,
                   3,
                   (int)Message::OUT_OF_RANGE_SAMPLE_VOLUME,
                   (int)(i + 1),
                   (int)volume);
        volume = clamp<uint8_t>(volume, 0U, format::MAX_VOLUME);
        sample.volume = u8_to_s8(volume);

        if (sample.volume != 0) {
          on_sample_load(i + 1U, sample);
        }
      }

      ++sample_header;
    }

    // ----------------------------- State -------------------------------------
    m_song_state = {};
    m_song_state.ticks_per_row = format::INITIAL_SPEED;
    m_song_state.mode = Mode::PLAY_SONG_ONCE;

    m_row_state = {};
    m_row_actions = {};

    m_stats = {};
    m_stats.max_bpm = format::INITIAL_BPM;

    m_tick_timer.reset(config::SAMPLES_PER_AMIGA_VBLANK);

    // ----------------------------- Initial -----------------------------------
    // TODO: Do from update().
    fetch_pattern();
    fetch_row();

    m_playing = true;
    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  // ~~~ 54 + 344 = 388 cloks ~~~
  void tick() /* called from interrupt */ {
    // ■■■■■■■■■■■■■
    // ■  5 clocks ■
    // ■■■■■■■■■■■■■
    if (!m_playing) {
      return;
    }

    // 28 clocks
#if MOD8_OPTION_DOWNSAMPLING_WITH_LERP && MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 > 0
    m_output_left.s16 += m_slope_left;
    m_output_right.s16 += m_slope_right;
#endif

    // TODO: Mix only used channels
#if MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 > 0
#if MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 == 1

    // -- 5 clocks --
    if (m_mixing_counter & 1) {
      // ~~ 200 clocks ~~
      m_channels[0].fetch_sample();
      m_channels[3].fetch_sample();
    } else {
      // ~~ 200 clocks ~~
      m_channels[1].fetch_sample();
      m_channels[2].fetch_sample();
    }
#else   // MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 == 1
    m_channels[m_mixing_counter - 1].fetch_sample();
#endif  // MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 == 1

    // -- 5 clocks --
    if (--m_mixing_counter != 0) {
      return;
    }

    // -- 4 clocks --
    m_mixing_counter = config::DOWNSAMPLING_FACTOR;
#else   // MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 > 0
    // ~~~~ 344 clocks ~~~~
    m_channels[0].fetch_sample();
    m_channels[1].fetch_sample();
    m_channels[2].fetch_sample();
    m_channels[3].fetch_sample();
#endif  // MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 > 0

    // ■■■■■■■■■■■■■
    // ■ 20 clocks ■
    // ■■■■■■■■■■■■■
    // NOTE: For the sake of economy, the adjustment of the degree of separation
    //       of the right and left channels 0-100% is implemented in hardware.
    // Range [-16384; 16256]
    const int16_t new_left = m_channels[0].sampler().get_sample()
                           + m_channels[3].sampler().get_sample();

    const int16_t new_right = m_channels[1].sampler().get_sample()
                            + m_channels[2].sampler().get_sample();


#if MOD8_OPTION_DOWNSAMPLING_WITH_LERP && MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 > 0
    // -- 36 clocks --
    // TODO: Avoid implementation-defined behaviour
    // Range [-32640; 32640]
    m_slope_left = (new_left - m_output_left.s16) / config::DOWNSAMPLING_FACTOR;
    m_slope_right = (new_right - m_output_right.s16) / config::DOWNSAMPLING_FACTOR;
#else
    // ■■■■■■■■■■■■■
    // ■ 12 clocks ■
    // ■■■■■■■■■■■■■
    // Range : [-32768; 32512]
    // TODO: Avoid implementation-defined behaviour
    // TODO: Shape with 1/2 LSB noise to avoid hearing of carrier frequency on low sampling rates?
    m_output_left = { static_cast<int16_t>(new_left * 2) };
    m_output_right = { static_cast<int16_t>(new_right * 2) };
#endif
    // ■■■■■■■■■■■■■■■■
    // ■ 18/43 clocks ■
    // ■■■■■■■■■■■■■■■■
    m_tick_timer.clock();
  }

  //////////////////////////////////////////////////////////////////////////////
  enum class UpdateResult : uint8_t {
    INACTIVE,
    IDLE,
    TICK
  };

  UpdateResult update() {
    if (!m_playing) {
      return UpdateResult::INACTIVE;
    }

    if (!m_tick_timer.is_fired()) {
      return UpdateResult::IDLE;
    }

    m_stats.playback_duration += static_cast<uint32_t>(m_tick_timer.get_period())
                               * config::DOWNSAMPLING_FACTOR;

    if (++m_row_state.tick >= m_song_state.ticks_per_row) {
      m_row_state.tick = 0;

      if (m_row_state.delay != 0) {
        m_row_state.delay--;
      } else {
        if (!internal_fetch_next_row()) {
          stop();
          return UpdateResult::TICK;
        }
      }
    }

    for (Channel &channel : m_channels) {
      channel.tick();
    }

    return UpdateResult::TICK;
  }

  //////////////////////////////////////////////////////////////////////////////
  void stop() {
    for (Channel &channel : m_channels) {
      channel.reset();
    }

    m_playing = false;

    events::on_play_song_end(m_song_info);
  }

  //////////////////////////////////////////////////////////////////////////////
  enum class Mode : uint8_t {
    PLAY_SONG_ONCE,
    LOOP_SONG_ONCE,
    LOOP_SONG,
    LOOP_PATTERN
  };

  //////////////////////////////////////////////////////////////////////////////
  void set_mode(Mode mode) {
    m_song_state.mode = mode;
  }

  //////////////////////////////////////////////////////////////////////////////
#if defined(ARDUINO_ARCH_AVR)
  MOD8_ATTR_INLINE uint8_t output_left_u8() const {
    return m_output_left.b16.b1;
  }

  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE uint8_t output_right_u8() const {
    return m_output_right.b16.b1;
  }

#else  // defined(ARDUINO_ARCH_AVR)

  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE int16_t output_left_s16() const {
    return m_output_left;
  }

  //////////////////////////////////////////////////////////////////////////////
  MOD8_ATTR_INLINE int16_t output_right_s16() const {
    return m_output_right;
  }

#endif  // defined(ARDUINO_ARCH_AVR)

private:
  //////////////////////////////////////////////////////////////////////////////
  bool internal_fetch_next_row() {
    // TODO: Simplify the code.
    if (m_row_actions.actions & ACTION_STOP) {
      return false;
    }

    if (m_row_actions.actions & ACTION_JUMP_TO_ROW) {
      m_song_state.row = m_row_actions.jump_to_row;
    } else if (++m_song_state.row == format::NUM_ROWS
               || (m_row_actions.actions & (ACTION_PATTERN_BREAK | ACTION_JUMP_TO_ORDER))) {
      if (m_song_state.mode != Mode::LOOP_PATTERN) {
        if (m_row_actions.actions & ACTION_JUMP_TO_ORDER) {
          if (m_row_actions.jump_to_order <= m_song_state.order) {
            if (m_song_state.mode == Mode::PLAY_SONG_ONCE) {
              return false;
            }

            if (m_song_state.mode == Mode::LOOP_SONG_ONCE
                && m_song_state.loop_counter++ == 1) {
              return false;
            }
          } else if (m_row_actions.jump_to_order >= m_song_info.order_count) {
            return false;
          }

          m_song_state.order = m_row_actions.jump_to_order;
        } else if (++m_song_state.order == m_song_info.order_count) {
          m_song_state.order = 0;
          if (m_song_state.mode != Mode::LOOP_SONG) {
            return false;
          }
        }
      }

      for (auto &state : m_pattern_state) {
        state.reset();
      }

      if (m_row_actions.actions & ACTION_PATTERN_BREAK) {
        if (m_row_actions.jump_to_row >= format::NUM_ROWS) {
          return false;
        }

        m_song_state.row = m_row_actions.jump_to_row;
      } else {
        m_song_state.row = 0;
      }

      fetch_pattern();
    }

    m_row_actions.actions = 0;
    fetch_row();
    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  void fetch_pattern() {
    using events::on_message;
    using events::on_play_pattern;
    using events::Message;
    using memory::read_song_byte;

    const uint8_t pattern = read_song_byte(&m_song_data->orders[m_song_state.order]);

    // TODO: Stop or panic
    on_message(pattern >= m_song_info.pattern_count, 1, (int)Message::OUT_OF_RANGE_PATTERN);

    const auto *patterns = reinterpret_cast<const format::Pattern *>(m_song_data + 1);

    m_pattern_data = reinterpret_cast<const format::Row *>(&patterns[pattern]);

    on_play_pattern(m_song_state.order, pattern);
  }

  //////////////////////////////////////////////////////////////////////////////
  void fetch_row() {
    using events::on_message;
    using events::on_play_note;
    using events::on_play_row_begin;
    using events::on_play_row_end;
    using events::Message;
    using math::hi_nibble;
    using math::lo_nibble;
    using memory::read_song_byte;

    on_play_row_begin(m_song_state.row);

    const auto *notes = reinterpret_cast<const uint8_t *>(
      m_pattern_data + m_song_state.row);
    for (uint8_t i = 0; i != format::NUM_CHANNELS; ++i) {
      /*
        _____byte 1_____   byte2_    _____byte 3_____   byte4_
        /                \ /      \  /                \ /      \
        0000          0000-00000000  0000          0000-00000000

        Upper four    12 bits for    Lower four    Effect command.
        bits of sam-  note period.   bits of sam-
        ple number.                  ple number. 
      */
      const uint8_t byte1 = read_song_byte(notes++);
      const uint8_t byte2 = read_song_byte(notes++);
      const uint8_t byte3 = read_song_byte(notes++);
      const uint8_t byte4 = read_song_byte(notes++);

      const uint8_t sample = (byte1 & 0xf0U) | (byte3 >> 4U);
      const auto period = static_cast<uint16_t>(((byte1 & 0xfU) << 8U) | byte2);
      const uint8_t effect = byte3 & 0xfU;
      const uint8_t param = byte4;

      on_play_note(i, period, sample, effect, param);

      Channel &channel = m_channels[i];

      channel.reset_row();

      if (sample == 0) {
        channel.set_sample(nullptr);
      } else if (sample <= format::NUM_SAMPLES) {
        channel.set_sample(&m_samples[sample - 1]);
      } else {
        on_message(true, 1, (int)Message::OUT_OF_RANGE_SAMPLE);
      }

      on_message(period && (period < format::MIN_PERIOD || period > format::MAX_PERIOD),
                 1,
                 (int)Message::OUT_OF_RANGE_PERIOD);
      channel.set_period(period);

      switch (effect) {
        case 0x0:  // Normal play or Arpeggio
          if (param) {
            channel.use_arpeggio(hi_nibble(param), lo_nibble(param));
          }
          break;

        case 0x1:  // Porta Up
          channel.use_period_dec(param);
          break;

        case 0x2:  // Porta Down
          channel.use_period_inc(param);
          break;

        case 0x3:  // Porta To Note
          channel.use_period_portamento(param);
          break;

        case 0x4:  // Vibrato
          channel.use_period_vibrato(hi_nibble(param), lo_nibble(param));
          break;

        case 0x5:  // Porta + Volume Slide
          channel.use_volume_dec(lo_nibble(param));
          channel.use_volume_inc(hi_nibble(param));
          channel.use_period_portamento(0);
          break;

        case 0x6:  // Vibrato + Volume Slide
          channel.use_volume_dec(lo_nibble(param));
          channel.use_volume_inc(hi_nibble(param));
          channel.use_period_vibrato(0, 0);
          break;

        case 0x7:  // Tremolo
          channel.use_volume_tremolo(hi_nibble(param), lo_nibble(param));
          break;

        case 0x9:  // Sample Offset
          channel.set_sample_offset(param);
          break;

        case 0xA:  // Volume Slide
          channel.use_volume_dec(lo_nibble(param));
          channel.use_volume_inc(hi_nibble(param));
          break;

        case 0xB:
          // B - Position Jump                       Bxx : songposition
          //
          // Causes playback to jump to pattern position xx.
          // B00 would restart a song from the beginning (first pattern in the Order List).
          // If Dxx is on the same row, the pattern specified by Bxx will be the pattern Dxx jumps in.
          // Ranges from 00h to 7Fh (127; maximum amount of patterns for the MOD format).
          on_message(param >= m_song_info.order_count, 1, (int)Message::OUT_OF_RANGE_EFFECT_PARAM);
          m_row_actions.actions |= ACTION_JUMP_TO_ORDER;
          m_row_actions.jump_to_order = param;
          break;

        case 0xC:  // Set Volume
          channel.set_volume(param);
          break;

        case 0xD:
          // D - Pattern Break                       Dxy : break position in next patt
          //
          // This effect is equivalent to a position jump to the next pattern in the
          // pattern table, with the arguments x*10+y specifying the line within
          // that pattern to start playing at. Note that this is NOT x*16+y.
          {
            const uint8_t param_x = hi_nibble(param);
            const uint8_t param_y = lo_nibble(param);
            const uint8_t pos = param_x * 10U + param_y;

            on_message(pos >= format::NUM_ROWS,
                       3,
                       (int)Message::OUT_OF_RANGE_EFFECT_PARAM,
                       (int)effect,
                       (int)param);

            m_row_actions.actions |= ACTION_PATTERN_BREAK;
            m_row_actions.jump_to_row = pos;
            break;
          }

        case 0xE:
          {
            const uint8_t ext_effect = param & 0xf0U;
            const uint8_t ext_param = lo_nibble(param);

            switch (ext_effect) {
              case 0x10:  // Fine Portamento Up
                channel.dec_period(ext_param);
                break;

              case 0x20:  // Fine Portamento Down
                channel.inc_period(ext_param);
                break;

              case 0x60:
                // E6 - Loop                           E60 : Set loop point
                //                                     E6x : jump to loop, play x times
                //
                // This effect allows a section of a pattern to be 'looped', or played
                // through, a certain number of times in succession. If the effect argument
                // yyyy is zero, the effect specifies the loop's start point. Otherwise, it
                // specifies the number of times to play this line and the preceeding lines
                // from the start point. If no start point was specified in the current
                // pattern being played, the loop start defaults to the first line in the
                // pattern. Therefore, you cannot loop through multiple patterns.
                {
                  auto &state = m_pattern_state[i];

                  if (!ext_param) {
                    state.loop_start_row = m_song_state.row;
                  } else {
                    if (!state.loop_counter) {
                      state.loop_counter = ext_param;
                      m_row_actions.actions |= ACTION_JUMP_TO_ROW;
                      m_row_actions.jump_to_row = state.loop_start_row;
                    } else {
                      if (--state.loop_counter) {
                        m_row_actions.actions |= ACTION_JUMP_TO_ROW;
                        m_row_actions.jump_to_row = state.loop_start_row;
                      }
                    }
                  }
                }

                break;

              case 0x90:  // Retrig Note
                channel.use_note_repeat(ext_param);
                break;

              case 0xA0:  // Fine Volume Slide Up
                channel.inc_volume(ext_param);
                break;

              case 0xB0:  // Fine Volume Slide Down
                channel.dec_volume(ext_param);
                break;

              case 0xC0:  // Cut Note
                channel.use_note_cut(ext_param);
                break;

              case 0xD0:  // Delay Note
                channel.use_note_delay(ext_param);
                break;

              case 0xE0:  // Pattern Delay
                m_row_state.delay = ext_param;
                break;

              case 0x00:  // Set Filter
              case 0x30:  // Glissando Control
              case 0x40:  // Set Vibrato Waveform
              case 0x50:  // Set Finetune
              case 0x70:  // Set Tremolo Waveform
              case 0x80:  // Set Panning
              case 0xF0:  // Invert Loop
              default:
                on_message(true, 3, (int)Message::UNSUPPORTED_EFFECT, (int)effect, (int)param);
                break;
            }

            break;
          }

        case 0xF:  // Set Speed
          if (param == 0) {
#if MOD8_OPTION_STOP_ON_F00_CMD
            m_row_actions.actions |= ACTION_STOP;
#endif
          } else if (param <= format::MAX_TICKS_PER_ROW) {
            m_song_state.ticks_per_row = param;
          } else {
            // BPM - Beats Per Minute.
            // LPM - Lines Per Minute.
            // TPS - Ticks Per Second.

            // LPM = BPM * 4
            // LPS = LPM / 60 = BPM * 4 / 60
            // TPS = BPM * (4 / 60) * DEFAULT_SPEED = BPM * (4 / 60) * 6
            // TPS = BPM * (24 / 60) = BPM * (4 / 10) = BPM * 2 / 5

            // Default BPM = 125
            // Default TPS = 125 * 2 / 5 = 50 ~ VBLANK

            // TIMER_PERIOD = SAMPLING_FREQ / TPS
            // TIMER_PERIOD = SAMPLING_FREQ * 5 / (2 * param)
            // TIMER_PERIOD ∈ [306; 3906]
            m_stats.max_bpm = math::maximum(m_stats.max_bpm, param);

            const auto tick_period = static_cast<uint16_t>(
              5UL * config::SAMPLING_FREQ / param / 2U);
            m_tick_timer.set_period(tick_period);
          }

          break;

        case 0x8:  // Panning
        default:
          on_message(true, 3, (int)Message::UNSUPPORTED_EFFECT, (int)effect, (int)param);
          break;
      }
    }

    on_play_row_end();
  }

  //////////////////////////////////////////////////////////////////////////////
  volatile bool m_playing;

  //////////////////////////////////////////////////////////////////////////////
#if defined(ARDUINO_ARCH_AVR)
  math::Int16 m_output_left;
  math::Int16 m_output_right;
#else   // defined(ARDUINO_ARCH_AVR)
  int16_t m_output_left;
  int16_t m_output_right;
#endif  // defined(ARDUINO_ARCH_AVR)

// TODO: Extract class Mixer?
#if MOD8_PARAM_DOWNSAMPLING_FACTOR_LOG2 > 0
  uint8_t m_mixing_counter;
#if MOD8_OPTION_DOWNSAMPLING_WITH_LERP
  int16_t m_slope_left;
  int16_t m_slope_right;
#endif
#endif

  Timer m_tick_timer;

  //////////////////////////////////////////////////////////////////////////////
  Song m_song_info;                          //
  Sample m_samples[format::NUM_SAMPLES];     //
  Channel m_channels[format::NUM_CHANNELS];  //
                                             //
  const format::Song *m_song_data;           // NOTE: PROGMEM
  const format::Row *m_pattern_data;         // NOTE: PROGMEM

  //////////////////////////////////////////////////////////////////////////////
  struct {
    Mode mode;              // ∈ {Mode}
    uint8_t loop_counter;   // ∈ [0; 1]
    uint8_t order;          // ∈ [0; NUM_ORDERS)
    uint8_t row;            // ∈ [0; NUM_ROWS)
    uint8_t ticks_per_row;  // ∈ [1; MAX_TICKS_PER_ROW]
  } m_song_state;

  struct {
    uint8_t loop_start_row;  // ∈ [0; NUM_ROWS)
    uint8_t loop_counter;    // ∈ [0; 15]

    MOD8_ATTR_INLINE void reset() {
      loop_start_row = 0;
      loop_counter = 0;
    }
  } m_pattern_state[format::NUM_CHANNELS];

  //////////////////////////////////////////////////////////////////////////////
  struct {
    uint8_t tick;   // ∈ [0; MAX_TICKS_PER_ROW]
    uint8_t delay;  // ∈ [0; 15]
  } m_row_state;

  enum Action : uint8_t {
    ACTION_NONE = 0U,
    ACTION_JUMP_TO_ROW = 1U,
    ACTION_STOP = 2U,
    ACTION_JUMP_TO_ORDER = 4U,
    ACTION_PATTERN_BREAK = 8U,
  };

  struct {
    uint8_t actions;        // ∈ {Action}
    uint8_t jump_to_order;  // ∈ [0; NUM_ORDERS)
    uint8_t jump_to_row;    // ∈ [0; NUM_ROWS)
  } m_row_actions;

  //////////////////////////////////////////////////////////////////////////////
  Stats m_stats;
};

namespace internal {

#if defined(ARDUINO_ARCH_AVR)

constexpr uint16_t SIZE_OF_CHANNEL = sizeof(Channel);
MOD8_INTERNAL_CONSTEXPR_PRINT(SIZE_OF_CHANNEL);
static_assert(SIZE_OF_CHANNEL == 57, "Size of class Channel was changed!");

#endif  // defined(ARDUINO_ARCH_AVR)

}  // namespace internal

}  // namespace mod8
