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
#include "modules/video_coding/timing.h"
#include "test/time_controller/simulated_time_controller.h"

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

class FuzzyFrameObject : public EncodedFrame {
 public:
  FuzzyFrameObject() {}
  ~FuzzyFrameObject() {}

  int64_t ReceivedTime() const override { return 0; }
  int64_t RenderTime() const override { return _renderTimeMs; }
};
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 10000) {
    return;
  }
  DataReader reader(data, size);
  GlobalSimulatedTimeController time_controller(Timestamp::Seconds(0));
  rtc::TaskQueue task_queue(
      time_controller.GetTaskQueueFactory()->CreateTaskQueue(
          "time_tq", TaskQueueFactory::Priority::NORMAL));
  VCMTiming timing(time_controller.GetClock());
  video_coding::FrameBuffer frame_buffer(time_controller.GetClock(), &timing,
                                         nullptr);

  bool next_frame_task_running = false;

  while (reader.MoreToRead()) {
    if (reader.GetNum<uint8_t>() % 2) {
      std::unique_ptr<FuzzyFrameObject> frame(new FuzzyFrameObject());
      frame->SetId(reader.GetNum<int64_t>());
      frame->SetSpatialIndex(reader.GetNum<uint8_t>() % 5);
      frame->SetTimestamp(reader.GetNum<uint32_t>());
      frame->num_references =
          reader.GetNum<uint8_t>() % EncodedFrame::kMaxFrameReferences;

      for (size_t r = 0; r < frame->num_references; ++r)
        frame->references[r] = reader.GetNum<int64_t>();

      frame_buffer.InsertFrame(std::move(frame));
    } else {
      if (!next_frame_task_running) {
        next_frame_task_running = true;
        bool keyframe_required = reader.GetNum<uint8_t>() % 2;
        int max_wait_time_ms = reader.GetNum<uint8_t>();
        task_queue.PostTask([&task_queue, &frame_buffer,
                             &next_frame_task_running, keyframe_required,
                             max_wait_time_ms] {
          frame_buffer.NextFrame(
              max_wait_time_ms, keyframe_required, &task_queue,
              [&next_frame_task_running](
                  std::unique_ptr<EncodedFrame> frame,
                  video_coding::FrameBuffer::ReturnReason res) {
                next_frame_task_running = false;
              });
        });
      }
    }

    time_controller.AdvanceTime(TimeDelta::Millis(reader.GetNum<uint8_t>()));
  }
}

}  // namespace webrtc
