/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"
#ifdef WEBRTC_ANDROID
#include "webrtc/pc/test/androidtestinitializer.h"
#endif
#include "webrtc/pc/test/peerconnectiontestwrapper.h"
// Notice that mockpeerconnectionobservers.h must be included after the above!
#include "webrtc/pc/test/mockpeerconnectionobservers.h"
#include "webrtc/test/mock_audio_decoder.h"
#include "webrtc/test/mock_audio_decoder_factory.h"

using testing::AtLeast;
using testing::Invoke;
using testing::StrictMock;
using testing::_;

using webrtc::DataChannelInterface;
using webrtc::FakeConstraints;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;

namespace {

const int kMaxWait = 10000;

}  // namespace

class PeerConnectionEndToEndTest
    : public sigslot::has_slots<>,
      public testing::Test {
 public:
  typedef std::vector<rtc::scoped_refptr<DataChannelInterface> >
      DataChannelList;

  PeerConnectionEndToEndTest() {
    RTC_CHECK(network_thread_.Start());
    RTC_CHECK(worker_thread_.Start());
    caller_ = new rtc::RefCountedObject<PeerConnectionTestWrapper>(
        "caller", &network_thread_, &worker_thread_);
    callee_ = new rtc::RefCountedObject<PeerConnectionTestWrapper>(
        "callee", &network_thread_, &worker_thread_);
    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun.l.google.com:19302";
    config_.servers.push_back(ice_server);

#ifdef WEBRTC_ANDROID
    webrtc::InitializeAndroidObjects();
#endif
  }

  void CreatePcs(
      const MediaConstraintsInterface* pc_constraints,
      rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory,
      rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory) {
    EXPECT_TRUE(caller_->CreatePc(
        pc_constraints, config_, audio_encoder_factory, audio_decoder_factory));
    EXPECT_TRUE(callee_->CreatePc(
        pc_constraints, config_, audio_encoder_factory, audio_decoder_factory));
    PeerConnectionTestWrapper::Connect(caller_.get(), callee_.get());

    caller_->SignalOnDataChannel.connect(
        this, &PeerConnectionEndToEndTest::OnCallerAddedDataChanel);
    callee_->SignalOnDataChannel.connect(
        this, &PeerConnectionEndToEndTest::OnCalleeAddedDataChannel);
  }

  void GetAndAddUserMedia() {
    FakeConstraints audio_constraints;
    FakeConstraints video_constraints;
    GetAndAddUserMedia(true, audio_constraints, true, video_constraints);
  }

  void GetAndAddUserMedia(bool audio, FakeConstraints audio_constraints,
                          bool video, FakeConstraints video_constraints) {
    caller_->GetAndAddUserMedia(audio, audio_constraints,
                                video, video_constraints);
    callee_->GetAndAddUserMedia(audio, audio_constraints,
                                video, video_constraints);
  }

  void Negotiate() {
    caller_->CreateOffer(NULL);
  }

  void WaitForCallEstablished() {
    caller_->WaitForCallEstablished();
    callee_->WaitForCallEstablished();
  }

  void WaitForConnection() {
    caller_->WaitForConnection();
    callee_->WaitForConnection();
  }

  void OnCallerAddedDataChanel(DataChannelInterface* dc) {
    caller_signaled_data_channels_.push_back(dc);
  }

  void OnCalleeAddedDataChannel(DataChannelInterface* dc) {
    callee_signaled_data_channels_.push_back(dc);
  }

  // Tests that |dc1| and |dc2| can send to and receive from each other.
  void TestDataChannelSendAndReceive(
      DataChannelInterface* dc1, DataChannelInterface* dc2) {
    std::unique_ptr<webrtc::MockDataChannelObserver> dc1_observer(
        new webrtc::MockDataChannelObserver(dc1));

    std::unique_ptr<webrtc::MockDataChannelObserver> dc2_observer(
        new webrtc::MockDataChannelObserver(dc2));

    static const std::string kDummyData = "abcdefg";
    webrtc::DataBuffer buffer(kDummyData);
    EXPECT_TRUE(dc1->Send(buffer));
    EXPECT_EQ_WAIT(kDummyData, dc2_observer->last_message(), kMaxWait);

    EXPECT_TRUE(dc2->Send(buffer));
    EXPECT_EQ_WAIT(kDummyData, dc1_observer->last_message(), kMaxWait);

    EXPECT_EQ(1U, dc1_observer->received_message_count());
    EXPECT_EQ(1U, dc2_observer->received_message_count());
  }

  void WaitForDataChannelsToOpen(DataChannelInterface* local_dc,
                                 const DataChannelList& remote_dc_list,
                                 size_t remote_dc_index) {
    EXPECT_EQ_WAIT(DataChannelInterface::kOpen, local_dc->state(), kMaxWait);

    EXPECT_TRUE_WAIT(remote_dc_list.size() > remote_dc_index, kMaxWait);
    EXPECT_EQ_WAIT(DataChannelInterface::kOpen,
                   remote_dc_list[remote_dc_index]->state(),
                   kMaxWait);
    EXPECT_EQ(local_dc->id(), remote_dc_list[remote_dc_index]->id());
  }

  void CloseDataChannels(DataChannelInterface* local_dc,
                         const DataChannelList& remote_dc_list,
                         size_t remote_dc_index) {
    local_dc->Close();
    EXPECT_EQ_WAIT(DataChannelInterface::kClosed, local_dc->state(), kMaxWait);
    EXPECT_EQ_WAIT(DataChannelInterface::kClosed,
                   remote_dc_list[remote_dc_index]->state(),
                   kMaxWait);
  }

 protected:
  rtc::Thread network_thread_;
  rtc::Thread worker_thread_;
  rtc::scoped_refptr<PeerConnectionTestWrapper> caller_;
  rtc::scoped_refptr<PeerConnectionTestWrapper> callee_;
  DataChannelList caller_signaled_data_channels_;
  DataChannelList callee_signaled_data_channels_;
  webrtc::PeerConnectionInterface::RTCConfiguration config_;
};

