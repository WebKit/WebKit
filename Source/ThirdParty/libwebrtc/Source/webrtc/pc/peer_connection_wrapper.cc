/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/peer_connection_wrapper.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "api/data_channel_interface.h"
#include "api/function_view.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_proxy.h"
#include "pc/test/fake_video_track_source.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/wait_until.h"

namespace webrtc {

using ::testing::Eq;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;

PeerConnectionWrapper::PeerConnectionWrapper(
    scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
    scoped_refptr<PeerConnectionInterface> pc,
    std::unique_ptr<MockPeerConnectionObserver> observer)
    : pc_factory_(std::move(pc_factory)),
      observer_(std::move(observer)),
      pc_(std::move(pc)) {
  RTC_DCHECK(pc_factory_);
  RTC_DCHECK(pc_);
  RTC_DCHECK(observer_);
  observer_->SetPeerConnectionInterface(pc_.get());
}

PeerConnectionWrapper::~PeerConnectionWrapper() {
  if (pc_)
    pc_->Close();
}

PeerConnectionFactoryInterface* PeerConnectionWrapper::pc_factory() {
  return pc_factory_.get();
}

PeerConnectionInterface* PeerConnectionWrapper::pc() {
  return pc_.get();
}

MockPeerConnectionObserver* PeerConnectionWrapper::observer() {
  return observer_.get();
}

PeerConnection* PeerConnectionWrapper::GetInternalPeerConnection() {
  auto* pci =
      static_cast<PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(
          pc());
  return static_cast<PeerConnection*>(pci->internal());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOffer() {
  return CreateOffer(RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface> PeerConnectionWrapper::CreateOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    std::string* error_out) {
  return CreateSdp(
      [this, options](CreateSessionDescriptionObserver* observer) {
        pc()->CreateOffer(observer, options);
      },
      error_out);
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOfferAndSetAsLocal() {
  return CreateOfferAndSetAsLocal(RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOfferAndSetAsLocal(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  std::unique_ptr<SessionDescriptionInterface> offer = CreateOffer(options);
  if (!offer) {
    return nullptr;
  }
  EXPECT_TRUE(SetLocalDescription(offer->Clone()));
  return offer;
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswer() {
  return CreateAnswer(RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    std::string* error_out) {
  return CreateSdp(
      [this, options](CreateSessionDescriptionObserver* observer) {
        pc()->CreateAnswer(observer, options);
      },
      error_out);
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswerAndSetAsLocal() {
  return CreateAnswerAndSetAsLocal(RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswerAndSetAsLocal(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  std::unique_ptr<SessionDescriptionInterface> answer = CreateAnswer(options);
  if (!answer) {
    return nullptr;
  }
  EXPECT_TRUE(SetLocalDescription(answer->Clone()));
  return answer;
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateRollback() {
  return CreateRollbackSessionDescription();
}

std::unique_ptr<SessionDescriptionInterface> PeerConnectionWrapper::CreateSdp(
    FunctionView<void(CreateSessionDescriptionObserver*)> fn,
    std::string* error_out) {
  // Tests in SdpMungingTest call this method from outside the signaling thread.
  const bool signaling_is_current =
      GetInternalPeerConnection()->signaling_thread()->IsCurrent();
  Event done;
  auto observer = make_ref_counted<MockCreateSessionDescriptionObserver>(
      [&]() { done.Set(); });
  fn(observer.get());
  if (signaling_is_current) {
    EXPECT_THAT(
        WaitUntil([&] { return observer->called(); }, ::testing::IsTrue()),
        IsRtcOk());
  } else {
    done.Wait(Event::kForever);
    EXPECT_TRUE(observer->called());
  }
  if (error_out && !observer->result()) {
    *error_out = observer->error();
  }
  return observer->MoveDescription();
}

bool PeerConnectionWrapper::SetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    std::string* error_out) {
  return SetSdp(
      [this, &desc](SetSessionDescriptionObserver* observer) {
        pc()->SetLocalDescription(observer, desc.release());
      },
      error_out);
}

bool PeerConnectionWrapper::SetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    RTCError* error_out) {
  auto observer = make_ref_counted<FakeSetLocalDescriptionObserver>();
  pc()->SetLocalDescription(std::move(desc), observer);
  EXPECT_THAT(
      WaitUntil([&] { return observer->called(); }, ::testing::IsTrue()),
      IsRtcOk());
  bool ok = observer->error().ok();
  if (error_out)
    *error_out = std::move(observer->error());
  return ok;
}

bool PeerConnectionWrapper::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    std::string* error_out) {
  return SetSdp(
      [this, &desc](SetSessionDescriptionObserver* observer) {
        pc()->SetRemoteDescription(observer, desc.release());
      },
      error_out);
}

bool PeerConnectionWrapper::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    RTCError* error_out) {
  auto observer = make_ref_counted<FakeSetRemoteDescriptionObserver>();
  pc()->SetRemoteDescription(std::move(desc), observer);
  EXPECT_THAT(
      WaitUntil([&] { return observer->called(); }, ::testing::IsTrue()),
      IsRtcOk());
  bool ok = observer->error().ok();
  if (error_out)
    *error_out = std::move(observer->error());
  return ok;
}

bool PeerConnectionWrapper::SetSdp(
    FunctionView<void(SetSessionDescriptionObserver*)> fn,
    std::string* error_out) {
  auto observer = make_ref_counted<MockSetSessionDescriptionObserver>();
  fn(observer.get());
  EXPECT_THAT(
      WaitUntil([&] { return observer->called(); }, ::testing::IsTrue()),
      IsRtcOk());
  if (error_out && !observer->result()) {
    *error_out = observer->error();
  }
  return observer->result();
}

bool PeerConnectionWrapper::ExchangeOfferAnswerWith(
    PeerConnectionWrapper* answerer) {
  return ExchangeOfferAnswerWith(answerer, RTCOfferAnswerOptions(),
                                 RTCOfferAnswerOptions());
}

bool PeerConnectionWrapper::ExchangeOfferAnswerWith(
    PeerConnectionWrapper* answerer,
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_options,
    const PeerConnectionInterface::RTCOfferAnswerOptions& answer_options) {
  RTC_DCHECK(answerer);
  if (answerer == this) {
    RTC_LOG(LS_ERROR) << "Cannot exchange offer/answer with ourself!";
    return false;
  }
  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateOffer(offer_options);
  EXPECT_TRUE(offer);
  if (!offer) {
    return false;
  }
  bool set_local_offer = SetLocalDescription(offer->Clone());
  EXPECT_TRUE(set_local_offer);
  if (!set_local_offer) {
    return false;
  }
  bool set_remote_offer = answerer->SetRemoteDescription(std::move(offer));
  EXPECT_TRUE(set_remote_offer);
  if (!set_remote_offer) {
    return false;
  }
  std::unique_ptr<SessionDescriptionInterface> answer =
      answerer->CreateAnswer(answer_options);
  EXPECT_TRUE(answer);
  if (!answer) {
    return false;
  }
  bool set_local_answer = answerer->SetLocalDescription(answer->Clone());
  EXPECT_TRUE(set_local_answer);
  if (!set_local_answer) {
    return false;
  }
  bool set_remote_answer = SetRemoteDescription(std::move(answer));
  EXPECT_TRUE(set_remote_answer);
  return set_remote_answer;
}

scoped_refptr<RtpTransceiverInterface> PeerConnectionWrapper::AddTransceiver(
    MediaType media_type) {
  RTCErrorOr<scoped_refptr<RtpTransceiverInterface>> result =
      pc()->AddTransceiver(media_type);
  EXPECT_EQ(RTCErrorType::NONE, result.error().type());
  return result.MoveValue();
}

scoped_refptr<RtpTransceiverInterface> PeerConnectionWrapper::AddTransceiver(
    MediaType media_type,
    const RtpTransceiverInit& init) {
  RTCErrorOr<scoped_refptr<RtpTransceiverInterface>> result =
      pc()->AddTransceiver(media_type, init);
  EXPECT_EQ(RTCErrorType::NONE, result.error().type());
  return result.MoveValue();
}

scoped_refptr<RtpTransceiverInterface> PeerConnectionWrapper::AddTransceiver(
    scoped_refptr<MediaStreamTrackInterface> track) {
  RTCErrorOr<scoped_refptr<RtpTransceiverInterface>> result =
      pc()->AddTransceiver(track);
  EXPECT_EQ(RTCErrorType::NONE, result.error().type());
  return result.MoveValue();
}

scoped_refptr<RtpTransceiverInterface> PeerConnectionWrapper::AddTransceiver(
    scoped_refptr<MediaStreamTrackInterface> track,
    const RtpTransceiverInit& init) {
  RTCErrorOr<scoped_refptr<RtpTransceiverInterface>> result =
      pc()->AddTransceiver(track, init);
  EXPECT_EQ(RTCErrorType::NONE, result.error().type());
  return result.MoveValue();
}

scoped_refptr<AudioTrackInterface> PeerConnectionWrapper::CreateAudioTrack(
    const std::string& label) {
  return pc_factory()->CreateAudioTrack(label, nullptr);
}

scoped_refptr<VideoTrackInterface> PeerConnectionWrapper::CreateVideoTrack(
    const std::string& label) {
  return pc_factory()->CreateVideoTrack(FakeVideoTrackSource::Create(), label);
}

scoped_refptr<RtpSenderInterface> PeerConnectionWrapper::AddTrack(
    scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids) {
  RTCErrorOr<scoped_refptr<RtpSenderInterface>> result =
      pc()->AddTrack(track, stream_ids);
  EXPECT_EQ(RTCErrorType::NONE, result.error().type());
  return result.MoveValue();
}

scoped_refptr<RtpSenderInterface> PeerConnectionWrapper::AddTrack(
    scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids,
    const std::vector<RtpEncodingParameters>& init_send_encodings) {
  RTCErrorOr<scoped_refptr<RtpSenderInterface>> result =
      pc()->AddTrack(track, stream_ids, init_send_encodings);
  EXPECT_EQ(RTCErrorType::NONE, result.error().type());
  return result.MoveValue();
}

scoped_refptr<RtpSenderInterface> PeerConnectionWrapper::AddAudioTrack(
    const std::string& track_label,
    const std::vector<std::string>& stream_ids) {
  return AddTrack(CreateAudioTrack(track_label), stream_ids);
}

scoped_refptr<RtpSenderInterface> PeerConnectionWrapper::AddVideoTrack(
    const std::string& track_label,
    const std::vector<std::string>& stream_ids) {
  return AddTrack(CreateVideoTrack(track_label), stream_ids);
}

scoped_refptr<DataChannelInterface> PeerConnectionWrapper::CreateDataChannel(
    const std::string& label,
    const std::optional<DataChannelInit>& config) {
  const DataChannelInit* config_ptr = config.has_value() ? &(*config) : nullptr;
  auto result = pc()->CreateDataChannelOrError(label, config_ptr);
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "CreateDataChannel failed: "
                      << ToString(result.error().type()) << " "
                      << result.error().message();
    return nullptr;
  }
  return result.MoveValue();
}

PeerConnectionInterface::SignalingState
PeerConnectionWrapper::signaling_state() {
  return pc()->signaling_state();
}

bool PeerConnectionWrapper::IsIceGatheringDone() {
  return observer()->ice_gathering_complete_;
}

bool PeerConnectionWrapper::IsIceConnected() {
  return observer()->ice_connected_;
}

scoped_refptr<const RTCStatsReport> PeerConnectionWrapper::GetStats() {
  auto callback = make_ref_counted<MockRTCStatsCollectorCallback>();
  pc()->GetStats(callback.get());
  EXPECT_THAT(
      WaitUntil([&] { return callback->called(); }, ::testing::IsTrue()),
      IsRtcOk());
  return callback->report();
}

bool PeerConnectionWrapper::RunUntilIceGatheringDone(test::RunLoop& run_loop,
                                                     TimeDelta timeout) {
  if (observer()->ice_gathering_complete()) {
    return true;
  }
  observer()->SetIceGatheringCompleteCallback(run_loop.QuitClosure());
  run_loop.RunFor(timeout);

  // Clear the callback just in case the timeout happened before it fired.
  observer()->SetIceGatheringCompleteCallback(nullptr);

  return observer()->ice_gathering_complete();
}

}  // namespace webrtc
