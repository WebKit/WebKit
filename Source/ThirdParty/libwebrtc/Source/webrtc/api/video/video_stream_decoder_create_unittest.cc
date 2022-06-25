/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_stream_decoder_create.h"

#include "api/task_queue/default_task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class NullCallbacks : public VideoStreamDecoderInterface::Callbacks {
 public:
  ~NullCallbacks() override = default;
  void OnNonDecodableState() override {}
  void OnDecodedFrame(VideoFrame frame,
                      const VideoStreamDecoderInterface::Callbacks::FrameInfo&
                          frame_info) override {}
};

TEST(VideoStreamDecoderCreate, CreateVideoStreamDecoder) {
  std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings = {
      {/*payload_type=*/111, {SdpVideoFormat("VP8"), /*number_of_cores=*/2}}};
  NullCallbacks callbacks;
  std::unique_ptr<VideoDecoderFactory> decoder_factory =
      CreateBuiltinVideoDecoderFactory();

  std::unique_ptr<TaskQueueFactory> task_queue_factory =
      CreateDefaultTaskQueueFactory();

  std::unique_ptr<VideoStreamDecoderInterface> decoder =
      CreateVideoStreamDecoder(&callbacks, decoder_factory.get(),
                               task_queue_factory.get(), decoder_settings);
  EXPECT_TRUE(decoder);
}

}  // namespace
}  // namespace webrtc
