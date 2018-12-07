/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include <limits>

#include "common_audio/wav_header.h"
#include "test/gtest.h"

namespace webrtc {

// Doesn't take ownership of the buffer.
class ReadableWavBuffer : public ReadableWav {
 public:
  ReadableWavBuffer(const uint8_t* buf, size_t size, bool check_read_size)
      : buf_(buf),
        size_(size),
        pos_(0),
        buf_exhausted_(false),
        check_read_size_(check_read_size) {}

  ~ReadableWavBuffer() override {
    // Verify the entire buffer has been read.
    if (check_read_size_)
      EXPECT_EQ(size_, pos_);
  }

  size_t Read(void* buf, size_t num_bytes) override {
    // Verify we don't try to read outside of a properly sized header.
    if (size_ >= kWavHeaderSize)
      EXPECT_GE(size_, pos_ + num_bytes);
    EXPECT_FALSE(buf_exhausted_);

    const size_t bytes_remaining = size_ - pos_;
    if (num_bytes > bytes_remaining) {
      // The caller is signalled about an exhausted buffer when we return fewer
      // bytes than requested. There should not be another read attempt after
      // this point.
      buf_exhausted_ = true;
      num_bytes = bytes_remaining;
    }
    memcpy(buf, &buf_[pos_], num_bytes);
    pos_ += num_bytes;
    return num_bytes;
  }

  bool Eof() const override { return pos_ == size_; }

  bool SeekForward(uint32_t num_bytes) override {
    // Verify we don't try to read outside of a properly sized header.
    if (size_ >= kWavHeaderSize)
      EXPECT_GE(size_, pos_ + num_bytes);
    EXPECT_FALSE(buf_exhausted_);

    const size_t bytes_remaining = size_ - pos_;
    if (num_bytes > bytes_remaining) {
      // Error: cannot seek beyond EOF.
      return false;
    }
    if (num_bytes == bytes_remaining) {
      // There should not be another read attempt after this point.
      buf_exhausted_ = true;
    }
    pos_ += num_bytes;
    return true;
  }

