/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/fakes/conference_transport.h"

#include <string>

#include "webrtc/base/byteorder.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/system_wrappers/include/sleep.h"

namespace {
  static const unsigned int kReflectorSsrc = 0x0000;
  static const unsigned int kLocalSsrc = 0x0001;
  static const unsigned int kFirstRemoteSsrc = 0x0002;
  static const webrtc::CodecInst kCodecInst =
      {120, "opus", 48000, 960, 2, 64000};
  static const int kAudioLevelHeaderId = 1;

  static unsigned int ParseRtcpSsrc(const void* data, size_t len) {
    const size_t ssrc_pos = 4;
    unsigned int ssrc = 0;
    if (len >= (ssrc_pos + sizeof(ssrc))) {
      ssrc = rtc::GetBE32(static_cast<const char*>(data) + ssrc_pos);
    }
    return ssrc;
  }
}  // namespace

namespace voetest {

ConferenceTransport::ConferenceTransport()
    : packet_event_(webrtc::EventWrapper::Create()),
      thread_(Run, this, "ConferenceTransport"),
      rtt_ms_(0),
      stream_count_(0),
      rtp_header_parser_(webrtc::RtpHeaderParser::Create()) {
  rtp_header_parser_->
      RegisterRtpHeaderExtension(webrtc::kRtpExtensionAudioLevel,
                                 kAudioLevelHeaderId);

  local_voe_ = webrtc::VoiceEngine::Create();
  local_base_ = webrtc::VoEBase::GetInterface(local_voe_);
  local_network_ = webrtc::VoENetwork::GetInterface(local_voe_);
  local_rtp_rtcp_ = webrtc::VoERTP_RTCP::GetInterface(local_voe_);

  // In principle, we can use one VoiceEngine to achieve the same goal. Well, in
  // here, we use two engines to make it more like reality.
  remote_voe_ = webrtc::VoiceEngine::Create();
  remote_base_ = webrtc::VoEBase::GetInterface(remote_voe_);
  remote_codec_ = webrtc::VoECodec::GetInterface(remote_voe_);
  remote_network_ = webrtc::VoENetwork::GetInterface(remote_voe_);
  remote_rtp_rtcp_ = webrtc::VoERTP_RTCP::GetInterface(remote_voe_);
  remote_file_ = webrtc::VoEFile::GetInterface(remote_voe_);

  EXPECT_EQ(0, local_base_->Init());
  local_sender_ = local_base_->CreateChannel();
  EXPECT_EQ(0, local_network_->RegisterExternalTransport(local_sender_, *this));
  EXPECT_EQ(0, local_rtp_rtcp_->SetLocalSSRC(local_sender_, kLocalSsrc));
  EXPECT_EQ(0, local_rtp_rtcp_->
      SetSendAudioLevelIndicationStatus(local_sender_, true,
                                        kAudioLevelHeaderId));

  EXPECT_EQ(0, local_base_->StartSend(local_sender_));

  EXPECT_EQ(0, remote_base_->Init());
  reflector_ = remote_base_->CreateChannel();
  EXPECT_EQ(0, remote_network_->RegisterExternalTransport(reflector_, *this));
  EXPECT_EQ(0, remote_rtp_rtcp_->SetLocalSSRC(reflector_, kReflectorSsrc));

  thread_.Start();
  thread_.SetPriority(rtc::kHighPriority);
}

ConferenceTransport::~ConferenceTransport() {
  // Must stop sending, otherwise DispatchPackets() cannot quit.
  EXPECT_EQ(0, remote_network_->DeRegisterExternalTransport(reflector_));
  EXPECT_EQ(0, local_network_->DeRegisterExternalTransport(local_sender_));

  while (!streams_.empty()) {
    auto stream = streams_.begin();
    RemoveStream(stream->first);
  }

  thread_.Stop();

  remote_file_->Release();
  remote_rtp_rtcp_->Release();
  remote_network_->Release();
  remote_base_->Release();

  local_rtp_rtcp_->Release();
  local_network_->Release();
  local_base_->Release();

  EXPECT_TRUE(webrtc::VoiceEngine::Delete(remote_voe_));
  EXPECT_TRUE(webrtc::VoiceEngine::Delete(local_voe_));
}

bool ConferenceTransport::SendRtp(const uint8_t* data,
                                  size_t len,
                                  const webrtc::PacketOptions& options) {
  StorePacket(Packet::Rtp, data, len);
  return true;
}

bool ConferenceTransport::SendRtcp(const uint8_t* data, size_t len) {
  StorePacket(Packet::Rtcp, data, len);
  return true;
}

int ConferenceTransport::GetReceiverChannelForSsrc(unsigned int sender_ssrc)
    const {
  rtc::CritScope lock(&stream_crit_);
  auto it = streams_.find(sender_ssrc);
  if (it != streams_.end()) {
    return it->second.second;
  }
  return -1;
}

void ConferenceTransport::StorePacket(Packet::Type type,
                                      const void* data,
                                      size_t len) {
  {
    rtc::CritScope lock(&pq_crit_);
    packet_queue_.push_back(Packet(type, data, len, rtc::TimeMillis()));
  }
  packet_event_->Set();
}

// This simulates the flow of RTP and RTCP packets. Complications like that
// a packet is first sent to the reflector, and then forwarded to the receiver
// are simplified, in this particular case, to a direct link between the sender
// and the receiver.
void ConferenceTransport::SendPacket(const Packet& packet) {
  int destination = -1;

  switch (packet.type_) {
    case Packet::Rtp: {
      webrtc::RTPHeader rtp_header;
      rtp_header_parser_->Parse(packet.data_, packet.len_, &rtp_header);
      if (rtp_header.ssrc == kLocalSsrc) {
        remote_network_->ReceivedRTPPacket(reflector_, packet.data_,
                                           packet.len_, webrtc::PacketTime());
      } else {
        if (loudest_filter_.ForwardThisPacket(rtp_header)) {
          destination = GetReceiverChannelForSsrc(rtp_header.ssrc);
          if (destination != -1) {
            local_network_->ReceivedRTPPacket(destination, packet.data_,
                                              packet.len_,
                                              webrtc::PacketTime());
          }
        }
      }
      break;
    }
    case Packet::Rtcp: {
      unsigned int sender_ssrc = ParseRtcpSsrc(packet.data_, packet.len_);
      if (sender_ssrc == kLocalSsrc) {
        remote_network_->ReceivedRTCPPacket(reflector_, packet.data_,
                                            packet.len_);
      } else if (sender_ssrc == kReflectorSsrc) {
        local_network_->ReceivedRTCPPacket(local_sender_, packet.data_,
                                           packet.len_);
      } else {
        destination = GetReceiverChannelForSsrc(sender_ssrc);
        if (destination != -1) {
          local_network_->ReceivedRTCPPacket(destination, packet.data_,
                                             packet.len_);
        }
      }
      break;
    }
  }
}

bool ConferenceTransport::DispatchPackets() {
  switch (packet_event_->Wait(1000)) {
    case webrtc::kEventSignaled:
      break;
    case webrtc::kEventTimeout:
      return true;
    case webrtc::kEventError:
      ADD_FAILURE() << "kEventError encountered.";
      return true;
  }

  while (true) {
    Packet packet;
    {
      rtc::CritScope lock(&pq_crit_);
      if (packet_queue_.empty())
        break;
      packet = packet_queue_.front();
      packet_queue_.pop_front();
    }

    int32_t elapsed_time_ms = rtc::TimeSince(packet.send_time_ms_);
    int32_t sleep_ms = rtt_ms_ / 2 - elapsed_time_ms;
    if (sleep_ms > 0) {
      // Every packet should be delayed by half of RTT.
      webrtc::SleepMs(sleep_ms);
    }

    SendPacket(packet);
  }
  return true;
}

void ConferenceTransport::SetRtt(unsigned int rtt_ms) {
  rtt_ms_ = rtt_ms;
}

unsigned int ConferenceTransport::AddStream(std::string file_name,
                                            webrtc::FileFormats format) {
  const int new_sender = remote_base_->CreateChannel();
  EXPECT_EQ(0, remote_network_->RegisterExternalTransport(new_sender, *this));

  const unsigned int remote_ssrc = kFirstRemoteSsrc + stream_count_++;
  EXPECT_EQ(0, remote_rtp_rtcp_->SetLocalSSRC(new_sender, remote_ssrc));
  EXPECT_EQ(0, remote_rtp_rtcp_->
      SetSendAudioLevelIndicationStatus(new_sender, true, kAudioLevelHeaderId));

  EXPECT_EQ(0, remote_codec_->SetSendCodec(new_sender, kCodecInst));
  EXPECT_EQ(0, remote_base_->StartSend(new_sender));
  EXPECT_EQ(0, remote_file_->StartPlayingFileAsMicrophone(
      new_sender, file_name.c_str(), true, false, format, 1.0));

  const int new_receiver = local_base_->CreateChannel();
  EXPECT_EQ(0, local_base_->AssociateSendChannel(new_receiver, local_sender_));

  EXPECT_EQ(0, local_network_->RegisterExternalTransport(new_receiver, *this));
  // Receive channels have to have the same SSRC in order to send receiver
  // reports with this SSRC.
  EXPECT_EQ(0, local_rtp_rtcp_->SetLocalSSRC(new_receiver, kLocalSsrc));

  {
    rtc::CritScope lock(&stream_crit_);
    streams_[remote_ssrc] = std::make_pair(new_sender, new_receiver);
  }
  return remote_ssrc;  // remote ssrc used as stream id.
}

bool ConferenceTransport::RemoveStream(unsigned int id) {
  rtc::CritScope lock(&stream_crit_);
  auto it = streams_.find(id);
  if (it == streams_.end()) {
    return false;
  }
  EXPECT_EQ(0, remote_network_->
      DeRegisterExternalTransport(it->second.second));
  EXPECT_EQ(0, local_network_->
      DeRegisterExternalTransport(it->second.first));
  EXPECT_EQ(0, remote_base_->DeleteChannel(it->second.second));
  EXPECT_EQ(0, local_base_->DeleteChannel(it->second.first));
  streams_.erase(it);
  return true;
}

bool ConferenceTransport::StartPlayout(unsigned int id) {
  int dst = GetReceiverChannelForSsrc(id);
  if (dst == -1) {
    return false;
  }
  EXPECT_EQ(0, local_base_->StartPlayout(dst));
  return true;
}

bool ConferenceTransport::GetReceiverStatistics(unsigned int id,
                                                webrtc::CallStatistics* stats) {
  int dst = GetReceiverChannelForSsrc(id);
  if (dst == -1) {
    return false;
  }
  EXPECT_EQ(0, local_rtp_rtcp_->GetRTCPStatistics(dst, *stats));
  return true;
}
}  // namespace voetest
