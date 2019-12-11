/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtcstatscollector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/candidate.h"
#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "media/base/mediachannel.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/port.h"
#include "pc/peerconnection.h"
#include "pc/rtcstatstraversal.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {

std::string RTCCertificateIDFromFingerprint(const std::string& fingerprint) {
  return "RTCCertificate_" + fingerprint;
}

std::string RTCCodecStatsIDFromMidDirectionAndPayload(const std::string& mid,
                                                      bool inbound,
                                                      uint32_t payload_type) {
  char buf[1024];
  rtc::SimpleStringBuilder sb(buf);
  sb << "RTCCodec_" << mid << (inbound ? "_Inbound_" : "_Outbound_")
     << payload_type;
  return sb.str();
}

std::string RTCIceCandidatePairStatsIDFromConnectionInfo(
    const cricket::ConnectionInfo& info) {
  char buf[4096];
  rtc::SimpleStringBuilder sb(buf);
  sb << "RTCIceCandidatePair_" << info.local_candidate.id() << "_"
     << info.remote_candidate.id();
  return sb.str();
}

const char kSender[] = "sender";
const char kReceiver[] = "receiver";

std::string RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(
    const char* direction,
    int attachment_id) {
  char buf[1024];
  rtc::SimpleStringBuilder sb(buf);
  sb << "RTCMediaStreamTrack_" << direction << "_" << attachment_id;
  return sb.str();
}

std::string RTCTransportStatsIDFromTransportChannel(
    const std::string& transport_name, int channel_component) {
  char buf[1024];
  rtc::SimpleStringBuilder sb(buf);
  sb << "RTCTransport_" << transport_name << "_" << channel_component;
  return sb.str();
}

std::string RTCInboundRTPStreamStatsIDFromSSRC(bool audio, uint32_t ssrc) {
  char buf[1024];
  rtc::SimpleStringBuilder sb(buf);
  sb << "RTCInboundRTP" << (audio ? "Audio" : "Video") << "Stream_" << ssrc;
  return sb.str();
}

std::string RTCOutboundRTPStreamStatsIDFromSSRC(bool audio, uint32_t ssrc) {
  char buf[1024];
  rtc::SimpleStringBuilder sb(buf);
  sb << "RTCOutboundRTP" << (audio ? "Audio" : "Video") << "Stream_" << ssrc;
  return sb.str();
}

const char* CandidateTypeToRTCIceCandidateType(const std::string& type) {
  if (type == cricket::LOCAL_PORT_TYPE)
    return RTCIceCandidateType::kHost;
  if (type == cricket::STUN_PORT_TYPE)
    return RTCIceCandidateType::kSrflx;
  if (type == cricket::PRFLX_PORT_TYPE)
    return RTCIceCandidateType::kPrflx;
  if (type == cricket::RELAY_PORT_TYPE)
    return RTCIceCandidateType::kRelay;
  RTC_NOTREACHED();
  return nullptr;
}

