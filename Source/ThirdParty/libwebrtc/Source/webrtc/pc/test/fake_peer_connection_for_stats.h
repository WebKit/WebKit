/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKE_PEER_CONNECTION_FOR_STATS_H_
#define PC_TEST_FAKE_PEER_CONNECTION_FOR_STATS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "media/base/fake_media_engine.h"
#include "pc/channel.h"
#include "pc/stream_collection.h"
#include "pc/test/fake_data_channel_controller.h"
#include "pc/test/fake_peer_connection_base.h"

namespace webrtc {

// Fake VoiceMediaChannel where the result of GetStats can be configured.
class FakeVoiceMediaChannelForStats : public cricket::FakeVoiceMediaChannel {
 public:
  explicit FakeVoiceMediaChannelForStats(TaskQueueBase* network_thread)
      : cricket::FakeVoiceMediaChannel(nullptr,
                                       cricket::AudioOptions(),
                                       network_thread) {}

  void SetStats(const cricket::VoiceMediaInfo& voice_info) {
    stats_ = voice_info;
  }

  // VoiceMediaChannel overrides.
  bool GetStats(cricket::VoiceMediaInfo* info,
                bool get_and_clear_legacy_stats) override {
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
  explicit FakeVideoMediaChannelForStats(TaskQueueBase* network_thread)
      : cricket::FakeVideoMediaChannel(nullptr,
                                       cricket::VideoOptions(),
                                       network_thread) {}

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

class VoiceChannelForTesting : public cricket::VoiceChannel {
 public:
  VoiceChannelForTesting(rtc::Thread* worker_thread,
                         rtc::Thread* network_thread,
                         rtc::Thread* signaling_thread,
                         std::unique_ptr<cricket::VoiceMediaChannel> channel,
                         const std::string& content_name,
                         bool srtp_required,
                         webrtc::CryptoOptions crypto_options,
                         rtc::UniqueRandomIdGenerator* ssrc_generator,
                         std::string transport_name)
      : VoiceChannel(worker_thread,
                     network_thread,
                     signaling_thread,
                     std::move(channel),
                     content_name,
                     srtp_required,
                     std::move(crypto_options),
                     ssrc_generator),
        test_transport_name_(std::move(transport_name)) {}

 private:
  absl::string_view transport_name() const override {
    return test_transport_name_;
  }

  const std::string test_transport_name_;
};

class VideoChannelForTesting : public cricket::VideoChannel {
 public:
  VideoChannelForTesting(rtc::Thread* worker_thread,
                         rtc::Thread* network_thread,
                         rtc::Thread* signaling_thread,
                         std::unique_ptr<cricket::VideoMediaChannel> channel,
                         const std::string& content_name,
                         bool srtp_required,
                         webrtc::CryptoOptions crypto_options,
                         rtc::UniqueRandomIdGenerator* ssrc_generator,
                         std::string transport_name)
      : VideoChannel(worker_thread,
                     network_thread,
                     signaling_thread,
                     std::move(channel),
                     content_name,
                     srtp_required,
                     std::move(crypto_options),
                     ssrc_generator),
        test_transport_name_(std::move(transport_name)) {}

 private:
  absl::string_view transport_name() const override {
    return test_transport_name_;
  }

  const std::string test_transport_name_;
};

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
        // TODO(hta): remove separate thread variables and use context.
        dependencies_(MakeDependencies()),
        context_(ConnectionContext::Create(&dependencies_)),
        local_streams_(StreamCollection::Create()),
        remote_streams_(StreamCollection::Create()) {}

  ~FakePeerConnectionForStats() {
    for (auto transceiver : transceivers_) {
      transceiver->internal()->ClearChannel();
    }
  }

  static PeerConnectionFactoryDependencies MakeDependencies() {
    PeerConnectionFactoryDependencies dependencies;
    dependencies.network_thread = rtc::Thread::Current();
    dependencies.worker_thread = rtc::Thread::Current();
    dependencies.signaling_thread = rtc::Thread::Current();
    return dependencies;
  }

  rtc::scoped_refptr<StreamCollection> mutable_local_streams() {
    return local_streams_;
  }

  rtc::scoped_refptr<StreamCollection> mutable_remote_streams() {
    return remote_streams_;
  }

  rtc::scoped_refptr<RtpSenderInterface> AddSender(
      rtc::scoped_refptr<RtpSenderInternal> sender) {
    // TODO(steveanton): Switch tests to use RtpTransceivers directly.
    auto sender_proxy = RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        signaling_thread_, sender);
    GetOrCreateFirstTransceiverOfType(sender->media_type())
        ->internal()
        ->AddSender(sender_proxy);
    return sender_proxy;
  }

  void RemoveSender(rtc::scoped_refptr<RtpSenderInterface> sender) {
    GetOrCreateFirstTransceiverOfType(sender->media_type())
        ->internal()
        ->RemoveSender(sender.get());
  }

