/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/end_to_end_tests/multi_stream_tester.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/test/simulated_network.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/engine/internaldecoderfactory.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "test/call_test.h"
#include "test/encoder_settings.h"

namespace webrtc {

MultiStreamTester::MultiStreamTester(
    test::SingleThreadedTaskQueueForTesting* task_queue)
    : task_queue_(task_queue) {
  // TODO(sprang): Cleanup when msvc supports explicit initializers for array.
  codec_settings[0] = {1, 640, 480};
  codec_settings[1] = {2, 320, 240};
  codec_settings[2] = {3, 240, 160};
  class multi_stream_test {
   public:
    multi_stream_test();
  };
}

MultiStreamTester::~MultiStreamTester() {}

void MultiStreamTester::RunTest() {
  webrtc::RtcEventLogNullImpl event_log;
  Call::Config config(&event_log);
  std::unique_ptr<Call> sender_call;
  std::unique_ptr<Call> receiver_call;
  std::unique_ptr<test::DirectTransport> sender_transport;
  std::unique_ptr<test::DirectTransport> receiver_transport;

  VideoSendStream* send_streams[kNumStreams];
  VideoReceiveStream* receive_streams[kNumStreams];
  test::FrameGeneratorCapturer* frame_generators[kNumStreams];
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  std::unique_ptr<VideoBitrateAllocatorFactory> bitrate_allocator_factory =
      CreateBuiltinVideoBitrateAllocatorFactory();
  InternalDecoderFactory decoder_factory;

  task_queue_->SendTask([&]() {
    sender_call = absl::WrapUnique(Call::Create(config));
    receiver_call = absl::WrapUnique(Call::Create(config));
    sender_transport =
        absl::WrapUnique(CreateSendTransport(task_queue_, sender_call.get()));
    receiver_transport = absl::WrapUnique(
        CreateReceiveTransport(task_queue_, receiver_call.get()));

    sender_transport->SetReceiver(receiver_call->Receiver());
    receiver_transport->SetReceiver(sender_call->Receiver());

    for (size_t i = 0; i < kNumStreams; ++i) {
      uint32_t ssrc = codec_settings[i].ssrc;
      int width = codec_settings[i].width;
      int height = codec_settings[i].height;

      VideoSendStream::Config send_config(sender_transport.get());
      send_config.rtp.ssrcs.push_back(ssrc);
      send_config.encoder_settings.encoder_factory = &encoder_factory;
      send_config.encoder_settings.bitrate_allocator_factory =
          bitrate_allocator_factory.get();
      send_config.rtp.payload_name = "VP8";
      send_config.rtp.payload_type = kVideoPayloadType;
      VideoEncoderConfig encoder_config;
      test::FillEncoderConfiguration(kVideoCodecVP8, 1, &encoder_config);
      encoder_config.max_bitrate_bps = 100000;

      UpdateSendConfig(i, &send_config, &encoder_config, &frame_generators[i]);

      send_streams[i] = sender_call->CreateVideoSendStream(
          send_config.Copy(), encoder_config.Copy());
      send_streams[i]->Start();

      VideoReceiveStream::Config receive_config(receiver_transport.get());
      receive_config.rtp.remote_ssrc = ssrc;
      receive_config.rtp.local_ssrc = test::CallTest::kReceiverLocalVideoSsrc;
      VideoReceiveStream::Decoder decoder =
          test::CreateMatchingDecoder(send_config);
      decoder.decoder_factory = &decoder_factory;
      receive_config.decoders.push_back(decoder);

      UpdateReceiveConfig(i, &receive_config);

      receive_streams[i] =
          receiver_call->CreateVideoReceiveStream(std::move(receive_config));
      receive_streams[i]->Start();

      frame_generators[i] = test::FrameGeneratorCapturer::Create(
          width, height, absl::nullopt, absl::nullopt, 30,
          Clock::GetRealTimeClock());
      send_streams[i]->SetSource(frame_generators[i],
                                 DegradationPreference::MAINTAIN_FRAMERATE);
      frame_generators[i]->Start();
    }
  });

  Wait();

  task_queue_->SendTask([&]() {
    for (size_t i = 0; i < kNumStreams; ++i) {
      frame_generators[i]->Stop();
      sender_call->DestroyVideoSendStream(send_streams[i]);
      receiver_call->DestroyVideoReceiveStream(receive_streams[i]);
      delete frame_generators[i];
    }

    sender_transport.reset();
    receiver_transport.reset();

    sender_call.reset();
    receiver_call.reset();
  });
}

void MultiStreamTester::UpdateSendConfig(
    size_t stream_index,
    VideoSendStream::Config* send_config,
    VideoEncoderConfig* encoder_config,
    test::FrameGeneratorCapturer** frame_generator) {}

void MultiStreamTester::UpdateReceiveConfig(
    size_t stream_index,
    VideoReceiveStream::Config* receive_config) {}

test::DirectTransport* MultiStreamTester::CreateSendTransport(
    test::SingleThreadedTaskQueueForTesting* task_queue,
    Call* sender_call) {
  return new test::DirectTransport(
      task_queue,
      absl::make_unique<FakeNetworkPipe>(
          Clock::GetRealTimeClock(),
          absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig())),
      sender_call, payload_type_map_);
}

test::DirectTransport* MultiStreamTester::CreateReceiveTransport(
    test::SingleThreadedTaskQueueForTesting* task_queue,
    Call* receiver_call) {
  return new test::DirectTransport(
      task_queue,
      absl::make_unique<FakeNetworkPipe>(
          Clock::GetRealTimeClock(),
          absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig())),
      receiver_call, payload_type_map_);
}
}  // namespace webrtc
