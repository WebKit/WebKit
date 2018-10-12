/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/video_file_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/stringutils.h"

namespace webrtc {
namespace test {

namespace {

// Common base class for .yuv and .y4m files.
class VideoFile : public Video {
 public:
  VideoFile(int width,
            int height,
            const std::vector<fpos_t>& frame_positions,
            FILE* file)
      : width_(width),
        height_(height),
        frame_positions_(frame_positions),
        file_(file) {}

  ~VideoFile() override { fclose(file_); }

  size_t number_of_frames() const override { return frame_positions_.size(); }
  int width() const override { return width_; }
  int height() const override { return height_; }

  rtc::scoped_refptr<I420BufferInterface> GetFrame(
      size_t frame_index) const override {
    RTC_CHECK_LT(frame_index, frame_positions_.size());

    fsetpos(file_, &frame_positions_[frame_index]);
    rtc::scoped_refptr<I420Buffer> buffer = I420Buffer::Create(width_, height_);
    fread(reinterpret_cast<char*>(buffer->MutableDataY()), /* size= */ 1,
          width_ * height_, file_);
    fread(reinterpret_cast<char*>(buffer->MutableDataU()), /* size= */ 1,
          buffer->ChromaWidth() * buffer->ChromaHeight(), file_);
    fread(reinterpret_cast<char*>(buffer->MutableDataV()), /* size= */ 1,
          buffer->ChromaWidth() * buffer->ChromaHeight(), file_);

    if (ferror(file_) != 0) {
      RTC_LOG(LS_ERROR) << "Could not read YUV data for frame " << frame_index;
      return nullptr;
    }
    return buffer;
  }