std::unique_ptr<webrtc::AudioDecoder> CreateForwardingMockDecoder(
    std::unique_ptr<webrtc::AudioDecoder> real_decoder) {
  class ForwardingMockDecoder : public StrictMock<webrtc::MockAudioDecoder> {
   public:
    ForwardingMockDecoder(std::unique_ptr<AudioDecoder> decoder)
        : decoder_(std::move(decoder)) {}

   private:
    std::unique_ptr<AudioDecoder> decoder_;
  };

  const auto dec = real_decoder.get();  // For lambda capturing.
  auto mock_decoder =
      rtc::MakeUnique<ForwardingMockDecoder>(std::move(real_decoder));
  EXPECT_CALL(*mock_decoder, Channels())
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke([dec] { return dec->Channels(); }));
  EXPECT_CALL(*mock_decoder, DecodeInternal(_, _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(
          Invoke([dec](const uint8_t* encoded, size_t encoded_len,
                       int sample_rate_hz, int16_t* decoded,
                       webrtc::AudioDecoder::SpeechType* speech_type) {
            return dec->Decode(encoded, encoded_len, sample_rate_hz,
                               std::numeric_limits<size_t>::max(), decoded,
                               speech_type);
          }));
  EXPECT_CALL(*mock_decoder, Die());
  EXPECT_CALL(*mock_decoder, HasDecodePlc()).WillRepeatedly(Invoke([dec] {
    return dec->HasDecodePlc();
  }));
  EXPECT_CALL(*mock_decoder, IncomingPacket(_, _, _, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke([dec](const uint8_t* payload, size_t payload_len,
                                   uint16_t rtp_sequence_number,
                                   uint32_t rtp_timestamp,
                                   uint32_t arrival_timestamp) {
        return dec->IncomingPacket(payload, payload_len, rtp_sequence_number,
                                   rtp_timestamp, arrival_timestamp);
      }));
  EXPECT_CALL(*mock_decoder, PacketDuration(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke([dec](const uint8_t* encoded, size_t encoded_len) {
        return dec->PacketDuration(encoded, encoded_len);
      }));
  EXPECT_CALL(*mock_decoder, SampleRateHz())
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke([dec] { return dec->SampleRateHz(); }));

  return std::move(mock_decoder);
}

rtc::scoped_refptr<webrtc::AudioDecoderFactory>
CreateForwardingMockDecoderFactory(
    webrtc::AudioDecoderFactory* real_decoder_factory) {
  rtc::scoped_refptr<webrtc::MockAudioDecoderFactory> mock_decoder_factory =
      new rtc::RefCountedObject<StrictMock<webrtc::MockAudioDecoderFactory>>;
  EXPECT_CALL(*mock_decoder_factory, GetSupportedDecoders())
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke([real_decoder_factory] {
        return real_decoder_factory->GetSupportedDecoders();
      }));
  EXPECT_CALL(*mock_decoder_factory, IsSupportedDecoder(_))
      .Times(AtLeast(1))
      .WillRepeatedly(
          Invoke([real_decoder_factory](const webrtc::SdpAudioFormat& format) {
            return real_decoder_factory->IsSupportedDecoder(format);
          }));
  EXPECT_CALL(*mock_decoder_factory, MakeAudioDecoderMock(_, _))
      .Times(AtLeast(2))
      .WillRepeatedly(
          Invoke([real_decoder_factory](
                     const webrtc::SdpAudioFormat& format,
                     std::unique_ptr<webrtc::AudioDecoder>* return_value) {
            auto real_decoder = real_decoder_factory->MakeAudioDecoder(format);
            *return_value =
                real_decoder
                    ? CreateForwardingMockDecoder(std::move(real_decoder))
                    : nullptr;
          }));
  return mock_decoder_factory;
}

// Disabled for TSan v2, see
// https://bugs.chromium.org/p/webrtc/issues/detail?id=4719 for details.
// Disabled for Mac, see
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5231 for details.
#if !defined(THREAD_SANITIZER) && !defined(WEBRTC_MAC)
TEST_F(PeerConnectionEndToEndTest, Call) {
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> real_decoder_factory =
      webrtc::CreateBuiltinAudioDecoderFactory();
  CreatePcs(nullptr, webrtc::CreateBuiltinAudioEncoderFactory(),
            CreateForwardingMockDecoderFactory(real_decoder_factory.get()));
  GetAndAddUserMedia();
  Negotiate();
  WaitForCallEstablished();
}
#endif // if !defined(THREAD_SANITIZER) && !defined(WEBRTC_MAC)

#if !defined(ADDRESS_SANITIZER)
TEST_F(PeerConnectionEndToEndTest, CallWithLegacySdp) {
  FakeConstraints pc_constraints;
  pc_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                              false);
  CreatePcs(&pc_constraints, webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::CreateBuiltinAudioDecoderFactory());
  GetAndAddUserMedia();
  Negotiate();
  WaitForCallEstablished();
}
#endif  // !defined(ADDRESS_SANITIZER)

