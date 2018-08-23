/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKEPEERCONNECTIONFORSTATS_H_
#define PC_TEST_FAKEPEERCONNECTIONFORSTATS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "media/base/fakemediaengine.h"
#include "pc/streamcollection.h"
#include "pc/test/fakedatachannelprovider.h"
#include "pc/test/fakepeerconnectionbase.h"

namespace webrtc {

// Fake VoiceMediaChannel where the result of GetStats can be configured.
class FakeVoiceMediaChannelForStats : public cricket::FakeVoiceMediaChannel {
 public:
  FakeVoiceMediaChannelForStats()
      : cricket::FakeVoiceMediaChannel(nullptr, cricket::AudioOptions()) {}

  void SetStats(const cricket::VoiceMediaInfo& voice_info) {
    stats_ = voice_info;
  }

  // VoiceMediaChannel overrides.
  bool GetStats(cricket::VoiceMediaInfo* info) override {
    if (stats_) {
      *info = *stats_;
      return true;
    }
    return false;
  }

 private:
  absl::optional<cricket::VoiceMediaInfo> stats_;
};

// Fake VideoMediaChannel where the result of GetStats can be configured.
class FakeVideoMediaChannelForStats : public cricket::FakeVideoMediaChannel {
 public:
  FakeVideoMediaChannelForStats()
      : cricket::FakeVideoMediaChannel(nullptr, cricket::VideoOptions()) {}

  void SetStats(const cricket::VideoMediaInfo& video_info) {
    stats_ = video_info;
  }

  // VideoMediaChannel overrides.
  bool GetStats(cricket::VideoMediaInfo* info) override {
    if (stats_) {
      *info = *stats_;
      return true;
    }
    return false;
  }

 private:
  absl::optional<cricket::VideoMediaInfo> stats_;
};

constexpr bool kDefaultRtcpMuxRequired = true;
constexpr bool kDefaultSrtpRequired = true;

// This class is intended to be fed into the StatsCollector and
// RTCStatsCollector so that the stats functionality can be unit tested.
// Individual tests can configure this fake as needed to simulate scenarios
// under which to test the stats collectors.
class FakePeerConnectionForStats : public FakePeerConnectionBase {
 public:
  // TODO(steveanton): Add support for specifying separate threads to test
  // multi-threading correctness.
  FakePeerConnectionForStats()
      : network_thread_(rtc::Thread::Current()),
        worker_thread_(rtc::Thread::Current()),
        signaling_thread_(rtc::Thread::Current()),
        local_streams_(StreamCollection::Create()),
        remote_streams_(StreamCollection::Create()) {}

  rtc::scoped_refptr<StreamCollection> mutable_local_streams() {
    return local_streams_;
  }

  rtc::scoped_refptr<StreamCollection> mutable_remote_streams() {
    return remote_streams_;
  }

  void AddSender(rtc::scoped_refptr<RtpSenderInternal> sender) {
    // TODO(steveanton): Switch tests to use RtpTransceivers directly.
    auto sender_proxy = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread_, sender);
    GetOrCreateFirstTransceiverOfType(sender->media_type())
        ->internal()
        ->AddSender(sender_proxy);
  }

