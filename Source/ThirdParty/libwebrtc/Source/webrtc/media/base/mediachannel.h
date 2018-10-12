/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_MEDIACHANNEL_H_
#define MEDIA_BASE_MEDIACHANNEL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_options.h"
#include "api/crypto/framedecryptorinterface.h"
#include "api/crypto/frameencryptorinterface.h"
#include "api/rtcerror.h"
#include "api/rtpparameters.h"
#include "api/rtpreceiverinterface.h"
#include "api/video/video_content_type.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video/video_timing.h"
#include "api/video_codecs/video_encoder_config.h"
#include "media/base/codec.h"
#include "media/base/mediaconfig.h"
#include "media/base/mediaconstants.h"
#include "media/base/streamparams.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/dscp.h"
#include "rtc_base/logging.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/socket.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace rtc {
class Timing;
}

namespace webrtc {
class AudioSinkInterface;
class VideoFrame;
}  // namespace webrtc

namespace cricket {

class AudioSource;
class VideoCapturer;
struct RtpHeader;
struct VideoFormat;

const int kScreencastDefaultFps = 5;

template <class T>
static std::string ToStringIfSet(const char* key,
                                 const absl::optional<T>& val) {
  std::string str;
  if (val) {
    str = key;
    str += ": ";
    str += val ? rtc::ToString(*val) : "";
    str += ", ";
  }
  return str;
}

template <class T>
static std::string VectorToString(const std::vector<T>& vals) {
  rtc::StringBuilder ost;  // no-presubmit-check TODO(webrtc:8982)
  ost << "[";
  for (size_t i = 0; i < vals.size(); ++i) {
    if (i > 0) {
      ost << ", ";
    }
    ost << vals[i].ToString();
  }
  ost << "]";
  return ost.Release();
}

// Options that can be applied to a VideoMediaChannel or a VideoMediaEngine.
// Used to be flags, but that makes it hard to selectively apply options.
// We are moving all of the setting of options to structs like this,
// but some things currently still use flags.
struct VideoOptions {
  VideoOptions();
  ~VideoOptions();

  void SetAll(const VideoOptions& change) {
    SetFrom(&video_noise_reduction, change.video_noise_reduction);
    SetFrom(&screencast_min_bitrate_kbps, change.screencast_min_bitrate_kbps);
    SetFrom(&is_screencast, change.is_screencast);
  }

  bool operator==(const VideoOptions& o) const {
    return video_noise_reduction == o.video_noise_reduction &&
           screencast_min_bitrate_kbps == o.screencast_min_bitrate_kbps &&
           is_screencast == o.is_screencast;
  }
  bool operator!=(const VideoOptions& o) const { return !(*this == o); }

  std::string ToString() const {
    rtc::StringBuilder ost;
    ost << "VideoOptions {";
    ost << ToStringIfSet("noise reduction", video_noise_reduction);
    ost << ToStringIfSet("screencast min bitrate kbps",
                         screencast_min_bitrate_kbps);
    ost << ToStringIfSet("is_screencast ", is_screencast);
    ost << "}";
    return ost.Release();
  }

  // Enable denoising? This flag comes from the getUserMedia
  // constraint 'googNoiseReduction', and WebRtcVideoEngine passes it
  // on to the codec options. Disabled by default.
  absl::optional<bool> video_noise_reduction;
  // Force screencast to use a minimum bitrate. This flag comes from
  // the PeerConnection constraint 'googScreencastMinBitrate'. It is
  // copied to the encoder config by WebRtcVideoChannel.
  absl::optional<int> screencast_min_bitrate_kbps;
  // Set by screencast sources. Implies selection of encoding settings
  // suitable for screencast. Most likely not the right way to do
  // things, e.g., screencast of a text document and screencast of a
  // youtube video have different needs.
  absl::optional<bool> is_screencast;

