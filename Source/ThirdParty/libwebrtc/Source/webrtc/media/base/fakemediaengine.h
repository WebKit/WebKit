/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_FAKEMEDIAENGINE_H_
#define MEDIA_BASE_FAKEMEDIAENGINE_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "api/call/audio_sink.h"
#include "media/base/audiosource.h"
#include "media/base/mediaengine.h"
#include "media/base/rtputils.h"
#include "media/base/streamparams.h"
#include "media/engine/webrtcvideoengine.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "p2p/base/sessiondescription.h"
#include "rtc_base/checks.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/stringutils.h"

using webrtc::RtpExtension;

namespace cricket {

class FakeMediaEngine;
class FakeVideoEngine;
class FakeVoiceEngine;

// A common helper class that handles sending and receiving RTP/RTCP packets.
template <class Base> class RtpHelper : public Base {
 public:
  RtpHelper()
      : sending_(false),
        playout_(false),
        fail_set_send_codecs_(false),
        fail_set_recv_codecs_(false),
        send_ssrc_(0),
        ready_to_send_(false),
        transport_overhead_per_packet_(0),
        num_network_route_changes_(0) {}
  virtual ~RtpHelper() = default;
  const std::vector<RtpExtension>& recv_extensions() {
    return recv_extensions_;
  }
  const std::vector<RtpExtension>& send_extensions() {
    return send_extensions_;
  }
  bool sending() const { return sending_; }
  bool playout() const { return playout_; }
  const std::list<std::string>& rtp_packets() const { return rtp_packets_; }
  const std::list<std::string>& rtcp_packets() const { return rtcp_packets_; }

  bool SendRtp(const void* data,
               size_t len,
               const rtc::PacketOptions& options) {
    if (!sending_) {
      return false;
    }
    rtc::CopyOnWriteBuffer packet(reinterpret_cast<const uint8_t*>(data), len,
                                  kMaxRtpPacketLen);
    return Base::SendPacket(&packet, options);
  }
  bool SendRtcp(const void* data, size_t len) {
    rtc::CopyOnWriteBuffer packet(reinterpret_cast<const uint8_t*>(data), len,
                                  kMaxRtpPacketLen);
    return Base::SendRtcp(&packet, rtc::PacketOptions());
  }

  bool CheckRtp(const void* data, size_t len) {
    bool success = !rtp_packets_.empty();
    if (success) {
      std::string packet = rtp_packets_.front();
      rtp_packets_.pop_front();
      success = (packet == std::string(static_cast<const char*>(data), len));
    }
    return success;
  }
  bool CheckRtcp(const void* data, size_t len) {
    bool success = !rtcp_packets_.empty();
    if (success) {
      std::string packet = rtcp_packets_.front();
      rtcp_packets_.pop_front();
      success = (packet == std::string(static_cast<const char*>(data), len));
    }
    return success;
  }
  bool CheckNoRtp() { return rtp_packets_.empty(); }
  bool CheckNoRtcp() { return rtcp_packets_.empty(); }
  void set_fail_set_send_codecs(bool fail) { fail_set_send_codecs_ = fail; }
  void set_fail_set_recv_codecs(bool fail) { fail_set_recv_codecs_ = fail; }
  virtual bool AddSendStream(const StreamParams& sp) {
    if (std::find(send_streams_.begin(), send_streams_.end(), sp) !=
        send_streams_.end()) {
      return false;
    }
    send_streams_.push_back(sp);
    rtp_send_parameters_[sp.first_ssrc()] =
        CreateRtpParametersWithOneEncoding();
    return true;
  }
  virtual bool RemoveSendStream(uint32_t ssrc) {
    auto parameters_iterator = rtp_send_parameters_.find(ssrc);
    if (parameters_iterator != rtp_send_parameters_.end()) {
      rtp_send_parameters_.erase(parameters_iterator);
    }
    return RemoveStreamBySsrc(&send_streams_, ssrc);
  }
  virtual bool AddRecvStream(const StreamParams& sp) {
    if (std::find(receive_streams_.begin(), receive_streams_.end(), sp) !=
        receive_streams_.end()) {
      return false;
    }
    receive_streams_.push_back(sp);
    rtp_receive_parameters_[sp.first_ssrc()] =
        CreateRtpParametersWithOneEncoding();
    return true;
  }
  virtual bool RemoveRecvStream(uint32_t ssrc) {
    auto parameters_iterator = rtp_receive_parameters_.find(ssrc);
    if (parameters_iterator != rtp_receive_parameters_.end()) {
      rtp_receive_parameters_.erase(parameters_iterator);
    }
    return RemoveStreamBySsrc(&receive_streams_, ssrc);
  }

  virtual webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const {
    auto parameters_iterator = rtp_send_parameters_.find(ssrc);
    if (parameters_iterator != rtp_send_parameters_.end()) {
      return parameters_iterator->second;
    }
    return webrtc::RtpParameters();
  }
  virtual bool SetRtpSendParameters(uint32_t ssrc,
                                    const webrtc::RtpParameters& parameters) {
    auto parameters_iterator = rtp_send_parameters_.find(ssrc);
    if (parameters_iterator != rtp_send_parameters_.end()) {
      parameters_iterator->second = parameters;
      return true;
    }
    // Replicate the behavior of the real media channel: return false
    // when setting parameters for unknown SSRCs.
    return false;
  }

  virtual webrtc::RtpParameters GetRtpReceiveParameters(uint32_t ssrc) const {
    auto parameters_iterator = rtp_receive_parameters_.find(ssrc);
    if (parameters_iterator != rtp_receive_parameters_.end()) {
      return parameters_iterator->second;
    }
    return webrtc::RtpParameters();
  }
  virtual bool SetRtpReceiveParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters) {
    auto parameters_iterator = rtp_receive_parameters_.find(ssrc);
    if (parameters_iterator != rtp_receive_parameters_.end()) {
      parameters_iterator->second = parameters;
      return true;
    }
    // Replicate the behavior of the real media channel: return false
    // when setting parameters for unknown SSRCs.
    return false;
  }