const char* DataStateToRTCDataChannelState(
    DataChannelInterface::DataState state) {
  switch (state) {
    case DataChannelInterface::kConnecting:
      return RTCDataChannelState::kConnecting;
    case DataChannelInterface::kOpen:
      return RTCDataChannelState::kOpen;
    case DataChannelInterface::kClosing:
      return RTCDataChannelState::kClosing;
    case DataChannelInterface::kClosed:
      return RTCDataChannelState::kClosed;
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

const char* IceCandidatePairStateToRTCStatsIceCandidatePairState(
    cricket::IceCandidatePairState state) {
  switch (state) {
    case cricket::IceCandidatePairState::WAITING:
      return RTCStatsIceCandidatePairState::kWaiting;
    case cricket::IceCandidatePairState::IN_PROGRESS:
      return RTCStatsIceCandidatePairState::kInProgress;
    case cricket::IceCandidatePairState::SUCCEEDED:
      return RTCStatsIceCandidatePairState::kSucceeded;
    case cricket::IceCandidatePairState::FAILED:
      return RTCStatsIceCandidatePairState::kFailed;
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

const char* DtlsTransportStateToRTCDtlsTransportState(
    cricket::DtlsTransportState state) {
  switch (state) {
    case cricket::DTLS_TRANSPORT_NEW:
      return RTCDtlsTransportState::kNew;
    case cricket::DTLS_TRANSPORT_CONNECTING:
      return RTCDtlsTransportState::kConnecting;
    case cricket::DTLS_TRANSPORT_CONNECTED:
      return RTCDtlsTransportState::kConnected;
    case cricket::DTLS_TRANSPORT_CLOSED:
      return RTCDtlsTransportState::kClosed;
    case cricket::DTLS_TRANSPORT_FAILED:
      return RTCDtlsTransportState::kFailed;
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

const char* NetworkAdapterTypeToStatsType(rtc::AdapterType type) {
  switch (type) {
    case rtc::ADAPTER_TYPE_CELLULAR:
      return RTCNetworkType::kCellular;
    case rtc::ADAPTER_TYPE_ETHERNET:
      return RTCNetworkType::kEthernet;
    case rtc::ADAPTER_TYPE_WIFI:
      return RTCNetworkType::kWifi;
    case rtc::ADAPTER_TYPE_VPN:
      return RTCNetworkType::kVpn;
    case rtc::ADAPTER_TYPE_UNKNOWN:
    case rtc::ADAPTER_TYPE_LOOPBACK:
    case rtc::ADAPTER_TYPE_ANY:
      return RTCNetworkType::kUnknown;
  }
  RTC_NOTREACHED();
  return nullptr;
}

double DoubleAudioLevelFromIntAudioLevel(int audio_level) {
  RTC_DCHECK_GE(audio_level, 0);
  RTC_DCHECK_LE(audio_level, 32767);
  return audio_level / 32767.0;
}

std::unique_ptr<RTCCodecStats> CodecStatsFromRtpCodecParameters(
    uint64_t timestamp_us,
    const std::string& mid,
    bool inbound,
    const RtpCodecParameters& codec_params) {
  RTC_DCHECK_GE(codec_params.payload_type, 0);
  RTC_DCHECK_LE(codec_params.payload_type, 127);
  RTC_DCHECK(codec_params.clock_rate);
  uint32_t payload_type = static_cast<uint32_t>(codec_params.payload_type);
  std::unique_ptr<RTCCodecStats> codec_stats(new RTCCodecStats(
      RTCCodecStatsIDFromMidDirectionAndPayload(mid, inbound, payload_type),
      timestamp_us));
  codec_stats->payload_type = payload_type;
  codec_stats->mime_type = codec_params.mime_type();
  if (codec_params.clock_rate) {
    codec_stats->clock_rate = static_cast<uint32_t>(*codec_params.clock_rate);
  }
  return codec_stats;
}

void SetMediaStreamTrackStatsFromMediaStreamTrackInterface(
    const MediaStreamTrackInterface& track,
    RTCMediaStreamTrackStats* track_stats) {
  track_stats->track_identifier = track.id();
  track_stats->ended = (track.state() == MediaStreamTrackInterface::kEnded);
}

// Provides the media independent counters (both audio and video).
void SetInboundRTPStreamStatsFromMediaReceiverInfo(
    const cricket::MediaReceiverInfo& media_receiver_info,
    RTCInboundRTPStreamStats* inbound_stats) {
  RTC_DCHECK(inbound_stats);
  inbound_stats->ssrc = media_receiver_info.ssrc();
  // TODO(hbos): Support the remote case. https://crbug.com/657855
  inbound_stats->is_remote = false;
  inbound_stats->packets_received =
      static_cast<uint32_t>(media_receiver_info.packets_rcvd);
  inbound_stats->bytes_received =
      static_cast<uint64_t>(media_receiver_info.bytes_rcvd);
  inbound_stats->packets_lost =
      static_cast<int32_t>(media_receiver_info.packets_lost);
  inbound_stats->fraction_lost =
      static_cast<double>(media_receiver_info.fraction_lost);
}

void SetInboundRTPStreamStatsFromVoiceReceiverInfo(
    const std::string& mid,
    const cricket::VoiceReceiverInfo& voice_receiver_info,
    RTCInboundRTPStreamStats* inbound_audio) {
  SetInboundRTPStreamStatsFromMediaReceiverInfo(
      voice_receiver_info, inbound_audio);
  inbound_audio->media_type = "audio";
  inbound_audio->kind = "audio";
  if (voice_receiver_info.codec_payload_type) {
    inbound_audio->codec_id = RTCCodecStatsIDFromMidDirectionAndPayload(
        mid, true, *voice_receiver_info.codec_payload_type);
  }
  inbound_audio->jitter =
      static_cast<double>(voice_receiver_info.jitter_ms) /
          rtc::kNumMillisecsPerSec;
  // |fir_count|, |pli_count| and |sli_count| are only valid for video and are
  // purposefully left undefined for audio.
}

void SetInboundRTPStreamStatsFromVideoReceiverInfo(
    const std::string& mid,
    const cricket::VideoReceiverInfo& video_receiver_info,
    RTCInboundRTPStreamStats* inbound_video) {
  SetInboundRTPStreamStatsFromMediaReceiverInfo(
      video_receiver_info, inbound_video);
  inbound_video->media_type = "video";
  inbound_video->kind = "video";
  if (video_receiver_info.codec_payload_type) {
    inbound_video->codec_id = RTCCodecStatsIDFromMidDirectionAndPayload(
        mid, true, *video_receiver_info.codec_payload_type);
  }
  inbound_video->fir_count =
      static_cast<uint32_t>(video_receiver_info.firs_sent);
  inbound_video->pli_count =
      static_cast<uint32_t>(video_receiver_info.plis_sent);
  inbound_video->nack_count =
      static_cast<uint32_t>(video_receiver_info.nacks_sent);
  inbound_video->frames_decoded = video_receiver_info.frames_decoded;
  if (video_receiver_info.qp_sum)
    inbound_video->qp_sum = *video_receiver_info.qp_sum;
}

// Provides the media independent counters (both audio and video).
void SetOutboundRTPStreamStatsFromMediaSenderInfo(
    const cricket::MediaSenderInfo& media_sender_info,
    RTCOutboundRTPStreamStats* outbound_stats) {
  RTC_DCHECK(outbound_stats);
  outbound_stats->ssrc = media_sender_info.ssrc();
  // TODO(hbos): Support the remote case. https://crbug.com/657856
  outbound_stats->is_remote = false;
  outbound_stats->packets_sent =
      static_cast<uint32_t>(media_sender_info.packets_sent);
  outbound_stats->bytes_sent =
      static_cast<uint64_t>(media_sender_info.bytes_sent);
}

void SetOutboundRTPStreamStatsFromVoiceSenderInfo(
    const std::string& mid,
    const cricket::VoiceSenderInfo& voice_sender_info,
    RTCOutboundRTPStreamStats* outbound_audio) {
  SetOutboundRTPStreamStatsFromMediaSenderInfo(
      voice_sender_info, outbound_audio);
  outbound_audio->media_type = "audio";
  outbound_audio->kind = "audio";
  if (voice_sender_info.codec_payload_type) {
    outbound_audio->codec_id = RTCCodecStatsIDFromMidDirectionAndPayload(
        mid, false, *voice_sender_info.codec_payload_type);
  }
  // |fir_count|, |pli_count| and |sli_count| are only valid for video and are
  // purposefully left undefined for audio.
}

void SetOutboundRTPStreamStatsFromVideoSenderInfo(
    const std::string& mid,
    const cricket::VideoSenderInfo& video_sender_info,
    RTCOutboundRTPStreamStats* outbound_video) {
  SetOutboundRTPStreamStatsFromMediaSenderInfo(
      video_sender_info, outbound_video);
  outbound_video->media_type = "video";
  outbound_video->kind = "video";
  if (video_sender_info.codec_payload_type) {
    outbound_video->codec_id = RTCCodecStatsIDFromMidDirectionAndPayload(
        mid, false, *video_sender_info.codec_payload_type);
  }
  outbound_video->fir_count =
      static_cast<uint32_t>(video_sender_info.firs_rcvd);
  outbound_video->pli_count =
      static_cast<uint32_t>(video_sender_info.plis_rcvd);
  outbound_video->nack_count =
      static_cast<uint32_t>(video_sender_info.nacks_rcvd);
  if (video_sender_info.qp_sum)
    outbound_video->qp_sum = *video_sender_info.qp_sum;
  outbound_video->frames_encoded = video_sender_info.frames_encoded;
}

void ProduceCertificateStatsFromSSLCertificateStats(
    int64_t timestamp_us, const rtc::SSLCertificateStats& certificate_stats,
    RTCStatsReport* report) {
  RTCCertificateStats* prev_certificate_stats = nullptr;
  for (const rtc::SSLCertificateStats* s = &certificate_stats; s;
       s = s->issuer.get()) {
    std::string certificate_stats_id =
        RTCCertificateIDFromFingerprint(s->fingerprint);
    // It is possible for the same certificate to show up multiple times, e.g.
    // if local and remote side use the same certificate in a loopback call.
    // If the report already contains stats for this certificate, skip it.
    if (report->Get(certificate_stats_id)) {
      RTC_DCHECK_EQ(s, &certificate_stats);
      break;
    }
    RTCCertificateStats* certificate_stats = new RTCCertificateStats(
        certificate_stats_id, timestamp_us);
    certificate_stats->fingerprint = s->fingerprint;
    certificate_stats->fingerprint_algorithm = s->fingerprint_algorithm;
    certificate_stats->base64_certificate = s->base64_certificate;
    if (prev_certificate_stats)
      prev_certificate_stats->issuer_certificate_id = certificate_stats->id();
    report->AddStats(std::unique_ptr<RTCCertificateStats>(certificate_stats));
    prev_certificate_stats = certificate_stats;
  }
}

const std::string& ProduceIceCandidateStats(
    int64_t timestamp_us, const cricket::Candidate& candidate, bool is_local,
    const std::string& transport_id, RTCStatsReport* report) {
  const std::string& id = "RTCIceCandidate_" + candidate.id();
  const RTCStats* stats = report->Get(id);
  if (!stats) {
    std::unique_ptr<RTCIceCandidateStats> candidate_stats;
    if (is_local)
      candidate_stats.reset(new RTCLocalIceCandidateStats(id, timestamp_us));
    else
      candidate_stats.reset(new RTCRemoteIceCandidateStats(id, timestamp_us));
    candidate_stats->transport_id = transport_id;
    if (is_local) {
      candidate_stats->network_type =
          NetworkAdapterTypeToStatsType(candidate.network_type());
      if (candidate.type() == cricket::RELAY_PORT_TYPE) {
        std::string relay_protocol = candidate.relay_protocol();
        RTC_DCHECK(relay_protocol.compare("udp") == 0 ||
                   relay_protocol.compare("tcp") == 0 ||
                   relay_protocol.compare("tls") == 0);
        candidate_stats->relay_protocol = relay_protocol;
      }
    } else {
      // We don't expect to know the adapter type of remote candidates.
      RTC_DCHECK_EQ(rtc::ADAPTER_TYPE_UNKNOWN, candidate.network_type());
    }
    candidate_stats->ip = candidate.address().ipaddr().ToString();
    candidate_stats->port = static_cast<int32_t>(candidate.address().port());
    candidate_stats->protocol = candidate.protocol();
    candidate_stats->candidate_type = CandidateTypeToRTCIceCandidateType(
        candidate.type());
    candidate_stats->priority = static_cast<int32_t>(candidate.priority());

    stats = candidate_stats.get();
    report->AddStats(std::move(candidate_stats));
  }
  RTC_DCHECK_EQ(stats->type(), is_local ? RTCLocalIceCandidateStats::kType
                                        : RTCRemoteIceCandidateStats::kType);
  return stats->id();
}

std::unique_ptr<RTCMediaStreamTrackStats>
ProduceMediaStreamTrackStatsFromVoiceSenderInfo(
    int64_t timestamp_us,
    const AudioTrackInterface& audio_track,
    const cricket::VoiceSenderInfo& voice_sender_info,
    int attachment_id) {
  std::unique_ptr<RTCMediaStreamTrackStats> audio_track_stats(
      new RTCMediaStreamTrackStats(
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(kSender,
                                                               attachment_id),
          timestamp_us, RTCMediaStreamTrackKind::kAudio));
  SetMediaStreamTrackStatsFromMediaStreamTrackInterface(
      audio_track, audio_track_stats.get());
  audio_track_stats->remote_source = false;
  audio_track_stats->detached = false;
  if (voice_sender_info.audio_level >= 0) {
    audio_track_stats->audio_level = DoubleAudioLevelFromIntAudioLevel(
        voice_sender_info.audio_level);
  }
  audio_track_stats->total_audio_energy = voice_sender_info.total_input_energy;
  audio_track_stats->total_samples_duration =
      voice_sender_info.total_input_duration;
  if (voice_sender_info.apm_statistics.echo_return_loss) {
    audio_track_stats->echo_return_loss =
        *voice_sender_info.apm_statistics.echo_return_loss;
  }
  if (voice_sender_info.apm_statistics.echo_return_loss_enhancement) {
    audio_track_stats->echo_return_loss_enhancement =
        *voice_sender_info.apm_statistics.echo_return_loss_enhancement;
  }
  return audio_track_stats;
}

std::unique_ptr<RTCMediaStreamTrackStats>
ProduceMediaStreamTrackStatsFromVoiceReceiverInfo(
    int64_t timestamp_us,
    const AudioTrackInterface& audio_track,
    const cricket::VoiceReceiverInfo& voice_receiver_info,
    int attachment_id) {
  // Since receiver tracks can't be reattached, we use the SSRC as
  // an attachment identifier.
  std::unique_ptr<RTCMediaStreamTrackStats> audio_track_stats(
      new RTCMediaStreamTrackStats(
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(kReceiver,
                                                               attachment_id),
          timestamp_us, RTCMediaStreamTrackKind::kAudio));
  SetMediaStreamTrackStatsFromMediaStreamTrackInterface(
      audio_track, audio_track_stats.get());
  audio_track_stats->remote_source = true;
  audio_track_stats->detached = false;
  if (voice_receiver_info.audio_level >= 0) {
    audio_track_stats->audio_level = DoubleAudioLevelFromIntAudioLevel(
        voice_receiver_info.audio_level);
  }
  audio_track_stats->jitter_buffer_delay =
      voice_receiver_info.jitter_buffer_delay_seconds;
  audio_track_stats->total_audio_energy =
      voice_receiver_info.total_output_energy;
  audio_track_stats->total_samples_received =
      voice_receiver_info.total_samples_received;
  audio_track_stats->total_samples_duration =
      voice_receiver_info.total_output_duration;
  audio_track_stats->concealed_samples = voice_receiver_info.concealed_samples;
  audio_track_stats->concealment_events =
      voice_receiver_info.concealment_events;
  audio_track_stats->jitter_buffer_flushes =
      voice_receiver_info.jitter_buffer_flushes;
  audio_track_stats->delayed_packet_outage_samples =
      voice_receiver_info.delayed_packet_outage_samples;
  return audio_track_stats;
}

std::unique_ptr<RTCMediaStreamTrackStats>
ProduceMediaStreamTrackStatsFromVideoSenderInfo(
    int64_t timestamp_us,
    const VideoTrackInterface& video_track,
    const cricket::VideoSenderInfo& video_sender_info,
    int attachment_id) {
  std::unique_ptr<RTCMediaStreamTrackStats> video_track_stats(
      new RTCMediaStreamTrackStats(
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(kSender,

                                                               attachment_id),
          timestamp_us, RTCMediaStreamTrackKind::kVideo));
  SetMediaStreamTrackStatsFromMediaStreamTrackInterface(
      video_track, video_track_stats.get());
  video_track_stats->remote_source = false;
  video_track_stats->detached = false;
  video_track_stats->frame_width = static_cast<uint32_t>(
      video_sender_info.send_frame_width);
  video_track_stats->frame_height = static_cast<uint32_t>(
      video_sender_info.send_frame_height);
  // TODO(hbos): Will reduce this by frames dropped due to congestion control
  // when available. https://crbug.com/659137
  video_track_stats->frames_sent = video_sender_info.frames_encoded;
  video_track_stats->huge_frames_sent = video_sender_info.huge_frames_sent;
  return video_track_stats;
}

std::unique_ptr<RTCMediaStreamTrackStats>
ProduceMediaStreamTrackStatsFromVideoReceiverInfo(
    int64_t timestamp_us,
    const VideoTrackInterface& video_track,
    const cricket::VideoReceiverInfo& video_receiver_info,
    int attachment_id) {
  std::unique_ptr<RTCMediaStreamTrackStats> video_track_stats(
      new RTCMediaStreamTrackStats(
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(kReceiver,

                                                               attachment_id),
          timestamp_us, RTCMediaStreamTrackKind::kVideo));
  SetMediaStreamTrackStatsFromMediaStreamTrackInterface(
      video_track, video_track_stats.get());
  video_track_stats->remote_source = true;
  video_track_stats->detached = false;
  if (video_receiver_info.frame_width > 0 &&
      video_receiver_info.frame_height > 0) {
    video_track_stats->frame_width = static_cast<uint32_t>(
        video_receiver_info.frame_width);
    video_track_stats->frame_height = static_cast<uint32_t>(
        video_receiver_info.frame_height);
  }
  video_track_stats->frames_received = video_receiver_info.frames_received;
  // TODO(hbos): When we support receiving simulcast, this should be the total
  // number of frames correctly decoded, independent of which SSRC it was
  // received from. Since we don't support that, this is correct and is the same
  // value as "RTCInboundRTPStreamStats.framesDecoded". https://crbug.com/659137
  video_track_stats->frames_decoded = video_receiver_info.frames_decoded;
  RTC_DCHECK_GE(video_receiver_info.frames_received,
                video_receiver_info.frames_rendered);
  video_track_stats->frames_dropped = video_receiver_info.frames_received -
                                      video_receiver_info.frames_rendered;
  return video_track_stats;
}

void ProduceSenderMediaTrackStats(
    int64_t timestamp_us,
    const TrackMediaInfoMap& track_media_info_map,
    std::vector<rtc::scoped_refptr<RtpSenderInternal>> senders,
    RTCStatsReport* report) {
  // This function iterates over the senders to generate outgoing track stats.

  // TODO(hbos): Return stats of detached tracks. We have to perform stats
  // gathering at the time of detachment to get accurate stats and timestamps.
  // https://crbug.com/659137
  for (const auto& sender : senders) {
    if (sender->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      AudioTrackInterface* track =
          static_cast<AudioTrackInterface*>(sender->track().get());
      if (!track)
        continue;
      cricket::VoiceSenderInfo null_sender_info;
      const cricket::VoiceSenderInfo* voice_sender_info = &null_sender_info;
      // TODO(hta): Checking on ssrc is not proper. There should be a way
      // to see from a sender whether it's connected or not.
      // Related to https://crbug.com/8694 (using ssrc 0 to indicate "none")
      if (sender->ssrc() != 0) {
        // When pc.close is called, sender info is discarded, so
        // we generate zeroes instead. Bug: It should be retained.
        // https://crbug.com/807174
        const cricket::VoiceSenderInfo* sender_info =
            track_media_info_map.GetVoiceSenderInfoBySsrc(sender->ssrc());
        if (sender_info) {
          voice_sender_info = sender_info;
        } else {
          RTC_LOG(LS_INFO)
              << "RTCStatsCollector: No voice sender info for sender with ssrc "
              << sender->ssrc();
        }
      }
      std::unique_ptr<RTCMediaStreamTrackStats> audio_track_stats =
          ProduceMediaStreamTrackStatsFromVoiceSenderInfo(
              timestamp_us, *track, *voice_sender_info, sender->AttachmentId());
      report->AddStats(std::move(audio_track_stats));
    } else if (sender->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      VideoTrackInterface* track =
          static_cast<VideoTrackInterface*>(sender->track().get());
      if (!track)
        continue;
      cricket::VideoSenderInfo null_sender_info;
      const cricket::VideoSenderInfo* video_sender_info = &null_sender_info;
      // TODO(hta): Check on state not ssrc when state is available
      // Related to https://bugs.webrtc.org/8694 (using ssrc 0 to indicate
      // "none")
      if (sender->ssrc() != 0) {
        // When pc.close is called, sender info is discarded, so
        // we generate zeroes instead. Bug: It should be retained.
        // https://crbug.com/807174
        const cricket::VideoSenderInfo* sender_info =
            track_media_info_map.GetVideoSenderInfoBySsrc(sender->ssrc());
        if (sender_info) {
          video_sender_info = sender_info;
        } else {
          RTC_LOG(LS_INFO) << "No video sender info for sender with ssrc "
                           << sender->ssrc();
        }
      }
      std::unique_ptr<RTCMediaStreamTrackStats> video_track_stats =
          ProduceMediaStreamTrackStatsFromVideoSenderInfo(
              timestamp_us, *track, *video_sender_info, sender->AttachmentId());
      report->AddStats(std::move(video_track_stats));
    } else {
      RTC_NOTREACHED();
    }
  }
}

void ProduceReceiverMediaTrackStats(
    int64_t timestamp_us,
    const TrackMediaInfoMap& track_media_info_map,
    std::vector<rtc::scoped_refptr<RtpReceiverInternal>> receivers,
    RTCStatsReport* report) {
  // This function iterates over the receivers to find the remote tracks.
  for (const auto& receiver : receivers) {
    if (receiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      AudioTrackInterface* track =
          static_cast<AudioTrackInterface*>(receiver->track().get());
      const cricket::VoiceReceiverInfo* voice_receiver_info =
          track_media_info_map.GetVoiceReceiverInfo(*track);
      if (!voice_receiver_info) {
        continue;
      }
      std::unique_ptr<RTCMediaStreamTrackStats> audio_track_stats =
          ProduceMediaStreamTrackStatsFromVoiceReceiverInfo(
              timestamp_us, *track, *voice_receiver_info,
              receiver->AttachmentId());
      report->AddStats(std::move(audio_track_stats));
    } else if (receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      VideoTrackInterface* track =
          static_cast<VideoTrackInterface*>(receiver->track().get());
      const cricket::VideoReceiverInfo* video_receiver_info =
          track_media_info_map.GetVideoReceiverInfo(*track);
      if (!video_receiver_info) {
        continue;
      }
      std::unique_ptr<RTCMediaStreamTrackStats> video_track_stats =
          ProduceMediaStreamTrackStatsFromVideoReceiverInfo(
              timestamp_us, *track, *video_receiver_info,
              receiver->AttachmentId());
      report->AddStats(std::move(video_track_stats));
    } else {
      RTC_NOTREACHED();
    }
  }
}

rtc::scoped_refptr<RTCStatsReport> CreateReportFilteredBySelector(
    bool filter_by_sender_selector,
    rtc::scoped_refptr<const RTCStatsReport> report,
    rtc::scoped_refptr<RtpSenderInternal> sender_selector,
    rtc::scoped_refptr<RtpReceiverInternal> receiver_selector) {
  std::vector<std::string> rtpstream_ids;
  if (filter_by_sender_selector) {
    // Filter mode: RTCStatsCollector::RequestInfo::kSenderSelector
    if (sender_selector) {
      // Find outbound-rtp(s) of the sender, i.e. the outbound-rtp(s) that
      // reference the sender stats.
      // Because we do not implement sender stats, we look at outbound-rtp(s)
      // that reference the track attachment stats for the sender instead.
      std::string track_id =
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(
              kSender, sender_selector->AttachmentId());
      for (const auto& stats : *report) {
        if (stats.type() != RTCOutboundRTPStreamStats::kType)
          continue;
        const auto& outbound_rtp = stats.cast_to<RTCOutboundRTPStreamStats>();
        if (outbound_rtp.track_id.is_defined() &&
            *outbound_rtp.track_id == track_id) {
          rtpstream_ids.push_back(outbound_rtp.id());
        }
      }
    }
  } else {
    // Filter mode: RTCStatsCollector::RequestInfo::kReceiverSelector
    if (receiver_selector) {
      // Find inbound-rtp(s) of the receiver, i.e. the inbound-rtp(s) that
      // reference the receiver stats.
      // Because we do not implement receiver stats, we look at inbound-rtp(s)
      // that reference the track attachment stats for the receiver instead.
      std::string track_id =
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(
              kReceiver, receiver_selector->AttachmentId());
      for (const auto& stats : *report) {
        if (stats.type() != RTCInboundRTPStreamStats::kType)
          continue;
        const auto& inbound_rtp = stats.cast_to<RTCInboundRTPStreamStats>();
        if (inbound_rtp.track_id.is_defined() &&
            *inbound_rtp.track_id == track_id) {
          rtpstream_ids.push_back(inbound_rtp.id());
        }
      }
    }
  }
  if (rtpstream_ids.empty())
    return RTCStatsReport::Create(report->timestamp_us());
  return TakeReferencedStats(report->Copy(), rtpstream_ids);
}

}  // namespace

RTCStatsCollector::RequestInfo::RequestInfo(
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback)
    : RequestInfo(FilterMode::kAll, std::move(callback), nullptr, nullptr) {}

RTCStatsCollector::RequestInfo::RequestInfo(
    rtc::scoped_refptr<RtpSenderInternal> selector,
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback)
    : RequestInfo(FilterMode::kSenderSelector,
                  std::move(callback),
                  std::move(selector),
                  nullptr) {}

RTCStatsCollector::RequestInfo::RequestInfo(
    rtc::scoped_refptr<RtpReceiverInternal> selector,
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback)
    : RequestInfo(FilterMode::kReceiverSelector,
                  std::move(callback),
                  nullptr,
                  std::move(selector)) {}

RTCStatsCollector::RequestInfo::RequestInfo(
    RTCStatsCollector::RequestInfo::FilterMode filter_mode,
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback,
    rtc::scoped_refptr<RtpSenderInternal> sender_selector,
    rtc::scoped_refptr<RtpReceiverInternal> receiver_selector)
    : filter_mode_(filter_mode),
      callback_(std::move(callback)),
      sender_selector_(std::move(sender_selector)),
      receiver_selector_(std::move(receiver_selector)) {
  RTC_DCHECK(callback_);
  RTC_DCHECK(!sender_selector_ || !receiver_selector_);
}

rtc::scoped_refptr<RTCStatsCollector> RTCStatsCollector::Create(
    PeerConnectionInternal* pc,
    int64_t cache_lifetime_us) {
  return rtc::scoped_refptr<RTCStatsCollector>(
      new rtc::RefCountedObject<RTCStatsCollector>(pc, cache_lifetime_us));
}

RTCStatsCollector::RTCStatsCollector(PeerConnectionInternal* pc,
                                     int64_t cache_lifetime_us)
    : pc_(pc),
      signaling_thread_(pc->signaling_thread()),
      worker_thread_(pc->worker_thread()),
      network_thread_(pc->network_thread()),
      num_pending_partial_reports_(0),
      partial_report_timestamp_us_(0),
      cache_timestamp_us_(0),
      cache_lifetime_us_(cache_lifetime_us) {
  RTC_DCHECK(pc_);
  RTC_DCHECK(signaling_thread_);
  RTC_DCHECK(worker_thread_);
  RTC_DCHECK(network_thread_);
  RTC_DCHECK_GE(cache_lifetime_us_, 0);
  pc_->SignalDataChannelCreated().connect(
      this, &RTCStatsCollector::OnDataChannelCreated);
}

RTCStatsCollector::~RTCStatsCollector() {
  RTC_DCHECK_EQ(num_pending_partial_reports_, 0);
}

void RTCStatsCollector::GetStatsReport(
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback) {
  GetStatsReportInternal(RequestInfo(std::move(callback)));
}

void RTCStatsCollector::GetStatsReport(
    rtc::scoped_refptr<RtpSenderInternal> selector,
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback) {
  GetStatsReportInternal(RequestInfo(std::move(selector), std::move(callback)));
}

void RTCStatsCollector::GetStatsReport(
    rtc::scoped_refptr<RtpReceiverInternal> selector,
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback) {
  GetStatsReportInternal(RequestInfo(std::move(selector), std::move(callback)));
}

void RTCStatsCollector::GetStatsReportInternal(
    RTCStatsCollector::RequestInfo request) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  requests_.push_back(std::move(request));

  // "Now" using a monotonically increasing timer.
  int64_t cache_now_us = rtc::TimeMicros();
  if (cached_report_ &&
      cache_now_us - cache_timestamp_us_ <= cache_lifetime_us_) {
    // We have a fresh cached report to deliver. Deliver asynchronously, since
    // the caller may not be expecting a synchronous callback, and it avoids
    // reentrancy problems.
    std::vector<RequestInfo> requests;
    requests.swap(requests_);
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&RTCStatsCollector::DeliverCachedReport, this, cached_report_,
                  std::move(requests)));
  } else if (!num_pending_partial_reports_) {
    // Only start gathering stats if we're not already gathering stats. In the
    // case of already gathering stats, |callback_| will be invoked when there
    // are no more pending partial reports.

    // "Now" using a system clock, relative to the UNIX epoch (Jan 1, 1970,
    // UTC), in microseconds. The system clock could be modified and is not
    // necessarily monotonically increasing.
    int64_t timestamp_us = rtc::TimeUTCMicros();

    num_pending_partial_reports_ = 2;
    partial_report_timestamp_us_ = cache_now_us;

    // Prepare |transceiver_stats_infos_| for use in
    // |ProducePartialResultsOnNetworkThread| and
    // |ProducePartialResultsOnSignalingThread|.
    transceiver_stats_infos_ = PrepareTransceiverStatsInfos_s();
    // Prepare |transport_names_| for use in
    // |ProducePartialResultsOnNetworkThread|.
    transport_names_ = PrepareTransportNames_s();

    // Prepare |call_stats_| here since GetCallStats() will hop to the worker
    // thread.
    // TODO(holmer): To avoid the hop we could move BWE and BWE stats to the
    // network thread, where it more naturally belongs.
    call_stats_ = pc_->GetCallStats();

    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, network_thread_,
        rtc::Bind(&RTCStatsCollector::ProducePartialResultsOnNetworkThread,
                  rtc::scoped_refptr<RTCStatsCollector>(this), timestamp_us));
    ProducePartialResultsOnSignalingThread(timestamp_us);
  }
}

void RTCStatsCollector::ClearCachedStatsReport() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  cached_report_ = nullptr;
}