 private:
  template <typename T>
  static void SetFrom(absl::optional<T>* s, const absl::optional<T>& o) {
    if (o) {
      *s = o;
    }
  }
};

// TODO(isheriff): Remove this once client usage is fixed to use RtpExtension.
struct RtpHeaderExtension {
  RtpHeaderExtension() : id(0) {}
  RtpHeaderExtension(const std::string& uri, int id) : uri(uri), id(id) {}

  std::string ToString() const {
    rtc::StringBuilder ost;
    ost << "{";
    ost << "uri: " << uri;
    ost << ", id: " << id;
    ost << "}";
    return ost.Release();
  }

  std::string uri;
  int id;
};

class MediaChannel : public sigslot::has_slots<> {
 public:
  class NetworkInterface {
   public:
    enum SocketType { ST_RTP, ST_RTCP };
    virtual bool SendPacket(rtc::CopyOnWriteBuffer* packet,
                            const rtc::PacketOptions& options) = 0;
    virtual bool SendRtcp(rtc::CopyOnWriteBuffer* packet,
                          const rtc::PacketOptions& options) = 0;
    virtual int SetOption(SocketType type,
                          rtc::Socket::Option opt,
                          int option) = 0;
    virtual ~NetworkInterface() {}
  };

  explicit MediaChannel(const MediaConfig& config);
  MediaChannel();
  ~MediaChannel() override;

  // Sets the abstract interface class for sending RTP/RTCP data.
  virtual void SetInterface(NetworkInterface* iface);
  // Called when a RTP packet is received.
  virtual void OnPacketReceived(rtc::CopyOnWriteBuffer* packet,
                                const rtc::PacketTime& packet_time) = 0;
  // Called when a RTCP packet is received.
  virtual void OnRtcpReceived(rtc::CopyOnWriteBuffer* packet,
                              const rtc::PacketTime& packet_time) = 0;
  // Called when the socket's ability to send has changed.
  virtual void OnReadyToSend(bool ready) = 0;
  // Called when the network route used for sending packets changed.
  virtual void OnNetworkRouteChanged(
      const std::string& transport_name,
      const rtc::NetworkRoute& network_route) = 0;
  // Creates a new outgoing media stream with SSRCs and CNAME as described
  // by sp.
  virtual bool AddSendStream(const StreamParams& sp) = 0;
  // Removes an outgoing media stream.
  // SSRC must be the first SSRC of the media stream if the stream uses
  // multiple SSRCs. In the case of an ssrc of 0, the possibly cached
  // StreamParams is removed.
  virtual bool RemoveSendStream(uint32_t ssrc) = 0;
  // Creates a new incoming media stream with SSRCs, CNAME as described
  // by sp. In the case of a sp without SSRCs, the unsignaled sp is cached
  // to be used later for unsignaled streams received.
  virtual bool AddRecvStream(const StreamParams& sp) = 0;
  // Removes an incoming media stream.
  // ssrc must be the first SSRC of the media stream if the stream uses
  // multiple SSRCs.
  virtual bool RemoveRecvStream(uint32_t ssrc) = 0;
  // Returns the absoulte sendtime extension id value from media channel.
  virtual int GetRtpSendTimeExtnId() const;
  // Set the frame encryptor to use on all outgoing frames. This is optional.
  // This pointers lifetime is managed by the set of RtpSender it is attached
  // to.
  // TODO(benwright) make pure virtual once internal supports it.
  virtual void SetFrameEncryptor(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor);
  // Set the frame decryptor to use on all incoming frames. This is optional.
  // This pointers lifetimes is managed by the set of RtpReceivers it is
  // attached to.
  // TODO(benwright) make pure virtual once internal supports it.
  virtual void SetFrameDecryptor(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor);

  // Base method to send packet using NetworkInterface.
  bool SendPacket(rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options) {
    return DoSendPacket(packet, false, options);
  }

  bool SendRtcp(rtc::CopyOnWriteBuffer* packet,
                const rtc::PacketOptions& options) {
    return DoSendPacket(packet, true, options);
  }

