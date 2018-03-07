/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <functional>
#include <list>
#include <memory>
#include <string>

#include "call/call.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread_annotations.h"
#include "test/call_test.h"
#include "test/direct_transport.h"
#include "test/encoder_settings.h"
#include "test/fake_decoder.h"
#include "test/fake_encoder.h"
#include "test/frame_generator_capturer.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
// Note: If you consider to re-use this class, think twice and instead consider
// writing tests that don't depend on the logging system.
class LogObserver {
 public:
  LogObserver() { rtc::LogMessage::AddLogToStream(&callback_, rtc::LS_INFO); }

  ~LogObserver() { rtc::LogMessage::RemoveLogToStream(&callback_); }

  void PushExpectedLogLine(const std::string& expected_log_line) {
    callback_.PushExpectedLogLine(expected_log_line);
  }

  bool Wait() { return callback_.Wait(); }

 private:
  class Callback : public rtc::LogSink {
   public:
    Callback() : done_(false, false) {}

    void OnLogMessage(const std::string& message) override {
      rtc::CritScope lock(&crit_sect_);
      // Ignore log lines that are due to missing AST extensions, these are
      // logged when we switch back from AST to TOF until the wrapping bitrate
      // estimator gives up on using AST.
      if (message.find("BitrateEstimator") != std::string::npos &&
          message.find("packet is missing") == std::string::npos) {
        received_log_lines_.push_back(message);
      }

      int num_popped = 0;
      while (!received_log_lines_.empty() && !expected_log_lines_.empty()) {
        std::string a = received_log_lines_.front();
        std::string b = expected_log_lines_.front();
        received_log_lines_.pop_front();
        expected_log_lines_.pop_front();
        num_popped++;
        EXPECT_TRUE(a.find(b) != std::string::npos) << a << " != " << b;
      }
      if (expected_log_lines_.size() <= 0) {
        if (num_popped > 0) {
          done_.Set();
        }
        return;
      }
    }

    bool Wait() { return done_.Wait(test::CallTest::kDefaultTimeoutMs); }

    void PushExpectedLogLine(const std::string& expected_log_line) {
      rtc::CritScope lock(&crit_sect_);
      expected_log_lines_.push_back(expected_log_line);
    }

   private:
    typedef std::list<std::string> Strings;
    rtc::CriticalSection crit_sect_;
    Strings received_log_lines_ RTC_GUARDED_BY(crit_sect_);
    Strings expected_log_lines_ RTC_GUARDED_BY(crit_sect_);
    rtc::Event done_;
  };

  Callback callback_;
};
}  // namespace

static const int kTOFExtensionId = 4;
static const int kASTExtensionId = 5;

class BitrateEstimatorTest : public test::CallTest {
 public:
  BitrateEstimatorTest() : receive_config_(nullptr) {}

  virtual ~BitrateEstimatorTest() { EXPECT_TRUE(streams_.empty()); }