void RTCStatsCollector::WaitForPendingRequest() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  if (num_pending_partial_reports_) {
    rtc::Thread::Current()->ProcessMessages(0);
    while (num_pending_partial_reports_) {
      rtc::Thread::Current()->SleepMs(1);
      rtc::Thread::Current()->ProcessMessages(0);
    }
  }
}

void RTCStatsCollector::ProducePartialResultsOnSignalingThread(
    int64_t timestamp_us) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create(
      timestamp_us);

  ProduceDataChannelStats_s(timestamp_us, report.get());
  ProduceMediaStreamStats_s(timestamp_us, report.get());
  ProduceMediaStreamTrackStats_s(timestamp_us, report.get());
  ProducePeerConnectionStats_s(timestamp_us, report.get());

  AddPartialResults(report);
}

void RTCStatsCollector::ProducePartialResultsOnNetworkThread(
    int64_t timestamp_us) {
  RTC_DCHECK(network_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create(
      timestamp_us);

  std::map<std::string, cricket::TransportStats> transport_stats_by_name =
      pc_->GetTransportStatsByNames(transport_names_);

  std::map<std::string, CertificateStatsPair> transport_cert_stats =
      PrepareTransportCertificateStats_n(transport_stats_by_name);

  ProduceCertificateStats_n(timestamp_us, transport_cert_stats, report.get());
  ProduceCodecStats_n(timestamp_us, transceiver_stats_infos_, report.get());
  ProduceIceCandidateAndPairStats_n(timestamp_us, transport_stats_by_name,
                                    call_stats_, report.get());
  ProduceRTPStreamStats_n(timestamp_us, transceiver_stats_infos_, report.get());
  ProduceTransportStats_n(timestamp_us, transport_stats_by_name,
                          transport_cert_stats, report.get());

  AddPartialResults(report);
}

void RTCStatsCollector::AddPartialResults(
    const rtc::scoped_refptr<RTCStatsReport>& partial_report) {
  if (!signaling_thread_->IsCurrent()) {
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&RTCStatsCollector::AddPartialResults_s,
                  rtc::scoped_refptr<RTCStatsCollector>(this),
                  partial_report));
    return;
  }
  AddPartialResults_s(partial_report);
}

