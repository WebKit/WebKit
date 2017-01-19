/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_MEDIACHANNEL_H_
#define WEBRTC_MEDIA_BASE_MEDIACHANNEL_H_

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/rtpparameters.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/networkroute.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/socket.h"
#include "webrtc/base/window.h"
#include "webrtc/config.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/media/base/streamparams.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/media/base/videosourceinterface.h"
// TODO(juberti): re-evaluate this include
#include "webrtc/pc/audiomonitor.h"

namespace rtc {
class RateLimiter;
class Timing;
}

namespace webrtc {
class AudioSinkInterface;
class VideoFrame;
}

namespace cricket {

class AudioSource;
class VideoCapturer;
struct RtpHeader;
struct VideoFormat;

const int kScreencastDefaultFps = 5;

template <class T>
static std::string ToStringIfSet(const char* key, const rtc::Optional<T>& val) {
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
    std::ostringstream ost;
    ost << "[";
    for (size_t i = 0; i < vals.size(); ++i) {
      if (i > 0) {
        ost << ", ";
      }
      ost << vals[i].ToString();
    }
    ost << "]";
    return ost.str();
}

template <typename T>
static T MinPositive(T a, T b) {
  if (a <= 0) {
    return b;
  }
  if (b <= 0) {
    return a;
  }
  return std::min(a, b);
}

// Construction-time settings, passed to
// MediaControllerInterface::Create, and passed on when creating
// MediaChannels.
struct MediaConfig {
  // Set DSCP value on packets. This flag comes from the
  // PeerConnection constraint 'googDscp'.
  bool enable_dscp = false;

  // Video-specific config.
  struct Video {
    // Enable WebRTC CPU Overuse Detection. This flag comes from the
    // PeerConnection constraint 'googCpuOveruseDetection'.
    bool enable_cpu_overuse_detection = true;

    // Enable WebRTC suspension of video. No video frames will be sent
    // when the bitrate is below the configured minimum bitrate. This
    // flag comes from the PeerConnection constraint
    // 'googSuspendBelowMinBitrate', and WebRtcVideoChannel2 copies it
    // to VideoSendStream::Config::suspend_below_min_bitrate.
    bool suspend_below_min_bitrate = false;

    // Set to true if the renderer has an algorithm of frame selection.
    // If the value is true, then WebRTC will hand over a frame as soon as
    // possible without delay, and rendering smoothness is completely the duty
    // of the renderer;
    // If the value is false, then WebRTC is responsible to delay frame release
    // in order to increase rendering smoothness.
    //
    // This flag comes from PeerConnection's RtcConfiguration, but is
    // currently only set by the command line flag
    // 'disable-rtc-smoothness-algorithm'.
    // WebRtcVideoChannel2::AddRecvStream copies it to the created
    // WebRtcVideoReceiveStream, where it is returned by the
    // SmoothsRenderedFrames method. This method is used by the
    // VideoReceiveStream, where the value is passed on to the
    // IncomingVideoStream constructor.
    bool disable_prerenderer_smoothing = false;
  } video;
};

// Options that can be applied to a VoiceMediaChannel or a VoiceMediaEngine.
// Used to be flags, but that makes it hard to selectively apply options.
// We are moving all of the setting of options to structs like this,
// but some things currently still use flags.
struct AudioOptions {
  void SetAll(const AudioOptions& change) {
    SetFrom(&echo_cancellation, change.echo_cancellation);
    SetFrom(&auto_gain_control, change.auto_gain_control);
    SetFrom(&noise_suppression, change.noise_suppression);
    SetFrom(&highpass_filter, change.highpass_filter);
    SetFrom(&stereo_swapping, change.stereo_swapping);
    SetFrom(&audio_jitter_buffer_max_packets,
            change.audio_jitter_buffer_max_packets);
    SetFrom(&audio_jitter_buffer_fast_accelerate,
            change.audio_jitter_buffer_fast_accelerate);
    SetFrom(&typing_detection, change.typing_detection);
    SetFrom(&aecm_generate_comfort_noise, change.aecm_generate_comfort_noise);
    SetFrom(&adjust_agc_delta, change.adjust_agc_delta);
    SetFrom(&experimental_agc, change.experimental_agc);
    SetFrom(&extended_filter_aec, change.extended_filter_aec);
    SetFrom(&delay_agnostic_aec, change.delay_agnostic_aec);
    SetFrom(&experimental_ns, change.experimental_ns);
    SetFrom(&intelligibility_enhancer, change.intelligibility_enhancer);
    SetFrom(&level_control, change.level_control);
    SetFrom(&residual_echo_detector, change.residual_echo_detector);
    SetFrom(&tx_agc_target_dbov, change.tx_agc_target_dbov);
    SetFrom(&tx_agc_digital_compression_gain,
            change.tx_agc_digital_compression_gain);
    SetFrom(&tx_agc_limiter, change.tx_agc_limiter);
    SetFrom(&recording_sample_rate, change.recording_sample_rate);
    SetFrom(&playout_sample_rate, change.playout_sample_rate);
    SetFrom(&combined_audio_video_bwe, change.combined_audio_video_bwe);
    SetFrom(&audio_network_adaptor, change.audio_network_adaptor);
    SetFrom(&audio_network_adaptor_config, change.audio_network_adaptor_config);
    SetFrom(&level_control_initial_peak_level_dbfs,
            change.level_control_initial_peak_level_dbfs);
  }

