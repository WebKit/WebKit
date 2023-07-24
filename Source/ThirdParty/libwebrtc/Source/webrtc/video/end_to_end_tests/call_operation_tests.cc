/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/simulated_network.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "rtc_base/task_queue_for_test.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/frame_forwarder.h"
#include "test/gtest.h"
#include "test/null_transport.h"
#include "test/video_test_constants.h"

namespace webrtc {

class CallOperationEndToEndTest : public test::CallTest {};

TEST_F(CallOperationEndToEndTest, ReceiverCanBeStartedTwice) {
  CreateCalls();

  test::NullTransport transport;
  CreateSendConfig(1, 0, 0, &transport);
  CreateMatchingReceiveConfigs(&transport);

  CreateVideoStreams();

  video_receive_streams_[0]->Start();
  video_receive_streams_[0]->Start();

  DestroyStreams();
}

TEST_F(CallOperationEndToEndTest, ReceiverCanBeStoppedTwice) {
  CreateCalls();

  test::NullTransport transport;
  CreateSendConfig(1, 0, 0, &transport);
  CreateMatchingReceiveConfigs(&transport);

  CreateVideoStreams();

  video_receive_streams_[0]->Stop();
  video_receive_streams_[0]->Stop();

  DestroyStreams();
}

TEST_F(CallOperationEndToEndTest, ReceiverCanBeStoppedAndRestarted) {
  CreateCalls();

  test::NullTransport transport;
  CreateSendConfig(1, 0, 0, &transport);
  CreateMatchingReceiveConfigs(&transport);

  CreateVideoStreams();

  video_receive_streams_[0]->Stop();
  video_receive_streams_[0]->Start();
  video_receive_streams_[0]->Stop();

  DestroyStreams();
}

TEST_F(CallOperationEndToEndTest, RendersSingleDelayedFrame) {
  static const int kWidth = 320;
  static const int kHeight = 240;
  // This constant is chosen to be higher than the timeout in the video_render
  // module. This makes sure that frames aren't dropped if there are no other
  // frames in the queue.
  static const int kRenderDelayMs = 1000;

  class Renderer : public rtc::VideoSinkInterface<VideoFrame> {
   public:
    void OnFrame(const VideoFrame& video_frame) override {
      SleepMs(kRenderDelayMs);
      event_.Set();
    }

    bool Wait() {
      return event_.Wait(test::VideoTestConstants::kDefaultTimeout);
    }

    rtc::Event event_;
  } renderer;

  test::FrameForwarder frame_forwarder;

  SendTask(task_queue(), [this, &renderer, &frame_forwarder]() {
    CreateCalls();
    CreateSendTransport(BuiltInNetworkBehaviorConfig(),
                        /*observer=*/nullptr);

    CreateReceiveTransport(BuiltInNetworkBehaviorConfig(),
                           /*observer=*/nullptr);
    CreateSendConfig(1, 0, 0);
    CreateMatchingReceiveConfigs();

    video_receive_configs_[0].renderer = &renderer;

    CreateVideoStreams();
    Start();

    // Create frames that are smaller than the send width/height, this is
    // done to check that the callbacks are done after processing video.
    std::unique_ptr<test::FrameGeneratorInterface> frame_generator(
        test::CreateSquareFrameGenerator(kWidth, kHeight, absl::nullopt,
                                         absl::nullopt));
    GetVideoSendStream()->SetSource(&frame_forwarder,
                                    DegradationPreference::MAINTAIN_FRAMERATE);

    test::FrameGeneratorInterface::VideoFrameData frame_data =
        frame_generator->NextFrame();
    VideoFrame frame = VideoFrame::Builder()
                           .set_video_frame_buffer(frame_data.buffer)
                           .set_update_rect(frame_data.update_rect)
                           .build();
    frame_forwarder.IncomingCapturedFrame(frame);
  });

  EXPECT_TRUE(renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  SendTask(task_queue(), [this]() {
    Stop();
    DestroyStreams();
    DestroyCalls();
  });
}

TEST_F(CallOperationEndToEndTest, TransmitsFirstFrame) {
  class Renderer : public rtc::VideoSinkInterface<VideoFrame> {
   public:
    void OnFrame(const VideoFrame& video_frame) override { event_.Set(); }

    bool Wait() {
      return event_.Wait(test::VideoTestConstants::kDefaultTimeout);
    }

    rtc::Event event_;
  } renderer;

  std::unique_ptr<test::FrameGeneratorInterface> frame_generator;
  test::FrameForwarder frame_forwarder;

  std::unique_ptr<test::DirectTransport> sender_transport;
  std::unique_ptr<test::DirectTransport> receiver_transport;

  SendTask(task_queue(), [this, &renderer, &frame_generator,
                          &frame_forwarder]() {
    CreateCalls();
    CreateSendTransport(BuiltInNetworkBehaviorConfig(),
                        /*observer=*/nullptr);
    CreateReceiveTransport(BuiltInNetworkBehaviorConfig(),
                           /*observer=*/nullptr);

    CreateSendConfig(1, 0, 0);
    CreateMatchingReceiveConfigs();
    video_receive_configs_[0].renderer = &renderer;

    CreateVideoStreams();
    Start();

    frame_generator = test::CreateSquareFrameGenerator(
        test::VideoTestConstants::kDefaultWidth,
        test::VideoTestConstants::kDefaultHeight, absl::nullopt, absl::nullopt);
    GetVideoSendStream()->SetSource(&frame_forwarder,
                                    DegradationPreference::MAINTAIN_FRAMERATE);
    test::FrameGeneratorInterface::VideoFrameData frame_data =
        frame_generator->NextFrame();
    VideoFrame frame = VideoFrame::Builder()
                           .set_video_frame_buffer(frame_data.buffer)
                           .set_update_rect(frame_data.update_rect)
                           .build();
    frame_forwarder.IncomingCapturedFrame(frame);
  });

  EXPECT_TRUE(renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  SendTask(task_queue(), [this]() {
    Stop();
    DestroyStreams();
    DestroyCalls();
  });
}

}  // namespace webrtc