  int SetOption(NetworkInterface::SocketType type,
                rtc::Socket::Option opt,
                int option) {
    rtc::CritScope cs(&network_interface_crit_);
    if (!network_interface_)
      return -1;

    return network_interface_->SetOption(type, opt, option);
  }

 protected:
  virtual rtc::DiffServCodePoint PreferredDscp() const;

  bool DscpEnabled() const { return enable_dscp_; }

 private:
  // This method sets DSCP |value| on both RTP and RTCP channels.
  int SetDscp(rtc::DiffServCodePoint value) {
    int ret;
    ret = SetOption(NetworkInterface::ST_RTP, rtc::Socket::OPT_DSCP, value);
    if (ret == 0) {
      ret = SetOption(NetworkInterface::ST_RTCP, rtc::Socket::OPT_DSCP, value);
    }
    return ret;
  }

  bool DoSendPacket(rtc::CopyOnWriteBuffer* packet,
                    bool rtcp,
                    const rtc::PacketOptions& options) {
    rtc::CritScope cs(&network_interface_crit_);
    if (!network_interface_)
      return false;

    return (!rtcp) ? network_interface_->SendPacket(packet, options)
                   : network_interface_->SendRtcp(packet, options);
  }

  const bool enable_dscp_;
  // |network_interface_| can be accessed from the worker_thread and
  // from any MediaEngine threads. This critical section is to protect accessing
  // of network_interface_ object.
  rtc::CriticalSection network_interface_crit_;
  NetworkInterface* network_interface_;
};

// The stats information is structured as follows:
// Media are represented by either MediaSenderInfo or MediaReceiverInfo.
// Media contains a vector of SSRC infos that are exclusively used by this
// media. (SSRCs shared between media streams can't be represented.)

// Information about an SSRC.
// This data may be locally recorded, or received in an RTCP SR or RR.
struct SsrcSenderInfo {
  uint32_t ssrc = 0;
  double timestamp = 0.0;  // NTP timestamp, represented as seconds since epoch.
};

struct SsrcReceiverInfo {
  uint32_t ssrc = 0;
  double timestamp = 0.0;
};

struct MediaSenderInfo {
  MediaSenderInfo();
  ~MediaSenderInfo();
  void add_ssrc(const SsrcSenderInfo& stat) { local_stats.push_back(stat); }
  // Temporary utility function for call sites that only provide SSRC.
  // As more info is added into SsrcSenderInfo, this function should go away.
  void add_ssrc(uint32_t ssrc) {
    SsrcSenderInfo stat;
    stat.ssrc = ssrc;
    add_ssrc(stat);
  }
  // Utility accessor for clients that are only interested in ssrc numbers.
  std::vector<uint32_t> ssrcs() const {
    std::vector<uint32_t> retval;
    for (std::vector<SsrcSenderInfo>::const_iterator it = local_stats.begin();
         it != local_stats.end(); ++it) {
      retval.push_back(it->ssrc);
    }
    return retval;
  }
  // Returns true if the media has been connected.
  bool connected() const { return local_stats.size() > 0; }
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  // Call sites that compare this to zero should use connected() instead.
  // https://bugs.webrtc.org/8694
  uint32_t ssrc() const {
    if (connected()) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }
  int64_t bytes_sent = 0;
  int packets_sent = 0;
  int packets_lost = 0;
  float fraction_lost = 0.0f;
  int64_t rtt_ms = 0;
  std::string codec_name;
  absl::optional<int> codec_payload_type;
  std::vector<SsrcSenderInfo> local_stats;
  std::vector<SsrcReceiverInfo> remote_stats;
};

struct MediaReceiverInfo {
  MediaReceiverInfo();
  ~MediaReceiverInfo();
  void add_ssrc(const SsrcReceiverInfo& stat) { local_stats.push_back(stat); }
  // Temporary utility function for call sites that only provide SSRC.
  // As more info is added into SsrcSenderInfo, this function should go away.
  void add_ssrc(uint32_t ssrc) {
    SsrcReceiverInfo stat;
    stat.ssrc = ssrc;
    add_ssrc(stat);
  }
  std::vector<uint32_t> ssrcs() const {
    std::vector<uint32_t> retval;
    for (std::vector<SsrcReceiverInfo>::const_iterator it = local_stats.begin();
         it != local_stats.end(); ++it) {
      retval.push_back(it->ssrc);
    }
    return retval;
  }
  // Returns true if the media has been connected.
  bool connected() const { return local_stats.size() > 0; }
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  // Call sites that compare this to zero should use connected();
  // https://bugs.webrtc.org/8694
  uint32_t ssrc() const {
    if (connected()) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }

