/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/wav_file.h"

#include <errno.h>

#include <algorithm>
#include <cstdio>
#include <type_traits>
#include <utility>

#include "common_audio/include/audio_util.h"
#include "common_audio/wav_header.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/arch.h"

namespace webrtc {
namespace {

// We write 16-bit PCM WAV files.
constexpr WavFormat kWavFormat = kWavFormatPcm;
static_assert(std::is_trivially_destructible<WavFormat>::value, "");
constexpr size_t kBytesPerSample = 2;

// Doesn't take ownership of the file handle and won't close it.
class ReadableWavFile : public ReadableWav {
 public:
  explicit ReadableWavFile(FileWrapper* file) : file_(file) {}
  ReadableWavFile(const ReadableWavFile&) = delete;
  ReadableWavFile& operator=(const ReadableWavFile&) = delete;
  size_t Read(void* buf, size_t num_bytes) override {
    size_t count = file_->Read(buf, num_bytes);
    pos_ += count;
    return count;
  }
  bool SeekForward(uint32_t num_bytes) override {
    bool success = file_->SeekRelative(num_bytes);
    if (success) {
      pos_ += num_bytes;
    }
    return success;
  }
  int64_t GetPosition() { return pos_; }

 private:
  FileWrapper* file_;
  int64_t pos_ = 0;
};

}  // namespace

WavReader::WavReader(const std::string& filename)
    : WavReader(FileWrapper::OpenReadOnly(filename)) {}

WavReader::WavReader(FileWrapper file) : file_(std::move(file)) {
  RTC_CHECK(file_.is_open())
      << "Invalid file. Could not create file handle for wav file.";

  ReadableWavFile readable(&file_);
  WavFormat format;
  size_t bytes_per_sample;
  RTC_CHECK(ReadWavHeader(&readable, &num_channels_, &sample_rate_, &format,
                          &bytes_per_sample, &num_samples_));
  num_samples_remaining_ = num_samples_;
  RTC_CHECK_EQ(kWavFormat, format);
  RTC_CHECK_EQ(kBytesPerSample, bytes_per_sample);
  data_start_pos_ = readable.GetPosition();
}

WavReader::~WavReader() {
  Close();
}

void WavReader::Reset() {
  RTC_CHECK(file_.SeekTo(data_start_pos_))
      << "Failed to set position in the file to WAV data start position";
  num_samples_remaining_ = num_samples_;
}

int WavReader::sample_rate() const {
  return sample_rate_;
}

size_t WavReader::num_channels() const {
  return num_channels_;
}

size_t WavReader::num_samples() const {
  return num_samples_;
}

size_t WavReader::ReadSamples(size_t num_samples, int16_t* samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to big-endian when reading from WAV file"
#endif
  // There could be metadata after the audio; ensure we don't read it.
  num_samples = std::min(num_samples, num_samples_remaining_);
  const size_t num_bytes = num_samples * sizeof(*samples);
  const size_t read_bytes = file_.Read(samples, num_bytes);
  // If we didn't read what was requested, ensure we've reached the EOF.
  RTC_CHECK(read_bytes == num_bytes || file_.ReadEof());
  RTC_CHECK_EQ(read_bytes % 2, 0)
      << "End of file in the middle of a 16-bit sample";
  const size_t read_samples = read_bytes / 2;
  RTC_CHECK_LE(read_samples, num_samples_remaining_);
  num_samples_remaining_ -= read_samples;
  return read_samples;
}

size_t WavReader::ReadSamples(size_t num_samples, float* samples) {
  static const size_t kChunksize = 4096 / sizeof(uint16_t);
  size_t read = 0;
  for (size_t i = 0; i < num_samples; i += kChunksize) {
    int16_t isamples[kChunksize];
    size_t chunk = std::min(kChunksize, num_samples - i);
    chunk = ReadSamples(chunk, isamples);
    for (size_t j = 0; j < chunk; ++j)
      samples[i + j] = isamples[j];
    read += chunk;
  }
  return read;
}

void WavReader::Close() {
  file_.Close();
}

WavWriter::WavWriter(const std::string& filename,
                     int sample_rate,
                     size_t num_channels)
    // Unlike plain fopen, OpenWriteOnly takes care of filename utf8 ->
    // wchar conversion on windows.
    : WavWriter(FileWrapper::OpenWriteOnly(filename),
                sample_rate,
                num_channels) {}

WavWriter::WavWriter(FileWrapper file, int sample_rate, size_t num_channels)
    : sample_rate_(sample_rate),
      num_channels_(num_channels),
      num_samples_(0),
      file_(std::move(file)) {
  // Handle errors from the OpenWriteOnly call in above constructor.
  RTC_CHECK(file_.is_open()) << "Invalid file. Could not create wav file.";

  RTC_CHECK(CheckWavParameters(num_channels_, sample_rate_, kWavFormat,
                               kBytesPerSample, num_samples_));

  // Write a blank placeholder header, since we need to know the total number
  // of samples before we can fill in the real data.
  static const uint8_t blank_header[kWavHeaderSize] = {0};
  RTC_CHECK(file_.Write(blank_header, kWavHeaderSize));
}

WavWriter::~WavWriter() {
  Close();
}

int WavWriter::sample_rate() const {
  return sample_rate_;
}

size_t WavWriter::num_channels() const {
  return num_channels_;
}

size_t WavWriter::num_samples() const {
  return num_samples_;
}

void WavWriter::WriteSamples(const int16_t* samples, size_t num_samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to little-endian when writing to WAV file"
#endif
  RTC_CHECK(file_.Write(samples, sizeof(*samples) * num_samples));
  num_samples_ += num_samples;
  RTC_CHECK(num_samples_ >= num_samples);  // detect size_t overflow
}

void WavWriter::WriteSamples(const float* samples, size_t num_samples) {
  static const size_t kChunksize = 4096 / sizeof(uint16_t);
  for (size_t i = 0; i < num_samples; i += kChunksize) {
    int16_t isamples[kChunksize];
    const size_t chunk = std::min(kChunksize, num_samples - i);
    FloatS16ToS16(samples + i, chunk, isamples);
    WriteSamples(isamples, chunk);
  }
}

void WavWriter::Close() {
  RTC_CHECK(file_.Rewind());
  uint8_t header[kWavHeaderSize];
  WriteWavHeader(header, num_channels_, sample_rate_, kWavFormat,
                 kBytesPerSample, num_samples_);
  RTC_CHECK(file_.Write(header, kWavHeaderSize));
  RTC_CHECK(file_.Close());
}

}  // namespace webrtc