 private:
  const uint8_t* buf_;
  const size_t size_;
  size_t pos_;
  bool buf_exhausted_;
  const bool check_read_size_;
};

// Try various choices of WAV header parameters, and make sure that the good
// ones are accepted and the bad ones rejected.
TEST(WavHeaderTest, CheckWavParameters) {
  // Try some really stupid values for one parameter at a time.
  EXPECT_TRUE(CheckWavParameters(1, 8000, kWavFormatPcm, 1, 0));
  EXPECT_FALSE(CheckWavParameters(0, 8000, kWavFormatPcm, 1, 0));
  EXPECT_FALSE(CheckWavParameters(0x10000, 8000, kWavFormatPcm, 1, 0));
  EXPECT_FALSE(CheckWavParameters(1, 0, kWavFormatPcm, 1, 0));
  EXPECT_FALSE(CheckWavParameters(1, 8000, WavFormat(0), 1, 0));
  EXPECT_FALSE(CheckWavParameters(1, 8000, kWavFormatPcm, 0, 0));

  // Try invalid format/bytes-per-sample combinations.
  EXPECT_TRUE(CheckWavParameters(1, 8000, kWavFormatPcm, 2, 0));
  EXPECT_FALSE(CheckWavParameters(1, 8000, kWavFormatPcm, 4, 0));
  EXPECT_FALSE(CheckWavParameters(1, 8000, kWavFormatALaw, 2, 0));
  EXPECT_FALSE(CheckWavParameters(1, 8000, kWavFormatMuLaw, 2, 0));

  // Too large values.
  EXPECT_FALSE(CheckWavParameters(1 << 20, 1 << 20, kWavFormatPcm, 1, 0));
  EXPECT_FALSE(CheckWavParameters(1, 8000, kWavFormatPcm, 1,
                                  std::numeric_limits<uint32_t>::max()));

  // Not the same number of samples for each channel.
  EXPECT_FALSE(CheckWavParameters(3, 8000, kWavFormatPcm, 1, 5));
}

TEST(WavHeaderTest, ReadWavHeaderWithErrors) {
  size_t num_channels = 0;
  int sample_rate = 0;
  WavFormat format = kWavFormatPcm;
  size_t bytes_per_sample = 0;
  size_t num_samples = 0;

  // Test a few ways the header can be invalid. We start with the valid header
  // used in WriteAndReadWavHeader, and invalidate one field per test. The
  // invalid field is indicated in the array name, and in the comments with
  // *BAD*.
  {
    constexpr uint8_t kBadRiffID[] = {
        // clang-format off
        // clang formatting doesn't respect inline comments.
      'R', 'i', 'f', 'f',  // *BAD*
      0xbd, 0xd0, 0x5b, 0x07,  // size of whole file - 8: 123457689 + 44 - 8
      'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ',
      16, 0, 0, 0,  // size of fmt block - 8: 24 - 8
      6, 0,  // format: A-law (6)
      17, 0,  // channels: 17
      0x39, 0x30, 0, 0,  // sample rate: 12345
      0xc9, 0x33, 0x03, 0,  // byte rate: 1 * 17 * 12345
      17, 0,  // block align: NumChannels * BytesPerSample
      8, 0,  // bits per sample: 1 * 8
      'd', 'a', 't', 'a',
      0x99, 0xd0, 0x5b, 0x07,  // size of payload: 123457689
        // clang-format on
    };
    ReadableWavBuffer r(kBadRiffID, sizeof(kBadRiffID),
                        /*check_read_size=*/false);
    EXPECT_FALSE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                               &bytes_per_sample, &num_samples));
  }
  {
    constexpr uint8_t kBadBitsPerSample[] = {
        // clang-format off
        // clang formatting doesn't respect inline comments.
      'R', 'I', 'F', 'F',
      0xbd, 0xd0, 0x5b, 0x07,  // size of whole file - 8: 123457689 + 44 - 8
      'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ',
      16, 0, 0, 0,  // size of fmt block - 8: 24 - 8
      6, 0,  // format: A-law (6)
      17, 0,  // channels: 17
      0x39, 0x30, 0, 0,  // sample rate: 12345
      0xc9, 0x33, 0x03, 0,  // byte rate: 1 * 17 * 12345
      17, 0,  // block align: NumChannels * BytesPerSample
      1, 0,  // bits per sample: *BAD*
      'd', 'a', 't', 'a',
      0x99, 0xd0, 0x5b, 0x07,  // size of payload: 123457689
        // clang-format on
    };
    ReadableWavBuffer r(kBadBitsPerSample, sizeof(kBadBitsPerSample),
                        /*check_read_size=*/true);
    EXPECT_FALSE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                               &bytes_per_sample, &num_samples));
  }
  {
    constexpr uint8_t kBadByteRate[] = {
        // clang-format off
        // clang formatting doesn't respect inline comments.
      'R', 'I', 'F', 'F',
      0xbd, 0xd0, 0x5b, 0x07,  // size of whole file - 8: 123457689 + 44 - 8
      'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ',
      16, 0, 0, 0,  // size of fmt block - 8: 24 - 8
      6, 0,  // format: A-law (6)
      17, 0,  // channels: 17
      0x39, 0x30, 0, 0,  // sample rate: 12345
      0x00, 0x33, 0x03, 0,  // byte rate: *BAD*
      17, 0,  // block align: NumChannels * BytesPerSample
      8, 0,  // bits per sample: 1 * 8
      'd', 'a', 't', 'a',
      0x99, 0xd0, 0x5b, 0x07,  // size of payload: 123457689
        // clang-format on
    };
    ReadableWavBuffer r(kBadByteRate, sizeof(kBadByteRate),
                        /*check_read_size=*/true);
    EXPECT_FALSE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                               &bytes_per_sample, &num_samples));
  }
  {
    constexpr uint8_t kBadFmtHeaderSize[] = {
        // clang-format off
        // clang formatting doesn't respect inline comments.
      'R', 'I', 'F', 'F',
      0xbd, 0xd0, 0x5b, 0x07,  // size of whole file - 8: 123457689 + 44 - 8
      'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ',
      17, 0, 0, 0,  // size of fmt block *BAD*. Only 16 and 18 permitted.
      6, 0,  // format: A-law (6)
      17, 0,  // channels: 17
      0x39, 0x30, 0, 0,  // sample rate: 12345
      0xc9, 0x33, 0x03, 0,  // byte rate: 1 * 17 * 12345
      17, 0,  // block align: NumChannels * BytesPerSample
      8, 0,  // bits per sample: 1 * 8
      0,  // extra (though invalid) header byte
      'd', 'a', 't', 'a',
      0x99, 0xd0, 0x5b, 0x07,  // size of payload: 123457689
        // clang-format on
    };
    ReadableWavBuffer r(kBadFmtHeaderSize, sizeof(kBadFmtHeaderSize),
                        /*check_read_size=*/false);
    EXPECT_FALSE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                               &bytes_per_sample, &num_samples));
  }
  {
    constexpr uint8_t kNonZeroExtensionField[] = {
        // clang-format off
        // clang formatting doesn't respect inline comments.
      'R', 'I', 'F', 'F',
      0xbd, 0xd0, 0x5b, 0x07,  // size of whole file - 8: 123457689 + 44 - 8
      'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ',
      18, 0, 0, 0,  // size of fmt block - 8: 24 - 8
      6, 0,  // format: A-law (6)
      17, 0,  // channels: 17
      0x39, 0x30, 0, 0,  // sample rate: 12345
      0xc9, 0x33, 0x03, 0,  // byte rate: 1 * 17 * 12345
      17, 0,  // block align: NumChannels * BytesPerSample
      8, 0,  // bits per sample: 1 * 8
      1, 0,  // non-zero extension field *BAD*
      'd', 'a', 't', 'a',
      0x99, 0xd0, 0x5b, 0x07,  // size of payload: 123457689
        // clang-format on
    };
    ReadableWavBuffer r(kNonZeroExtensionField, sizeof(kNonZeroExtensionField),
                        /*check_read_size=*/false);
    EXPECT_FALSE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                               &bytes_per_sample, &num_samples));
  }
  {
    constexpr uint8_t kMissingDataChunk[] = {
        // clang-format off
        // clang formatting doesn't respect inline comments.
      'R', 'I', 'F', 'F',
      0xbd, 0xd0, 0x5b, 0x07,  // size of whole file - 8: 123457689 + 44 - 8
      'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ',
      16, 0, 0, 0,  // size of fmt block - 8: 24 - 8
      6, 0,  // format: A-law (6)
      17, 0,  // channels: 17
      0x39, 0x30, 0, 0,  // sample rate: 12345
      0xc9, 0x33, 0x03, 0,  // byte rate: 1 * 17 * 12345
      17, 0,  // block align: NumChannels * BytesPerSample
      8, 0,  // bits per sample: 1 * 8
        // clang-format on
    };
    ReadableWavBuffer r(kMissingDataChunk, sizeof(kMissingDataChunk),
                        /*check_read_size=*/true);
    EXPECT_FALSE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                               &bytes_per_sample, &num_samples));
  }
  {
    constexpr uint8_t kMissingFmtAndDataChunks[] = {
        // clang-format off
        // clang formatting doesn't respect inline comments.
      'R', 'I', 'F', 'F',
      0xbd, 0xd0, 0x5b, 0x07,  // size of whole file - 8: 123457689 + 44 - 8
      'W', 'A', 'V', 'E',
        // clang-format on
    };
    ReadableWavBuffer r(kMissingFmtAndDataChunks,
                        sizeof(kMissingFmtAndDataChunks),
                        /*check_read_size=*/true);
    EXPECT_FALSE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                               &bytes_per_sample, &num_samples));
  }
}