  int64_t bytes_rcvd = 0;
  int packets_rcvd = 0;
  int packets_lost = 0;
  float fraction_lost = 0.0f;
  std::string codec_name;
  absl::optional<int> codec_payload_type;
  std::vector<SsrcReceiverInfo> local_stats;
  std::vector<SsrcSenderInfo> remote_stats;
};

struct VoiceSenderInfo : public MediaSenderInfo {
  VoiceSenderInfo();
  ~VoiceSenderInfo();
  int ext_seqnum = 0;
  int jitter_ms = 0;
  int audio_level = 0;
  // See description of "totalAudioEnergy" in the WebRTC stats spec:
  // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy
  double total_input_energy = 0.0;
  double total_input_duration = 0.0;
  // TODO(bugs.webrtc.org/8572): Remove APM stats from this struct, since they
  // are no longer needed now that we have apm_statistics.
  int echo_delay_median_ms = 0;
  int echo_delay_std_ms = 0;
  int echo_return_loss = 0;
  int echo_return_loss_enhancement = 0;
  float residual_echo_likelihood = 0.0f;
  float residual_echo_likelihood_recent_max = 0.0f;
  bool typing_noise_detected = false;
  webrtc::ANAStats ana_statistics;
  webrtc::AudioProcessingStats apm_statistics;
};

struct VoiceReceiverInfo : public MediaReceiverInfo {
  VoiceReceiverInfo();
  ~VoiceReceiverInfo();
  int ext_seqnum = 0;
  int jitter_ms = 0;
  int jitter_buffer_ms = 0;
  int jitter_buffer_preferred_ms = 0;
  int delay_estimate_ms = 0;
  int audio_level = 0;
  // Stats below correspond to similarly-named fields in the WebRTC stats spec.
  // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats
  double total_output_energy = 0.0;
  uint64_t total_samples_received = 0;
  double total_output_duration = 0.0;
  uint64_t concealed_samples = 0;
  uint64_t concealment_events = 0;
  double jitter_buffer_delay_seconds = 0;
  // Stats below DO NOT correspond directly to anything in the WebRTC stats
  // fraction of synthesized audio inserted through expansion.
  float expand_rate = 0.0f;
  // fraction of synthesized speech inserted through expansion.
  float speech_expand_rate = 0.0f;
  // fraction of data out of secondary decoding, including FEC and RED.
  float secondary_decoded_rate = 0.0f;
  // Fraction of secondary data, including FEC and RED, that is discarded.
  // Discarding of secondary data can be caused by the reception of the primary
  // data, obsoleting the secondary data. It can also be caused by early
  // or late arrival of secondary data. This metric is the percentage of
  // discarded secondary data since last query of receiver info.
  float secondary_discarded_rate = 0.0f;
  // Fraction of data removed through time compression.
  float accelerate_rate = 0.0f;
  // Fraction of data inserted through time stretching.
  float preemptive_expand_rate = 0.0f;
  int decoding_calls_to_silence_generator = 0;
  int decoding_calls_to_neteq = 0;
  int decoding_normal = 0;
  int decoding_plc = 0;
  int decoding_cng = 0;
  int decoding_plc_cng = 0;
  int decoding_muted_output = 0;
  // Estimated capture start time in NTP time in ms.
  int64_t capture_start_ntp_time_ms = -1;
};

struct VideoSenderInfo : public MediaSenderInfo {
  VideoSenderInfo();
  ~VideoSenderInfo();
  std::vector<SsrcGroup> ssrc_groups;
  // TODO(hbos): Move this to |VideoMediaInfo::send_codecs|?
  std::string encoder_implementation_name;
  int firs_rcvd = 0;
  int plis_rcvd = 0;
  int nacks_rcvd = 0;
  int send_frame_width = 0;
  int send_frame_height = 0;
  int framerate_input = 0;
  int framerate_sent = 0;
  int nominal_bitrate = 0;
  int adapt_reason = 0;
  int adapt_changes = 0;
  int avg_encode_ms = 0;
  int encode_usage_percent = 0;
  uint32_t frames_encoded = 0;
  bool has_entered_low_resolution = false;
  absl::optional<uint64_t> qp_sum;
  webrtc::VideoContentType content_type = webrtc::VideoContentType::UNSPECIFIED;
  // https://w3c.github.io/webrtc-stats/#dom-rtcvideosenderstats-hugeframessent
  uint32_t huge_frames_sent = 0;
};

struct VideoReceiverInfo : public MediaReceiverInfo {
  VideoReceiverInfo();
  ~VideoReceiverInfo();
  std::vector<SsrcGroup> ssrc_groups;
  // TODO(hbos): Move this to |VideoMediaInfo::receive_codecs|?
  std::string decoder_implementation_name;
  int packets_concealed = 0;
  int firs_sent = 0;
  int plis_sent = 0;
  int nacks_sent = 0;
  int frame_width = 0;
  int frame_height = 0;
  int framerate_rcvd = 0;
  int framerate_decoded = 0;
  int framerate_output = 0;
  // Framerate as sent to the renderer.
  int framerate_render_input = 0;
  // Framerate that the renderer reports.
  int framerate_render_output = 0;
  uint32_t frames_received = 0;
  uint32_t frames_decoded = 0;
  uint32_t frames_rendered = 0;
  absl::optional<uint64_t> qp_sum;
  int64_t interframe_delay_max_ms = -1;

