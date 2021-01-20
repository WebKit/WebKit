/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/rtp_data_engine.h"

#include <string.h>

#include <memory>
#include <string>

#include "media/base/fake_network_interface.h"
#include "media/base/media_constants.h"
#include "media/base/rtp_utils.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"

class FakeDataReceiver : public sigslot::has_slots<> {
 public:
  FakeDataReceiver() : has_received_data_(false) {}

  void OnDataReceived(const cricket::ReceiveDataParams& params,
                      const char* data,
                      size_t len) {
    has_received_data_ = true;
    last_received_data_ = std::string(data, len);
    last_received_data_len_ = len;
    last_received_data_params_ = params;
  }

  bool has_received_data() const { return has_received_data_; }
  std::string last_received_data() const { return last_received_data_; }
  size_t last_received_data_len() const { return last_received_data_len_; }
  cricket::ReceiveDataParams last_received_data_params() const {
    return last_received_data_params_;
  }

 private:
  bool has_received_data_;
  std::string last_received_data_;
  size_t last_received_data_len_;
  cricket::ReceiveDataParams last_received_data_params_;
};

class RtpDataMediaChannelTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // Seed needed for each test to satisfy expectations.
    iface_.reset(new cricket::FakeNetworkInterface());
    dme_.reset(CreateEngine());
    receiver_.reset(new FakeDataReceiver());
  }

  void SetNow(double now) { clock_.SetTime(webrtc::Timestamp::Seconds(now)); }

  cricket::RtpDataEngine* CreateEngine() {
    cricket::RtpDataEngine* dme = new cricket::RtpDataEngine();
    return dme;
  }

  cricket::RtpDataMediaChannel* CreateChannel() {
    return CreateChannel(dme_.get());
  }

  cricket::RtpDataMediaChannel* CreateChannel(cricket::RtpDataEngine* dme) {
    cricket::MediaConfig config;
    cricket::RtpDataMediaChannel* channel =
        static_cast<cricket::RtpDataMediaChannel*>(dme->CreateChannel(config));
    channel->SetInterface(iface_.get());
    channel->SignalDataReceived.connect(receiver_.get(),
                                        &FakeDataReceiver::OnDataReceived);
    return channel;
  }

  FakeDataReceiver* receiver() { return receiver_.get(); }

  bool HasReceivedData() { return receiver_->has_received_data(); }

  std::string GetReceivedData() { return receiver_->last_received_data(); }

  size_t GetReceivedDataLen() { return receiver_->last_received_data_len(); }

  cricket::ReceiveDataParams GetReceivedDataParams() {
    return receiver_->last_received_data_params();
  }

  bool HasSentData(int count) { return (iface_->NumRtpPackets() > count); }

  std::string GetSentData(int index) {
    // Assume RTP header of length 12
    std::unique_ptr<const rtc::CopyOnWriteBuffer> packet(
        iface_->GetRtpPacket(index));
    if (packet->size() > 12) {
      return std::string(packet->data<char>() + 12, packet->size() - 12);
    } else {
      return "";
    }
  }

  cricket::RtpHeader GetSentDataHeader(int index) {
    std::unique_ptr<const rtc::CopyOnWriteBuffer> packet(
        iface_->GetRtpPacket(index));
    cricket::RtpHeader header;
    GetRtpHeader(packet->data(), packet->size(), &header);
    return header;
  }

 private:
  std::unique_ptr<cricket::RtpDataEngine> dme_;
  rtc::ScopedFakeClock clock_;
  std::unique_ptr<cricket::FakeNetworkInterface> iface_;
  std::unique_ptr<FakeDataReceiver> receiver_;
};

