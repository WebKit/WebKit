/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_buffer2.h"

#include "modules/video_coding/jitter_estimator.h"
#include "modules/video_coding/timing.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

namespace {

// When DataReader runs out of data provided in the constructor it will
// just set/return 0 instead.
struct DataReader {
  DataReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  void CopyTo(void* destination, size_t dest_size) {
    memset(destination, 0, dest_size);

    size_t bytes_to_copy = std::min(size_ - offset_, dest_size);
    memcpy(destination, data_ + offset_, bytes_to_copy);
    offset_ += bytes_to_copy;
  }

  template <typename T>
  T GetNum() {
    T res;
    if (offset_ + sizeof(res) < size_) {
      memcpy(&res, data_ + offset_, sizeof(res));
      offset_ += sizeof(res);
      return res;
    }

    offset_ = size_;
    return T(0);
  }

  bool MoreToRead() { return offset_ < size_; }

  const uint8_t* const data_;
  size_t size_;
  size_t offset_ = 0;
};

class FuzzyFrameObject : public video_coding::EncodedFrame {
 public:
  FuzzyFrameObject() {}
  ~FuzzyFrameObject() {}

  bool GetBitstream(uint8_t* destination) const override { return false; }
  int64_t ReceivedTime() const override { return 0; }
  int64_t RenderTime() const override { return _renderTimeMs; }
};
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  DataReader reader(data, size);
  Clock* clock = Clock::GetRealTimeClock();
  VCMJitterEstimator jitter_estimator(clock, 0, 0);
  VCMTiming timing(clock);
  video_coding::FrameBuffer frame_buffer(clock, &jitter_estimator, &timing,
                                         nullptr);

  while (reader.MoreToRead()) {
    if (reader.GetNum<uint8_t>() & 1) {
      std::unique_ptr<FuzzyFrameObject> frame(new FuzzyFrameObject());
      frame->id.picture_id = reader.GetNum<int64_t>();
      frame->id.spatial_layer = reader.GetNum<uint8_t>();
      frame->SetTimestamp(reader.GetNum<uint32_t>());
      frame->num_references = reader.GetNum<uint8_t>() %
                              video_coding::EncodedFrame::kMaxFrameReferences;

      for (size_t r = 0; r < frame->num_references; ++r)
        frame->references[r] = reader.GetNum<int64_t>();

      frame_buffer.InsertFrame(std::move(frame));
    } else {
      // Since we are not trying to trigger race conditions it does not make
      // sense to have a wait time > 0.
      const int kWaitTimeMs = 0;

      std::unique_ptr<video_coding::EncodedFrame> frame(new FuzzyFrameObject());
      bool keyframe_required = reader.GetNum<uint8_t>() % 2;
      frame_buffer.NextFrame(kWaitTimeMs, &frame, keyframe_required);
    }
  }
}

}  // namespace webrtc
