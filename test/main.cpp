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
#define MOD8_OPTION_PLAYER_EVENTS true
#define MOD8_PARAM_MIXING_FREQ 48000
#include <AVRModPlay.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

//------------------------------------------------------------------------------
std::vector<uint8_t> g_song;
mod8::Player g_player;

//------------------------------------------------------------------------------
const std::string RULER_THICK(62, '=');
const std::string RULER_THIN(62, '-');

//------------------------------------------------------------------------------
void print_dec(uint16_t n, int digits, char zero) {
  if (n == 0) {
    for (uint8_t i = 0; i != digits; ++i)
      printf("%c", zero);
    return;
  }

  printf("%0*d", digits, n);
}

// clang-format off

//------------------------------------------------------------------------------
struct WavHeader {
  uint8_t riff_chunk_id[4];
  uint8_t riff_chunk_size[4];
  uint8_t riff_format[4];
  uint8_t fmt_chunk_id[4];
  uint8_t fmt_chunk_size[4];
  uint8_t fmt_codec[2];
  uint8_t fmt_num_channels[2];
  uint8_t fmt_sample_rate[4];
  uint8_t fmt_byte_rate[4];
  uint8_t fmt_block_align[2];
  uint8_t fmt_bits_per_sample[2];
  uint8_t data_chunk_id[4];
  uint8_t data_chunk_size[4];
} wav_header = {
  'R', 'I', 'F', 'F',                     // $00: riff_chunk_id
  0x00, 0x00, 0x00, 0x00,                 // $04: riff_chunk_size
  'W', 'A', 'V', 'E',                     // $08: riff_format
  
  // format subchunk
  'f', 'm', 't', ' ',                     // $0C: fmt_chunk_id
  0x10, 0x00, 0x00, 0x00,                 // $10: fmt_chunk_size (16 for PCM)
  0x01, 0x00,                             // $14: fmt_codec (1 for PCM)
  0x02, 0x00,                             // $16: fmt_num_channels (2 for stereo)
  0x00, 0x00, 0x00, 0x00,                 // $18: fmt_sample_rate
  0x00, 0x00, 0x01, 0x00,                 // $1C: fmt_byte_rate
  0x04, 0x00,                             // $20: fmt_block_align (4 for 16-bit stereo)
  0x10, 0x00,                             // $22: fmt_bits_per_sample (16)
  
  // data subchunk
  'd', 'a', 't', 'a',                     // $24: data_chunk_id
  0x00, 0x00, 0x00, 0x00                  // $28: data_chunk_size
};

// clang-format on

static_assert(sizeof(WavHeader) == 44);

//------------------------------------------------------------------------------
constexpr void u32_to_le(uint32_t value, uint8_t (&output)[4]) {
  output[0] = (value >> 0x00U) & 0xFFU;
  output[1] = (value >> 0x08U) & 0xFFU;
  output[2] = (value >> 0x10U) & 0xFFU;
  output[3] = (value >> 0x18U) & 0xFFU;
}

//------------------------------------------------------------------------------
constexpr void u16_to_le(uint16_t value, uint8_t (&output)[2]) {
  output[0] = (value >> 0x00U) & 0xFFU;
  output[1] = (value >> 0x08U) & 0xFFU;
}

}

////////////////////////////////////////////////////////////////////////////////
#if MOD8_OPTION_PLAYER_EVENTS

namespace mod8 {
namespace events {

//------------------------------------------------------------------------------
void on_song_load_error(const Song &song) {
  printf("-ERROR-\n");
  printf("%s\n", reinterpret_cast<const char *>(song.name));
}

//------------------------------------------------------------------------------
void on_song_load(const Song &song) {
  printf("%s\n", RULER_THICK.c_str());
  printf("MIXF: %i [Hz]\n", mod8::config::MIXING_FREQ);

  printf("%s\n", RULER_THIN.c_str());
  printf("SONG: %s\n", reinterpret_cast<const char *>(song.name));
  printf("%s\n", RULER_THIN.c_str());
  printf("ORDS: %d\n", song.order_count);
  printf("PATS: %d\n", song.pattern_count);
  printf("FMTG: %c%c%c%c\n", song.tag[0], song.tag[1], song.tag[2], song.tag[3]);
}

//------------------------------------------------------------------------------
void on_sample_load(uint8_t sample_no, const Sample &sample) {
  printf("%s\n", RULER_THIN.c_str());
  printf("SMPL: #%02d\n", sample_no);
  printf("%s\n", RULER_THIN.c_str());
  printf("ADDR: $%04llX\n", sample.begin - g_song.data());
  printf("LNGT: $%04llX\n", sample.end - sample.begin);
  printf("FNTN: $%01X\n", sample.finetune);
  printf("VOLM: $%02X\n", sample.volume);
  printf("LPST: $%04llX\n", sample.loop_begin - sample.begin);
  printf("LPLN: $%04llX\n", sample.loop_end - sample.loop_begin);
}

//------------------------------------------------------------------------------
void on_play_pattern(uint8_t, uint8_t pattern) {
  printf("%s\n", RULER_THIN.c_str());
  printf("PTRN #%d\n", pattern);
  printf("%s\n", RULER_THIN.c_str());
}

//------------------------------------------------------------------------------
void on_play_row_begin(uint8_t row) {
  printf("%02d ", row);
}

//------------------------------------------------------------------------------
void on_play_row_end() {
  printf("\n");
}

//------------------------------------------------------------------------------
void on_play_note(uint8_t, uint16_t period, uint8_t sample, uint8_t effect, uint8_t param) {
  printf("| ");
  print_dec(period, 5, '.');
  printf(" ");
  print_dec(sample, 2, '.');
  printf(" ");

  if (effect || param) {
    printf("%01X%02X ", effect, param);
  } else {
    printf("... ");
  }
}

//------------------------------------------------------------------------------
void on_play_song_end(const Song &) {
  printf("%s\n", RULER_THICK.c_str());
}

//------------------------------------------------------------------------------
void on_message(bool condition, uint8_t count, ...) {
  if (!condition)
    return;

  va_list ap;
  va_start(ap, count);

  for (uint8_t i = 0; i != count; ++i) {
    fprintf(stderr, "%02X", va_arg(ap, int));
    if (count - 1 != i)
      fprintf(stderr, ":");
  }

  fprintf(stderr, "\n");
  va_end(ap);
}

}  // namespace events
}  // namespace mod8

