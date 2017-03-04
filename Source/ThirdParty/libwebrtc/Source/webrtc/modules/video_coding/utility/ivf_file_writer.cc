/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"

#include <string>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

// TODO(palmkvist): make logging more informative in the absence of a file name
// (or get one)

namespace webrtc {

const size_t kIvfHeaderSize = 32;

IvfFileWriter::IvfFileWriter(rtc::File file, size_t byte_limit)
    : codec_type_(kVideoCodecUnknown),
      bytes_written_(0),
      byte_limit_(byte_limit),
      num_frames_(0),
      width_(0),
      height_(0),
      last_timestamp_(-1),
      using_capture_timestamps_(false),
      file_(std::move(file)) {
  RTC_DCHECK(byte_limit == 0 || kIvfHeaderSize <= byte_limit)
      << "The byte_limit is too low, not even the header will fit.";
}

IvfFileWriter::~IvfFileWriter() {
  Close();
}

std::unique_ptr<IvfFileWriter> IvfFileWriter::Wrap(rtc::File file,
                                                   size_t byte_limit) {
  return std::unique_ptr<IvfFileWriter>(
      new IvfFileWriter(std::move(file), byte_limit));
}

bool IvfFileWriter::WriteHeader() {
  if (!file_.Seek(0)) {
    LOG(LS_WARNING) << "Unable to rewind ivf output file.";
    return false;
  }

  uint8_t ivf_header[kIvfHeaderSize] = {0};
  ivf_header[0] = 'D';
  ivf_header[1] = 'K';
  ivf_header[2] = 'I';
  ivf_header[3] = 'F';
  ByteWriter<uint16_t>::WriteLittleEndian(&ivf_header[4], 0);   // Version.
  ByteWriter<uint16_t>::WriteLittleEndian(&ivf_header[6], 32);  // Header size.

  switch (codec_type_) {
    case kVideoCodecVP8:
      ivf_header[8] = 'V';
      ivf_header[9] = 'P';
      ivf_header[10] = '8';
      ivf_header[11] = '0';
      break;
    case kVideoCodecVP9:
      ivf_header[8] = 'V';
      ivf_header[9] = 'P';
      ivf_header[10] = '9';
      ivf_header[11] = '0';
      break;
    case kVideoCodecH264:
      ivf_header[8] = 'H';
      ivf_header[9] = '2';
      ivf_header[10] = '6';
      ivf_header[11] = '4';
      break;
    default:
      LOG(LS_ERROR) << "Unknown CODEC type: " << codec_type_;
      return false;
  }

  ByteWriter<uint16_t>::WriteLittleEndian(&ivf_header[12], width_);
  ByteWriter<uint16_t>::WriteLittleEndian(&ivf_header[14], height_);
  // Render timestamps are in ms (1/1000 scale), while RTP timestamps use a
  // 90kHz clock.
  ByteWriter<uint32_t>::WriteLittleEndian(
      &ivf_header[16], using_capture_timestamps_ ? 1000 : 90000);
  ByteWriter<uint32_t>::WriteLittleEndian(&ivf_header[20], 1);
  ByteWriter<uint32_t>::WriteLittleEndian(&ivf_header[24],
                                          static_cast<uint32_t>(num_frames_));
  ByteWriter<uint32_t>::WriteLittleEndian(&ivf_header[28], 0);  // Reserved.

  if (file_.Write(ivf_header, kIvfHeaderSize) < kIvfHeaderSize) {
    LOG(LS_ERROR) << "Unable to write IVF header for ivf output file.";
    return false;
  }

  if (bytes_written_ < kIvfHeaderSize) {
    bytes_written_ = kIvfHeaderSize;
  }

  return true;
}

bool IvfFileWriter::InitFromFirstFrame(const EncodedImage& encoded_image,
                                       VideoCodecType codec_type) {
  width_ = encoded_image._encodedWidth;
  height_ = encoded_image._encodedHeight;
  RTC_CHECK_GT(width_, 0);
  RTC_CHECK_GT(height_, 0);
  using_capture_timestamps_ = encoded_image._timeStamp == 0;

  codec_type_ = codec_type;

  if (!WriteHeader())
    return false;

  const char* codec_name =
      CodecTypeToPayloadName(codec_type_).value_or("Unknown");
  LOG(LS_WARNING) << "Created IVF file for codec data of type " << codec_name
                  << " at resolution " << width_ << " x " << height_
                  << ", using " << (using_capture_timestamps_ ? "1" : "90")
                  << "kHz clock resolution.";
  return true;
}

bool IvfFileWriter::WriteFrame(const EncodedImage& encoded_image,
                               VideoCodecType codec_type) {
  if (!file_.IsOpen())
    return false;

  if (num_frames_ == 0 && !InitFromFirstFrame(encoded_image, codec_type))
    return false;
  RTC_DCHECK_EQ(codec_type_, codec_type);

  if ((encoded_image._encodedWidth > 0 || encoded_image._encodedHeight > 0) &&
      (encoded_image._encodedHeight != height_ ||
       encoded_image._encodedWidth != width_)) {
    LOG(LS_WARNING)
        << "Incomig frame has diffferent resolution then previous: (" << width_
        << "x" << height_ << ") -> (" << encoded_image._encodedWidth << "x"
        << encoded_image._encodedHeight << ")";
  }

  int64_t timestamp = using_capture_timestamps_
                          ? encoded_image.capture_time_ms_
                          : wrap_handler_.Unwrap(encoded_image._timeStamp);
  if (last_timestamp_ != -1 && timestamp <= last_timestamp_) {
    LOG(LS_WARNING) << "Timestamp no increasing: " << last_timestamp_ << " -> "
                    << timestamp;
  }
  last_timestamp_ = timestamp;

  const size_t kFrameHeaderSize = 12;
  if (byte_limit_ != 0 &&
      bytes_written_ + kFrameHeaderSize + encoded_image._length > byte_limit_) {
    LOG(LS_WARNING) << "Closing IVF file due to reaching size limit: "
                    << byte_limit_ << " bytes.";
    Close();
    return false;
  }
  uint8_t frame_header[kFrameHeaderSize] = {};
  ByteWriter<uint32_t>::WriteLittleEndian(
      &frame_header[0], static_cast<uint32_t>(encoded_image._length));
  ByteWriter<uint64_t>::WriteLittleEndian(&frame_header[4], timestamp);
  if (file_.Write(frame_header, kFrameHeaderSize) < kFrameHeaderSize ||
      file_.Write(encoded_image._buffer, encoded_image._length) <
          encoded_image._length) {
    LOG(LS_ERROR) << "Unable to write frame to file.";
    return false;
  }

  bytes_written_ += kFrameHeaderSize + encoded_image._length;

  ++num_frames_;
  return true;
}

bool IvfFileWriter::Close() {
  if (!file_.IsOpen())
    return false;

  if (num_frames_ == 0) {
    file_.Close();
    return true;
  }

  bool ret = WriteHeader();
  file_.Close();
  return ret;
}

}  // namespace webrtc