  bool operator==(const AudioOptions& o) const {
    return echo_cancellation == o.echo_cancellation &&
           auto_gain_control == o.auto_gain_control &&
           noise_suppression == o.noise_suppression &&
           highpass_filter == o.highpass_filter &&
           stereo_swapping == o.stereo_swapping &&
           audio_jitter_buffer_max_packets ==
               o.audio_jitter_buffer_max_packets &&
           audio_jitter_buffer_fast_accelerate ==
               o.audio_jitter_buffer_fast_accelerate &&
           typing_detection == o.typing_detection &&
           aecm_generate_comfort_noise == o.aecm_generate_comfort_noise &&
           experimental_agc == o.experimental_agc &&
           extended_filter_aec == o.extended_filter_aec &&
           delay_agnostic_aec == o.delay_agnostic_aec &&
           experimental_ns == o.experimental_ns &&
           intelligibility_enhancer == o.intelligibility_enhancer &&
           level_control == o.level_control &&
           residual_echo_detector == o.residual_echo_detector &&
           adjust_agc_delta == o.adjust_agc_delta &&
           tx_agc_target_dbov == o.tx_agc_target_dbov &&
           tx_agc_digital_compression_gain ==
               o.tx_agc_digital_compression_gain &&
           tx_agc_limiter == o.tx_agc_limiter &&
           recording_sample_rate == o.recording_sample_rate &&
           playout_sample_rate == o.playout_sample_rate &&
           combined_audio_video_bwe == o.combined_audio_video_bwe &&
           audio_network_adaptor == o.audio_network_adaptor &&
           audio_network_adaptor_config == o.audio_network_adaptor_config &&
           level_control_initial_peak_level_dbfs ==
               o.level_control_initial_peak_level_dbfs;
  }
  bool operator!=(const AudioOptions& o) const { return !(*this == o); }

  std::string ToString() const {
    std::ostringstream ost;
    ost << "AudioOptions {";
    ost << ToStringIfSet("aec", echo_cancellation);
    ost << ToStringIfSet("agc", auto_gain_control);
    ost << ToStringIfSet("ns", noise_suppression);
    ost << ToStringIfSet("hf", highpass_filter);
    ost << ToStringIfSet("swap", stereo_swapping);
    ost << ToStringIfSet("audio_jitter_buffer_max_packets",
                         audio_jitter_buffer_max_packets);
    ost << ToStringIfSet("audio_jitter_buffer_fast_accelerate",
                         audio_jitter_buffer_fast_accelerate);
    ost << ToStringIfSet("typing", typing_detection);
    ost << ToStringIfSet("comfort_noise", aecm_generate_comfort_noise);
    ost << ToStringIfSet("agc_delta", adjust_agc_delta);
    ost << ToStringIfSet("experimental_agc", experimental_agc);
    ost << ToStringIfSet("extended_filter_aec", extended_filter_aec);
    ost << ToStringIfSet("delay_agnostic_aec", delay_agnostic_aec);
    ost << ToStringIfSet("experimental_ns", experimental_ns);
    ost << ToStringIfSet("intelligibility_enhancer", intelligibility_enhancer);
    ost << ToStringIfSet("level_control", level_control);
    ost << ToStringIfSet("level_control_initial_peak_level_dbfs",
                         level_control_initial_peak_level_dbfs);
    ost << ToStringIfSet("residual_echo_detector", residual_echo_detector);
    ost << ToStringIfSet("tx_agc_target_dbov", tx_agc_target_dbov);
    ost << ToStringIfSet("tx_agc_digital_compression_gain",
        tx_agc_digital_compression_gain);
    ost << ToStringIfSet("tx_agc_limiter", tx_agc_limiter);
    ost << ToStringIfSet("recording_sample_rate", recording_sample_rate);
    ost << ToStringIfSet("playout_sample_rate", playout_sample_rate);
    ost << ToStringIfSet("combined_audio_video_bwe", combined_audio_video_bwe);
    ost << ToStringIfSet("audio_network_adaptor", audio_network_adaptor);
    // The adaptor config is a serialized proto buffer and therefore not human
    // readable. So we comment out the following line.
    // ost << ToStringIfSet("audio_network_adaptor_config",
    //     audio_network_adaptor_config);
    ost << "}";
    return ost.str();
  }