#endif  // MOD8_OPTION_PLAYER_EVENTS

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
  using FilePtr = std::unique_ptr<FILE, decltype(&fclose)>;

  //----------------------------------------------------------------------------
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <file.mod>\n", argv[0]);
    return EXIT_FAILURE;
  }

  //----------------------------------------------------------------------------
  {
    FILE *file = nullptr;
    fopen_s(&file, argv[1], "rb");
    if (file == nullptr) {
      fprintf(stderr, "Unable to open file: %s\n", argv[1]);
      return EXIT_FAILURE;
    }

    FilePtr file_scope(file, &fclose);

    if (fseek(file, 0, SEEK_END) == 0) {
      const long file_size = ftell(file);
      if (file_size >= 0) {
        const size_t song_size = static_cast<size_t>(file_size);
        g_song.resize(song_size);

        rewind(file);
        if (fread(g_song.data(), 1, song_size, file) != song_size) {
          fprintf(stderr, "Unable to read %zu bytes from file: %s\n", song_size, argv[1]);
          return EXIT_FAILURE;
        }
      }
    } else {
      fprintf(stderr, "Unable to seek file: %s\n", argv[1]);
      return EXIT_FAILURE;
    }
  }

  //----------------------------------------------------------------------------
  if (g_song.empty()) {
    fprintf(stderr, "File is empty: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  //----------------------------------------------------------------------------
  g_player.init();
  if (!g_player.load(g_song.data(), g_song.size())) {
    fprintf(stderr, "Parse error: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  //----------------------------------------------------------------------------
  const std::string output_file_name = std::string(argv[1]) + ".wav";
  FILE *output_file = nullptr;
  fopen_s(&output_file, output_file_name.c_str(), "wb");
  if (output_file == nullptr) {
    fprintf(
      stderr, "Unable to open output file for writing: %s\n", output_file_name.c_str());
    return EXIT_FAILURE;
  }

  FilePtr output_file_scope(output_file, &fclose);

  if (fwrite(&wav_header, sizeof(wav_header), 1, output_file) != 1) {
    fprintf(stderr, "Unable to write WAV header to file: %s\n", output_file_name.c_str());
    return EXIT_FAILURE;
  }

  //----------------------------------------------------------------------------
  size_t data_size = 0;

  while (g_player.update() != mod8::Player::UpdateResult::INACTIVE) {
    g_player.tick();

    const int16_t sample_l = g_player.output_left_s16();
    const int16_t sample_r = g_player.output_right_s16();

    uint8_t out_l[2];
    u16_to_le(static_cast<uint16_t>(sample_l), out_l);
    uint8_t out_r[2];
    u16_to_le(static_cast<uint16_t>(sample_r), out_r);

    if (fwrite(&out_l[0], sizeof(out_l), 1, output_file) != 1) {
      fprintf(stderr, "Unable to write WAV data to file: %s\n", output_file_name.c_str());
      return EXIT_FAILURE;
    }

    if (fwrite(&out_r[0], sizeof(out_r), 1, output_file) != 1) {
      fprintf(stderr, "Unable to write WAV data to file: %s\n", output_file_name.c_str());
      return EXIT_FAILURE;
    }

    data_size += sizeof(out_l);
    data_size += sizeof(out_r);
  }

  //----------------------------------------------------------------------------
  constexpr size_t BLOCK_SIZE = 2 * sizeof(int16_t);
  constexpr size_t RIFF_HEADER_SIZE = sizeof(WavHeader::riff_chunk_id)
                                    + sizeof(WavHeader::riff_chunk_size);
  u32_to_le(mod8::config::MIXING_FREQ, wav_header.fmt_sample_rate);
  u32_to_le(mod8::config::MIXING_FREQ * BLOCK_SIZE, wav_header.fmt_byte_rate);
  u32_to_le(static_cast<uint32_t>(data_size + sizeof(WavHeader) - RIFF_HEADER_SIZE),
            wav_header.data_chunk_size);
  u32_to_le(static_cast<uint32_t>(data_size), wav_header.data_chunk_size);

  rewind(output_file);
  if (fwrite(&wav_header, sizeof(wav_header), 1, output_file) != 1) {
    fprintf(stderr, "Unable to write WAV header to file: %s\n", output_file_name.c_str());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