void RTCStatsCollector::AddPartialResults_s(
    rtc::scoped_refptr<RTCStatsReport> partial_report) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK_GT(num_pending_partial_reports_, 0);
  if (!partial_report_)
    partial_report_ = partial_report;
  else
    partial_report_->TakeMembersFrom(partial_report);
  --num_pending_partial_reports_;
  if (!num_pending_partial_reports_) {
    cache_timestamp_us_ = partial_report_timestamp_us_;
    cached_report_ = partial_report_;
    partial_report_ = nullptr;
    transceiver_stats_infos_.clear();
    // Trace WebRTC Stats when getStats is called on Javascript.
    // This allows access to WebRTC stats from trace logs. To enable them,
    // select the "webrtc_stats" category when recording traces.
    TRACE_EVENT_INSTANT1("webrtc_stats", "webrtc_stats", "report",
                         cached_report_->ToJson());

    // Deliver report and clear |requests_|.
    std::vector<RequestInfo> requests;
    requests.swap(requests_);
    DeliverCachedReport(cached_report_, std::move(requests));
  }
}

void RTCStatsCollector::DeliverCachedReport(
    rtc::scoped_refptr<const RTCStatsReport> cached_report,
    std::vector<RTCStatsCollector::RequestInfo> requests) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK(!requests.empty());
  RTC_DCHECK(cached_report);

  for (const RequestInfo& request : requests) {
    if (request.filter_mode() == RequestInfo::FilterMode::kAll) {
      request.callback()->OnStatsDelivered(cached_report);
    } else {
      bool filter_by_sender_selector;
      rtc::scoped_refptr<RtpSenderInternal> sender_selector;
      rtc::scoped_refptr<RtpReceiverInternal> receiver_selector;
      if (request.filter_mode() == RequestInfo::FilterMode::kSenderSelector) {
        filter_by_sender_selector = true;
        sender_selector = request.sender_selector();
      } else {
        RTC_DCHECK(request.filter_mode() ==
                   RequestInfo::FilterMode::kReceiverSelector);
        filter_by_sender_selector = false;
        receiver_selector = request.receiver_selector();
      }
      request.callback()->OnStatsDelivered(CreateReportFilteredBySelector(
          filter_by_sender_selector, cached_report, sender_selector,
          receiver_selector));
    }
  }
}