  bool IsStreamMuted(uint32_t ssrc) const {
    bool ret = muted_streams_.find(ssrc) != muted_streams_.end();
    // If |ssrc = 0| check if the first send stream is muted.
    if (!ret && ssrc == 0 && !send_streams_.empty()) {
      return muted_streams_.find(send_streams_[0].first_ssrc()) !=
             muted_streams_.end();
    }
    return ret;
  }
  const std::vector<StreamParams>& send_streams() const {
    return send_streams_;
  }
  const std::vector<StreamParams>& recv_streams() const {
    return receive_streams_;
  }
  bool HasRecvStream(uint32_t ssrc) const {
    return GetStreamBySsrc(receive_streams_, ssrc) != nullptr;
  }
  bool HasSendStream(uint32_t ssrc) const {
    return GetStreamBySsrc(send_streams_, ssrc) != nullptr;
  }
  // TODO(perkj): This is to support legacy unit test that only check one
  // sending stream.
  uint32_t send_ssrc() const {
    if (send_streams_.empty())
      return 0;
    return send_streams_[0].first_ssrc();
  }

  // TODO(perkj): This is to support legacy unit test that only check one
  // sending stream.
  const std::string rtcp_cname() {
    if (send_streams_.empty())
      return "";
    return send_streams_[0].cname;
  }
  const RtcpParameters& send_rtcp_parameters() { return send_rtcp_parameters_; }
  const RtcpParameters& recv_rtcp_parameters() { return recv_rtcp_parameters_; }

  bool ready_to_send() const {
    return ready_to_send_;
  }

  int transport_overhead_per_packet() const {
    return transport_overhead_per_packet_;
  }

  rtc::NetworkRoute last_network_route() const { return last_network_route_; }
  int num_network_route_changes() const { return num_network_route_changes_; }
  void set_num_network_route_changes(int changes) {
    num_network_route_changes_ = changes;
  }