  webrtc::VideoContentType content_type = webrtc::VideoContentType::UNSPECIFIED;

  // All stats below are gathered per-VideoReceiver, but some will be correlated
  // across MediaStreamTracks.  NOTE(hta): when sinking stats into per-SSRC
  // structures, reflect this in the new layout.

  // Current frame decode latency.
  int decode_ms = 0;
  // Maximum observed frame decode latency.
  int max_decode_ms = 0;
  // Jitter (network-related) latency.
  int jitter_buffer_ms = 0;
  // Requested minimum playout latency.
  int min_playout_delay_ms = 0;
  // Requested latency to account for rendering delay.
  int render_delay_ms = 0;
  // Target overall delay: network+decode+render, accounting for
  // min_playout_delay_ms.
  int target_delay_ms = 0;
  // Current overall delay, possibly ramping towards target_delay_ms.
  int current_delay_ms = 0;

  // Estimated capture start time in NTP time in ms.
  int64_t capture_start_ntp_time_ms = -1;

  // Timing frame info: all important timestamps for a full lifetime of a
  // single 'timing frame'.
  absl::optional<webrtc::TimingFrameInfo> timing_frame_info;
};

struct DataSenderInfo : public MediaSenderInfo {
  uint32_t ssrc = 0;
};

struct DataReceiverInfo : public MediaReceiverInfo {
  uint32_t ssrc = 0;
};

struct BandwidthEstimationInfo {
  int available_send_bandwidth = 0;
  int available_recv_bandwidth = 0;
  int target_enc_bitrate = 0;
  int actual_enc_bitrate = 0;
  int retransmit_bitrate = 0;
  int transmit_bitrate = 0;
  int64_t bucket_delay = 0;
};

// Maps from payload type to |RtpCodecParameters|.
typedef std::map<int, webrtc::RtpCodecParameters> RtpCodecParametersMap;

struct VoiceMediaInfo {
  VoiceMediaInfo();
  ~VoiceMediaInfo();
  void Clear() {
    senders.clear();
    receivers.clear();
    send_codecs.clear();
    receive_codecs.clear();
  }
  std::vector<VoiceSenderInfo> senders;
  std::vector<VoiceReceiverInfo> receivers;
  RtpCodecParametersMap send_codecs;
  RtpCodecParametersMap receive_codecs;
};

struct VideoMediaInfo {
  VideoMediaInfo();
  ~VideoMediaInfo();
  void Clear() {
    senders.clear();
    receivers.clear();
    bw_estimations.clear();
    send_codecs.clear();
    receive_codecs.clear();
  }
  std::vector<VideoSenderInfo> senders;
  std::vector<VideoReceiverInfo> receivers;
  // Deprecated.
  // TODO(holmer): Remove once upstream projects no longer use this.
  std::vector<BandwidthEstimationInfo> bw_estimations;
  RtpCodecParametersMap send_codecs;
  RtpCodecParametersMap receive_codecs;
};

struct DataMediaInfo {
  DataMediaInfo();
  ~DataMediaInfo();
  void Clear() {
    senders.clear();
    receivers.clear();
  }
  std::vector<DataSenderInfo> senders;
  std::vector<DataReceiverInfo> receivers;
};

struct RtcpParameters {
  bool reduced_size = false;
};

template <class Codec>
struct RtpParameters {
  virtual ~RtpParameters() = default;