TEST_F(RtpDataMediaChannelTest, SetUnknownCodecs) {
  std::unique_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  cricket::DataCodec known_codec;
  known_codec.id = 103;
  known_codec.name = "google-data";
  cricket::DataCodec unknown_codec;
  unknown_codec.id = 104;
  unknown_codec.name = "unknown-data";

  cricket::DataSendParameters send_parameters_known;
  send_parameters_known.codecs.push_back(known_codec);
  cricket::DataRecvParameters recv_parameters_known;
  recv_parameters_known.codecs.push_back(known_codec);

  cricket::DataSendParameters send_parameters_unknown;
  send_parameters_unknown.codecs.push_back(unknown_codec);
  cricket::DataRecvParameters recv_parameters_unknown;
  recv_parameters_unknown.codecs.push_back(unknown_codec);

  cricket::DataSendParameters send_parameters_mixed;
  send_parameters_mixed.codecs.push_back(known_codec);
  send_parameters_mixed.codecs.push_back(unknown_codec);
  cricket::DataRecvParameters recv_parameters_mixed;
  recv_parameters_mixed.codecs.push_back(known_codec);
  recv_parameters_mixed.codecs.push_back(unknown_codec);

  EXPECT_TRUE(dmc->SetSendParameters(send_parameters_known));
  EXPECT_FALSE(dmc->SetSendParameters(send_parameters_unknown));
  EXPECT_TRUE(dmc->SetSendParameters(send_parameters_mixed));
  EXPECT_TRUE(dmc->SetRecvParameters(recv_parameters_known));
  EXPECT_FALSE(dmc->SetRecvParameters(recv_parameters_unknown));
  EXPECT_FALSE(dmc->SetRecvParameters(recv_parameters_mixed));
}

TEST_F(RtpDataMediaChannelTest, AddRemoveSendStream) {
  std::unique_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  cricket::StreamParams stream1;
  stream1.add_ssrc(41);
  EXPECT_TRUE(dmc->AddSendStream(stream1));
  cricket::StreamParams stream2;
  stream2.add_ssrc(42);
  EXPECT_TRUE(dmc->AddSendStream(stream2));

  EXPECT_TRUE(dmc->RemoveSendStream(41));
  EXPECT_TRUE(dmc->RemoveSendStream(42));
  EXPECT_FALSE(dmc->RemoveSendStream(43));
}

TEST_F(RtpDataMediaChannelTest, AddRemoveRecvStream) {
  std::unique_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  cricket::StreamParams stream1;
  stream1.add_ssrc(41);
  EXPECT_TRUE(dmc->AddRecvStream(stream1));
  cricket::StreamParams stream2;
  stream2.add_ssrc(42);
  EXPECT_TRUE(dmc->AddRecvStream(stream2));
  EXPECT_FALSE(dmc->AddRecvStream(stream2));

  EXPECT_TRUE(dmc->RemoveRecvStream(41));
  EXPECT_TRUE(dmc->RemoveRecvStream(42));
}

TEST_F(RtpDataMediaChannelTest, SendData) {
  std::unique_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  cricket::SendDataParams params;
  params.ssrc = 42;
  unsigned char data[] = "food";
  rtc::CopyOnWriteBuffer payload(data, 4);
  unsigned char padded_data[] = {
      0x00, 0x00, 0x00, 0x00, 'f', 'o', 'o', 'd',
  };
  cricket::SendDataResult result;

  // Not sending
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
  EXPECT_EQ(cricket::SDR_ERROR, result);
  EXPECT_FALSE(HasSentData(0));
  ASSERT_TRUE(dmc->SetSend(true));

  // Unknown stream name.
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
  EXPECT_EQ(cricket::SDR_ERROR, result);
  EXPECT_FALSE(HasSentData(0));

  cricket::StreamParams stream;
  stream.add_ssrc(42);
  ASSERT_TRUE(dmc->AddSendStream(stream));

  // Unknown codec;
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
  EXPECT_EQ(cricket::SDR_ERROR, result);
  EXPECT_FALSE(HasSentData(0));

  cricket::DataCodec codec;
  codec.id = 103;
  codec.name = cricket::kGoogleRtpDataCodecName;
  cricket::DataSendParameters parameters;
  parameters.codecs.push_back(codec);
  ASSERT_TRUE(dmc->SetSendParameters(parameters));

  // Length too large;
  std::string x10000(10000, 'x');
  EXPECT_FALSE(dmc->SendData(
      params, rtc::CopyOnWriteBuffer(x10000.data(), x10000.length()), &result));
  EXPECT_EQ(cricket::SDR_ERROR, result);
  EXPECT_FALSE(HasSentData(0));

  // Finally works!
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  ASSERT_TRUE(HasSentData(0));
  EXPECT_EQ(sizeof(padded_data), GetSentData(0).length());
  EXPECT_EQ(0, memcmp(padded_data, GetSentData(0).data(), sizeof(padded_data)));
  cricket::RtpHeader header0 = GetSentDataHeader(0);
  EXPECT_NE(0, header0.seq_num);
  EXPECT_NE(0U, header0.timestamp);
  EXPECT_EQ(header0.ssrc, 42U);
  EXPECT_EQ(header0.payload_type, 103);

  // Should bump timestamp by 180000 because the clock rate is 90khz.
  SetNow(2);

  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  ASSERT_TRUE(HasSentData(1));
  EXPECT_EQ(sizeof(padded_data), GetSentData(1).length());
  EXPECT_EQ(0, memcmp(padded_data, GetSentData(1).data(), sizeof(padded_data)));
  cricket::RtpHeader header1 = GetSentDataHeader(1);
  EXPECT_EQ(header1.ssrc, 42U);
  EXPECT_EQ(header1.payload_type, 103);
  EXPECT_EQ(static_cast<uint16_t>(header0.seq_num + 1),
            static_cast<uint16_t>(header1.seq_num));
  EXPECT_EQ(header0.timestamp + 180000, header1.timestamp);
}