  rtc::scoped_refptr<RtpReceiverInterface> AddReceiver(
      rtc::scoped_refptr<RtpReceiverInternal> receiver) {
    // TODO(steveanton): Switch tests to use RtpTransceivers directly.
    auto receiver_proxy =
        RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
            signaling_thread_, worker_thread_, receiver);
    GetOrCreateFirstTransceiverOfType(receiver->media_type())
        ->internal()
        ->AddReceiver(receiver_proxy);
    return receiver_proxy;
  }

  void RemoveReceiver(rtc::scoped_refptr<RtpReceiverInterface> receiver) {
    GetOrCreateFirstTransceiverOfType(receiver->media_type())
        ->internal()
        ->RemoveReceiver(receiver.get());
  }

  FakeVoiceMediaChannelForStats* AddVoiceChannel(
      const std::string& mid,
      const std::string& transport_name,
      cricket::VoiceMediaInfo initial_stats = cricket::VoiceMediaInfo()) {
    auto voice_media_channel =
        std::make_unique<FakeVoiceMediaChannelForStats>(network_thread_);
    auto* voice_media_channel_ptr = voice_media_channel.get();
    auto voice_channel = std::make_unique<VoiceChannelForTesting>(
        worker_thread_, network_thread_, signaling_thread_,
        std::move(voice_media_channel), mid, kDefaultSrtpRequired,
        webrtc::CryptoOptions(), context_->ssrc_generator(), transport_name);
    GetOrCreateFirstTransceiverOfType(cricket::MEDIA_TYPE_AUDIO)
        ->internal()
        ->SetChannel(std::move(voice_channel),
                     [](const std::string&) { return nullptr; });
    voice_media_channel_ptr->SetStats(initial_stats);
    return voice_media_channel_ptr;
  }

  FakeVideoMediaChannelForStats* AddVideoChannel(
      const std::string& mid,
      const std::string& transport_name,
      cricket::VideoMediaInfo initial_stats = cricket::VideoMediaInfo()) {
    auto video_media_channel =
        std::make_unique<FakeVideoMediaChannelForStats>(network_thread_);
    auto video_media_channel_ptr = video_media_channel.get();
    auto video_channel = std::make_unique<VideoChannelForTesting>(
        worker_thread_, network_thread_, signaling_thread_,
        std::move(video_media_channel), mid, kDefaultSrtpRequired,
        webrtc::CryptoOptions(), context_->ssrc_generator(), transport_name);
    GetOrCreateFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
        ->internal()
        ->SetChannel(std::move(video_channel),
                     [](const std::string&) { return nullptr; });
    video_media_channel_ptr->SetStats(initial_stats);
    return video_media_channel_ptr;
  }

  void AddSctpDataChannel(const std::string& label) {
    AddSctpDataChannel(label, InternalDataChannelInit());
  }

  void AddSctpDataChannel(const std::string& label,
                          const InternalDataChannelInit& init) {
    // TODO(bugs.webrtc.org/11547): Supply a separate network thread.
    AddSctpDataChannel(SctpDataChannel::Create(&data_channel_controller_, label,
                                               init, rtc::Thread::Current(),
                                               rtc::Thread::Current()));
  }

  void AddSctpDataChannel(rtc::scoped_refptr<SctpDataChannel> data_channel) {
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

  std::vector<DataChannelStats> GetDataChannelStats() const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    std::vector<DataChannelStats> stats;
    for (const auto& channel : sctp_data_channels_)
      stats.push_back(channel->GetStats());
    return stats;
  }

  cricket::CandidateStatsList GetPooledCandidateStats() const override {
    return {};
  }

  std::map<std::string, cricket::TransportStats> GetTransportStatsByNames(
      const std::set<std::string>& transport_names) override {
    RTC_DCHECK_RUN_ON(network_thread_);
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
      return it->second->Clone();
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
        signaling_thread_,
        rtc::make_ref_counted<RtpTransceiver>(media_type, context_.get()));
    transceivers_.push_back(transceiver);
    return transceiver;
  }

  rtc::Thread* const network_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const signaling_thread_;

  PeerConnectionFactoryDependencies dependencies_;
  rtc::scoped_refptr<ConnectionContext> context_;

  rtc::scoped_refptr<StreamCollection> local_streams_;
  rtc::scoped_refptr<StreamCollection> remote_streams_;

  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
      transceivers_;

  FakeDataChannelController data_channel_controller_;

  std::vector<rtc::scoped_refptr<SctpDataChannel>> sctp_data_channels_;

  std::map<std::string, cricket::TransportStats> transport_stats_by_name_;

  Call::Stats call_stats_;

  std::map<std::string, rtc::scoped_refptr<rtc::RTCCertificate>>
      local_certificates_by_transport_;
  std::map<std::string, std::unique_ptr<rtc::SSLCertChain>>
      remote_cert_chains_by_transport_;
};

}  // namespace webrtc

#endif  // PC_TEST_FAKE_PEER_CONNECTION_FOR_STATS_H_