  // Audio processing that attempts to filter away the output signal from
  // later inbound pickup.
  rtc::Optional<bool> echo_cancellation;
  // Audio processing to adjust the sensitivity of the local mic dynamically.
  rtc::Optional<bool> auto_gain_control;
  // Audio processing to filter out background noise.
  rtc::Optional<bool> noise_suppression;
  // Audio processing to remove background noise of lower frequencies.
  rtc::Optional<bool> highpass_filter;
  // Audio processing to swap the left and right channels.
  rtc::Optional<bool> stereo_swapping;
  // Audio receiver jitter buffer (NetEq) max capacity in number of packets.
  rtc::Optional<int> audio_jitter_buffer_max_packets;
  // Audio receiver jitter buffer (NetEq) fast accelerate mode.
  rtc::Optional<bool> audio_jitter_buffer_fast_accelerate;
  // Audio processing to detect typing.
  rtc::Optional<bool> typing_detection;
  rtc::Optional<bool> aecm_generate_comfort_noise;
  rtc::Optional<int> adjust_agc_delta;
  rtc::Optional<bool> experimental_agc;
  rtc::Optional<bool> extended_filter_aec;
  rtc::Optional<bool> delay_agnostic_aec;
  rtc::Optional<bool> experimental_ns;
  rtc::Optional<bool> intelligibility_enhancer;
  rtc::Optional<bool> level_control;
  // Specifies an optional initialization value for the level controller.
  rtc::Optional<float> level_control_initial_peak_level_dbfs;
  // Note that tx_agc_* only applies to non-experimental AGC.
  rtc::Optional<bool> residual_echo_detector;
  rtc::Optional<uint16_t> tx_agc_target_dbov;
  rtc::Optional<uint16_t> tx_agc_digital_compression_gain;
  rtc::Optional<bool> tx_agc_limiter;
  rtc::Optional<uint32_t> recording_sample_rate;
  rtc::Optional<uint32_t> playout_sample_rate;
  // Enable combined audio+bandwidth BWE.
  // TODO(pthatcher): This flag is set from the
  // "googCombinedAudioVideoBwe", but not used anywhere. So delete it,
  // and check if any other AudioOptions members are unused.
  rtc::Optional<bool> combined_audio_video_bwe;
  // Enable audio network adaptor.
  rtc::Optional<bool> audio_network_adaptor;
  // Config string for audio network adaptor.
  rtc::Optional<std::string> audio_network_adaptor_config;

 private:
  template <typename T>
  static void SetFrom(rtc::Optional<T>* s, const rtc::Optional<T>& o) {
    if (o) {
      *s = o;
    }
  }
};

// Options that can be applied to a VideoMediaChannel or a VideoMediaEngine.
// Used to be flags, but that makes it hard to selectively apply options.
// We are moving all of the setting of options to structs like this,
// but some things currently still use flags.
struct VideoOptions {
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
    std::ostringstream ost;
    ost << "VideoOptions {";
    ost << ToStringIfSet("noise reduction", video_noise_reduction);
    ost << ToStringIfSet("screencast min bitrate kbps",
                         screencast_min_bitrate_kbps);
    ost << ToStringIfSet("is_screencast ", is_screencast);
    ost << "}";
    return ost.str();
  }

  // Enable denoising? This flag comes from the getUserMedia
  // constraint 'googNoiseReduction', and WebRtcVideoEngine2 passes it
  // on to the codec options. Disabled by default.
  rtc::Optional<bool> video_noise_reduction;
  // Force screencast to use a minimum bitrate. This flag comes from
  // the PeerConnection constraint 'googScreencastMinBitrate'. It is
  // copied to the encoder config by WebRtcVideoChannel2.
  rtc::Optional<int> screencast_min_bitrate_kbps;
  // Set by screencast sources. Implies selection of encoding settings
  // suitable for screencast. Most likely not the right way to do
  // things, e.g., screencast of a text document and screencast of a
  // youtube video have different needs.
  rtc::Optional<bool> is_screencast;