// Try writing and reading a valid WAV header and make sure it looks OK.
TEST(WavHeaderTest, WriteAndReadWavHeader) {
  constexpr int kSize = 4 + kWavHeaderSize + 4;
  uint8_t buf[kSize];
  memset(buf, 0xa4, sizeof(buf));
  WriteWavHeader(buf + 4, 17, 12345, kWavFormatALaw, 1, 123457689);
  constexpr uint8_t kExpectedBuf[] = {
      // clang-format off
      // clang formatting doesn't respect inline comments.
    0xa4, 0xa4, 0xa4, 0xa4,  // untouched bytes before header
    'R', 'I', 'F', 'F',
    0xbd, 0xd0, 0x5b, 0x07,  // size of whole file - 8: 123457689 + 44 - 8
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    16, 0, 0, 0,  // size of fmt block - 8: 24 - 8
    6, 0,  // format: A-law (6)
    17, 0,  // channels: 17
    0x39, 0x30, 0, 0,  // sample rate: 12345
    0xc9, 0x33, 0x03, 0,  // byte rate: 1 * 17 * 12345
    17, 0,  // block align: NumChannels * BytesPerSample
    8, 0,  // bits per sample: 1 * 8
    'd', 'a', 't', 'a',
    0x99, 0xd0, 0x5b, 0x07,  // size of payload: 123457689
    0xa4, 0xa4, 0xa4, 0xa4,  // untouched bytes after header
      // clang-format on
  };
  static_assert(sizeof(kExpectedBuf) == kSize, "buffer size");
  EXPECT_EQ(0, memcmp(kExpectedBuf, buf, kSize));

  size_t num_channels = 0;
  int sample_rate = 0;
  WavFormat format = kWavFormatPcm;
  size_t bytes_per_sample = 0;
  size_t num_samples = 0;
  ReadableWavBuffer r(buf + 4, sizeof(buf) - 8,
                      /*check_read_size=*/true);
  EXPECT_TRUE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                            &bytes_per_sample, &num_samples));
  EXPECT_EQ(17u, num_channels);
  EXPECT_EQ(12345, sample_rate);
  EXPECT_EQ(kWavFormatALaw, format);
  EXPECT_EQ(1u, bytes_per_sample);
  EXPECT_EQ(123457689u, num_samples);
}