  virtual void SetUp() {
    task_queue_.SendTask([this]() {
      Call::Config config(event_log_.get());
      receiver_call_.reset(Call::Create(config));
      sender_call_.reset(Call::Create(config));

      send_transport_.reset(new test::DirectTransport(
          &task_queue_, sender_call_.get(), payload_type_map_));
      send_transport_->SetReceiver(receiver_call_->Receiver());
      receive_transport_.reset(new test::DirectTransport(
          &task_queue_, receiver_call_.get(), payload_type_map_));
      receive_transport_->SetReceiver(sender_call_->Receiver());

      video_send_config_ = VideoSendStream::Config(send_transport_.get());
      video_send_config_.rtp.ssrcs.push_back(kVideoSendSsrcs[0]);
      // Encoders will be set separately per stream.
      video_send_config_.encoder_settings.encoder = nullptr;
      video_send_config_.encoder_settings.payload_name = "FAKE";
      video_send_config_.encoder_settings.payload_type =
          kFakeVideoSendPayloadType;
      test::FillEncoderConfiguration(1, &video_encoder_config_);

      receive_config_ = VideoReceiveStream::Config(receive_transport_.get());
      // receive_config_.decoders will be set by every stream separately.
      receive_config_.rtp.remote_ssrc = video_send_config_.rtp.ssrcs[0];
      receive_config_.rtp.local_ssrc = kReceiverLocalVideoSsrc;
      receive_config_.rtp.remb = true;
      receive_config_.rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTimestampOffsetUri, kTOFExtensionId));
      receive_config_.rtp.extensions.push_back(
          RtpExtension(RtpExtension::kAbsSendTimeUri, kASTExtensionId));
    });
  }

  virtual void TearDown() {
    task_queue_.SendTask([this]() {
      std::for_each(streams_.begin(), streams_.end(),
                    std::mem_fun(&Stream::StopSending));

      while (!streams_.empty()) {
        delete streams_.back();
        streams_.pop_back();
      }

      send_transport_.reset();
      receive_transport_.reset();

      receiver_call_.reset();
      sender_call_.reset();
    });
  }

 protected:
  friend class Stream;

  class Stream {
   public:
    explicit Stream(BitrateEstimatorTest* test)
        : test_(test),
          is_sending_receiving_(false),
          send_stream_(nullptr),
          frame_generator_capturer_(),
          fake_encoder_(Clock::GetRealTimeClock()),
          fake_decoder_() {
      test_->video_send_config_.rtp.ssrcs[0]++;
      test_->video_send_config_.encoder_settings.encoder = &fake_encoder_;
      send_stream_ = test_->sender_call_->CreateVideoSendStream(
          test_->video_send_config_.Copy(),
          test_->video_encoder_config_.Copy());
      RTC_DCHECK_EQ(1, test_->video_encoder_config_.number_of_streams);
      frame_generator_capturer_.reset(test::FrameGeneratorCapturer::Create(
          kDefaultWidth, kDefaultHeight, kDefaultFramerate,
          Clock::GetRealTimeClock()));
      send_stream_->SetSource(
          frame_generator_capturer_.get(),
          VideoSendStream::DegradationPreference::kMaintainFramerate);
      send_stream_->Start();
      frame_generator_capturer_->Start();

      VideoReceiveStream::Decoder decoder;
      decoder.decoder = &fake_decoder_;
      decoder.payload_type =
          test_->video_send_config_.encoder_settings.payload_type;
      decoder.payload_name =
          test_->video_send_config_.encoder_settings.payload_name;
      test_->receive_config_.decoders.clear();
      test_->receive_config_.decoders.push_back(decoder);
      test_->receive_config_.rtp.remote_ssrc =
          test_->video_send_config_.rtp.ssrcs[0];
      test_->receive_config_.rtp.local_ssrc++;
      test_->receive_config_.renderer = &test->fake_renderer_;
      video_receive_stream_ = test_->receiver_call_->CreateVideoReceiveStream(
          test_->receive_config_.Copy());
      video_receive_stream_->Start();
      is_sending_receiving_ = true;
    }

    ~Stream() {
      EXPECT_FALSE(is_sending_receiving_);
      test_->sender_call_->DestroyVideoSendStream(send_stream_);
      frame_generator_capturer_.reset(nullptr);
      send_stream_ = nullptr;
      if (video_receive_stream_) {
        test_->receiver_call_->DestroyVideoReceiveStream(video_receive_stream_);
        video_receive_stream_ = nullptr;
      }
    }

    void StopSending() {
      if (is_sending_receiving_) {
        frame_generator_capturer_->Stop();
        send_stream_->Stop();
        if (video_receive_stream_) {
          video_receive_stream_->Stop();
        }
        is_sending_receiving_ = false;
      }
    }

   private:
    BitrateEstimatorTest* test_;
    bool is_sending_receiving_;
    VideoSendStream* send_stream_;
    VideoReceiveStream* video_receive_stream_;
    std::unique_ptr<test::FrameGeneratorCapturer> frame_generator_capturer_;
    test::FakeEncoder fake_encoder_;
    test::FakeDecoder fake_decoder_;
  };

  LogObserver receiver_log_;
  std::unique_ptr<test::DirectTransport> send_transport_;
  std::unique_ptr<test::DirectTransport> receive_transport_;
  std::unique_ptr<Call> sender_call_;
  std::unique_ptr<Call> receiver_call_;
  VideoReceiveStream::Config receive_config_;
  std::vector<Stream*> streams_;
};