void RTCStatsCollector::ProduceCertificateStats_n(
    int64_t timestamp_us,
    const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
    RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  for (const auto& transport_cert_stats_pair : transport_cert_stats) {
    if (transport_cert_stats_pair.second.local) {
      ProduceCertificateStatsFromSSLCertificateStats(
          timestamp_us, *transport_cert_stats_pair.second.local.get(), report);
    }
    if (transport_cert_stats_pair.second.remote) {
      ProduceCertificateStatsFromSSLCertificateStats(
          timestamp_us, *transport_cert_stats_pair.second.remote.get(), report);
    }
  }
}

void RTCStatsCollector::ProduceCodecStats_n(
    int64_t timestamp_us,
    const std::vector<RtpTransceiverStatsInfo>& transceiver_stats_infos,
    RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  for (const auto& stats : transceiver_stats_infos) {
    if (!stats.mid) {
      continue;
    }
    const cricket::VoiceMediaInfo* voice_media_info =
        stats.track_media_info_map->voice_media_info();
    const cricket::VideoMediaInfo* video_media_info =
        stats.track_media_info_map->video_media_info();
    // Audio
    if (voice_media_info) {
      // Inbound
      for (const auto& pair : voice_media_info->receive_codecs) {
        report->AddStats(CodecStatsFromRtpCodecParameters(
            timestamp_us, *stats.mid, true, pair.second));
      }
      // Outbound
      for (const auto& pair : voice_media_info->send_codecs) {
        report->AddStats(CodecStatsFromRtpCodecParameters(
            timestamp_us, *stats.mid, false, pair.second));
      }
    }
    // Video
    if (video_media_info) {
      // Inbound
      for (const auto& pair : video_media_info->receive_codecs) {
        report->AddStats(CodecStatsFromRtpCodecParameters(
            timestamp_us, *stats.mid, true, pair.second));
      }
      // Outbound
      for (const auto& pair : video_media_info->send_codecs) {
        report->AddStats(CodecStatsFromRtpCodecParameters(
            timestamp_us, *stats.mid, false, pair.second));
      }
    }
  }
}