  std::vector<Codec> codecs;
  std::vector<webrtc::RtpExtension> extensions;
  // TODO(pthatcher): Add streams.
  RtcpParameters rtcp;

  std::string ToString() const {
    rtc::StringBuilder ost;
    ost << "{";
    const char* separator = "";
    for (const auto& entry : ToStringMap()) {
      ost << separator << entry.first << ": " << entry.second;
      separator = ", ";
    }
    ost << "}";
    return ost.Release();
  }

 protected:
  virtual std::map<std::string, std::string> ToStringMap() const {
    return {{"codecs", VectorToString(codecs)},
            {"extensions", VectorToString(extensions)}};
  }
};

// TODO(deadbeef): Rename to RtpSenderParameters, since they're intended to
// encapsulate all the parameters needed for an RtpSender.
template <class Codec>
struct RtpSendParameters : RtpParameters<Codec> {
  int max_bandwidth_bps = -1;
  // This is the value to be sent in the MID RTP header extension (if the header
  // extension in included in the list of extensions).
  std::string mid;

 protected:
  std::map<std::string, std::string> ToStringMap() const override {
    auto params = RtpParameters<Codec>::ToStringMap();
    params["max_bandwidth_bps"] = rtc::ToString(max_bandwidth_bps);
    params["mid"] = (mid.empty() ? "<not set>" : mid);
    return params;
  }
};

struct AudioSendParameters : RtpSendParameters<AudioCodec> {
  AudioSendParameters();
  ~AudioSendParameters() override;
  AudioOptions options;