#ifdef HAVE_SCTP
// Verifies that a DataChannel created before the negotiation can transition to
// "OPEN" and transfer data.
TEST_F(PeerConnectionEndToEndTest, CreateDataChannelBeforeNegotiate) {
  CreatePcs(nullptr, webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::MockAudioDecoderFactory::CreateEmptyFactory());

  webrtc::DataChannelInit init;
  rtc::scoped_refptr<DataChannelInterface> caller_dc(
      caller_->CreateDataChannel("data", init));
  rtc::scoped_refptr<DataChannelInterface> callee_dc(
      callee_->CreateDataChannel("data", init));

  Negotiate();
  WaitForConnection();

  WaitForDataChannelsToOpen(caller_dc, callee_signaled_data_channels_, 0);
  WaitForDataChannelsToOpen(callee_dc, caller_signaled_data_channels_, 0);

  TestDataChannelSendAndReceive(caller_dc, callee_signaled_data_channels_[0]);
  TestDataChannelSendAndReceive(callee_dc, caller_signaled_data_channels_[0]);

  CloseDataChannels(caller_dc, callee_signaled_data_channels_, 0);
  CloseDataChannels(callee_dc, caller_signaled_data_channels_, 0);
}

// Verifies that a DataChannel created after the negotiation can transition to
// "OPEN" and transfer data.
TEST_F(PeerConnectionEndToEndTest, CreateDataChannelAfterNegotiate) {
  CreatePcs(nullptr, webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::MockAudioDecoderFactory::CreateEmptyFactory());

  webrtc::DataChannelInit init;

  // This DataChannel is for creating the data content in the negotiation.
  rtc::scoped_refptr<DataChannelInterface> dummy(
      caller_->CreateDataChannel("data", init));
  Negotiate();
  WaitForConnection();

  // Wait for the data channel created pre-negotiation to be opened.
  WaitForDataChannelsToOpen(dummy, callee_signaled_data_channels_, 0);

  // Create new DataChannels after the negotiation and verify their states.
  rtc::scoped_refptr<DataChannelInterface> caller_dc(
      caller_->CreateDataChannel("hello", init));
  rtc::scoped_refptr<DataChannelInterface> callee_dc(
      callee_->CreateDataChannel("hello", init));

  WaitForDataChannelsToOpen(caller_dc, callee_signaled_data_channels_, 1);
  WaitForDataChannelsToOpen(callee_dc, caller_signaled_data_channels_, 0);

  TestDataChannelSendAndReceive(caller_dc, callee_signaled_data_channels_[1]);
  TestDataChannelSendAndReceive(callee_dc, caller_signaled_data_channels_[0]);

  CloseDataChannels(caller_dc, callee_signaled_data_channels_, 1);
  CloseDataChannels(callee_dc, caller_signaled_data_channels_, 0);
}