void RTCStatsCollector::ProduceDataChannelStats_s(
    int64_t timestamp_us, RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  for (const rtc::scoped_refptr<DataChannel>& data_channel :
       pc_->sctp_data_channels()) {
    std::unique_ptr<RTCDataChannelStats> data_channel_stats(
        new RTCDataChannelStats(
            "RTCDataChannel_" + rtc::ToString(data_channel->id()),
            timestamp_us));
    data_channel_stats->label = data_channel->label();
    data_channel_stats->protocol = data_channel->protocol();
    data_channel_stats->datachannelid = data_channel->id();
    data_channel_stats->state =
        DataStateToRTCDataChannelState(data_channel->state());
    data_channel_stats->messages_sent = data_channel->messages_sent();
    data_channel_stats->bytes_sent = data_channel->bytes_sent();
    data_channel_stats->messages_received = data_channel->messages_received();
    data_channel_stats->bytes_received = data_channel->bytes_received();
    report->AddStats(std::move(data_channel_stats));
  }
}

void RTCStatsCollector::ProduceIceCandidateAndPairStats_n(
    int64_t timestamp_us,
    const std::map<std::string, cricket::TransportStats>&
        transport_stats_by_name,
    const Call::Stats& call_stats,
    RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  for (const auto& entry : transport_stats_by_name) {
    const std::string& transport_name = entry.first;
    const cricket::TransportStats& transport_stats = entry.second;
    for (const auto& channel_stats : transport_stats.channel_stats) {
      std::string transport_id = RTCTransportStatsIDFromTransportChannel(
          transport_name, channel_stats.component);
      for (const cricket::ConnectionInfo& info :
           channel_stats.connection_infos) {
        std::unique_ptr<RTCIceCandidatePairStats> candidate_pair_stats(
            new RTCIceCandidatePairStats(
                RTCIceCandidatePairStatsIDFromConnectionInfo(info),
                timestamp_us));

        candidate_pair_stats->transport_id = transport_id;
        // TODO(hbos): There could be other candidates that are not paired with
        // anything. We don't have a complete list. Local candidates come from
        // Port objects, and prflx candidates (both local and remote) are only
        // stored in candidate pairs. https://crbug.com/632723
        candidate_pair_stats->local_candidate_id = ProduceIceCandidateStats(
            timestamp_us, info.local_candidate, true, transport_id, report);
        candidate_pair_stats->remote_candidate_id = ProduceIceCandidateStats(
            timestamp_us, info.remote_candidate, false, transport_id, report);
        candidate_pair_stats->state =
            IceCandidatePairStateToRTCStatsIceCandidatePairState(info.state);
        candidate_pair_stats->priority = info.priority;
        candidate_pair_stats->nominated = info.nominated;
        // TODO(hbos): This writable is different than the spec. It goes to
        // false after a certain amount of time without a response passes.
        // https://crbug.com/633550
        candidate_pair_stats->writable = info.writable;
        candidate_pair_stats->bytes_sent =
            static_cast<uint64_t>(info.sent_total_bytes);
        candidate_pair_stats->bytes_received =
            static_cast<uint64_t>(info.recv_total_bytes);
        candidate_pair_stats->total_round_trip_time =
            static_cast<double>(info.total_round_trip_time_ms) /
            rtc::kNumMillisecsPerSec;
        if (info.current_round_trip_time_ms) {
          candidate_pair_stats->current_round_trip_time =
              static_cast<double>(*info.current_round_trip_time_ms) /
              rtc::kNumMillisecsPerSec;
        }
        if (info.best_connection) {
          // The bandwidth estimations we have are for the selected candidate
          // pair ("info.best_connection").
          RTC_DCHECK_GE(call_stats.send_bandwidth_bps, 0);
          RTC_DCHECK_GE(call_stats.recv_bandwidth_bps, 0);
          if (call_stats.send_bandwidth_bps > 0) {
            candidate_pair_stats->available_outgoing_bitrate =
                static_cast<double>(call_stats.send_bandwidth_bps);
          }
          if (call_stats.recv_bandwidth_bps > 0) {
            candidate_pair_stats->available_incoming_bitrate =
                static_cast<double>(call_stats.recv_bandwidth_bps);
          }
        }
        candidate_pair_stats->requests_received =
            static_cast<uint64_t>(info.recv_ping_requests);
        candidate_pair_stats->requests_sent = static_cast<uint64_t>(
            info.sent_ping_requests_before_first_response);
        candidate_pair_stats->responses_received =
            static_cast<uint64_t>(info.recv_ping_responses);
        candidate_pair_stats->responses_sent =
            static_cast<uint64_t>(info.sent_ping_responses);
        RTC_DCHECK_GE(info.sent_ping_requests_total,
                      info.sent_ping_requests_before_first_response);
        candidate_pair_stats->consent_requests_sent = static_cast<uint64_t>(
            info.sent_ping_requests_total -
            info.sent_ping_requests_before_first_response);

        report->AddStats(std::move(candidate_pair_stats));
      }
    }
  }
}

void RTCStatsCollector::ProduceMediaStreamStats_s(
    int64_t timestamp_us,
    RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  std::map<std::string, std::vector<std::string>> track_ids;

  for (const auto& stats : transceiver_stats_infos_) {
    for (const auto& sender : stats.transceiver->senders()) {
      std::string track_id =
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(
              kSender, sender->internal()->AttachmentId());
      for (auto& stream_id : sender->stream_ids()) {
        track_ids[stream_id].push_back(track_id);
      }
    }
    for (const auto& receiver : stats.transceiver->receivers()) {
      std::string track_id =
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(
              kReceiver, receiver->internal()->AttachmentId());
      for (auto& stream : receiver->streams()) {
        track_ids[stream->id()].push_back(track_id);
      }
    }
  }

  // Build stats for each stream ID known.
  for (auto& it : track_ids) {
    std::unique_ptr<RTCMediaStreamStats> stream_stats(
        new RTCMediaStreamStats("RTCMediaStream_" + it.first, timestamp_us));
    stream_stats->stream_identifier = it.first;
    stream_stats->track_ids = it.second;
    report->AddStats(std::move(stream_stats));
  }
}

void RTCStatsCollector::ProduceMediaStreamTrackStats_s(
    int64_t timestamp_us,
    RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  for (const RtpTransceiverStatsInfo& stats : transceiver_stats_infos_) {
    std::vector<rtc::scoped_refptr<RtpSenderInternal>> senders;
    for (const auto& sender : stats.transceiver->senders()) {
      senders.push_back(sender->internal());
    }
    ProduceSenderMediaTrackStats(timestamp_us, *stats.track_media_info_map,
                                 senders, report);

    std::vector<rtc::scoped_refptr<RtpReceiverInternal>> receivers;
    for (const auto& receiver : stats.transceiver->receivers()) {
      receivers.push_back(receiver->internal());
    }
    ProduceReceiverMediaTrackStats(timestamp_us, *stats.track_media_info_map,
                                   receivers, report);
  }
}