 protected:
  std::map<std::string, std::string> ToStringMap() const override;
};

struct AudioRecvParameters : RtpParameters<AudioCodec> {};

class VoiceMediaChannel : public MediaChannel {
 public:
  VoiceMediaChannel() {}
  explicit VoiceMediaChannel(const MediaConfig& config)
      : MediaChannel(config) {}
  ~VoiceMediaChannel() override {}
  virtual bool SetSendParameters(const AudioSendParameters& params) = 0;
  virtual bool SetRecvParameters(const AudioRecvParameters& params) = 0;
  virtual webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const = 0;
  virtual webrtc::RTCError SetRtpSendParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters) = 0;
  // Get the receive parameters for the incoming stream identified by |ssrc|.
  // If |ssrc| is 0, retrieve the receive parameters for the default receive
  // stream, which is used when SSRCs are not signaled. Note that calling with
  // an |ssrc| of 0 will return encoding parameters with an unset |ssrc|
  // member.
  virtual webrtc::RtpParameters GetRtpReceiveParameters(
      uint32_t ssrc) const = 0;
  virtual bool SetRtpReceiveParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters) = 0;
  // Starts or stops playout of received audio.
  virtual void SetPlayout(bool playout) = 0;
  // Starts or stops sending (and potentially capture) of local audio.
  virtual void SetSend(bool send) = 0;
  // Configure stream for sending.
  virtual bool SetAudioSend(uint32_t ssrc,
                            bool enable,
                            const AudioOptions* options,
                            AudioSource* source) = 0;
  // Set speaker output volume of the specified ssrc.
  virtual bool SetOutputVolume(uint32_t ssrc, double volume) = 0;
  // Returns if the telephone-event has been negotiated.
  virtual bool CanInsertDtmf() = 0;
  // Send a DTMF |event|. The DTMF out-of-band signal will be used.
  // The |ssrc| should be either 0 or a valid send stream ssrc.
  // The valid value for the |event| are 0 to 15 which corresponding to
  // DTMF event 0-9, *, #, A-D.
  virtual bool InsertDtmf(uint32_t ssrc, int event, int duration) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VoiceMediaInfo* info) = 0;

  virtual void SetRawAudioSink(
      uint32_t ssrc,
      std::unique_ptr<webrtc::AudioSinkInterface> sink) = 0;

  virtual std::vector<webrtc::RtpSource> GetSources(uint32_t ssrc) const = 0;
};

// TODO(deadbeef): Rename to VideoSenderParameters, since they're intended to
// encapsulate all the parameters needed for a video RtpSender.
struct VideoSendParameters : RtpSendParameters<VideoCodec> {
  VideoSendParameters();
  ~VideoSendParameters() override;
  // Use conference mode? This flag comes from the remote
  // description's SDP line 'a=x-google-flag:conference', copied over
  // by VideoChannel::SetRemoteContent_w, and ultimately used by
  // conference mode screencast logic in
  // WebRtcVideoChannel::WebRtcVideoSendStream::CreateVideoEncoderConfig.
  // The special screencast behaviour is disabled by default.
  bool conference_mode = false;

 protected:
  std::map<std::string, std::string> ToStringMap() const override;
};

// TODO(deadbeef): Rename to VideoReceiverParameters, since they're intended to
// encapsulate all the parameters needed for a video RtpReceiver.
struct VideoRecvParameters : RtpParameters<VideoCodec> {};

class VideoMediaChannel : public MediaChannel {
 public:
  VideoMediaChannel() {}
  explicit VideoMediaChannel(const MediaConfig& config)
      : MediaChannel(config) {}
  ~VideoMediaChannel() override {}

  virtual bool SetSendParameters(const VideoSendParameters& params) = 0;
  virtual bool SetRecvParameters(const VideoRecvParameters& params) = 0;
  virtual webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const = 0;
  virtual webrtc::RTCError SetRtpSendParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters) = 0;
  // Get the receive parameters for the incoming stream identified by |ssrc|.
  // If |ssrc| is 0, retrieve the receive parameters for the default receive
  // stream, which is used when SSRCs are not signaled. Note that calling with
  // an |ssrc| of 0 will return encoding parameters with an unset |ssrc|
  // member.
  virtual webrtc::RtpParameters GetRtpReceiveParameters(
      uint32_t ssrc) const = 0;
  virtual bool SetRtpReceiveParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters) = 0;
  // Gets the currently set codecs/payload types to be used for outgoing media.
  virtual bool GetSendCodec(VideoCodec* send_codec) = 0;
  // Starts or stops transmission (and potentially capture) of local video.
  virtual bool SetSend(bool send) = 0;
  // Configure stream for sending and register a source.
  // The |ssrc| must correspond to a registered send stream.
  virtual bool SetVideoSend(
      uint32_t ssrc,
      const VideoOptions* options,
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source) = 0;
  // Sets the sink object to be used for the specified stream.
  // If SSRC is 0, the sink is used for the 'default' stream.
  virtual bool SetSink(uint32_t ssrc,
                       rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) = 0;
  // This fills the "bitrate parts" (rtx, video bitrate) of the
  // BandwidthEstimationInfo, since that part that isn't possible to get
  // through webrtc::Call::GetStats, as they are statistics of the send
  // streams.
  // TODO(holmer): We should change this so that either BWE graphs doesn't
  // need access to bitrates of the streams, or change the (RTC)StatsCollector
  // so that it's getting the send stream stats separately by calling
  // GetStats(), and merges with BandwidthEstimationInfo by itself.
  virtual void FillBitrateInfo(BandwidthEstimationInfo* bwe_info) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VideoMediaInfo* info) = 0;

  virtual std::vector<webrtc::RtpSource> GetSources(uint32_t ssrc) const = 0;
};