// Verifies that DataChannel IDs are even/odd based on the DTLS roles.
TEST_F(PeerConnectionEndToEndTest, DataChannelIdAssignment) {
  CreatePcs(nullptr, webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::MockAudioDecoderFactory::CreateEmptyFactory());

  webrtc::DataChannelInit init;
  rtc::scoped_refptr<DataChannelInterface> caller_dc_1(
      caller_->CreateDataChannel("data", init));
  rtc::scoped_refptr<DataChannelInterface> callee_dc_1(
      callee_->CreateDataChannel("data", init));

  Negotiate();
  WaitForConnection();

  EXPECT_EQ(1U, caller_dc_1->id() % 2);
  EXPECT_EQ(0U, callee_dc_1->id() % 2);

  rtc::scoped_refptr<DataChannelInterface> caller_dc_2(
      caller_->CreateDataChannel("data", init));
  rtc::scoped_refptr<DataChannelInterface> callee_dc_2(
      callee_->CreateDataChannel("data", init));

  EXPECT_EQ(1U, caller_dc_2->id() % 2);
  EXPECT_EQ(0U, callee_dc_2->id() % 2);
}

// Verifies that the message is received by the right remote DataChannel when
// there are multiple DataChannels.
TEST_F(PeerConnectionEndToEndTest,
       MessageTransferBetweenTwoPairsOfDataChannels) {
  CreatePcs(nullptr, webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::MockAudioDecoderFactory::CreateEmptyFactory());

  webrtc::DataChannelInit init;

  rtc::scoped_refptr<DataChannelInterface> caller_dc_1(
      caller_->CreateDataChannel("data", init));
  rtc::scoped_refptr<DataChannelInterface> caller_dc_2(
      caller_->CreateDataChannel("data", init));

  Negotiate();
  WaitForConnection();
  WaitForDataChannelsToOpen(caller_dc_1, callee_signaled_data_channels_, 0);
  WaitForDataChannelsToOpen(caller_dc_2, callee_signaled_data_channels_, 1);

  std::unique_ptr<webrtc::MockDataChannelObserver> dc_1_observer(
      new webrtc::MockDataChannelObserver(callee_signaled_data_channels_[0]));

  std::unique_ptr<webrtc::MockDataChannelObserver> dc_2_observer(
      new webrtc::MockDataChannelObserver(callee_signaled_data_channels_[1]));

  const std::string message_1 = "hello 1";
  const std::string message_2 = "hello 2";

  caller_dc_1->Send(webrtc::DataBuffer(message_1));
  EXPECT_EQ_WAIT(message_1, dc_1_observer->last_message(), kMaxWait);

  caller_dc_2->Send(webrtc::DataBuffer(message_2));
  EXPECT_EQ_WAIT(message_2, dc_2_observer->last_message(), kMaxWait);

  EXPECT_EQ(1U, dc_1_observer->received_message_count());
  EXPECT_EQ(1U, dc_2_observer->received_message_count());
}
#endif  // HAVE_SCTP