 protected:
  bool MuteStream(uint32_t ssrc, bool mute) {
    if (!HasSendStream(ssrc) && ssrc != 0) {
      return false;
    }
    if (mute) {
      muted_streams_.insert(ssrc);
    } else {
      muted_streams_.erase(ssrc);
    }
    return true;
  }
  bool set_sending(bool send) {
    sending_ = send;
    return true;
  }
  void set_playout(bool playout) { playout_ = playout; }
  bool SetRecvRtpHeaderExtensions(const std::vector<RtpExtension>& extensions) {
    recv_extensions_ = extensions;
    return true;
  }
  bool SetSendRtpHeaderExtensions(const std::vector<RtpExtension>& extensions) {
    send_extensions_ = extensions;
    return true;
  }
  void set_send_rtcp_parameters(const RtcpParameters& params) {
    send_rtcp_parameters_ = params;
  }
  void set_recv_rtcp_parameters(const RtcpParameters& params) {
    recv_rtcp_parameters_ = params;
  }
  virtual void OnPacketReceived(rtc::CopyOnWriteBuffer* packet,
                                const rtc::PacketTime& packet_time) {
    rtp_packets_.push_back(std::string(packet->data<char>(), packet->size()));
  }
  virtual void OnRtcpReceived(rtc::CopyOnWriteBuffer* packet,
                              const rtc::PacketTime& packet_time) {
    rtcp_packets_.push_back(std::string(packet->data<char>(), packet->size()));
  }
  virtual void OnReadyToSend(bool ready) {
    ready_to_send_ = ready;
  }

  virtual void OnNetworkRouteChanged(const std::string& transport_name,
                                     const rtc::NetworkRoute& network_route) {
    last_network_route_ = network_route;
    ++num_network_route_changes_;
    transport_overhead_per_packet_ = network_route.packet_overhead;
  }
  bool fail_set_send_codecs() const { return fail_set_send_codecs_; }
  bool fail_set_recv_codecs() const { return fail_set_recv_codecs_; }

 private:
  bool sending_;
  bool playout_;
  std::vector<RtpExtension> recv_extensions_;
  std::vector<RtpExtension> send_extensions_;
  std::list<std::string> rtp_packets_;
  std::list<std::string> rtcp_packets_;
  std::vector<StreamParams> send_streams_;
  std::vector<StreamParams> receive_streams_;
  RtcpParameters send_rtcp_parameters_;
  RtcpParameters recv_rtcp_parameters_;
  std::set<uint32_t> muted_streams_;
  std::map<uint32_t, webrtc::RtpParameters> rtp_send_parameters_;
  std::map<uint32_t, webrtc::RtpParameters> rtp_receive_parameters_;
  bool fail_set_send_codecs_;
  bool fail_set_recv_codecs_;
  uint32_t send_ssrc_;
  std::string rtcp_cname_;
  bool ready_to_send_;
  int transport_overhead_per_packet_;
  rtc::NetworkRoute last_network_route_;
  int num_network_route_changes_;
};

class FakeVoiceMediaChannel : public RtpHelper<VoiceMediaChannel> {
 public:
  struct DtmfInfo {
    DtmfInfo(uint32_t ssrc, int event_code, int duration)
        : ssrc(ssrc),
          event_code(event_code),
          duration(duration) {}
    uint32_t ssrc;
    int event_code;
    int duration;
  };
  explicit FakeVoiceMediaChannel(FakeVoiceEngine* engine,
                                 const AudioOptions& options)
      : engine_(engine), max_bps_(-1) {
    output_scalings_[0] = 1.0;  // For default channel.
    SetOptions(options);
  }
  ~FakeVoiceMediaChannel();
  const std::vector<AudioCodec>& recv_codecs() const { return recv_codecs_; }
  const std::vector<AudioCodec>& send_codecs() const { return send_codecs_; }
  const std::vector<AudioCodec>& codecs() const { return send_codecs(); }
  const std::vector<DtmfInfo>& dtmf_info_queue() const {
    return dtmf_info_queue_;
  }
  const AudioOptions& options() const { return options_; }
  int max_bps() const { return max_bps_; }
  virtual bool SetSendParameters(const AudioSendParameters& params) {
    set_send_rtcp_parameters(params.rtcp);
    return (SetSendCodecs(params.codecs) &&
            SetSendRtpHeaderExtensions(params.extensions) &&
            SetMaxSendBandwidth(params.max_bandwidth_bps) &&
            SetOptions(params.options));
  }

  virtual bool SetRecvParameters(const AudioRecvParameters& params) {
    set_recv_rtcp_parameters(params.rtcp);
    return (SetRecvCodecs(params.codecs) &&
            SetRecvRtpHeaderExtensions(params.extensions));
  }