  void AddReceiver(rtc::scoped_refptr<RtpReceiverInternal> receiver) {
    // TODO(steveanton): Switch tests to use RtpTransceivers directly.
    auto receiver_proxy =
        RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
            signaling_thread_, receiver);
    GetOrCreateFirstTransceiverOfType(receiver->media_type())
        ->internal()
        ->AddReceiver(receiver_proxy);
  }

  FakeVoiceMediaChannelForStats* AddVoiceChannel(
      const std::string& mid,
      const std::string& transport_name) {
    RTC_DCHECK(!voice_channel_);
    auto voice_media_channel =
        absl::make_unique<FakeVoiceMediaChannelForStats>();
    auto* voice_media_channel_ptr = voice_media_channel.get();
    voice_channel_ = absl::make_unique<cricket::VoiceChannel>(
        worker_thread_, network_thread_, signaling_thread_, nullptr,
        std::move(voice_media_channel), mid, kDefaultSrtpRequired,
        rtc::CryptoOptions());
    voice_channel_->set_transport_name_for_testing(transport_name);
    GetOrCreateFirstTransceiverOfType(cricket::MEDIA_TYPE_AUDIO)
        ->internal()
        ->SetChannel(voice_channel_.get());
    return voice_media_channel_ptr;
  }

  FakeVideoMediaChannelForStats* AddVideoChannel(
      const std::string& mid,
      const std::string& transport_name) {
    RTC_DCHECK(!video_channel_);
    auto video_media_channel =
        absl::make_unique<FakeVideoMediaChannelForStats>();
    auto video_media_channel_ptr = video_media_channel.get();
    video_channel_ = absl::make_unique<cricket::VideoChannel>(
        worker_thread_, network_thread_, signaling_thread_,
        std::move(video_media_channel), mid, kDefaultSrtpRequired,
        rtc::CryptoOptions());
    video_channel_->set_transport_name_for_testing(transport_name);
    GetOrCreateFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
        ->internal()
        ->SetChannel(video_channel_.get());
    return video_media_channel_ptr;
  }

  void AddLocalTrack(uint32_t ssrc, const std::string& track_id) {
    local_track_id_by_ssrc_[ssrc] = track_id;
  }

  void AddRemoteTrack(uint32_t ssrc, const std::string& track_id) {
    remote_track_id_by_ssrc_[ssrc] = track_id;
  }

  void AddSctpDataChannel(const std::string& label) {
    AddSctpDataChannel(label, InternalDataChannelInit());
  }

  void AddSctpDataChannel(const std::string& label,
                          const InternalDataChannelInit& init) {
    AddSctpDataChannel(DataChannel::Create(&data_channel_provider_,
                                           cricket::DCT_SCTP, label, init));
  }

  void AddSctpDataChannel(rtc::scoped_refptr<DataChannel> data_channel) {
    sctp_data_channels_.push_back(data_channel);
  }

  void SetTransportStats(const std::string& transport_name,
                         const cricket::TransportChannelStats& channel_stats) {
    SetTransportStats(
        transport_name,
        std::vector<cricket::TransportChannelStats>{channel_stats});
  }

  void SetTransportStats(
      const std::string& transport_name,
      const std::vector<cricket::TransportChannelStats>& channel_stats_list) {
    cricket::TransportStats transport_stats;
    transport_stats.transport_name = transport_name;
    transport_stats.channel_stats = channel_stats_list;
    transport_stats_by_name_[transport_name] = transport_stats;
  }

  void SetCallStats(const Call::Stats& call_stats) { call_stats_ = call_stats; }

  void SetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate> certificate) {
    local_certificates_by_transport_[transport_name] = certificate;
  }

  void SetRemoteCertChain(const std::string& transport_name,
                          std::unique_ptr<rtc::SSLCertChain> chain) {
    remote_cert_chains_by_transport_[transport_name] = std::move(chain);
  }

  // PeerConnectionInterface overrides.

  rtc::scoped_refptr<StreamCollectionInterface> local_streams() override {
    return local_streams_;
  }

  rtc::scoped_refptr<StreamCollectionInterface> remote_streams() override {
    return remote_streams_;
  }

  std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const override {
    std::vector<rtc::scoped_refptr<RtpSenderInterface>> senders;
    for (auto transceiver : transceivers_) {
      for (auto sender : transceiver->internal()->senders()) {
        senders.push_back(sender);
      }
    }
    return senders;
  }

  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const override {
    std::vector<rtc::scoped_refptr<RtpReceiverInterface>> receivers;
    for (auto transceiver : transceivers_) {
      for (auto receiver : transceiver->internal()->receivers()) {
        receivers.push_back(receiver);
      }
    }
    return receivers;
  }

  // PeerConnectionInternal overrides.

  rtc::Thread* network_thread() const override { return network_thread_; }

  rtc::Thread* worker_thread() const override { return worker_thread_; }

  rtc::Thread* signaling_thread() const override { return signaling_thread_; }

  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  GetTransceiversInternal() const override {
    return transceivers_;
  }

  bool GetLocalTrackIdBySsrc(uint32_t ssrc, std::string* track_id) override {
    auto it = local_track_id_by_ssrc_.find(ssrc);
    if (it != local_track_id_by_ssrc_.end()) {
      *track_id = it->second;
      return true;
    } else {
      return false;
    }
  }

  bool GetRemoteTrackIdBySsrc(uint32_t ssrc, std::string* track_id) override {
    auto it = remote_track_id_by_ssrc_.find(ssrc);
    if (it != remote_track_id_by_ssrc_.end()) {
      *track_id = it->second;
      return true;
    } else {
      return false;
    }
  }

  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels()
      const override {
    return sctp_data_channels_;
  }

  cricket::CandidateStatsList GetPooledCandidateStats() const override {
    return {};
  }

  std::map<std::string, std::string> GetTransportNamesByMid() const override {
    std::map<std::string, std::string> transport_names_by_mid;
    if (voice_channel_) {
      transport_names_by_mid[voice_channel_->content_name()] =
          voice_channel_->transport_name();
    }
    if (video_channel_) {
      transport_names_by_mid[video_channel_->content_name()] =
          video_channel_->transport_name();
    }
    return transport_names_by_mid;
  }

  std::map<std::string, cricket::TransportStats> GetTransportStatsByNames(
      const std::set<std::string>& transport_names) override {
    std::map<std::string, cricket::TransportStats> transport_stats_by_name;
    for (const std::string& transport_name : transport_names) {
      transport_stats_by_name[transport_name] =
          GetTransportStatsByName(transport_name);
    }
    return transport_stats_by_name;
  }

  Call::Stats GetCallStats() override { return call_stats_; }

  bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) override {
    auto it = local_certificates_by_transport_.find(transport_name);
    if (it != local_certificates_by_transport_.end()) {
      *certificate = it->second;
      return true;
    } else {
      return false;
    }
  }

  std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain(
      const std::string& transport_name) override {
    auto it = remote_cert_chains_by_transport_.find(transport_name);
    if (it != remote_cert_chains_by_transport_.end()) {
      return it->second->UniqueCopy();
    } else {
      return nullptr;
    }
  }

 private:
  cricket::TransportStats GetTransportStatsByName(
      const std::string& transport_name) {
    auto it = transport_stats_by_name_.find(transport_name);
    if (it != transport_stats_by_name_.end()) {
      // If specific transport stats have been specified, return those.
      return it->second;
    }
    // Otherwise, generate some dummy stats.
    cricket::TransportChannelStats channel_stats;
    channel_stats.component = cricket::ICE_CANDIDATE_COMPONENT_RTP;
    cricket::TransportStats transport_stats;
    transport_stats.transport_name = transport_name;
    transport_stats.channel_stats.push_back(channel_stats);
    return transport_stats;
  }

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetOrCreateFirstTransceiverOfType(cricket::MediaType media_type) {
    for (auto transceiver : transceivers_) {
      if (transceiver->internal()->media_type() == media_type) {
        return transceiver;
      }
    }
    auto transceiver = RtpTransceiverProxyWithInternal<RtpTransceiver>::Create(
        signaling_thread_, new RtpTransceiver(media_type));
    transceivers_.push_back(transceiver);
    return transceiver;
  }

  rtc::Thread* const network_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const signaling_thread_;

  rtc::scoped_refptr<StreamCollection> local_streams_;
  rtc::scoped_refptr<StreamCollection> remote_streams_;

  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
      transceivers_;

  FakeDataChannelProvider data_channel_provider_;

  std::unique_ptr<cricket::VoiceChannel> voice_channel_;
  std::unique_ptr<cricket::VideoChannel> video_channel_;
  std::map<uint32_t, std::string> local_track_id_by_ssrc_;
  std::map<uint32_t, std::string> remote_track_id_by_ssrc_;

  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_;

  std::map<std::string, cricket::TransportStats> transport_stats_by_name_;

  Call::Stats call_stats_;

  std::map<std::string, rtc::scoped_refptr<rtc::RTCCertificate>>
      local_certificates_by_transport_;
  std::map<std::string, std::unique_ptr<rtc::SSLCertChain>>
      remote_cert_chains_by_transport_;
};

}  // namespace webrtc

#endif  // PC_TEST_FAKEPEERCONNECTIONFORSTATS_H_