#ifdef HAVE_QUIC
// Test that QUIC data channels can be used and that messages go to the correct
// remote data channel when both peers want to use QUIC. It is assumed that the
// application has externally negotiated the data channel parameters.
TEST_F(PeerConnectionEndToEndTest, MessageTransferBetweenQuicDataChannels) {
  config_.enable_quic = true;
  CreatePcs();

  webrtc::DataChannelInit init_1;
  init_1.id = 0;
  init_1.ordered = false;
  init_1.reliable = true;

  webrtc::DataChannelInit init_2;
  init_2.id = 1;
  init_2.ordered = false;
  init_2.reliable = true;

  rtc::scoped_refptr<DataChannelInterface> caller_dc_1(
      caller_->CreateDataChannel("data", init_1));
  ASSERT_NE(nullptr, caller_dc_1);
  rtc::scoped_refptr<DataChannelInterface> caller_dc_2(
      caller_->CreateDataChannel("data", init_2));
  ASSERT_NE(nullptr, caller_dc_2);
  rtc::scoped_refptr<DataChannelInterface> callee_dc_1(
      callee_->CreateDataChannel("data", init_1));
  ASSERT_NE(nullptr, callee_dc_1);
  rtc::scoped_refptr<DataChannelInterface> callee_dc_2(
      callee_->CreateDataChannel("data", init_2));
  ASSERT_NE(nullptr, callee_dc_2);

  Negotiate();
  WaitForConnection();
  EXPECT_TRUE_WAIT(caller_dc_1->state() == webrtc::DataChannelInterface::kOpen,
                   kMaxWait);
  EXPECT_TRUE_WAIT(callee_dc_1->state() == webrtc::DataChannelInterface::kOpen,
                   kMaxWait);
  EXPECT_TRUE_WAIT(caller_dc_2->state() == webrtc::DataChannelInterface::kOpen,
                   kMaxWait);
  EXPECT_TRUE_WAIT(callee_dc_2->state() == webrtc::DataChannelInterface::kOpen,
                   kMaxWait);

  std::unique_ptr<webrtc::MockDataChannelObserver> dc_1_observer(
      new webrtc::MockDataChannelObserver(callee_dc_1.get()));

  std::unique_ptr<webrtc::MockDataChannelObserver> dc_2_observer(
      new webrtc::MockDataChannelObserver(callee_dc_2.get()));

  const std::string message_1 = "hello 1";
  const std::string message_2 = "hello 2";

  // Send data from caller to callee.
  caller_dc_1->Send(webrtc::DataBuffer(message_1));
  EXPECT_EQ_WAIT(message_1, dc_1_observer->last_message(), kMaxWait);

  caller_dc_2->Send(webrtc::DataBuffer(message_2));
  EXPECT_EQ_WAIT(message_2, dc_2_observer->last_message(), kMaxWait);

  EXPECT_EQ(1U, dc_1_observer->received_message_count());
  EXPECT_EQ(1U, dc_2_observer->received_message_count());

  // Send data from callee to caller.
  dc_1_observer.reset(new webrtc::MockDataChannelObserver(caller_dc_1.get()));
  dc_2_observer.reset(new webrtc::MockDataChannelObserver(caller_dc_2.get()));

  callee_dc_1->Send(webrtc::DataBuffer(message_1));
  EXPECT_EQ_WAIT(message_1, dc_1_observer->last_message(), kMaxWait);

  callee_dc_2->Send(webrtc::DataBuffer(message_2));
  EXPECT_EQ_WAIT(message_2, dc_2_observer->last_message(), kMaxWait);

  EXPECT_EQ(1U, dc_1_observer->received_message_count());
  EXPECT_EQ(1U, dc_2_observer->received_message_count());
}
#endif  // HAVE_QUIC

