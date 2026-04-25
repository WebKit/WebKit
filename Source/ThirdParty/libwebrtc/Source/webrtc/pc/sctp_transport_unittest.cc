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

class FakeSctpTransportInternal : public SctpTransportInternal {
 public:
  explicit FakeSctpTransportInternal(DtlsTransportInternal* transport)
      : transport_(transport) {}

  void SetOnConnectedCallback(std::function<void()> callback) override {
    on_connected_callback_ = std::move(callback);
  }
  void SetDataChannelSink(DataChannelSink* sink) override {}
  DtlsTransportInternal* dtls_transport() const override { return transport_; }
  bool Start(const SctpOptions& options) override { return true; }
  bool OpenStream(int sid, PriorityValue priority) override { return true; }
  bool ResetStream(int sid) override { return true; }
  RTCError SendData(int sid,
                    const SendDataParams& params,
                    const CopyOnWriteBuffer& payload) override {
    return RTCError::OK();
  }
  bool ReadyToSendData() override { return true; }
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
  DtlsTransportInternal* const transport_;
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

  void TearDown() override {
    if (dtls_transport_ && internal_transport_) {
      internal_transport_->UnsubscribeDtlsTransportState(dtls_transport_.get());
      dtls_transport_->Clear(internal_transport_.get());
    }
  }

  void CreateTransport() {
    internal_transport_ = std::make_unique<FakeDtlsTransport>(
        "audio", ICE_CANDIDATE_COMPONENT_RTP);
    dtls_transport_ =
        make_ref_counted<DtlsTransport>(internal_transport_.get());
    internal_transport_->SubscribeDtlsTransportState(
        dtls_transport_.get(),
        [this](DtlsTransportInternal* transport, DtlsTransportState state) {
          dtls_transport_->OnInternalDtlsState(transport);
        });

    auto sctp_transport_internal = absl::WrapUnique(
        new FakeSctpTransportInternal(internal_transport_.get()));
    transport_ = make_ref_counted<SctpTransport>(
        std::move(sctp_transport_internal), dtls_transport_);
  }

  void CompleteSctpHandshake() {
    // The computed MaxChannels shall be the minimum of the outgoing
    // and incoming # of streams.
    MySctpTransportInternal()->set_max_outbound_streams(kTestMaxSctpStreams);
    MySctpTransportInternal()->set_max_inbound_streams(kTestMaxSctpStreams + 1);
    MySctpTransportInternal()->SendSignalAssociationChangeCommunicationUp();
  }

  FakeSctpTransportInternal* MySctpTransportInternal() {
    return static_cast<FakeSctpTransportInternal*>(transport_->internal());
  }

  AutoThread main_thread_;
  scoped_refptr<SctpTransport> transport_;
  scoped_refptr<DtlsTransport> dtls_transport_;
  std::unique_ptr<FakeDtlsTransport> internal_transport_;
  TestSctpTransportObserver observer_;
};

TEST(SctpTransportSimpleTest, CreateClearDelete) {
  AutoThread main_thread;
  std::unique_ptr<DtlsTransportInternal> internal_transport =
      std::make_unique<FakeDtlsTransport>("audio", ICE_CANDIDATE_COMPONENT_RTP);
  scoped_refptr<DtlsTransport> dtls_transport =
      make_ref_counted<DtlsTransport>(internal_transport.get());

  std::unique_ptr<SctpTransportInternal> fake_sctp_transport_internal =
      absl::WrapUnique(new FakeSctpTransportInternal(internal_transport.get()));
  scoped_refptr<SctpTransport> sctp_transport = make_ref_counted<SctpTransport>(
      std::move(fake_sctp_transport_internal), dtls_transport);
  ASSERT_TRUE(sctp_transport->internal());
  ASSERT_EQ(SctpTransportState::kConnecting,
            sctp_transport->Information().state());
  sctp_transport->Clear();
  ASSERT_FALSE(sctp_transport->internal());
  ASSERT_EQ(SctpTransportState::kClosed, sctp_transport->Information().state());
  dtls_transport->Clear(internal_transport.get());
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
  internal_transport_->SetDtlsState(DtlsTransportState::kClosed);
  ASSERT_THAT(WaitUntil([&] { return observer_.State(); },
                        ::testing::Eq(SctpTransportState::kClosed)),
              IsRtcOk());
}
}  // namespace webrtc