 private:
  template <typename T>
  static void SetFrom(rtc::Optional<T>* s, const rtc::Optional<T>& o) {
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
    std::ostringstream ost;
    ost << "{";
    ost << "uri: " << uri;
    ost << ", id: " << id;
    ost << "}";
    return ost.str();
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
    virtual int SetOption(SocketType type, rtc::Socket::Option opt,
                          int option) = 0;
    virtual ~NetworkInterface() {}
  };

  explicit MediaChannel(const MediaConfig& config)
      : enable_dscp_(config.enable_dscp), network_interface_(NULL) {}
  MediaChannel() : enable_dscp_(false), network_interface_(NULL) {}
  virtual ~MediaChannel() {}

  // Sets the abstract interface class for sending RTP/RTCP data.
  virtual void SetInterface(NetworkInterface *iface) {
    rtc::CritScope cs(&network_interface_crit_);
    network_interface_ = iface;
    SetDscp(enable_dscp_ ? PreferredDscp() : rtc::DSCP_DEFAULT);
  }
  virtual rtc::DiffServCodePoint PreferredDscp() const {
    return rtc::DSCP_DEFAULT;
  }
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
  // Called when the rtp transport overhead changed.
  virtual void OnTransportOverheadChanged(
      int transport_overhead_per_packet) = 0;
  // Creates a new outgoing media stream with SSRCs and CNAME as described
  // by sp.
  virtual bool AddSendStream(const StreamParams& sp) = 0;
  // Removes an outgoing media stream.
  // ssrc must be the first SSRC of the media stream if the stream uses
  // multiple SSRCs.
  virtual bool RemoveSendStream(uint32_t ssrc) = 0;
  // Creates a new incoming media stream with SSRCs and CNAME as described
  // by sp.
  virtual bool AddRecvStream(const StreamParams& sp) = 0;
  // Removes an incoming media stream.
  // ssrc must be the first SSRC of the media stream if the stream uses
  // multiple SSRCs.
  virtual bool RemoveRecvStream(uint32_t ssrc) = 0;

  // Returns the absoulte sendtime extension id value from media channel.
  virtual int GetRtpSendTimeExtnId() const {
    return -1;
  }

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

