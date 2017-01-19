/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/system_wrappers/include/atomic32.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"
#include "webrtc/voice_engine/test/auto_test/voe_standard_test.h"

class TestRtpObserver : public webrtc::VoERTPObserver {
 public:
  TestRtpObserver() : changed_ssrc_event_(voetest::EventWrapper::Create()) {}
  virtual ~TestRtpObserver() {}
  virtual void OnIncomingCSRCChanged(int channel,
                                     unsigned int CSRC,
                                     bool added) {}
  virtual void OnIncomingSSRCChanged(int channel,
                                     unsigned int SSRC);
  void WaitForChangedSsrc() {
    // 10 seconds should be enough.
    EXPECT_EQ(voetest::kEventSignaled, changed_ssrc_event_->Wait(10*1000));
  }
  void SetIncomingSsrc(unsigned int ssrc) {
    rtc::CritScope lock(&crit_);
    incoming_ssrc_ = ssrc;
  }
 public:
  rtc::CriticalSection crit_;
  unsigned int incoming_ssrc_;
  std::unique_ptr<voetest::EventWrapper> changed_ssrc_event_;
};

void TestRtpObserver::OnIncomingSSRCChanged(int channel,
                                            unsigned int SSRC) {
  char msg[128];
  sprintf(msg, "\n=> OnIncomingSSRCChanged(channel=%d, SSRC=%u)\n", channel,
          SSRC);
  TEST_LOG("%s", msg);

  {
    rtc::CritScope lock(&crit_);
    if (incoming_ssrc_ == SSRC)
      changed_ssrc_event_->Set();
  }
}

static const char* const RTCP_CNAME = "Whatever";

class RtpRtcpTest : public AfterStreamingFixture {
 protected:
  void SetUp() {
    // We need a second channel for this test, so set it up.
    second_channel_ = voe_base_->CreateChannel();
    EXPECT_GE(second_channel_, 0);

    transport_ = new LoopBackTransport(voe_network_, second_channel_);
    EXPECT_EQ(0, voe_network_->RegisterExternalTransport(second_channel_,
                                                         *transport_));

    EXPECT_EQ(0, voe_base_->StartPlayout(second_channel_));
    EXPECT_EQ(0, voe_rtp_rtcp_->SetLocalSSRC(second_channel_, 5678));
    EXPECT_EQ(0, voe_base_->StartSend(second_channel_));

    // We'll set up the RTCP CNAME and SSRC to something arbitrary here.
    voe_rtp_rtcp_->SetRTCP_CNAME(channel_, RTCP_CNAME);
  }

  void TearDown() {
    EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(second_channel_));
    voe_base_->DeleteChannel(second_channel_);
    delete transport_;
  }

  int second_channel_;
  LoopBackTransport* transport_;
};

TEST_F(RtpRtcpTest, RemoteRtcpCnameHasPropagatedToRemoteSide) {
  if (!FLAGS_include_timing_dependent_tests) {
    TEST_LOG("Skipping test - running in slow execution environment...\n");
    return;
  }

  // We need to sleep a bit here for the name to propagate. For instance,
  // 200 milliseconds is not enough, so we'll go with one second here.
  Sleep(1000);

  char char_buffer[256];
  voe_rtp_rtcp_->GetRemoteRTCP_CNAME(channel_, char_buffer);
  EXPECT_STREQ(RTCP_CNAME, char_buffer);
}

TEST_F(RtpRtcpTest, SSRCPropagatesCorrectly) {
  unsigned int local_ssrc = 1234;
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_rtp_rtcp_->SetLocalSSRC(channel_, local_ssrc));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  Sleep(1000);

  unsigned int ssrc;
  EXPECT_EQ(0, voe_rtp_rtcp_->GetLocalSSRC(channel_, ssrc));
  EXPECT_EQ(local_ssrc, ssrc);

  EXPECT_EQ(0, voe_rtp_rtcp_->GetRemoteSSRC(channel_, ssrc));
  EXPECT_EQ(local_ssrc, ssrc);
}