  virtual void SetPlayout(bool playout) { set_playout(playout); }
  virtual void SetSend(bool send) { set_sending(send); }
  virtual bool SetAudioSend(uint32_t ssrc,
                            bool enable,
                            const AudioOptions* options,
                            AudioSource* source) {
    if (!SetLocalSource(ssrc, source)) {
      return false;
    }
    if (!RtpHelper<VoiceMediaChannel>::MuteStream(ssrc, !enable)) {
      return false;
    }
    if (enable && options) {
      return SetOptions(*options);
    }
    return true;
  }

  bool HasSource(uint32_t ssrc) const {
    return local_sinks_.find(ssrc) != local_sinks_.end();
  }

  virtual bool AddRecvStream(const StreamParams& sp) {
    if (!RtpHelper<VoiceMediaChannel>::AddRecvStream(sp))
      return false;
    output_scalings_[sp.first_ssrc()] = 1.0;
    return true;
  }
  virtual bool RemoveRecvStream(uint32_t ssrc) {
    if (!RtpHelper<VoiceMediaChannel>::RemoveRecvStream(ssrc))
      return false;
    output_scalings_.erase(ssrc);
    return true;
  }

  virtual bool GetActiveStreams(StreamList* streams) { return true; }
  virtual int GetOutputLevel() { return 0; }

  virtual bool CanInsertDtmf() {
    for (std::vector<AudioCodec>::const_iterator it = send_codecs_.begin();
         it != send_codecs_.end(); ++it) {
      // Find the DTMF telephone event "codec".
      if (_stricmp(it->name.c_str(), "telephone-event") == 0) {
        return true;
      }
    }
    return false;
  }
  virtual bool InsertDtmf(uint32_t ssrc,
                          int event_code,
                          int duration) {
    dtmf_info_queue_.push_back(DtmfInfo(ssrc, event_code, duration));
    return true;
  }

  virtual bool SetOutputVolume(uint32_t ssrc, double volume) {
    if (0 == ssrc) {
      std::map<uint32_t, double>::iterator it;
      for (it = output_scalings_.begin(); it != output_scalings_.end(); ++it) {
        it->second = volume;
      }
      return true;
    } else if (output_scalings_.find(ssrc) != output_scalings_.end()) {
      output_scalings_[ssrc] = volume;
      return true;
    }
    return false;
  }
  bool GetOutputVolume(uint32_t ssrc, double* volume) {
    if (output_scalings_.find(ssrc) == output_scalings_.end())
      return false;
    *volume = output_scalings_[ssrc];
    return true;
  }

  virtual bool GetStats(VoiceMediaInfo* info) { return false; }

  virtual void SetRawAudioSink(
      uint32_t ssrc,
      std::unique_ptr<webrtc::AudioSinkInterface> sink) {
    sink_ = std::move(sink);
  }

  virtual std::vector<webrtc::RtpSource> GetSources(uint32_t ssrc) const {
    return std::vector<webrtc::RtpSource>();
  }

 private:
  class VoiceChannelAudioSink : public AudioSource::Sink {
   public:
    explicit VoiceChannelAudioSink(AudioSource* source) : source_(source) {
      source_->SetSink(this);
    }
    virtual ~VoiceChannelAudioSink() {
      if (source_) {
        source_->SetSink(nullptr);
      }
    }
    void OnData(const void* audio_data,
                int bits_per_sample,
                int sample_rate,
                size_t number_of_channels,
                size_t number_of_frames) override {}
    void OnClose() override { source_ = nullptr; }
    AudioSource* source() const { return source_; }

   private:
    AudioSource* source_;
  };

  bool SetRecvCodecs(const std::vector<AudioCodec>& codecs) {
    if (fail_set_recv_codecs()) {
      // Fake the failure in SetRecvCodecs.
      return false;
    }
    recv_codecs_ = codecs;
    return true;
  }
  bool SetSendCodecs(const std::vector<AudioCodec>& codecs) {
    if (fail_set_send_codecs()) {
      // Fake the failure in SetSendCodecs.
      return false;
    }
    send_codecs_ = codecs;
    return true;
  }
  bool SetMaxSendBandwidth(int bps) {
    max_bps_ = bps;
    return true;
  }
  bool SetOptions(const AudioOptions& options) {
    // Does a "merge" of current options and set options.
    options_.SetAll(options);
    return true;
  }
  bool SetLocalSource(uint32_t ssrc, AudioSource* source) {
    auto it = local_sinks_.find(ssrc);
    if (source) {
      if (it != local_sinks_.end()) {
        RTC_CHECK(it->second->source() == source);
      } else {
        local_sinks_.insert(std::make_pair(
            ssrc, rtc::MakeUnique<VoiceChannelAudioSink>(source)));
      }
    } else {
      if (it != local_sinks_.end()) {
        local_sinks_.erase(it);
      }
    }
    return true;
  }