static const char* kAbsSendTimeLog =
    "RemoteBitrateEstimatorAbsSendTime: Instantiating.";
static const char* kSingleStreamLog =
    "RemoteBitrateEstimatorSingleStream: Instantiating.";

TEST_F(BitrateEstimatorTest, InstantiatesTOFPerDefaultForVideo) {
  task_queue_.SendTask([this]() {
    video_send_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTimestampOffsetUri, kTOFExtensionId));
    receiver_log_.PushExpectedLogLine(kSingleStreamLog);
    receiver_log_.PushExpectedLogLine(kSingleStreamLog);
    streams_.push_back(new Stream(this));
  });
  EXPECT_TRUE(receiver_log_.Wait());
}

TEST_F(BitrateEstimatorTest, ImmediatelySwitchToASTForVideo) {
  task_queue_.SendTask([this]() {
    video_send_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTimeUri, kASTExtensionId));
    receiver_log_.PushExpectedLogLine(kSingleStreamLog);
    receiver_log_.PushExpectedLogLine(kSingleStreamLog);
    receiver_log_.PushExpectedLogLine("Switching to absolute send time RBE.");
    receiver_log_.PushExpectedLogLine(kAbsSendTimeLog);
    streams_.push_back(new Stream(this));
  });
  EXPECT_TRUE(receiver_log_.Wait());
}

TEST_F(BitrateEstimatorTest, SwitchesToASTForVideo) {
  task_queue_.SendTask([this]() {
    video_send_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTimestampOffsetUri, kTOFExtensionId));
    receiver_log_.PushExpectedLogLine(kSingleStreamLog);
    receiver_log_.PushExpectedLogLine(kSingleStreamLog);
    streams_.push_back(new Stream(this));
  });
  EXPECT_TRUE(receiver_log_.Wait());

  task_queue_.SendTask([this]() {
    video_send_config_.rtp.extensions[0] =
        RtpExtension(RtpExtension::kAbsSendTimeUri, kASTExtensionId);
    receiver_log_.PushExpectedLogLine("Switching to absolute send time RBE.");
    receiver_log_.PushExpectedLogLine(kAbsSendTimeLog);
    streams_.push_back(new Stream(this));
  });
  EXPECT_TRUE(receiver_log_.Wait());
}

// This test is flaky. See webrtc:5790.
TEST_F(BitrateEstimatorTest, DISABLED_SwitchesToASTThenBackToTOFForVideo) {
  task_queue_.SendTask([this]() {
    video_send_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTimestampOffsetUri, kTOFExtensionId));
    receiver_log_.PushExpectedLogLine(kSingleStreamLog);
    receiver_log_.PushExpectedLogLine(kAbsSendTimeLog);
    receiver_log_.PushExpectedLogLine(kSingleStreamLog);
    streams_.push_back(new Stream(this));
  });
  EXPECT_TRUE(receiver_log_.Wait());

  task_queue_.SendTask([this]() {
    video_send_config_.rtp.extensions[0] =
        RtpExtension(RtpExtension::kAbsSendTimeUri, kASTExtensionId);
    receiver_log_.PushExpectedLogLine(kAbsSendTimeLog);
    receiver_log_.PushExpectedLogLine("Switching to absolute send time RBE.");
    streams_.push_back(new Stream(this));
  });
  EXPECT_TRUE(receiver_log_.Wait());

  task_queue_.SendTask([this]() {
    video_send_config_.rtp.extensions[0] =
        RtpExtension(RtpExtension::kTimestampOffsetUri, kTOFExtensionId);
    receiver_log_.PushExpectedLogLine(kAbsSendTimeLog);
    receiver_log_.PushExpectedLogLine(
        "WrappingBitrateEstimator: Switching to transmission time offset RBE.");
    streams_.push_back(new Stream(this));
    streams_[0]->StopSending();
    streams_[1]->StopSending();
  });
  EXPECT_TRUE(receiver_log_.Wait());
}
}  // namespace webrtc