void RTCStatsCollector::ProducePeerConnectionStats_s(
    int64_t timestamp_us, RTCStatsReport* report) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  std::unique_ptr<RTCPeerConnectionStats> stats(
    new RTCPeerConnectionStats("RTCPeerConnection", timestamp_us));
  stats->data_channels_opened = internal_record_.data_channels_opened;
  stats->data_channels_closed = internal_record_.data_channels_closed;
  report->AddStats(std::move(stats));
}

void RTCStatsCollector::ProduceRTPStreamStats_n(
    int64_t timestamp_us,
    const std::vector<RtpTransceiverStatsInfo>& transceiver_stats_infos,
    RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  for (const RtpTransceiverStatsInfo& stats : transceiver_stats_infos) {
    if (stats.media_type == cricket::MEDIA_TYPE_AUDIO) {
      ProduceAudioRTPStreamStats_n(timestamp_us, stats, report);
    } else if (stats.media_type == cricket::MEDIA_TYPE_VIDEO) {
      ProduceVideoRTPStreamStats_n(timestamp_us, stats, report);
    } else {
      RTC_NOTREACHED();
    }
  }
}

void RTCStatsCollector::ProduceAudioRTPStreamStats_n(
    int64_t timestamp_us,
    const RtpTransceiverStatsInfo& stats,
    RTCStatsReport* report) const {
  if (!stats.mid || !stats.transport_name) {
    return;
  }
  RTC_DCHECK(stats.track_media_info_map);
  const TrackMediaInfoMap& track_media_info_map = *stats.track_media_info_map;
  RTC_DCHECK(track_media_info_map.voice_media_info());
  std::string mid = *stats.mid;
  std::string transport_id = RTCTransportStatsIDFromTransportChannel(
      *stats.transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  // Inbound
  for (const cricket::VoiceReceiverInfo& voice_receiver_info :
       track_media_info_map.voice_media_info()->receivers) {
    if (!voice_receiver_info.connected())
      continue;
    auto inbound_audio = absl::make_unique<RTCInboundRTPStreamStats>(
        RTCInboundRTPStreamStatsIDFromSSRC(true, voice_receiver_info.ssrc()),
        timestamp_us);
    SetInboundRTPStreamStatsFromVoiceReceiverInfo(mid, voice_receiver_info,
                                                  inbound_audio.get());
    // TODO(hta): This lookup should look for the sender, not the track.
    rtc::scoped_refptr<AudioTrackInterface> audio_track =
        track_media_info_map.GetAudioTrack(voice_receiver_info);
    if (audio_track) {
      inbound_audio->track_id =
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(
              kReceiver,
              track_media_info_map.GetAttachmentIdByTrack(audio_track).value());
    }
    inbound_audio->transport_id = transport_id;
    report->AddStats(std::move(inbound_audio));
  }
  // Outbound
  for (const cricket::VoiceSenderInfo& voice_sender_info :
       track_media_info_map.voice_media_info()->senders) {
    if (!voice_sender_info.connected())
      continue;
    auto outbound_audio = absl::make_unique<RTCOutboundRTPStreamStats>(
        RTCOutboundRTPStreamStatsIDFromSSRC(true, voice_sender_info.ssrc()),
        timestamp_us);
    SetOutboundRTPStreamStatsFromVoiceSenderInfo(mid, voice_sender_info,
                                                 outbound_audio.get());
    rtc::scoped_refptr<AudioTrackInterface> audio_track =
        track_media_info_map.GetAudioTrack(voice_sender_info);
    if (audio_track) {
      outbound_audio->track_id =
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(
              kSender,
              track_media_info_map.GetAttachmentIdByTrack(audio_track).value());
    }
    outbound_audio->transport_id = transport_id;
    report->AddStats(std::move(outbound_audio));
  }
}

void RTCStatsCollector::ProduceVideoRTPStreamStats_n(
    int64_t timestamp_us,
    const RtpTransceiverStatsInfo& stats,
    RTCStatsReport* report) const {
  if (!stats.mid || !stats.transport_name) {
    return;
  }
  RTC_DCHECK(stats.track_media_info_map);
  const TrackMediaInfoMap& track_media_info_map = *stats.track_media_info_map;
  RTC_DCHECK(track_media_info_map.video_media_info());
  std::string mid = *stats.mid;
  std::string transport_id = RTCTransportStatsIDFromTransportChannel(
      *stats.transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  // Inbound
  for (const cricket::VideoReceiverInfo& video_receiver_info :
       track_media_info_map.video_media_info()->receivers) {
    if (!video_receiver_info.connected())
      continue;
    auto inbound_video = absl::make_unique<RTCInboundRTPStreamStats>(
        RTCInboundRTPStreamStatsIDFromSSRC(false, video_receiver_info.ssrc()),
        timestamp_us);
    SetInboundRTPStreamStatsFromVideoReceiverInfo(mid, video_receiver_info,
                                                  inbound_video.get());
    rtc::scoped_refptr<VideoTrackInterface> video_track =
        track_media_info_map.GetVideoTrack(video_receiver_info);
    if (video_track) {
      inbound_video->track_id =
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(
              kReceiver,
              track_media_info_map.GetAttachmentIdByTrack(video_track).value());
    }
    inbound_video->transport_id = transport_id;
    report->AddStats(std::move(inbound_video));
  }
  // Outbound
  for (const cricket::VideoSenderInfo& video_sender_info :
       track_media_info_map.video_media_info()->senders) {
    if (!video_sender_info.connected())
      continue;
    auto outbound_video = absl::make_unique<RTCOutboundRTPStreamStats>(
        RTCOutboundRTPStreamStatsIDFromSSRC(false, video_sender_info.ssrc()),
        timestamp_us);
    SetOutboundRTPStreamStatsFromVideoSenderInfo(mid, video_sender_info,
                                                 outbound_video.get());
    rtc::scoped_refptr<VideoTrackInterface> video_track =
        track_media_info_map.GetVideoTrack(video_sender_info);
    if (video_track) {
      outbound_video->track_id =
          RTCMediaStreamTrackStatsIDFromDirectionAndAttachment(
              kSender,
              track_media_info_map.GetAttachmentIdByTrack(video_track).value());
    }
    outbound_video->transport_id = transport_id;
    report->AddStats(std::move(outbound_video));
  }
}

void RTCStatsCollector::ProduceTransportStats_n(
    int64_t timestamp_us,
    const std::map<std::string, cricket::TransportStats>&
        transport_stats_by_name,
    const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
    RTCStatsReport* report) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  for (const auto& entry : transport_stats_by_name) {
    const std::string& transport_name = entry.first;
    const cricket::TransportStats& transport_stats = entry.second;

    // Get reference to RTCP channel, if it exists.
    std::string rtcp_transport_stats_id;
    for (const cricket::TransportChannelStats& channel_stats :
         transport_stats.channel_stats) {
      if (channel_stats.component ==
          cricket::ICE_CANDIDATE_COMPONENT_RTCP) {
        rtcp_transport_stats_id = RTCTransportStatsIDFromTransportChannel(
            transport_name, channel_stats.component);
        break;
      }
    }

    // Get reference to local and remote certificates of this transport, if they
    // exist.
    const auto& certificate_stats_it =
        transport_cert_stats.find(transport_name);
    RTC_DCHECK(certificate_stats_it != transport_cert_stats.cend());
    std::string local_certificate_id;
    if (certificate_stats_it->second.local) {
      local_certificate_id = RTCCertificateIDFromFingerprint(
          certificate_stats_it->second.local->fingerprint);
    }
    std::string remote_certificate_id;
    if (certificate_stats_it->second.remote) {
      remote_certificate_id = RTCCertificateIDFromFingerprint(
          certificate_stats_it->second.remote->fingerprint);
    }

    // There is one transport stats for each channel.
    for (const cricket::TransportChannelStats& channel_stats :
         transport_stats.channel_stats) {
      std::unique_ptr<RTCTransportStats> transport_stats(
          new RTCTransportStats(RTCTransportStatsIDFromTransportChannel(
                                    transport_name, channel_stats.component),
                                timestamp_us));
      transport_stats->bytes_sent = 0;
      transport_stats->bytes_received = 0;
      transport_stats->dtls_state = DtlsTransportStateToRTCDtlsTransportState(
          channel_stats.dtls_state);
      for (const cricket::ConnectionInfo& info :
           channel_stats.connection_infos) {
        *transport_stats->bytes_sent += info.sent_total_bytes;
        *transport_stats->bytes_received += info.recv_total_bytes;
        if (info.best_connection) {
          transport_stats->selected_candidate_pair_id =
              RTCIceCandidatePairStatsIDFromConnectionInfo(info);
        }
      }
      if (channel_stats.component != cricket::ICE_CANDIDATE_COMPONENT_RTCP &&
          !rtcp_transport_stats_id.empty()) {
        transport_stats->rtcp_transport_stats_id = rtcp_transport_stats_id;
      }
      if (!local_certificate_id.empty())
        transport_stats->local_certificate_id = local_certificate_id;
      if (!remote_certificate_id.empty())
        transport_stats->remote_certificate_id = remote_certificate_id;
      report->AddStats(std::move(transport_stats));
    }
  }
}

std::map<std::string, RTCStatsCollector::CertificateStatsPair>
RTCStatsCollector::PrepareTransportCertificateStats_n(
    const std::map<std::string, cricket::TransportStats>&
        transport_stats_by_name) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  std::map<std::string, CertificateStatsPair> transport_cert_stats;
  for (const auto& entry : transport_stats_by_name) {
    const std::string& transport_name = entry.first;

    CertificateStatsPair certificate_stats_pair;
    rtc::scoped_refptr<rtc::RTCCertificate> local_certificate;
    if (pc_->GetLocalCertificate(transport_name, &local_certificate)) {
      certificate_stats_pair.local =
          local_certificate->GetSSLCertificateChain().GetStats();
    }

    std::unique_ptr<rtc::SSLCertChain> remote_cert_chain =
        pc_->GetRemoteSSLCertChain(transport_name);
    if (remote_cert_chain) {
      certificate_stats_pair.remote = remote_cert_chain->GetStats();
    }

    transport_cert_stats.insert(
        std::make_pair(transport_name, std::move(certificate_stats_pair)));
  }
  return transport_cert_stats;
}