  FakeVoiceEngine* engine_;
  std::vector<AudioCodec> recv_codecs_;
  std::vector<AudioCodec> send_codecs_;
  std::map<uint32_t, double> output_scalings_;
  std::vector<DtmfInfo> dtmf_info_queue_;
  AudioOptions options_;
  std::map<uint32_t, std::unique_ptr<VoiceChannelAudioSink>> local_sinks_;
  std::unique_ptr<webrtc::AudioSinkInterface> sink_;
  int max_bps_;
};

// A helper function to compare the FakeVoiceMediaChannel::DtmfInfo.
inline bool CompareDtmfInfo(const FakeVoiceMediaChannel::DtmfInfo& info,
                            uint32_t ssrc,
                            int event_code,
                            int duration) {
  return (info.duration == duration && info.event_code == event_code &&
          info.ssrc == ssrc);
}

class FakeVideoMediaChannel : public RtpHelper<VideoMediaChannel> {
 public:
  FakeVideoMediaChannel(FakeVideoEngine* engine, const VideoOptions& options)
      : engine_(engine), max_bps_(-1) {
    SetOptions(options);
  }

  ~FakeVideoMediaChannel();

  const std::vector<VideoCodec>& recv_codecs() const { return recv_codecs_; }
  const std::vector<VideoCodec>& send_codecs() const { return send_codecs_; }
  const std::vector<VideoCodec>& codecs() const { return send_codecs(); }
  bool rendering() const { return playout(); }
  const VideoOptions& options() const { return options_; }
  const std::map<uint32_t, rtc::VideoSinkInterface<webrtc::VideoFrame>*>&
  sinks() const {
    return sinks_;
  }
  int max_bps() const { return max_bps_; }
  bool SetSendParameters(const VideoSendParameters& params) override {
    set_send_rtcp_parameters(params.rtcp);
    return (SetSendCodecs(params.codecs) &&
            SetSendRtpHeaderExtensions(params.extensions) &&
            SetMaxSendBandwidth(params.max_bandwidth_bps));
  }
  bool SetRecvParameters(const VideoRecvParameters& params) override {
    set_recv_rtcp_parameters(params.rtcp);
    return (SetRecvCodecs(params.codecs) &&
            SetRecvRtpHeaderExtensions(params.extensions));
  }
  bool AddSendStream(const StreamParams& sp) override {
    return RtpHelper<VideoMediaChannel>::AddSendStream(sp);
  }
  bool RemoveSendStream(uint32_t ssrc) override {
    return RtpHelper<VideoMediaChannel>::RemoveSendStream(ssrc);
  }

  bool GetSendCodec(VideoCodec* send_codec) override {
    if (send_codecs_.empty()) {
      return false;
    }
    *send_codec = send_codecs_[0];
    return true;
  }
  bool SetSink(uint32_t ssrc,
               rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
    if (ssrc != 0 && sinks_.find(ssrc) == sinks_.end()) {
      return false;
    }
    if (ssrc != 0) {
      sinks_[ssrc] = sink;
    }
    return true;
  }
  bool HasSink(uint32_t ssrc) const {
    return sinks_.find(ssrc) != sinks_.end() && sinks_.at(ssrc) != nullptr;
  }

  bool SetSend(bool send) override { return set_sending(send); }
  bool SetVideoSend(
      uint32_t ssrc,
      bool enable,
      const VideoOptions* options,
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source) override {
    if (!RtpHelper<VideoMediaChannel>::MuteStream(ssrc, !enable)) {
      return false;
    }
    if (enable && options) {
      if (!SetOptions(*options)) {
        return false;
      }
    }
    sources_[ssrc] = source;
    return true;
  }