 private:
  // This method sets DSCP |value| on both RTP and RTCP channels.
  int SetDscp(rtc::DiffServCodePoint value) {
    int ret;
    ret = SetOption(NetworkInterface::ST_RTP,
                    rtc::Socket::OPT_DSCP,
                    value);
    if (ret == 0) {
      ret = SetOption(NetworkInterface::ST_RTCP,
                      rtc::Socket::OPT_DSCP,
                      value);
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
  SsrcSenderInfo()
      : ssrc(0),
    timestamp(0) {
  }
  uint32_t ssrc;
  double timestamp;  // NTP timestamp, represented as seconds since epoch.
};

struct SsrcReceiverInfo {
  SsrcReceiverInfo()
      : ssrc(0),
        timestamp(0) {
  }
  uint32_t ssrc;
  double timestamp;
};

struct MediaSenderInfo {
  MediaSenderInfo()
      : bytes_sent(0),
        packets_sent(0),
        packets_lost(0),
        fraction_lost(0.0),
        rtt_ms(0) {
  }
  void add_ssrc(const SsrcSenderInfo& stat) {
    local_stats.push_back(stat);
  }
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
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  uint32_t ssrc() const {
    if (local_stats.size() > 0) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }
  int64_t bytes_sent;
  int packets_sent;
  int packets_lost;
  float fraction_lost;
  int64_t rtt_ms;
  std::string codec_name;
  std::vector<SsrcSenderInfo> local_stats;
  std::vector<SsrcReceiverInfo> remote_stats;
};

struct MediaReceiverInfo {
  MediaReceiverInfo()
      : bytes_rcvd(0),
        packets_rcvd(0),
        packets_lost(0),
        fraction_lost(0.0) {
  }
  void add_ssrc(const SsrcReceiverInfo& stat) {
    local_stats.push_back(stat);
  }
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
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  uint32_t ssrc() const {
    if (local_stats.size() > 0) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }

  int64_t bytes_rcvd;
  int packets_rcvd;
  int packets_lost;
  float fraction_lost;
  std::string codec_name;
  std::vector<SsrcReceiverInfo> local_stats;
  std::vector<SsrcSenderInfo> remote_stats;
};

struct VoiceSenderInfo : public MediaSenderInfo {
  VoiceSenderInfo()
      : ext_seqnum(0),
        jitter_ms(0),
        audio_level(0),
        aec_quality_min(0.0),
        echo_delay_median_ms(0),
        echo_delay_std_ms(0),
        echo_return_loss(0),
        echo_return_loss_enhancement(0),
        residual_echo_likelihood(0.0f),
        typing_noise_detected(false) {
  }

  int ext_seqnum;
  int jitter_ms;
  int audio_level;
  float aec_quality_min;
  int echo_delay_median_ms;
  int echo_delay_std_ms;
  int echo_return_loss;
  int echo_return_loss_enhancement;
  float residual_echo_likelihood;
  bool typing_noise_detected;
};

struct VoiceReceiverInfo : public MediaReceiverInfo {
  VoiceReceiverInfo()
      : ext_seqnum(0),
        jitter_ms(0),
        jitter_buffer_ms(0),
        jitter_buffer_preferred_ms(0),
        delay_estimate_ms(0),
        audio_level(0),
        expand_rate(0),
        speech_expand_rate(0),
        secondary_decoded_rate(0),
        accelerate_rate(0),
        preemptive_expand_rate(0),
        decoding_calls_to_silence_generator(0),
        decoding_calls_to_neteq(0),
        decoding_normal(0),
        decoding_plc(0),
        decoding_cng(0),
        decoding_plc_cng(0),
        decoding_muted_output(0),
        capture_start_ntp_time_ms(-1) {}

  int ext_seqnum;
  int jitter_ms;
  int jitter_buffer_ms;
  int jitter_buffer_preferred_ms;
  int delay_estimate_ms;
  int audio_level;
  // fraction of synthesized audio inserted through expansion.
  float expand_rate;
  // fraction of synthesized speech inserted through expansion.
  float speech_expand_rate;
  // fraction of data out of secondary decoding, including FEC and RED.
  float secondary_decoded_rate;
  // Fraction of data removed through time compression.
  float accelerate_rate;
  // Fraction of data inserted through time stretching.
  float preemptive_expand_rate;
  int decoding_calls_to_silence_generator;
  int decoding_calls_to_neteq;
  int decoding_normal;
  int decoding_plc;
  int decoding_cng;
  int decoding_plc_cng;
  int decoding_muted_output;
  // Estimated capture start time in NTP time in ms.
  int64_t capture_start_ntp_time_ms;
};

struct VideoSenderInfo : public MediaSenderInfo {
  VideoSenderInfo()
      : packets_cached(0),
        firs_rcvd(0),
        plis_rcvd(0),
        nacks_rcvd(0),
        send_frame_width(0),
        send_frame_height(0),
        framerate_input(0),
        framerate_sent(0),
        nominal_bitrate(0),
        preferred_bitrate(0),
        adapt_reason(0),
        adapt_changes(0),
        avg_encode_ms(0),
        encode_usage_percent(0),
        frames_encoded(0) {}

  std::vector<SsrcGroup> ssrc_groups;
  // TODO(hbos): Move this to |VideoMediaInfo::send_codecs|?
  std::string encoder_implementation_name;
  // TODO(hbos): Move this to |MediaSenderInfo| when supported by
  // |VoiceSenderInfo| as well (which also extends that class).
  rtc::Optional<uint32_t> codec_payload_type;
  int packets_cached;
  int firs_rcvd;
  int plis_rcvd;
  int nacks_rcvd;
  int send_frame_width;
  int send_frame_height;
  int framerate_input;
  int framerate_sent;
  int nominal_bitrate;
  int preferred_bitrate;
  int adapt_reason;
  int adapt_changes;
  int avg_encode_ms;
  int encode_usage_percent;
  uint32_t frames_encoded;
  rtc::Optional<uint64_t> qp_sum;
};

struct VideoReceiverInfo : public MediaReceiverInfo {
  VideoReceiverInfo()
      : packets_concealed(0),
        firs_sent(0),
        plis_sent(0),
        nacks_sent(0),
        frame_width(0),
        frame_height(0),
        framerate_rcvd(0),
        framerate_decoded(0),
        framerate_output(0),
        framerate_render_input(0),
        framerate_render_output(0),
        frames_decoded(0),
        decode_ms(0),
        max_decode_ms(0),
        jitter_buffer_ms(0),
        min_playout_delay_ms(0),
        render_delay_ms(0),
        target_delay_ms(0),
        current_delay_ms(0),
        capture_start_ntp_time_ms(-1) {
  }

  std::vector<SsrcGroup> ssrc_groups;
  // TODO(hbos): Move this to |VideoMediaInfo::receive_codecs|?
  std::string decoder_implementation_name;
  // TODO(hbos): Move this to |MediaReceiverInfo| when supported by
  // |VoiceReceiverInfo| as well (which also extends that class).
  rtc::Optional<uint32_t> codec_payload_type;
  int packets_concealed;
  int firs_sent;
  int plis_sent;
  int nacks_sent;
  int frame_width;
  int frame_height;
  int framerate_rcvd;
  int framerate_decoded;
  int framerate_output;
  // Framerate as sent to the renderer.
  int framerate_render_input;
  // Framerate that the renderer reports.
  int framerate_render_output;
  uint32_t frames_decoded;

  // All stats below are gathered per-VideoReceiver, but some will be correlated
  // across MediaStreamTracks.  NOTE(hta): when sinking stats into per-SSRC
  // structures, reflect this in the new layout.

  // Current frame decode latency.
  int decode_ms;
  // Maximum observed frame decode latency.
  int max_decode_ms;
  // Jitter (network-related) latency.
  int jitter_buffer_ms;
  // Requested minimum playout latency.
  int min_playout_delay_ms;
  // Requested latency to account for rendering delay.
  int render_delay_ms;
  // Target overall delay: network+decode+render, accounting for
  // min_playout_delay_ms.
  int target_delay_ms;
  // Current overall delay, possibly ramping towards target_delay_ms.
  int current_delay_ms;

  // Estimated capture start time in NTP time in ms.
  int64_t capture_start_ntp_time_ms;
};

struct DataSenderInfo : public MediaSenderInfo {
  DataSenderInfo()
      : ssrc(0) {
  }

  uint32_t ssrc;
};

struct DataReceiverInfo : public MediaReceiverInfo {
  DataReceiverInfo()
      : ssrc(0) {
  }

  uint32_t ssrc;
};

struct BandwidthEstimationInfo {
  BandwidthEstimationInfo()
      : available_send_bandwidth(0),
        available_recv_bandwidth(0),
        target_enc_bitrate(0),
        actual_enc_bitrate(0),
        retransmit_bitrate(0),
        transmit_bitrate(0),
        bucket_delay(0) {
  }

  int available_send_bandwidth;
  int available_recv_bandwidth;
  int target_enc_bitrate;
  int actual_enc_bitrate;
  int retransmit_bitrate;
  int transmit_bitrate;
  int64_t bucket_delay;
};

// Maps from payload type to |RtpCodecParameters|.
typedef std::map<int, webrtc::RtpCodecParameters> RtpCodecParametersMap;

struct VoiceMediaInfo {
  void Clear() {
    senders.clear();
    receivers.clear();
  }
  std::vector<VoiceSenderInfo> senders;
  std::vector<VoiceReceiverInfo> receivers;
};

struct VideoMediaInfo {
  void Clear() {
    senders.clear();
    receivers.clear();
    bw_estimations.clear();
    send_codecs.clear();
    receive_codecs.clear();
  }
  std::vector<VideoSenderInfo> senders;
  std::vector<VideoReceiverInfo> receivers;
  std::vector<BandwidthEstimationInfo> bw_estimations;
  RtpCodecParametersMap send_codecs;
  RtpCodecParametersMap receive_codecs;
};

struct DataMediaInfo {
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
  virtual std::string ToString() const {
    std::ostringstream ost;
    ost << "{";
    ost << "codecs: " << VectorToString(codecs) << ", ";
    ost << "extensions: " << VectorToString(extensions);
    ost << "}";
    return ost.str();
  }

  std::vector<Codec> codecs;
  std::vector<webrtc::RtpExtension> extensions;
  // TODO(pthatcher): Add streams.
  RtcpParameters rtcp;
  virtual ~RtpParameters() = default;
};

// TODO(deadbeef): Rename to RtpSenderParameters, since they're intended to
// encapsulate all the parameters needed for an RtpSender.
template <class Codec>
struct RtpSendParameters : RtpParameters<Codec> {
  std::string ToString() const override {
    std::ostringstream ost;
    ost << "{";
    ost << "codecs: " << VectorToString(this->codecs) << ", ";
    ost << "extensions: " << VectorToString(this->extensions) << ", ";
    ost << "max_bandwidth_bps: " << max_bandwidth_bps << ", ";
    ost << "}";
    return ost.str();
  }

  int max_bandwidth_bps = -1;
};

struct AudioSendParameters : RtpSendParameters<AudioCodec> {
  std::string ToString() const override {
    std::ostringstream ost;
    ost << "{";
    ost << "codecs: " << VectorToString(this->codecs) << ", ";
    ost << "extensions: " << VectorToString(this->extensions) << ", ";
    ost << "max_bandwidth_bps: " << max_bandwidth_bps << ", ";
    ost << "options: " << options.ToString();
    ost << "}";
    return ost.str();
  }

  AudioOptions options;
};

struct AudioRecvParameters : RtpParameters<AudioCodec> {
};

class VoiceMediaChannel : public MediaChannel {
 public:
  enum Error {
    ERROR_NONE = 0,                       // No error.
    ERROR_OTHER,                          // Other errors.
    ERROR_REC_DEVICE_OPEN_FAILED = 100,   // Could not open mic.
    ERROR_REC_DEVICE_MUTED,               // Mic was muted by OS.
    ERROR_REC_DEVICE_SILENT,              // No background noise picked up.
    ERROR_REC_DEVICE_SATURATION,          // Mic input is clipping.
    ERROR_REC_DEVICE_REMOVED,             // Mic was removed while active.
    ERROR_REC_RUNTIME_ERROR,              // Processing is encountering errors.
    ERROR_REC_SRTP_ERROR,                 // Generic SRTP failure.
    ERROR_REC_SRTP_AUTH_FAILED,           // Failed to authenticate packets.
    ERROR_REC_TYPING_NOISE_DETECTED,      // Typing noise is detected.
    ERROR_PLAY_DEVICE_OPEN_FAILED = 200,  // Could not open playout.
    ERROR_PLAY_DEVICE_MUTED,              // Playout muted by OS.
    ERROR_PLAY_DEVICE_REMOVED,            // Playout removed while active.
    ERROR_PLAY_RUNTIME_ERROR,             // Errors in voice processing.
    ERROR_PLAY_SRTP_ERROR,                // Generic SRTP failure.
    ERROR_PLAY_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_PLAY_SRTP_REPLAY,               // Packet replay detected.
  };

  VoiceMediaChannel() {}
  explicit VoiceMediaChannel(const MediaConfig& config)
      : MediaChannel(config) {}
  virtual ~VoiceMediaChannel() {}
  virtual bool SetSendParameters(const AudioSendParameters& params) = 0;
  virtual bool SetRecvParameters(const AudioRecvParameters& params) = 0;
  virtual webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const = 0;
  virtual bool SetRtpSendParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters) = 0;
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
  // Gets current energy levels for all incoming streams.
  virtual bool GetActiveStreams(AudioInfo::StreamList* actives) = 0;
  // Get the current energy level of the stream sent to the speaker.
  virtual int GetOutputLevel() = 0;
  // Get the time in milliseconds since last recorded keystroke, or negative.
  virtual int GetTimeSinceLastTyping() = 0;
  // Temporarily exposed field for tuning typing detect options.
  virtual void SetTypingDetectionParameters(int time_window,
    int cost_per_typing, int reporting_threshold, int penalty_decay,
    int type_event_delay) = 0;
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
};

// TODO(deadbeef): Rename to VideoSenderParameters, since they're intended to
// encapsulate all the parameters needed for a video RtpSender.
struct VideoSendParameters : RtpSendParameters<VideoCodec> {
  // Use conference mode? This flag comes from the remote
  // description's SDP line 'a=x-google-flag:conference', copied over
  // by VideoChannel::SetRemoteContent_w, and ultimately used by
  // conference mode screencast logic in
  // WebRtcVideoChannel2::WebRtcVideoSendStream::CreateVideoEncoderConfig.
  // The special screencast behaviour is disabled by default.
  bool conference_mode = false;
};

// TODO(deadbeef): Rename to VideoReceiverParameters, since they're intended to
// encapsulate all the parameters needed for a video RtpReceiver.
struct VideoRecvParameters : RtpParameters<VideoCodec> {
};

class VideoMediaChannel : public MediaChannel {
 public:
  enum Error {
    ERROR_NONE = 0,                       // No error.
    ERROR_OTHER,                          // Other errors.
    ERROR_REC_DEVICE_OPEN_FAILED = 100,   // Could not open camera.
    ERROR_REC_DEVICE_NO_DEVICE,           // No camera.
    ERROR_REC_DEVICE_IN_USE,              // Device is in already use.
    ERROR_REC_DEVICE_REMOVED,             // Device is removed.
    ERROR_REC_SRTP_ERROR,                 // Generic sender SRTP failure.
    ERROR_REC_SRTP_AUTH_FAILED,           // Failed to authenticate packets.
    ERROR_REC_CPU_MAX_CANT_DOWNGRADE,     // Can't downgrade capture anymore.
    ERROR_PLAY_SRTP_ERROR = 200,          // Generic receiver SRTP failure.
    ERROR_PLAY_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_PLAY_SRTP_REPLAY,               // Packet replay detected.
  };

