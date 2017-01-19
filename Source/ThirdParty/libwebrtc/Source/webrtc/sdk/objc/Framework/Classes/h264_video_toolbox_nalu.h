/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_H264_VIDEO_TOOLBOX_NALU_H_
#define WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_H264_VIDEO_TOOLBOX_NALU_H_

#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"

#include <CoreMedia/CoreMedia.h>
#include <vector>

#include "webrtc/base/buffer.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/modules/include/module_common_types.h"

using webrtc::H264::NaluIndex;

namespace webrtc {

// Converts a sample buffer emitted from the VideoToolbox encoder into a buffer
// suitable for RTP. The sample buffer is in avcc format whereas the rtp buffer
// needs to be in Annex B format. Data is written directly to |annexb_buffer|
// and a new RTPFragmentationHeader is returned in |out_header|.
bool H264CMSampleBufferToAnnexBBuffer(
    CMSampleBufferRef avcc_sample_buffer,
    bool is_keyframe,
    rtc::Buffer* annexb_buffer,
    webrtc::RTPFragmentationHeader** out_header);

// Converts a buffer received from RTP into a sample buffer suitable for the
// VideoToolbox decoder. The RTP buffer is in annex b format whereas the sample
// buffer is in avcc format.
// If |is_keyframe| is true then |video_format| is ignored since the format will
// be read from the buffer. Otherwise |video_format| must be provided.
// Caller is responsible for releasing the created sample buffer.
bool H264AnnexBBufferToCMSampleBuffer(const uint8_t* annexb_buffer,
                                      size_t annexb_buffer_size,
                                      CMVideoFormatDescriptionRef video_format,
                                      CMSampleBufferRef* out_sample_buffer);

// Returns true if the type of the first NALU in the supplied Annex B buffer is
// the SPS type.
bool H264AnnexBBufferHasVideoFormatDescription(const uint8_t* annexb_buffer,
                                               size_t annexb_buffer_size);

// Returns a video format description created from the sps/pps information in
// the Annex B buffer. If there is no such information, nullptr is returned.
// The caller is responsible for releasing the description.
CMVideoFormatDescriptionRef CreateVideoFormatDescription(
    const uint8_t* annexb_buffer,
    size_t annexb_buffer_size);

// Helper class for reading NALUs from an RTP Annex B buffer.
class AnnexBBufferReader final {
 public:
  AnnexBBufferReader(const uint8_t* annexb_buffer, size_t length);
  ~AnnexBBufferReader() {}
  AnnexBBufferReader(const AnnexBBufferReader& other) = delete;
  void operator=(const AnnexBBufferReader& other) = delete;

  // Returns a pointer to the beginning of the next NALU slice without the
  // header bytes and its length. Returns false if no more slices remain.
  bool ReadNalu(const uint8_t** out_nalu, size_t* out_length);

  // Returns the number of unread NALU bytes, including the size of the header.
  // If the buffer has no remaining NALUs this will return zero.
  size_t BytesRemaining() const;

 private:
  // Returns the the next offset that contains NALU data.
  size_t FindNextNaluHeader(const uint8_t* start,
                            size_t length,
                            size_t offset) const;

  const uint8_t* const start_;
  std::vector<NaluIndex> offsets_;
  std::vector<NaluIndex>::iterator offset_;
  const size_t length_;
};

// Helper class for writing NALUs using avcc format into a buffer.
class AvccBufferWriter final {
 public:
  AvccBufferWriter(uint8_t* const avcc_buffer, size_t length);
  ~AvccBufferWriter() {}
  AvccBufferWriter(const AvccBufferWriter& other) = delete;
  void operator=(const AvccBufferWriter& other) = delete;

  // Writes the data slice into the buffer. Returns false if there isn't
  // enough space left.
  bool WriteNalu(const uint8_t* data, size_t data_size);

  // Returns the unused bytes in the buffer.
  size_t BytesRemaining() const;

 private:
  uint8_t* const start_;
  size_t offset_;
  const size_t length_;
};

}  // namespace webrtc

#endif // WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_H264_VIDEO_TOOLBOX_NALU_H_