  bool HasSource(uint32_t ssrc) const {
    return sources_.find(ssrc) != sources_.end() &&
           sources_.at(ssrc) != nullptr;
  }
  bool AddRecvStream(const StreamParams& sp) override {
    if (!RtpHelper<VideoMediaChannel>::AddRecvStream(sp))
      return false;
    sinks_[sp.first_ssrc()] = NULL;
    return true;
  }
  bool RemoveRecvStream(uint32_t ssrc) override {
    if (!RtpHelper<VideoMediaChannel>::RemoveRecvStream(ssrc))
      return false;
    sinks_.erase(ssrc);
    return true;
  }

  void FillBitrateInfo(BandwidthEstimationInfo* bwe_info) override {}
  bool GetStats(VideoMediaInfo* info) override { return false; }

 private:
  bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) {
    if (fail_set_recv_codecs()) {
      // Fake the failure in SetRecvCodecs.
      return false;
    }
    recv_codecs_ = codecs;
    return true;
  }
  bool SetSendCodecs(const std::vector<VideoCodec>& codecs) {
    if (fail_set_send_codecs()) {
      // Fake the failure in SetSendCodecs.
      return false;
    }
    send_codecs_ = codecs;

    return true;
  }
  bool SetOptions(const VideoOptions& options) {
    options_ = options;
    return true;
  }
  bool SetMaxSendBandwidth(int bps) {
    max_bps_ = bps;
    return true;
  }

  FakeVideoEngine* engine_;
  std::vector<VideoCodec> recv_codecs_;
  std::vector<VideoCodec> send_codecs_;
  std::map<uint32_t, rtc::VideoSinkInterface<webrtc::VideoFrame>*> sinks_;
  std::map<uint32_t, rtc::VideoSourceInterface<webrtc::VideoFrame>*> sources_;
  VideoOptions options_;
  int max_bps_;
};

// Dummy option class, needed for the DataTraits abstraction in
// channel_unittest.c.
class DataOptions {};

class FakeDataMediaChannel : public RtpHelper<DataMediaChannel> {
 public:
  explicit FakeDataMediaChannel(void* unused, const DataOptions& options)
      : send_blocked_(false), max_bps_(-1) {}
  ~FakeDataMediaChannel() {}
  const std::vector<DataCodec>& recv_codecs() const { return recv_codecs_; }
  const std::vector<DataCodec>& send_codecs() const { return send_codecs_; }
  const std::vector<DataCodec>& codecs() const { return send_codecs(); }
  int max_bps() const { return max_bps_; }

  virtual bool SetSendParameters(const DataSendParameters& params) {
    set_send_rtcp_parameters(params.rtcp);
    return (SetSendCodecs(params.codecs) &&
            SetMaxSendBandwidth(params.max_bandwidth_bps));
  }
  virtual bool SetRecvParameters(const DataRecvParameters& params) {
    set_recv_rtcp_parameters(params.rtcp);
    return SetRecvCodecs(params.codecs);
  }
  virtual bool SetSend(bool send) { return set_sending(send); }
  virtual bool SetReceive(bool receive) {
    set_playout(receive);
    return true;
  }
  virtual bool AddRecvStream(const StreamParams& sp) {
    if (!RtpHelper<DataMediaChannel>::AddRecvStream(sp))
      return false;
    return true;
  }
  virtual bool RemoveRecvStream(uint32_t ssrc) {
    if (!RtpHelper<DataMediaChannel>::RemoveRecvStream(ssrc))
      return false;
    return true;
  }

  virtual bool SendData(const SendDataParams& params,
                        const rtc::CopyOnWriteBuffer& payload,
                        SendDataResult* result) {
    if (send_blocked_) {
      *result = SDR_BLOCK;
      return false;
    } else {
      last_sent_data_params_ = params;
      last_sent_data_ = std::string(payload.data<char>(), payload.size());
      return true;
    }
  }

  SendDataParams last_sent_data_params() { return last_sent_data_params_; }
  std::string last_sent_data() { return last_sent_data_; }
  bool is_send_blocked() { return send_blocked_; }
  void set_send_blocked(bool blocked) { send_blocked_ = blocked; }