  VideoMediaChannel() {}
  explicit VideoMediaChannel(const MediaConfig& config)
      : MediaChannel(config) {}
  virtual ~VideoMediaChannel() {}

  virtual bool SetSendParameters(const VideoSendParameters& params) = 0;
  virtual bool SetRecvParameters(const VideoRecvParameters& params) = 0;
  virtual webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const = 0;
  virtual bool SetRtpSendParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters) = 0;
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
      bool enable,
      const VideoOptions* options,
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source) = 0;
  // Sets the sink object to be used for the specified stream.
  // If SSRC is 0, the renderer is used for the 'default' stream.
  virtual bool SetSink(uint32_t ssrc,
                       rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VideoMediaInfo* info) = 0;
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
  // For SCTP, this is really SID, not SSRC.
  uint32_t ssrc;
  // The type of message (binary, text, or control).
  DataMessageType type;
  // A per-stream value incremented per packet in the stream.
  int seq_num;
  // A per-stream value monotonically increasing with time.
  int timestamp;

  ReceiveDataParams() :
      ssrc(0),
      type(DMT_TEXT),
      seq_num(0),
      timestamp(0) {
  }
};

struct SendDataParams {
  // The in-packet stream indentifier.
  // For SCTP, this is really SID, not SSRC.
  uint32_t ssrc;
  // The type of message (binary, text, or control).
  DataMessageType type;

