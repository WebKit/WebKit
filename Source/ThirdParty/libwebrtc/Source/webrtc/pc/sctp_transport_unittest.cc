/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sctp_transport.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/dtls_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/priority.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sctp_transport_interface.h"
#include "api/test/rtc_error_matchers.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/sctp/sctp_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "p2p/dtls/fake_dtls_transport.h"
#include "pc/dtls_transport.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

constexpr int kTestMaxSctpStreams = 1234;

using ::testing::ElementsAre;

namespace {

class FakeCricketSctpTransport : public SctpTransportInternal {
 public:
  void SetOnConnectedCallback(std::function<void()> callback) override {
    on_connected_callback_ = std::move(callback);
  }
  void SetDataChannelSink(DataChannelSink* sink) override {}
  void SetDtlsTransport(DtlsTransportInternal* transport) override {}
  bool Start(const SctpOptions& options) override { return true; }
  bool OpenStream(int sid, PriorityValue priority) override { return true; }
  bool ResetStream(int sid) override { return true; }
  RTCError SendData(int sid,
                    const SendDataParams& params,
                    const CopyOnWriteBuffer& payload) override {
    return RTCError::OK();
  }
  bool ReadyToSendData() override { return true; }
  void set_debug_name_for_testing(const char* debug_name) override {}
  int max_message_size() const override { return 0; }
  std::optional<int> max_outbound_streams() const override {
    return max_outbound_streams_;
  }
  std::optional<int> max_inbound_streams() const override {
    return max_inbound_streams_;
  }
  size_t buffered_amount(int sid) const override { return 0; }
  size_t buffered_amount_low_threshold(int sid) const override { return 0; }
  void SetBufferedAmountLowThreshold(int sid, size_t bytes) override {}

  void SendSignalAssociationChangeCommunicationUp() {
    ASSERT_TRUE(on_connected_callback_);
    on_connected_callback_();
  }

  void set_max_outbound_streams(int streams) {
    max_outbound_streams_ = streams;
  }
  void set_max_inbound_streams(int streams) { max_inbound_streams_ = streams; }

 private:
  std::optional<int> max_outbound_streams_;
  std::optional<int> max_inbound_streams_;
  std::function<void()> on_connected_callback_;
};

}  // namespace

class TestSctpTransportObserver : public SctpTransportObserverInterface {
 public:
  TestSctpTransportObserver() : info_(SctpTransportState::kNew) {}

  void OnStateChange(SctpTransportInformation info) override {
    info_ = info;
    states_.push_back(info.state());
  }

  SctpTransportState State() {
    if (!states_.empty()) {
      return states_[states_.size() - 1];
    } else {
      return SctpTransportState::kNew;
    }
  }

  const std::vector<SctpTransportState>& States() { return states_; }

  SctpTransportInformation LastReceivedInformation() { return info_; }

 private:
  std::vector<SctpTransportState> states_;
  SctpTransportInformation info_;
};

class SctpTransportTest : public ::testing::Test {
 public:
  SctpTransport* transport() { return transport_.get(); }
  SctpTransportObserverInterface* observer() { return &observer_; }

  void CreateTransport() {
    std::unique_ptr<DtlsTransportInternal> cricket_transport =
        std::make_unique<FakeDtlsTransport>("audio",
                                            ICE_CANDIDATE_COMPONENT_RTP);
    dtls_transport_ =
        make_ref_counted<DtlsTransport>(std::move(cricket_transport));

    auto cricket_sctp_transport =
        absl::WrapUnique(new FakeCricketSctpTransport());
    transport_ = make_ref_counted<SctpTransport>(
        std::move(cricket_sctp_transport), dtls_transport_);
  }

  void CompleteSctpHandshake() {
    // The computed MaxChannels shall be the minimum of the outgoing
    // and incoming # of streams.
    CricketSctpTransport()->set_max_outbound_streams(kTestMaxSctpStreams);
    CricketSctpTransport()->set_max_inbound_streams(kTestMaxSctpStreams + 1);
    CricketSctpTransport()->SendSignalAssociationChangeCommunicationUp();
  }