// Try reading an atypical but valid WAV header and make sure it's parsed OK.
TEST(WavHeaderTest, ReadAtypicalWavHeader) {
  constexpr uint8_t kBuf[] = {
      // clang-format off
      // clang formatting doesn't respect inline comments.
    'R', 'I', 'F', 'F',
    0xbf, 0xd0, 0x5b, 0x07,  // Size of whole file - 8 + extra 2 bytes of zero
                             // extension: 123457689 + 44 - 8 + 2 (atypical).
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    18, 0, 0, 0,             // Size of fmt block (with an atypical extension
                             // size field).
    6, 0,                    // Format: A-law (6).
    17, 0,                   // Channels: 17.
    0x39, 0x30, 0, 0,        // Sample rate: 12345.
    0xc9, 0x33, 0x03, 0,     // Byte rate: 1 * 17 * 12345.
    17, 0,                   // Block align: NumChannels * BytesPerSample.
    8, 0,                    // Bits per sample: 1 * 8.
    0, 0,                    // Zero extension size field (atypical).
    'd', 'a', 't', 'a',
    0x99, 0xd0, 0x5b, 0x07,  // Size of payload: 123457689.
      // clang-format on
  };

  size_t num_channels = 0;
  int sample_rate = 0;
  WavFormat format = kWavFormatPcm;
  size_t bytes_per_sample = 0;
  size_t num_samples = 0;
  ReadableWavBuffer r(kBuf, sizeof(kBuf), /*check_read_size=*/true);
  EXPECT_TRUE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                            &bytes_per_sample, &num_samples));
  EXPECT_EQ(17u, num_channels);
  EXPECT_EQ(12345, sample_rate);
  EXPECT_EQ(kWavFormatALaw, format);
  EXPECT_EQ(1u, bytes_per_sample);
  EXPECT_EQ(123457689u, num_samples);
}