 private:
  bool SetRecvCodecs(const std::vector<DataCodec>& codecs) {
    if (fail_set_recv_codecs()) {
      // Fake the failure in SetRecvCodecs.
      return false;
    }
    recv_codecs_ = codecs;
    return true;
  }
  bool SetSendCodecs(const std::vector<DataCodec>& codecs) {
    if (fail_set_send_codecs()) {
      // Fake the failure in SetSendCodecs.
      return false;
    }
    send_codecs_ = codecs;
    return true;
  }
  bool SetMaxSendBandwidth(int bps) {
    max_bps_ = bps;
    return true;
  }

  std::vector<DataCodec> recv_codecs_;
  std::vector<DataCodec> send_codecs_;
  SendDataParams last_sent_data_params_;
  std::string last_sent_data_;
  bool send_blocked_;
  int max_bps_;
};

// A base class for all of the shared parts between FakeVoiceEngine
// and FakeVideoEngine.
class FakeBaseEngine {
 public:
  FakeBaseEngine()
      : options_changed_(false),
        fail_create_channel_(false) {}
  void set_fail_create_channel(bool fail) { fail_create_channel_ = fail; }

  RtpCapabilities GetCapabilities() const { return capabilities_; }
  void set_rtp_header_extensions(const std::vector<RtpExtension>& extensions) {
    capabilities_.header_extensions = extensions;
  }

  void set_rtp_header_extensions(
      const std::vector<cricket::RtpHeaderExtension>& extensions) {
    for (const cricket::RtpHeaderExtension& ext : extensions) {
      RtpExtension webrtc_ext;
      webrtc_ext.uri = ext.uri;
      webrtc_ext.id = ext.id;
      capabilities_.header_extensions.push_back(webrtc_ext);
    }
  }

 protected:
  // Flag used by optionsmessagehandler_unittest for checking whether any
  // relevant setting has been updated.
  // TODO(thaloun): Replace with explicit checks of before & after values.
  bool options_changed_;
  bool fail_create_channel_;
  RtpCapabilities capabilities_;
};

class FakeVoiceEngine : public FakeBaseEngine {
 public:
  FakeVoiceEngine() {
    // Add a fake audio codec. Note that the name must not be "" as there are
    // sanity checks against that.
    codecs_.push_back(AudioCodec(101, "fake_audio_codec", 0, 0, 1));
  }
  void Init() {}
  rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const {
    return rtc::scoped_refptr<webrtc::AudioState>();
  }

  VoiceMediaChannel* CreateChannel(webrtc::Call* call,
                                   const MediaConfig& config,
                                   const AudioOptions& options) {
    if (fail_create_channel_) {
      return nullptr;
    }

    FakeVoiceMediaChannel* ch = new FakeVoiceMediaChannel(this, options);
    channels_.push_back(ch);
    return ch;
  }
  FakeVoiceMediaChannel* GetChannel(size_t index) {
    return (channels_.size() > index) ? channels_[index] : NULL;
  }
  void UnregisterChannel(VoiceMediaChannel* channel) {
    channels_.erase(std::find(channels_.begin(), channels_.end(), channel));
  }

  // TODO(ossu): For proper testing, These should either individually settable
  //             or the voice engine should reference mockable factories.
  const std::vector<AudioCodec>& send_codecs() { return codecs_; }
  const std::vector<AudioCodec>& recv_codecs() { return codecs_; }
  void SetCodecs(const std::vector<AudioCodec>& codecs) { codecs_ = codecs; }

  int GetInputLevel() { return 0; }

  bool StartAecDump(rtc::PlatformFile file, int64_t max_size_bytes) {
    return false;
  }

  void StopAecDump() {}

  bool StartRtcEventLog(rtc::PlatformFile file, int64_t max_size_bytes) {
    return false;
  }

  void StopRtcEventLog() {}

 private:
  std::vector<FakeVoiceMediaChannel*> channels_;
  std::vector<AudioCodec> codecs_;

  friend class FakeMediaEngine;
};

class FakeVideoEngine : public FakeBaseEngine {
 public:
  FakeVideoEngine() : capture_(false) {
    // Add a fake video codec. Note that the name must not be "" as there are
    // sanity checks against that.
    codecs_.push_back(VideoCodec(0, "fake_video_codec"));
  }

  bool SetOptions(const VideoOptions& options) {
    options_ = options;
    options_changed_ = true;
    return true;
  }