  // For SCTP, whether to send messages flagged as ordered or not.
  // If false, messages can be received out of order.
  bool ordered;
  // For SCTP, whether the messages are sent reliably or not.
  // If false, messages may be lost.
  bool reliable;
  // For SCTP, if reliable == false, provide partial reliability by
  // resending up to this many times.  Either count or millis
  // is supported, not both at the same time.
  int max_rtx_count;
  // For SCTP, if reliable == false, provide partial reliability by
  // resending for up to this many milliseconds.  Either count or millis
  // is supported, not both at the same time.
  int max_rtx_ms;

  SendDataParams() :
      ssrc(0),
      type(DMT_TEXT),
      // TODO(pthatcher): Make these true by default?
      ordered(false),
      reliable(false),
      max_rtx_count(0),
      max_rtx_ms(0) {
  }
};

enum SendDataResult { SDR_SUCCESS, SDR_ERROR, SDR_BLOCK };

struct DataSendParameters : RtpSendParameters<DataCodec> {
  std::string ToString() const {
    std::ostringstream ost;
    // Options and extensions aren't used.
    ost << "{";
    ost << "codecs: " << VectorToString(codecs) << ", ";
    ost << "max_bandwidth_bps: " << max_bandwidth_bps;
    ost << "}";
    return ost.str();
  }
};