// Try reading a valid WAV header which contains an optional chunk and make sure
// it's parsed OK.
TEST(WavHeaderTest, ReadWavHeaderWithOptionalChunk) {
  constexpr uint8_t kBuf[] = {
      // clang-format off
      // clang formatting doesn't respect inline comments.
    'R', 'I', 'F', 'F',
    0xcd, 0xd0, 0x5b, 0x07,  // Size of whole file - 8 + an extra 16 bytes of
                             // "metadata" (8 bytes header, 16 bytes payload):
                             // 123457689 + 44 - 8 + 16.
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    16, 0, 0, 0,             // Size of fmt block.
    6, 0,                    // Format: A-law (6).
    17, 0,                   // Channels: 17.
    0x39, 0x30, 0, 0,        // Sample rate: 12345.
    0xc9, 0x33, 0x03, 0,     // Byte rate: 1 * 17 * 12345.
    17, 0,                   // Block align: NumChannels * BytesPerSample.
    8, 0,                    // Bits per sample: 1 * 8.
    'L', 'I', 'S', 'T',      // Metadata chunk ID.
    16, 0, 0, 0,             // Metadata chunk payload size.
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // Metadata (16 bytes).
    'd', 'a', 't', 'a',
    0x99, 0xd0, 0x5b, 0x07,  // Size of payload: 123457689.
      // clang-format on
  };

  size_t num_channels = 0;
  int sample_rate = 0;
  WavFormat format = kWavFormatPcm;
  size_t bytes_per_sample = 0;
  size_t num_samples = 0;
  ReadableWavBuffer r(kBuf, sizeof(kBuf), /*check_read_size=*/true);
  EXPECT_TRUE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                            &bytes_per_sample, &num_samples));
  EXPECT_EQ(17u, num_channels);
  EXPECT_EQ(12345, sample_rate);
  EXPECT_EQ(kWavFormatALaw, format);
  EXPECT_EQ(1u, bytes_per_sample);
  EXPECT_EQ(123457689u, num_samples);
}

// Try reading an invalid WAV header which has the the data chunk before the
// format one and make sure it's not parsed.
TEST(WavHeaderTest, ReadWavHeaderWithDataBeforeFormat) {
  constexpr uint8_t kBuf[] = {
      // clang-format off
      // clang formatting doesn't respect inline comments.
    'R', 'I', 'F', 'F',
    52,  0,   0,   0,    // Size of whole file - 8: 16 + 44 - 8.
    'W', 'A', 'V', 'E',
    'd', 'a', 't', 'a',
    16, 0, 0, 0,         // Data chunk payload size.
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // Data 16 bytes.
    'f', 'm', 't', ' ',
    16,  0,   0,   0,    // Size of fmt block.
    6,   0,              // Format: A-law (6).
    1,   0,              // Channels: 1.
    60,  0,   0,   0,    // Sample rate: 60.
    60,  0,   0,   0,    // Byte rate: 1 * 1 * 60.
    1,   0,              // Block align: NumChannels * BytesPerSample.
    8,   0,              // Bits per sample: 1 * 8.
      // clang-format on
  };

  size_t num_channels = 0;
  int sample_rate = 0;
  WavFormat format = kWavFormatPcm;
  size_t bytes_per_sample = 0;
  size_t num_samples = 0;
  ReadableWavBuffer r(kBuf, sizeof(kBuf), /*check_read_size=*/false);
  EXPECT_FALSE(ReadWavHeader(&r, &num_channels, &sample_rate, &format,
                             &bytes_per_sample, &num_samples));
}

}  // namespace webrtc
