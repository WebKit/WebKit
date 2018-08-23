/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "rtc_base/file.h"
#include "test/call_test.h"
#include "test/function_video_encoder_factory.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

class LogEndToEndTest : public test::CallTest {
  void SetUp() { paths_.clear(); }
  void TearDown() {
    for (const auto& path : paths_) {
      rtc::RemoveFile(path);
    }
  }

 public:
  int AddFile() {
    paths_.push_back(test::TempFilename(test::OutputPath(), "test_file"));
    return static_cast<int>(paths_.size()) - 1;
  }

  rtc::PlatformFile OpenFile(int idx) {
    return rtc::OpenPlatformFile(paths_[idx]);
  }

  void LogSend(bool open) {
    if (open) {
      GetVideoSendStream()->EnableEncodedFrameRecording(
          std::vector<rtc::PlatformFile>(1, OpenFile(AddFile())), 0);
    } else {
      GetVideoSendStream()->DisableEncodedFrameRecording();
    }
  }
  void LogReceive(bool open) {
    if (open) {
      video_receive_streams_[0]->EnableEncodedFrameRecording(
          OpenFile(AddFile()), 0);
    } else {
      video_receive_streams_[0]->DisableEncodedFrameRecording();
    }
  }

  std::vector<std::string> paths_;
};

TEST_F(LogEndToEndTest, LogsEncodedFramesWhenRequested) {
  static const int kNumFramesToRecord = 10;
  class LogEncodingObserver : public test::EndToEndTest,
                              public EncodedFrameObserver {
   public:
    explicit LogEncodingObserver(LogEndToEndTest* fixture)
        : EndToEndTest(kDefaultTimeoutMs),
          fixture_(fixture),
          encoder_factory_([]() { return VP8Encoder::Create(); }),
          recorded_frames_(0) {}

    void PerformTest() override {
      fixture_->LogSend(true);
      fixture_->LogReceive(true);
      ASSERT_TRUE(Wait()) << "Timed out while waiting for frame logging.";
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      decoder_ = VP8Decoder::Create();

      send_config->post_encode_callback = this;
      send_config->rtp.payload_name = "VP8";
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      encoder_config->codec_type = kVideoCodecVP8;

      (*receive_configs)[0].decoders.resize(1);
      (*receive_configs)[0].decoders[0].payload_type =
          send_config->rtp.payload_type;
      (*receive_configs)[0].decoders[0].payload_name =
          send_config->rtp.payload_name;
      (*receive_configs)[0].decoders[0].decoder = decoder_.get();
    }

    void EncodedFrameCallback(const EncodedFrame& encoded_frame) override {
      rtc::CritScope lock(&crit_);
      if (recorded_frames_++ > kNumFramesToRecord) {
        fixture_->LogSend(false);
        fixture_->LogReceive(false);
        rtc::File send_file(fixture_->OpenFile(0));
        rtc::File receive_file(fixture_->OpenFile(1));
        uint8_t out[100];
        // If logging has worked correctly neither file should be empty, i.e.
        // we should be able to read something from them.
        EXPECT_LT(0u, send_file.Read(out, 100));
        EXPECT_LT(0u, receive_file.Read(out, 100));
        observation_complete_.Set();
      }
    }

   private:
    LogEndToEndTest* const fixture_;
    test::FunctionVideoEncoderFactory encoder_factory_;
    std::unique_ptr<VideoDecoder> decoder_;
    rtc::CriticalSection crit_;
    int recorded_frames_ RTC_GUARDED_BY(crit_);
  } test(this);

  RunBaseTest(&test);
}
}  // namespace webrtc