TEST_F(RtpDataMediaChannelTest, SendDataRate) {
  std::unique_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  ASSERT_TRUE(dmc->SetSend(true));

  cricket::DataCodec codec;
  codec.id = 103;
  codec.name = cricket::kGoogleRtpDataCodecName;
  cricket::DataSendParameters parameters;
  parameters.codecs.push_back(codec);
  ASSERT_TRUE(dmc->SetSendParameters(parameters));

  cricket::StreamParams stream;
  stream.add_ssrc(42);
  ASSERT_TRUE(dmc->AddSendStream(stream));

  cricket::SendDataParams params;
  params.ssrc = 42;
  unsigned char data[] = "food";
  rtc::CopyOnWriteBuffer payload(data, 4);
  cricket::SendDataResult result;

  // With rtp overhead of 32 bytes, each one of our packets is 36
  // bytes, or 288 bits.  So, a limit of 872bps will allow 3 packets,
  // but not four.
  parameters.max_bandwidth_bps = 872;
  ASSERT_TRUE(dmc->SetSendParameters(parameters));

  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
  EXPECT_FALSE(dmc->SendData(params, payload, &result));

  SetNow(0.9);
  EXPECT_FALSE(dmc->SendData(params, payload, &result));

  SetNow(1.1);
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  SetNow(1.9);
  EXPECT_TRUE(dmc->SendData(params, payload, &result));

  SetNow(2.2);
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
}

TEST_F(RtpDataMediaChannelTest, ReceiveData) {
  // PT= 103, SN=2, TS=3, SSRC = 4, data = "abcde"
  unsigned char data[] = {0x80, 0x67, 0x00, 0x02, 0x00, 0x00, 0x00,
                          0x03, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00,
                          0x00, 0x00, 'a',  'b',  'c',  'd',  'e'};
  rtc::CopyOnWriteBuffer packet(data, sizeof(data));

  std::unique_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  // SetReceived not called.
  dmc->OnPacketReceived(packet, /* packet_time_us */ -1);
  EXPECT_FALSE(HasReceivedData());

  dmc->SetReceive(true);

  // Unknown payload id
  dmc->OnPacketReceived(packet, /* packet_time_us */ -1);
  EXPECT_FALSE(HasReceivedData());

  cricket::DataCodec codec;
  codec.id = 103;
  codec.name = cricket::kGoogleRtpDataCodecName;
  cricket::DataRecvParameters parameters;
  parameters.codecs.push_back(codec);
  ASSERT_TRUE(dmc->SetRecvParameters(parameters));

  // Unknown stream
  dmc->OnPacketReceived(packet, /* packet_time_us */ -1);
  EXPECT_FALSE(HasReceivedData());

  cricket::StreamParams stream;
  stream.add_ssrc(42);
  ASSERT_TRUE(dmc->AddRecvStream(stream));

  // Finally works!
  dmc->OnPacketReceived(packet, /* packet_time_us */ -1);
  EXPECT_TRUE(HasReceivedData());
  EXPECT_EQ("abcde", GetReceivedData());
  EXPECT_EQ(5U, GetReceivedDataLen());
}

TEST_F(RtpDataMediaChannelTest, InvalidRtpPackets) {
  unsigned char data[] = {0x80, 0x65, 0x00, 0x02};
  rtc::CopyOnWriteBuffer packet(data, sizeof(data));

  std::unique_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  // Too short
  dmc->OnPacketReceived(packet, /* packet_time_us */ -1);
  EXPECT_FALSE(HasReceivedData());
}