 private:
  const int width_;
  const int height_;
  const std::vector<fpos_t> frame_positions_;
  FILE* const file_;
};

}  // namespace

Video::Iterator::Iterator(const rtc::scoped_refptr<const Video>& video,
                          size_t index)
    : video_(video), index_(index) {}

Video::Iterator::Iterator(const Video::Iterator& other) = default;
Video::Iterator::Iterator(Video::Iterator&& other) = default;
Video::Iterator& Video::Iterator::operator=(Video::Iterator&&) = default;
Video::Iterator& Video::Iterator::operator=(const Video::Iterator&) = default;
Video::Iterator::~Iterator() = default;

rtc::scoped_refptr<I420BufferInterface> Video::Iterator::operator*() const {
  return video_->GetFrame(index_);
}
bool Video::Iterator::operator==(const Video::Iterator& other) const {
  return index_ == other.index_;
}
bool Video::Iterator::operator!=(const Video::Iterator& other) const {
  return !(*this == other);
}

Video::Iterator Video::Iterator::operator++(int) {
  const Iterator copy = *this;
  ++*this;
  return copy;
}

Video::Iterator& Video::Iterator::operator++() {
  ++index_;
  return *this;
}

Video::Iterator Video::begin() const {
  return Iterator(this, 0);
}

Video::Iterator Video::end() const {
  return Iterator(this, number_of_frames());
}

rtc::scoped_refptr<Video> OpenY4mFile(const std::string& file_name) {
  FILE* file = fopen(file_name.c_str(), "rb");
  if (file == nullptr) {
    RTC_LOG(LS_ERROR) << "Could not open input file for reading: " << file_name;
    return nullptr;
  }

  int parse_file_header_result = -1;
  fscanf(file, "YUV4MPEG2 %n", &parse_file_header_result);
  if (parse_file_header_result == -1) {
    RTC_LOG(LS_ERROR) << "File " << file_name
                      << " does not start with YUV4MPEG2 header";
    return nullptr;
  }

  std::string header_line;
  while (true) {
    const int c = fgetc(file);
    if (c == EOF) {
      RTC_LOG(LS_ERROR) << "Could not read header line";
      return nullptr;
    }
    if (c == '\n')
      break;
    header_line.push_back(static_cast<char>(c));
  }

  absl::optional<int> width;
  absl::optional<int> height;
  absl::optional<float> fps;

  std::vector<std::string> fields;
  rtc::tokenize(header_line, ' ', &fields);
  for (const std::string& field : fields) {
    const char prefix = field.front();
    const std::string suffix = field.substr(1);
    switch (prefix) {
      case 'W':
        width = rtc::StringToNumber<int>(suffix);
        break;
      case 'H':
        height = rtc::StringToNumber<int>(suffix);
        break;
      case 'C':
        if (suffix != "420" && suffix != "420mpeg2") {
          RTC_LOG(LS_ERROR)
              << "Does not support any other color space than I420 or "
                 "420mpeg2, but was: "
              << suffix;
          return nullptr;
        }
        break;
      case 'F': {
        std::vector<std::string> fraction;
        rtc::tokenize(suffix, ':', &fraction);
        if (fraction.size() == 2) {
          const absl::optional<int> numerator =
              rtc::StringToNumber<int>(fraction[0]);
          const absl::optional<int> denominator =
              rtc::StringToNumber<int>(fraction[1]);
          if (numerator && denominator && *denominator != 0)
            fps = *numerator / static_cast<float>(*denominator);
          break;
        }
      }
    }
  }
  if (!width || !height) {
    RTC_LOG(LS_ERROR) << "Could not find width and height in file header";
    return nullptr;
  }
  if (!fps) {
    RTC_LOG(LS_ERROR) << "Could not find fps in file header";
    return nullptr;
  }
  RTC_LOG(LS_INFO) << "Video has resolution: " << *width << "x" << *height
                   << " " << *fps << " fps";
  if (*width % 2 != 0 || *height % 2 != 0) {
    RTC_LOG(LS_ERROR)
        << "Only supports even width/height so that chroma size is a "
           "whole number.";
    return nullptr;
  }

  const int i420_frame_size = 3 * *width * *height / 2;
  std::vector<fpos_t> frame_positions;
  while (true) {
    int parse_frame_header_result = -1;
    fscanf(file, "FRAME\n%n", &parse_frame_header_result);
    if (parse_frame_header_result == -1) {
      if (!feof(file)) {
        RTC_LOG(LS_ERROR) << "Did not find FRAME header, ignoring rest of file";
      }
      break;
    }
    fpos_t pos;
    fgetpos(file, &pos);
    frame_positions.push_back(pos);
    // Skip over YUV pixel data.
    fseek(file, i420_frame_size, SEEK_CUR);
  }
  if (frame_positions.empty()) {
    RTC_LOG(LS_ERROR) << "Could not find any frames in the file";
    return nullptr;
  }
  RTC_LOG(LS_INFO) << "Video has " << frame_positions.size() << " frames";

  return new rtc::RefCountedObject<VideoFile>(*width, *height, frame_positions,
                                              file);
}

rtc::scoped_refptr<Video> OpenYuvFile(const std::string& file_name,
                                      int width,
                                      int height) {
  FILE* file = fopen(file_name.c_str(), "rb");
  if (file == nullptr) {
    RTC_LOG(LS_ERROR) << "Could not open input file for reading: " << file_name;
    return nullptr;
  }

  if (width % 2 != 0 || height % 2 != 0) {
    RTC_LOG(LS_ERROR)
        << "Only supports even width/height so that chroma size is a "
           "whole number.";
    return nullptr;
  }

  // Seek to end of file.
  fseek(file, 0, SEEK_END);
  const size_t file_size = ftell(file);
  // Seek back to beginning of file.
  fseek(file, 0, SEEK_SET);

  const int i420_frame_size = 3 * width * height / 2;
  const size_t number_of_frames = file_size / i420_frame_size;

  std::vector<fpos_t> frame_positions;
  for (size_t i = 0; i < number_of_frames; ++i) {
    fpos_t pos;
    fgetpos(file, &pos);
    frame_positions.push_back(pos);
    fseek(file, i420_frame_size, SEEK_CUR);
  }
  if (frame_positions.empty()) {
    RTC_LOG(LS_ERROR) << "Could not find any frames in the file";
    return nullptr;
  }
  RTC_LOG(LS_INFO) << "Video has " << frame_positions.size() << " frames";

  return new rtc::RefCountedObject<VideoFile>(width, height, frame_positions,
                                              file);
}

rtc::scoped_refptr<Video> OpenYuvOrY4mFile(const std::string& file_name,
                                           int width,
                                           int height) {
  if (rtc::ends_with(file_name.c_str(), ".yuv"))
    return OpenYuvFile(file_name, width, height);
  if (rtc::ends_with(file_name.c_str(), ".y4m"))
    return OpenY4mFile(file_name);

  RTC_LOG(LS_ERROR) << "Video file does not end in either .yuv or .y4m: "
                    << file_name;

  return nullptr;
}

}  // namespace test
}  // namespace webrtc