  FakeCricketSctpTransport* CricketSctpTransport() {
    return static_cast<FakeCricketSctpTransport*>(transport_->internal());
  }

  AutoThread main_thread_;
  scoped_refptr<SctpTransport> transport_;
  scoped_refptr<DtlsTransport> dtls_transport_;
  TestSctpTransportObserver observer_;
};

TEST(SctpTransportSimpleTest, CreateClearDelete) {
  AutoThread main_thread;
  std::unique_ptr<DtlsTransportInternal> cricket_transport =
      std::make_unique<FakeDtlsTransport>("audio", ICE_CANDIDATE_COMPONENT_RTP);
  scoped_refptr<DtlsTransport> dtls_transport =
      make_ref_counted<DtlsTransport>(std::move(cricket_transport));

  std::unique_ptr<SctpTransportInternal> fake_cricket_sctp_transport =
      absl::WrapUnique(new FakeCricketSctpTransport());
  scoped_refptr<SctpTransport> sctp_transport = make_ref_counted<SctpTransport>(
      std::move(fake_cricket_sctp_transport), dtls_transport);
  ASSERT_TRUE(sctp_transport->internal());
  ASSERT_EQ(SctpTransportState::kConnecting,
            sctp_transport->Information().state());
  sctp_transport->Clear();
  ASSERT_FALSE(sctp_transport->internal());
  ASSERT_EQ(SctpTransportState::kClosed, sctp_transport->Information().state());
}

TEST_F(SctpTransportTest, EventsObservedWhenConnecting) {
  CreateTransport();
  transport()->RegisterObserver(observer());
  CompleteSctpHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.State(); },
                        ::testing::Eq(SctpTransportState::kConnected)),
              IsRtcOk());
  EXPECT_THAT(observer_.States(), ElementsAre(SctpTransportState::kConnected));
}

TEST_F(SctpTransportTest, CloseWhenClearing) {
  CreateTransport();
  transport()->RegisterObserver(observer());
  CompleteSctpHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.State(); },
                        ::testing::Eq(SctpTransportState::kConnected)),
              IsRtcOk());
  transport()->Clear();
  ASSERT_THAT(WaitUntil([&] { return observer_.State(); },
                        ::testing::Eq(SctpTransportState::kClosed)),
              IsRtcOk());
}

TEST_F(SctpTransportTest, MaxChannelsSignalled) {
  CreateTransport();
  transport()->RegisterObserver(observer());
  EXPECT_FALSE(transport()->Information().MaxChannels());
  EXPECT_FALSE(observer_.LastReceivedInformation().MaxChannels());
  CompleteSctpHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.State(); },
                        ::testing::Eq(SctpTransportState::kConnected)),
              IsRtcOk());
  EXPECT_TRUE(transport()->Information().MaxChannels());
  EXPECT_EQ(kTestMaxSctpStreams, *(transport()->Information().MaxChannels()));
  EXPECT_TRUE(observer_.LastReceivedInformation().MaxChannels());
  EXPECT_EQ(kTestMaxSctpStreams,
            *(observer_.LastReceivedInformation().MaxChannels()));
}

TEST_F(SctpTransportTest, CloseWhenTransportCloses) {
  CreateTransport();
  transport()->RegisterObserver(observer());
  CompleteSctpHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.State(); },
                        ::testing::Eq(SctpTransportState::kConnected)),
              IsRtcOk());
  static_cast<FakeDtlsTransport*>(dtls_transport_->internal())
      ->SetDtlsState(DtlsTransportState::kClosed);
  ASSERT_THAT(WaitUntil([&] { return observer_.State(); },
                        ::testing::Eq(SctpTransportState::kClosed)),
              IsRtcOk());
}
}  // namespace webrtc