struct DataRecvParameters : RtpParameters<DataCodec> {
};

class DataMediaChannel : public MediaChannel {
 public:
  enum Error {
    ERROR_NONE = 0,                       // No error.
    ERROR_OTHER,                          // Other errors.
    ERROR_SEND_SRTP_ERROR = 200,          // Generic SRTP failure.
    ERROR_SEND_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_RECV_SRTP_ERROR,                // Generic SRTP failure.
    ERROR_RECV_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_RECV_SRTP_REPLAY,               // Packet replay detected.
  };

  virtual ~DataMediaChannel() {}

  virtual bool SetSendParameters(const DataSendParameters& params) = 0;
  virtual bool SetRecvParameters(const DataRecvParameters& params) = 0;

  // TODO(pthatcher): Implement this.
  virtual bool GetStats(DataMediaInfo* info) { return true; }

  virtual bool SetSend(bool send) = 0;
  virtual bool SetReceive(bool receive) = 0;

  virtual void OnNetworkRouteChanged(const std::string& transport_name,
                                     const rtc::NetworkRoute& network_route) {}

  virtual bool SendData(
      const SendDataParams& params,
      const rtc::CopyOnWriteBuffer& payload,
      SendDataResult* result = NULL) = 0;
  // Signals when data is received (params, data, len)
  sigslot::signal3<const ReceiveDataParams&,
                   const char*,
                   size_t> SignalDataReceived;
  // Signal when the media channel is ready to send the stream. Arguments are:
  //     writable(bool)
  sigslot::signal1<bool> SignalReadyToSend;
  // Signal for notifying that the remote side has closed the DataChannel.
  sigslot::signal1<uint32_t> SignalStreamClosedRemotely;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_MEDIACHANNEL_H_