#ifdef HAVE_SCTP
// Verifies that a DataChannel added from an OPEN message functions after
// a channel has been previously closed (webrtc issue 3778).
// This previously failed because the new channel re-uses the ID of the closed
// channel, and the closed channel was incorrectly still assigned to the id.
// TODO(deadbeef): This is disabled because there's currently a race condition
// caused by the fact that a data channel signals that it's closed before it
// really is. Re-enable this test once that's fixed.
// See: https://bugs.chromium.org/p/webrtc/issues/detail?id=4453
TEST_F(PeerConnectionEndToEndTest,
       DISABLED_DataChannelFromOpenWorksAfterClose) {
  CreatePcs(nullptr, webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::MockAudioDecoderFactory::CreateEmptyFactory());

  webrtc::DataChannelInit init;
  rtc::scoped_refptr<DataChannelInterface> caller_dc(
      caller_->CreateDataChannel("data", init));

  Negotiate();
  WaitForConnection();

  WaitForDataChannelsToOpen(caller_dc, callee_signaled_data_channels_, 0);
  CloseDataChannels(caller_dc, callee_signaled_data_channels_, 0);

  // Create a new channel and ensure it works after closing the previous one.
  caller_dc = caller_->CreateDataChannel("data2", init);

  WaitForDataChannelsToOpen(caller_dc, callee_signaled_data_channels_, 1);
  TestDataChannelSendAndReceive(caller_dc, callee_signaled_data_channels_[1]);

  CloseDataChannels(caller_dc, callee_signaled_data_channels_, 1);
}

// This tests that if a data channel is closed remotely while not referenced
// by the application (meaning only the PeerConnection contributes to its
// reference count), no memory access violation will occur.
// See: https://code.google.com/p/chromium/issues/detail?id=565048
TEST_F(PeerConnectionEndToEndTest, CloseDataChannelRemotelyWhileNotReferenced) {
  CreatePcs(nullptr, webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::MockAudioDecoderFactory::CreateEmptyFactory());

  webrtc::DataChannelInit init;
  rtc::scoped_refptr<DataChannelInterface> caller_dc(
      caller_->CreateDataChannel("data", init));

  Negotiate();
  WaitForConnection();

  WaitForDataChannelsToOpen(caller_dc, callee_signaled_data_channels_, 0);
  // This removes the reference to the remote data channel that we hold.
  callee_signaled_data_channels_.clear();
  caller_dc->Close();
  EXPECT_EQ_WAIT(DataChannelInterface::kClosed, caller_dc->state(), kMaxWait);

  // Wait for a bit longer so the remote data channel will receive the
  // close message and be destroyed.
  rtc::Thread::Current()->ProcessMessages(100);
}
#endif  // HAVE_SCTP