enum DataMessageType {
  // Chrome-Internal use only.  See SctpDataMediaChannel for the actual PPID
  // values.
  DMT_NONE = 0,
  DMT_CONTROL = 1,
  DMT_BINARY = 2,
  DMT_TEXT = 3,
};

// Info about data received in DataMediaChannel.  For use in
// DataMediaChannel::SignalDataReceived and in all of the signals that
// signal fires, on up the chain.
struct ReceiveDataParams {
  // The in-packet stream indentifier.
  // RTP data channels use SSRCs, SCTP data channels use SIDs.
  union {
    uint32_t ssrc;
    int sid = 0;
  };
  // The type of message (binary, text, or control).
  DataMessageType type = DMT_TEXT;
  // A per-stream value incremented per packet in the stream.
  int seq_num = 0;
  // A per-stream value monotonically increasing with time.
  int timestamp = 0;
};

struct SendDataParams {
  // The in-packet stream indentifier.
  // RTP data channels use SSRCs, SCTP data channels use SIDs.
  union {
    uint32_t ssrc;
    int sid = 0;
  };
  // The type of message (binary, text, or control).
  DataMessageType type = DMT_TEXT;

  // TODO(pthatcher): Make |ordered| and |reliable| true by default?
  // For SCTP, whether to send messages flagged as ordered or not.
  // If false, messages can be received out of order.
  bool ordered = false;
  // For SCTP, whether the messages are sent reliably or not.
  // If false, messages may be lost.
  bool reliable = false;
  // For SCTP, if reliable == false, provide partial reliability by
  // resending up to this many times.  Either count or millis
  // is supported, not both at the same time.
  int max_rtx_count = 0;
  // For SCTP, if reliable == false, provide partial reliability by
  // resending for up to this many milliseconds.  Either count or millis
  // is supported, not both at the same time.
  int max_rtx_ms = 0;
};

enum SendDataResult { SDR_SUCCESS, SDR_ERROR, SDR_BLOCK };

struct DataSendParameters : RtpSendParameters<DataCodec> {};

struct DataRecvParameters : RtpParameters<DataCodec> {};

class DataMediaChannel : public MediaChannel {
 public:
  DataMediaChannel();
  explicit DataMediaChannel(const MediaConfig& config);
  ~DataMediaChannel() override;

  virtual bool SetSendParameters(const DataSendParameters& params) = 0;
  virtual bool SetRecvParameters(const DataRecvParameters& params) = 0;

  // TODO(pthatcher): Implement this.
  virtual bool GetStats(DataMediaInfo* info);

  virtual bool SetSend(bool send) = 0;
  virtual bool SetReceive(bool receive) = 0;

  void OnNetworkRouteChanged(const std::string& transport_name,
                             const rtc::NetworkRoute& network_route) override {}

  virtual bool SendData(const SendDataParams& params,
                        const rtc::CopyOnWriteBuffer& payload,
                        SendDataResult* result = NULL) = 0;
  // Signals when data is received (params, data, len)
  sigslot::signal3<const ReceiveDataParams&, const char*, size_t>
      SignalDataReceived;
  // Signal when the media channel is ready to send the stream. Arguments are:
  //     writable(bool)
  sigslot::signal1<bool> SignalReadyToSend;
};

}  // namespace cricket

#endif  // MEDIA_BASE_MEDIACHANNEL_H_