  VideoMediaChannel* CreateChannel(webrtc::Call* call,
                                   const MediaConfig& config,
                                   const VideoOptions& options) {
    if (fail_create_channel_) {
      return nullptr;
    }

    FakeVideoMediaChannel* ch = new FakeVideoMediaChannel(this, options);
    channels_.emplace_back(ch);
    return ch;
  }

  FakeVideoMediaChannel* GetChannel(size_t index) {
    return (channels_.size() > index) ? channels_[index] : nullptr;
  }

  void UnregisterChannel(VideoMediaChannel* channel) {
    auto it = std::find(channels_.begin(), channels_.end(), channel);
    RTC_DCHECK(it != channels_.end());
    channels_.erase(it);
  }

  const std::vector<VideoCodec>& codecs() const { return codecs_; }

  void SetCodecs(const std::vector<VideoCodec> codecs) { codecs_ = codecs; }

  bool SetCapture(bool capture) {
    capture_ = capture;
    return true;
  }

 private:
  std::vector<FakeVideoMediaChannel*> channels_;
  std::vector<VideoCodec> codecs_;
  bool capture_;
  VideoOptions options_;

  friend class FakeMediaEngine;
};

class FakeMediaEngine :
    public CompositeMediaEngine<FakeVoiceEngine, FakeVideoEngine> {
 public:
  FakeMediaEngine()
      : CompositeMediaEngine<FakeVoiceEngine, FakeVideoEngine>(std::tuple<>(),
                                                               std::tuple<>()) {
  }

  virtual ~FakeMediaEngine() {}

  void SetAudioCodecs(const std::vector<AudioCodec>& codecs) {
    voice().SetCodecs(codecs);
  }
  void SetVideoCodecs(const std::vector<VideoCodec>& codecs) {
    video().SetCodecs(codecs);
  }

  void SetAudioRtpHeaderExtensions(
      const std::vector<RtpExtension>& extensions) {
    voice().set_rtp_header_extensions(extensions);
  }
  void SetVideoRtpHeaderExtensions(
      const std::vector<RtpExtension>& extensions) {
    video().set_rtp_header_extensions(extensions);
  }

  void SetAudioRtpHeaderExtensions(
      const std::vector<cricket::RtpHeaderExtension>& extensions) {
    voice().set_rtp_header_extensions(extensions);
  }
  void SetVideoRtpHeaderExtensions(
      const std::vector<cricket::RtpHeaderExtension>& extensions) {
    video().set_rtp_header_extensions(extensions);
  }

  FakeVoiceMediaChannel* GetVoiceChannel(size_t index) {
    return voice().GetChannel(index);
  }
  FakeVideoMediaChannel* GetVideoChannel(size_t index) {
    return video().GetChannel(index);
  }

  bool capture() const { return video().capture_; }
  bool options_changed() const { return video().options_changed_; }
  void clear_options_changed() { video().options_changed_ = false; }
  void set_fail_create_channel(bool fail) {
    voice().set_fail_create_channel(fail);
    video().set_fail_create_channel(fail);
  }
};

// Have to come afterwards due to declaration order
inline FakeVoiceMediaChannel::~FakeVoiceMediaChannel() {
  if (engine_) {
    engine_->UnregisterChannel(this);
  }
}

inline FakeVideoMediaChannel::~FakeVideoMediaChannel() {
  if (engine_) {
    engine_->UnregisterChannel(this);
  }
}

class FakeDataEngine : public DataEngineInterface {
 public:
  virtual DataMediaChannel* CreateChannel(const MediaConfig& config) {
    FakeDataMediaChannel* ch = new FakeDataMediaChannel(this, DataOptions());
    channels_.push_back(ch);
    return ch;
  }

  FakeDataMediaChannel* GetChannel(size_t index) {
    return (channels_.size() > index) ? channels_[index] : NULL;
  }

  void UnregisterChannel(DataMediaChannel* channel) {
    channels_.erase(std::find(channels_.begin(), channels_.end(), channel));
  }

  virtual void SetDataCodecs(const std::vector<DataCodec>& data_codecs) {
    data_codecs_ = data_codecs;
  }

  virtual const std::vector<DataCodec>& data_codecs() { return data_codecs_; }

 private:
  std::vector<FakeDataMediaChannel*> channels_;
  std::vector<DataCodec> data_codecs_;
};

}  // namespace cricket

#endif  // MEDIA_BASE_FAKEMEDIAENGINE_H_