std::vector<RTCStatsCollector::RtpTransceiverStatsInfo>
RTCStatsCollector::PrepareTransceiverStatsInfos_s() const {
  std::vector<RtpTransceiverStatsInfo> transceiver_stats_infos;

  // These are used to invoke GetStats for all the media channels together in
  // one worker thread hop.
  std::map<cricket::VoiceMediaChannel*,
           std::unique_ptr<cricket::VoiceMediaInfo>>
      voice_stats;
  std::map<cricket::VideoMediaChannel*,
           std::unique_ptr<cricket::VideoMediaInfo>>
      video_stats;

  for (const auto& transceiver : pc_->GetTransceiversInternal()) {
    cricket::MediaType media_type = transceiver->media_type();

    // Prepare stats entry. The TrackMediaInfoMap will be filled in after the
    // stats have been fetched on the worker thread.
    transceiver_stats_infos.emplace_back();
    RtpTransceiverStatsInfo& stats = transceiver_stats_infos.back();
    stats.transceiver = transceiver->internal();
    stats.media_type = media_type;

    cricket::ChannelInterface* channel = transceiver->internal()->channel();
    if (!channel) {
      // The remaining fields require a BaseChannel.
      continue;
    }

    stats.mid = channel->content_name();
    stats.transport_name = channel->transport_name();

    if (media_type == cricket::MEDIA_TYPE_AUDIO) {
      auto* voice_channel = static_cast<cricket::VoiceChannel*>(channel);
      RTC_DCHECK(voice_stats.find(voice_channel->media_channel()) ==
                 voice_stats.end());
      voice_stats[voice_channel->media_channel()] =
          absl::make_unique<cricket::VoiceMediaInfo>();
    } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
      auto* video_channel = static_cast<cricket::VideoChannel*>(channel);
      RTC_DCHECK(video_stats.find(video_channel->media_channel()) ==
                 video_stats.end());
      video_stats[video_channel->media_channel()] =
          absl::make_unique<cricket::VideoMediaInfo>();
    } else {
      RTC_NOTREACHED();
    }
  }

  // Call GetStats for all media channels together on the worker thread in one
  // hop.
  worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
    for (const auto& entry : voice_stats) {
      if (!entry.first->GetStats(entry.second.get())) {
        RTC_LOG(LS_WARNING) << "Failed to get voice stats.";
      }
    }
    for (const auto& entry : video_stats) {
      if (!entry.first->GetStats(entry.second.get())) {
        RTC_LOG(LS_WARNING) << "Failed to get video stats.";
      }
    }
  });

  // Create the TrackMediaInfoMap for each transceiver stats object.
  for (auto& stats : transceiver_stats_infos) {
    auto transceiver = stats.transceiver;
    std::unique_ptr<cricket::VoiceMediaInfo> voice_media_info;
    std::unique_ptr<cricket::VideoMediaInfo> video_media_info;
    if (transceiver->channel()) {
      cricket::MediaType media_type = transceiver->media_type();
      if (media_type == cricket::MEDIA_TYPE_AUDIO) {
        auto* voice_channel =
            static_cast<cricket::VoiceChannel*>(transceiver->channel());
        RTC_DCHECK(voice_stats[voice_channel->media_channel()]);
        voice_media_info =
            std::move(voice_stats[voice_channel->media_channel()]);
      } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
        auto* video_channel =
            static_cast<cricket::VideoChannel*>(transceiver->channel());
        RTC_DCHECK(video_stats[video_channel->media_channel()]);
        video_media_info =
            std::move(video_stats[video_channel->media_channel()]);
      }
    }
    std::vector<rtc::scoped_refptr<RtpSenderInternal>> senders;
    for (const auto& sender : transceiver->senders()) {
      senders.push_back(sender->internal());
    }
    std::vector<rtc::scoped_refptr<RtpReceiverInternal>> receivers;
    for (const auto& receiver : transceiver->receivers()) {
      receivers.push_back(receiver->internal());
    }
    stats.track_media_info_map = absl::make_unique<TrackMediaInfoMap>(
        std::move(voice_media_info), std::move(video_media_info), senders,
        receivers);
  }

  return transceiver_stats_infos;
}

std::set<std::string> RTCStatsCollector::PrepareTransportNames_s() const {
  std::set<std::string> transport_names;
  for (const auto& transceiver : pc_->GetTransceiversInternal()) {
    if (transceiver->internal()->channel()) {
      transport_names.insert(
          transceiver->internal()->channel()->transport_name());
    }
  }
  if (pc_->rtp_data_channel()) {
    transport_names.insert(pc_->rtp_data_channel()->transport_name());
  }
  if (pc_->sctp_transport_name()) {
    transport_names.insert(*pc_->sctp_transport_name());
  }
  return transport_names;
}

void RTCStatsCollector::OnDataChannelCreated(DataChannel* channel) {
  channel->SignalOpened.connect(this, &RTCStatsCollector::OnDataChannelOpened);
  channel->SignalClosed.connect(this, &RTCStatsCollector::OnDataChannelClosed);
}

void RTCStatsCollector::OnDataChannelOpened(DataChannel* channel) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  bool result = internal_record_.opened_data_channels.insert(
      reinterpret_cast<uintptr_t>(channel)).second;
  ++internal_record_.data_channels_opened;
  RTC_DCHECK(result);
}

void RTCStatsCollector::OnDataChannelClosed(DataChannel* channel) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  // Only channels that have been fully opened (and have increased the
  // |data_channels_opened_| counter) increase the closed counter.
  if (internal_record_.opened_data_channels.erase(
          reinterpret_cast<uintptr_t>(channel))) {
    ++internal_record_.data_channels_closed;
  }
}

const char* CandidateTypeToRTCIceCandidateTypeForTesting(
    const std::string& type) {
  return CandidateTypeToRTCIceCandidateType(type);
}

const char* DataStateToRTCDataChannelStateForTesting(
    DataChannelInterface::DataState state) {
  return DataStateToRTCDataChannelState(state);
}

}  // namespace webrtc
