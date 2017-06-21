/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/statscollector.h"

#include <memory>
#include <utility>
#include <vector>

#include "webrtc/base/base64.h"
#include "webrtc/base/checks.h"
#include "webrtc/pc/channel.h"
#include "webrtc/pc/peerconnection.h"

namespace webrtc {
namespace {

// The following is the enum RTCStatsIceCandidateType from
// http://w3c.github.io/webrtc-stats/#rtcstatsicecandidatetype-enum such that
// our stats report for ice candidate type could conform to that.
const char STATSREPORT_LOCAL_PORT_TYPE[] = "host";
const char STATSREPORT_STUN_PORT_TYPE[] = "serverreflexive";
const char STATSREPORT_PRFLX_PORT_TYPE[] = "peerreflexive";
const char STATSREPORT_RELAY_PORT_TYPE[] = "relayed";

// Strings used by the stats collector to report adapter types. This fits the
// general stype of http://w3c.github.io/webrtc-stats than what
// AdapterTypeToString does.
const char* STATSREPORT_ADAPTER_TYPE_ETHERNET = "lan";
const char* STATSREPORT_ADAPTER_TYPE_WIFI = "wlan";
const char* STATSREPORT_ADAPTER_TYPE_WWAN = "wwan";
const char* STATSREPORT_ADAPTER_TYPE_VPN = "vpn";
const char* STATSREPORT_ADAPTER_TYPE_LOOPBACK = "loopback";

template<typename ValueType>
struct TypeForAdd {
  const StatsReport::StatsValueName name;
  const ValueType& value;
};

typedef TypeForAdd<bool> BoolForAdd;
typedef TypeForAdd<float> FloatForAdd;
typedef TypeForAdd<int64_t> Int64ForAdd;
typedef TypeForAdd<int> IntForAdd;

StatsReport::Id GetTransportIdFromProxy(const ProxyTransportMap& map,
                                        const std::string& proxy) {
  RTC_DCHECK(!proxy.empty());
  auto found = map.find(proxy);
  if (found == map.end()) {
    return StatsReport::Id();
  }

  return StatsReport::NewComponentId(
      found->second, cricket::ICE_CANDIDATE_COMPONENT_RTP);
}

StatsReport* AddTrackReport(StatsCollection* reports,
                            const std::string& track_id) {
  // Adds an empty track report.
  StatsReport::Id id(
      StatsReport::NewTypedId(StatsReport::kStatsReportTypeTrack, track_id));
  StatsReport* report = reports->ReplaceOrAddNew(id);
  report->AddString(StatsReport::kStatsValueNameTrackId, track_id);
  return report;
}

template <class TrackVector>
void CreateTrackReports(const TrackVector& tracks, StatsCollection* reports,
                        TrackIdMap& track_ids) {
  for (const auto& track : tracks) {
    const std::string& track_id = track->id();
    StatsReport* report = AddTrackReport(reports, track_id);
    RTC_DCHECK(report != nullptr);
    track_ids[track_id] = report;
  }
}

void ExtractCommonSendProperties(const cricket::MediaSenderInfo& info,
                                 StatsReport* report) {
  report->AddString(StatsReport::kStatsValueNameCodecName, info.codec_name);
  report->AddInt64(StatsReport::kStatsValueNameBytesSent, info.bytes_sent);
  if (info.rtt_ms >= 0) {
    report->AddInt64(StatsReport::kStatsValueNameRtt, info.rtt_ms);
  }
}

void ExtractCommonReceiveProperties(const cricket::MediaReceiverInfo& info,
                                    StatsReport* report) {
  report->AddString(StatsReport::kStatsValueNameCodecName, info.codec_name);
}

void SetAudioProcessingStats(StatsReport* report,
                             bool typing_noise_detected,
                             int echo_return_loss,
                             int echo_return_loss_enhancement,
                             int echo_delay_median_ms,
                             float aec_quality_min,
                             int echo_delay_std_ms,
                             float residual_echo_likelihood,
                             float residual_echo_likelihood_recent_max) {
  report->AddBoolean(StatsReport::kStatsValueNameTypingNoiseState,
                     typing_noise_detected);
  if (aec_quality_min >= 0.0f) {
    report->AddFloat(StatsReport::kStatsValueNameEchoCancellationQualityMin,
                     aec_quality_min);
  }
  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameEchoDelayMedian, echo_delay_median_ms },
    { StatsReport::kStatsValueNameEchoDelayStdDev, echo_delay_std_ms },
  };
  for (const auto& i : ints) {
    if (i.value >= 0) {
      report->AddInt(i.name, i.value);
    }
  }
  // These can take on valid negative values.
  report->AddInt(StatsReport::kStatsValueNameEchoReturnLoss, echo_return_loss);
  report->AddInt(StatsReport::kStatsValueNameEchoReturnLossEnhancement,
                 echo_return_loss_enhancement);
  if (residual_echo_likelihood >= 0.0f) {
    report->AddFloat(StatsReport::kStatsValueNameResidualEchoLikelihood,
                     residual_echo_likelihood);
  }
  if (residual_echo_likelihood_recent_max >= 0.0f) {
    report->AddFloat(
        StatsReport::kStatsValueNameResidualEchoLikelihoodRecentMax,
        residual_echo_likelihood_recent_max);
  }
}

void ExtractStats(const cricket::VoiceReceiverInfo& info, StatsReport* report) {
  ExtractCommonReceiveProperties(info, report);
  const FloatForAdd floats[] = {
    { StatsReport::kStatsValueNameExpandRate, info.expand_rate },
    { StatsReport::kStatsValueNameSecondaryDecodedRate,
      info.secondary_decoded_rate },
    { StatsReport::kStatsValueNameSpeechExpandRate, info.speech_expand_rate },
    { StatsReport::kStatsValueNameAccelerateRate, info.accelerate_rate },
    { StatsReport::kStatsValueNamePreemptiveExpandRate,
      info.preemptive_expand_rate },
  };

  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameCurrentDelayMs, info.delay_estimate_ms },
    { StatsReport::kStatsValueNameDecodingCNG, info.decoding_cng },
    { StatsReport::kStatsValueNameDecodingCTN, info.decoding_calls_to_neteq },
    { StatsReport::kStatsValueNameDecodingCTSG,
      info.decoding_calls_to_silence_generator },
    { StatsReport::kStatsValueNameDecodingMutedOutput,
      info.decoding_muted_output },
    { StatsReport::kStatsValueNameDecodingNormal, info.decoding_normal },
    { StatsReport::kStatsValueNameDecodingPLC, info.decoding_plc },
    { StatsReport::kStatsValueNameDecodingPLCCNG, info.decoding_plc_cng },
    { StatsReport::kStatsValueNameJitterBufferMs, info.jitter_buffer_ms },
    { StatsReport::kStatsValueNameJitterReceived, info.jitter_ms },
    { StatsReport::kStatsValueNamePacketsLost, info.packets_lost },
    { StatsReport::kStatsValueNamePacketsReceived, info.packets_rcvd },
    { StatsReport::kStatsValueNamePreferredJitterBufferMs,
      info.jitter_buffer_preferred_ms },
  };

  for (const auto& f : floats)
    report->AddFloat(f.name, f.value);

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
  if (info.audio_level >= 0) {
    report->AddInt(StatsReport::kStatsValueNameAudioOutputLevel,
                   info.audio_level);
  }

  report->AddInt64(StatsReport::kStatsValueNameBytesReceived,
                   info.bytes_rcvd);
  if (info.capture_start_ntp_time_ms >= 0) {
    report->AddInt64(StatsReport::kStatsValueNameCaptureStartNtpTimeMs,
                     info.capture_start_ntp_time_ms);
  }
  report->AddString(StatsReport::kStatsValueNameMediaType, "audio");
}

void ExtractStats(const cricket::VoiceSenderInfo& info, StatsReport* report) {
  ExtractCommonSendProperties(info, report);

  SetAudioProcessingStats(
      report, info.typing_noise_detected, info.echo_return_loss,
      info.echo_return_loss_enhancement, info.echo_delay_median_ms,
      info.aec_quality_min, info.echo_delay_std_ms,
      info.residual_echo_likelihood, info.residual_echo_likelihood_recent_max);

  RTC_DCHECK_GE(info.audio_level, 0);
  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameAudioInputLevel, info.audio_level},
    { StatsReport::kStatsValueNameJitterReceived, info.jitter_ms },
    { StatsReport::kStatsValueNamePacketsLost, info.packets_lost },
    { StatsReport::kStatsValueNamePacketsSent, info.packets_sent },
  };

  for (const auto& i : ints) {
    if (i.value >= 0) {
      report->AddInt(i.name, i.value);
    }
  }
  report->AddString(StatsReport::kStatsValueNameMediaType, "audio");
}

void ExtractStats(const cricket::VideoReceiverInfo& info, StatsReport* report) {
  ExtractCommonReceiveProperties(info, report);
  report->AddString(StatsReport::kStatsValueNameCodecImplementationName,
                    info.decoder_implementation_name);
  report->AddInt64(StatsReport::kStatsValueNameBytesReceived,
                   info.bytes_rcvd);
  if (info.capture_start_ntp_time_ms >= 0) {
    report->AddInt64(StatsReport::kStatsValueNameCaptureStartNtpTimeMs,
                     info.capture_start_ntp_time_ms);
  }
  if (info.qp_sum)
    report->AddInt64(StatsReport::kStatsValueNameQpSum, *info.qp_sum);

  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameCurrentDelayMs, info.current_delay_ms },
    { StatsReport::kStatsValueNameDecodeMs, info.decode_ms },
    { StatsReport::kStatsValueNameFirsSent, info.firs_sent },
    { StatsReport::kStatsValueNameFrameHeightReceived, info.frame_height },
    { StatsReport::kStatsValueNameFrameRateDecoded, info.framerate_decoded },
    { StatsReport::kStatsValueNameFrameRateOutput, info.framerate_output },
    { StatsReport::kStatsValueNameFrameRateReceived, info.framerate_rcvd },
    { StatsReport::kStatsValueNameFrameWidthReceived, info.frame_width },
    { StatsReport::kStatsValueNameJitterBufferMs, info.jitter_buffer_ms },
    { StatsReport::kStatsValueNameMaxDecodeMs, info.max_decode_ms },
    { StatsReport::kStatsValueNameMinPlayoutDelayMs,
      info.min_playout_delay_ms },
    { StatsReport::kStatsValueNameNacksSent, info.nacks_sent },
    { StatsReport::kStatsValueNamePacketsLost, info.packets_lost },
    { StatsReport::kStatsValueNamePacketsReceived, info.packets_rcvd },
    { StatsReport::kStatsValueNamePlisSent, info.plis_sent },
    { StatsReport::kStatsValueNameRenderDelayMs, info.render_delay_ms },
    { StatsReport::kStatsValueNameTargetDelayMs, info.target_delay_ms },
    { StatsReport::kStatsValueNameFramesDecoded, info.frames_decoded },
  };

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
  report->AddString(StatsReport::kStatsValueNameMediaType, "video");
}

void ExtractStats(const cricket::VideoSenderInfo& info, StatsReport* report) {
  ExtractCommonSendProperties(info, report);

  report->AddString(StatsReport::kStatsValueNameCodecImplementationName,
                    info.encoder_implementation_name);
  report->AddBoolean(StatsReport::kStatsValueNameBandwidthLimitedResolution,
                     (info.adapt_reason & 0x2) > 0);
  report->AddBoolean(StatsReport::kStatsValueNameCpuLimitedResolution,
                     (info.adapt_reason & 0x1) > 0);
  if (info.qp_sum)
    report->AddInt(StatsReport::kStatsValueNameQpSum, *info.qp_sum);

  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameAdaptationChanges, info.adapt_changes },
    { StatsReport::kStatsValueNameAvgEncodeMs, info.avg_encode_ms },
    { StatsReport::kStatsValueNameEncodeUsagePercent,
      info.encode_usage_percent },
    { StatsReport::kStatsValueNameFirsReceived, info.firs_rcvd },
    { StatsReport::kStatsValueNameFrameHeightSent, info.send_frame_height },
    { StatsReport::kStatsValueNameFrameRateInput, info.framerate_input },
    { StatsReport::kStatsValueNameFrameRateSent, info.framerate_sent },
    { StatsReport::kStatsValueNameFrameWidthSent, info.send_frame_width },
    { StatsReport::kStatsValueNameNacksReceived, info.nacks_rcvd },
    { StatsReport::kStatsValueNamePacketsLost, info.packets_lost },
    { StatsReport::kStatsValueNamePacketsSent, info.packets_sent },
    { StatsReport::kStatsValueNamePlisReceived, info.plis_rcvd },
    { StatsReport::kStatsValueNameFramesEncoded, info.frames_encoded },
  };

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
  report->AddString(StatsReport::kStatsValueNameMediaType, "video");
}

void ExtractStats(const cricket::BandwidthEstimationInfo& info,
                  double stats_gathering_started,
                  StatsReport* report) {
  RTC_DCHECK(report->type() == StatsReport::kStatsReportTypeBwe);

  report->set_timestamp(stats_gathering_started);
  const IntForAdd ints[] = {
    { StatsReport::kStatsValueNameAvailableSendBandwidth,
      info.available_send_bandwidth },
    { StatsReport::kStatsValueNameAvailableReceiveBandwidth,
      info.available_recv_bandwidth },
    { StatsReport::kStatsValueNameTargetEncBitrate, info.target_enc_bitrate },
    { StatsReport::kStatsValueNameActualEncBitrate, info.actual_enc_bitrate },
    { StatsReport::kStatsValueNameRetransmitBitrate, info.retransmit_bitrate },
    { StatsReport::kStatsValueNameTransmitBitrate, info.transmit_bitrate },
  };
  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
  report->AddInt64(StatsReport::kStatsValueNameBucketDelay, info.bucket_delay);
}

void ExtractRemoteStats(const cricket::MediaSenderInfo& info,
                        StatsReport* report) {
  report->set_timestamp(info.remote_stats[0].timestamp);
  // TODO(hta): Extract some stats here.
}

void ExtractRemoteStats(const cricket::MediaReceiverInfo& info,
                        StatsReport* report) {
  report->set_timestamp(info.remote_stats[0].timestamp);
  // TODO(hta): Extract some stats here.
}

// Template to extract stats from a data vector.
// In order to use the template, the functions that are called from it,
// ExtractStats and ExtractRemoteStats, must be defined and overloaded
// for each type.
template<typename T>
void ExtractStatsFromList(const std::vector<T>& data,
                          const StatsReport::Id& transport_id,
                          StatsCollector* collector,
                          StatsReport::Direction direction) {
  for (const auto& d : data) {
    uint32_t ssrc = d.ssrc();
    // Each track can have stats for both local and remote objects.
    // TODO(hta): Handle the case of multiple SSRCs per object.
    StatsReport* report = collector->PrepareReport(true, ssrc, transport_id,
                                                   direction);
    if (report)
      ExtractStats(d, report);

    if (!d.remote_stats.empty()) {
      report = collector->PrepareReport(false, ssrc, transport_id, direction);
      if (report)
        ExtractRemoteStats(d, report);
    }
  }
}

}  // namespace

const char* IceCandidateTypeToStatsType(const std::string& candidate_type) {
  if (candidate_type == cricket::LOCAL_PORT_TYPE) {
    return STATSREPORT_LOCAL_PORT_TYPE;
  }
  if (candidate_type == cricket::STUN_PORT_TYPE) {
    return STATSREPORT_STUN_PORT_TYPE;
  }
  if (candidate_type == cricket::PRFLX_PORT_TYPE) {
    return STATSREPORT_PRFLX_PORT_TYPE;
  }
  if (candidate_type == cricket::RELAY_PORT_TYPE) {
    return STATSREPORT_RELAY_PORT_TYPE;
  }
  RTC_NOTREACHED();
  return "unknown";
}

const char* AdapterTypeToStatsType(rtc::AdapterType type) {
  switch (type) {
    case rtc::ADAPTER_TYPE_UNKNOWN:
      return "unknown";
    case rtc::ADAPTER_TYPE_ETHERNET:
      return STATSREPORT_ADAPTER_TYPE_ETHERNET;
    case rtc::ADAPTER_TYPE_WIFI:
      return STATSREPORT_ADAPTER_TYPE_WIFI;
    case rtc::ADAPTER_TYPE_CELLULAR:
      return STATSREPORT_ADAPTER_TYPE_WWAN;
    case rtc::ADAPTER_TYPE_VPN:
      return STATSREPORT_ADAPTER_TYPE_VPN;
    case rtc::ADAPTER_TYPE_LOOPBACK:
      return STATSREPORT_ADAPTER_TYPE_LOOPBACK;
    default:
      RTC_NOTREACHED();
      return "";
  }
}

StatsCollector::StatsCollector(PeerConnection* pc)
    : pc_(pc), stats_gathering_started_(0) {
  RTC_DCHECK(pc_);
}

StatsCollector::~StatsCollector() {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
}

// Wallclock time in ms.
double StatsCollector::GetTimeNow() {
  return rtc::TimeUTCMicros() /
         static_cast<double>(rtc::kNumMicrosecsPerMillisec);
}

// Adds a MediaStream with tracks that can be used as a |selector| in a call
// to GetStats.
void StatsCollector::AddStream(MediaStreamInterface* stream) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  RTC_DCHECK(stream != NULL);

  CreateTrackReports<AudioTrackVector>(stream->GetAudioTracks(),
                                       &reports_, track_ids_);
  CreateTrackReports<VideoTrackVector>(stream->GetVideoTracks(),
                                       &reports_, track_ids_);
}

void StatsCollector::AddLocalAudioTrack(AudioTrackInterface* audio_track,
                                        uint32_t ssrc) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  RTC_DCHECK(audio_track != NULL);
#if RTC_DCHECK_IS_ON
  for (const auto& track : local_audio_tracks_)
    RTC_DCHECK(track.first != audio_track || track.second != ssrc);
#endif

  local_audio_tracks_.push_back(std::make_pair(audio_track, ssrc));

  // Create the kStatsReportTypeTrack report for the new track if there is no
  // report yet.
  StatsReport::Id id(StatsReport::NewTypedId(StatsReport::kStatsReportTypeTrack,
                                             audio_track->id()));
  StatsReport* report = reports_.Find(id);
  if (!report) {
    report = reports_.InsertNew(id);
    report->AddString(StatsReport::kStatsValueNameTrackId, audio_track->id());
  }
}

void StatsCollector::RemoveLocalAudioTrack(AudioTrackInterface* audio_track,
                                           uint32_t ssrc) {
  RTC_DCHECK(audio_track != NULL);
  local_audio_tracks_.erase(
      std::remove_if(
          local_audio_tracks_.begin(), local_audio_tracks_.end(),
          [audio_track, ssrc](const LocalAudioTrackVector::value_type& track) {
            return track.first == audio_track && track.second == ssrc;
          }),
      local_audio_tracks_.end());
}

void StatsCollector::GetStats(MediaStreamTrackInterface* track,
                              StatsReports* reports) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  RTC_DCHECK(reports != NULL);
  RTC_DCHECK(reports->empty());

  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  if (!track) {
    reports->reserve(reports_.size());
    for (auto* r : reports_)
      reports->push_back(r);
    return;
  }

  StatsReport* report = reports_.Find(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeSession, pc_->session()->id()));
  if (report)
    reports->push_back(report);

  report = reports_.Find(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeTrack, track->id()));

  if (!report)
    return;

  reports->push_back(report);

  std::string track_id;
  for (const auto* r : reports_) {
    if (r->type() != StatsReport::kStatsReportTypeSsrc)
      continue;

    const StatsReport::Value* v =
        r->FindValue(StatsReport::kStatsValueNameTrackId);
    if (v && v->string_val() == track->id())
      reports->push_back(r);
  }
}

void
StatsCollector::UpdateStats(PeerConnectionInterface::StatsOutputLevel level) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  double time_now = GetTimeNow();
  // Calls to UpdateStats() that occur less than kMinGatherStatsPeriod number of
  // ms apart will be ignored.
  const double kMinGatherStatsPeriod = 50;
  if (stats_gathering_started_ != 0 &&
      stats_gathering_started_ + kMinGatherStatsPeriod > time_now) {
    return;
  }
  stats_gathering_started_ = time_now;

  // TODO(pthatcher): Merge PeerConnection and WebRtcSession so there is no
  // pc_->session().
  if (pc_->session()) {
    // TODO(tommi): All of these hop over to the worker thread to fetch
    // information.  We could use an AsyncInvoker to run all of these and post
    // the information back to the signaling thread where we can create and
    // update stats reports.  That would also clean up the threading story a bit
    // since we'd be creating/updating the stats report objects consistently on
    // the same thread (this class has no locks right now).
    ExtractSessionInfo();
    ExtractBweInfo();
    ExtractVoiceInfo();
    ExtractVideoInfo(level);
    ExtractSenderInfo();
    ExtractDataInfo();
    UpdateTrackReports();
  }
}

StatsReport* StatsCollector::PrepareReport(
    bool local,
    uint32_t ssrc,
    const StatsReport::Id& transport_id,
    StatsReport::Direction direction) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  StatsReport::Id id(StatsReport::NewIdWithDirection(
      local ? StatsReport::kStatsReportTypeSsrc
            : StatsReport::kStatsReportTypeRemoteSsrc,
      rtc::ToString<uint32_t>(ssrc), direction));
  StatsReport* report = reports_.Find(id);

  // Use the ID of the track that is currently mapped to the SSRC, if any.
  std::string track_id;
  if (!GetTrackIdBySsrc(ssrc, &track_id, direction)) {
    if (!report) {
      // The ssrc is not used by any track or existing report, return NULL
      // in such case to indicate no report is prepared for the ssrc.
      return NULL;
    }

    // The ssrc is not used by any existing track. Keeps the old track id
    // since we want to report the stats for inactive ssrc.
    const StatsReport::Value* v =
        report->FindValue(StatsReport::kStatsValueNameTrackId);
    if (v)
      track_id = v->string_val();
  }

  if (!report)
    report = reports_.InsertNew(id);

  // FYI - for remote reports, the timestamp will be overwritten later.
  report->set_timestamp(stats_gathering_started_);

  report->AddInt64(StatsReport::kStatsValueNameSsrc, ssrc);
  report->AddString(StatsReport::kStatsValueNameTrackId, track_id);
  // Add the mapping of SSRC to transport.
  report->AddId(StatsReport::kStatsValueNameTransportId, transport_id);
  return report;
}

bool StatsCollector::IsValidTrack(const std::string& track_id) {
  return reports_.Find(StatsReport::NewTypedId(
             StatsReport::kStatsReportTypeTrack, track_id)) != nullptr;
}

StatsReport* StatsCollector::AddCertificateReports(
    const rtc::SSLCertificate* cert) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  RTC_DCHECK(cert != NULL);

  std::unique_ptr<rtc::SSLCertificateStats> first_stats = cert->GetStats();
  StatsReport* first_report = nullptr;
  StatsReport* prev_report = nullptr;
  for (rtc::SSLCertificateStats* stats = first_stats.get(); stats;
       stats = stats->issuer.get()) {
    StatsReport::Id id(StatsReport::NewTypedId(
        StatsReport::kStatsReportTypeCertificate, stats->fingerprint));

    StatsReport* report = reports_.ReplaceOrAddNew(id);
    report->set_timestamp(stats_gathering_started_);
    report->AddString(StatsReport::kStatsValueNameFingerprint,
                      stats->fingerprint);
    report->AddString(StatsReport::kStatsValueNameFingerprintAlgorithm,
                      stats->fingerprint_algorithm);
    report->AddString(StatsReport::kStatsValueNameDer,
                      stats->base64_certificate);
    if (!first_report)
      first_report = report;
    else
      prev_report->AddId(StatsReport::kStatsValueNameIssuerId, id);
    prev_report = report;
  }
  return first_report;
}

StatsReport* StatsCollector::AddConnectionInfoReport(
    const std::string& content_name, int component, int connection_id,
    const StatsReport::Id& channel_report_id,
    const cricket::ConnectionInfo& info) {
  StatsReport::Id id(StatsReport::NewCandidatePairId(content_name, component,
                                                     connection_id));
  StatsReport* report = reports_.ReplaceOrAddNew(id);
  report->set_timestamp(stats_gathering_started_);

  const BoolForAdd bools[] = {
    {StatsReport::kStatsValueNameActiveConnection, info.best_connection},
    {StatsReport::kStatsValueNameReceiving, info.receiving},
    {StatsReport::kStatsValueNameWritable, info.writable},
  };
  for (const auto& b : bools)
    report->AddBoolean(b.name, b.value);

  report->AddId(StatsReport::kStatsValueNameChannelId, channel_report_id);
  report->AddId(StatsReport::kStatsValueNameLocalCandidateId,
                AddCandidateReport(info.local_candidate, true)->id());
  report->AddId(StatsReport::kStatsValueNameRemoteCandidateId,
                AddCandidateReport(info.remote_candidate, false)->id());

  const Int64ForAdd int64s[] = {
      {StatsReport::kStatsValueNameBytesReceived, info.recv_total_bytes},
      {StatsReport::kStatsValueNameBytesSent, info.sent_total_bytes},
      {StatsReport::kStatsValueNamePacketsSent, info.sent_total_packets},
      {StatsReport::kStatsValueNameRtt, info.rtt},
      {StatsReport::kStatsValueNameSendPacketsDiscarded,
       info.sent_discarded_packets},
      {StatsReport::kStatsValueNameSentPingRequestsTotal,
       info.sent_ping_requests_total},
      {StatsReport::kStatsValueNameSentPingRequestsBeforeFirstResponse,
       info.sent_ping_requests_before_first_response},
      {StatsReport::kStatsValueNameSentPingResponses, info.sent_ping_responses},
      {StatsReport::kStatsValueNameRecvPingRequests, info.recv_ping_requests},
      {StatsReport::kStatsValueNameRecvPingResponses, info.recv_ping_responses},
  };
  for (const auto& i : int64s)
    report->AddInt64(i.name, i.value);

  report->AddString(StatsReport::kStatsValueNameLocalAddress,
                    info.local_candidate.address().ToString());
  report->AddString(StatsReport::kStatsValueNameLocalCandidateType,
                    info.local_candidate.type());
  report->AddString(StatsReport::kStatsValueNameRemoteAddress,
                    info.remote_candidate.address().ToString());
  report->AddString(StatsReport::kStatsValueNameRemoteCandidateType,
                    info.remote_candidate.type());
  report->AddString(StatsReport::kStatsValueNameTransportType,
                    info.local_candidate.protocol());

  return report;
}

StatsReport* StatsCollector::AddCandidateReport(
    const cricket::Candidate& candidate,
    bool local) {
  StatsReport::Id id(StatsReport::NewCandidateId(local, candidate.id()));
  StatsReport* report = reports_.Find(id);
  if (!report) {
    report = reports_.InsertNew(id);
    report->set_timestamp(stats_gathering_started_);
    if (local) {
      report->AddString(StatsReport::kStatsValueNameCandidateNetworkType,
                        AdapterTypeToStatsType(candidate.network_type()));
    }
    report->AddString(StatsReport::kStatsValueNameCandidateIPAddress,
                      candidate.address().ipaddr().ToString());
    report->AddString(StatsReport::kStatsValueNameCandidatePortNumber,
                      candidate.address().PortAsString());
    report->AddInt(StatsReport::kStatsValueNameCandidatePriority,
                   candidate.priority());
    report->AddString(StatsReport::kStatsValueNameCandidateType,
                      IceCandidateTypeToStatsType(candidate.type()));
    report->AddString(StatsReport::kStatsValueNameCandidateTransportType,
                      candidate.protocol());
  }

  return report;
}

void StatsCollector::ExtractSessionInfo() {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());

  // Extract information from the base session.
  StatsReport::Id id(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeSession, pc_->session()->id()));
  StatsReport* report = reports_.ReplaceOrAddNew(id);
  report->set_timestamp(stats_gathering_started_);
  report->AddBoolean(StatsReport::kStatsValueNameInitiator,
                     pc_->session()->initial_offerer());

  std::unique_ptr<SessionStats> stats = pc_->session()->GetStats_s();
  if (!stats) {
    return;
  }

  // Store the proxy map away for use in SSRC reporting.
  // TODO(tommi): This shouldn't be necessary if we post the stats back to the
  // signaling thread after fetching them on the worker thread, then just use
  // the proxy map directly from the session stats.
  // As is, if GetStats() failed, we could be using old (incorrect?) proxy
  // data.
  proxy_to_transport_ = stats->proxy_to_transport;

  for (const auto& transport_iter : stats->transport_stats) {
    // Attempt to get a copy of the certificates from the transport and
    // expose them in stats reports.  All channels in a transport share the
    // same local and remote certificates.
    //
    StatsReport::Id local_cert_report_id, remote_cert_report_id;
    rtc::scoped_refptr<rtc::RTCCertificate> certificate;
    if (pc_->session()->GetLocalCertificate(
            transport_iter.second.transport_name, &certificate)) {
      StatsReport* r = AddCertificateReports(&(certificate->ssl_certificate()));
      if (r)
        local_cert_report_id = r->id();
    }

    std::unique_ptr<rtc::SSLCertificate> cert =
        pc_->session()->GetRemoteSSLCertificate(
            transport_iter.second.transport_name);
    if (cert) {
      StatsReport* r = AddCertificateReports(cert.get());
      if (r)
        remote_cert_report_id = r->id();
    }

    for (const auto& channel_iter : transport_iter.second.channel_stats) {
      StatsReport::Id id(StatsReport::NewComponentId(
          transport_iter.second.transport_name, channel_iter.component));
      StatsReport* channel_report = reports_.ReplaceOrAddNew(id);
      channel_report->set_timestamp(stats_gathering_started_);
      channel_report->AddInt(StatsReport::kStatsValueNameComponent,
                             channel_iter.component);
      if (local_cert_report_id.get()) {
        channel_report->AddId(StatsReport::kStatsValueNameLocalCertificateId,
                              local_cert_report_id);
      }
      if (remote_cert_report_id.get()) {
        channel_report->AddId(StatsReport::kStatsValueNameRemoteCertificateId,
                              remote_cert_report_id);
      }
      int srtp_crypto_suite = channel_iter.srtp_crypto_suite;
      if (srtp_crypto_suite != rtc::SRTP_INVALID_CRYPTO_SUITE &&
          rtc::SrtpCryptoSuiteToName(srtp_crypto_suite).length()) {
        channel_report->AddString(
            StatsReport::kStatsValueNameSrtpCipher,
            rtc::SrtpCryptoSuiteToName(srtp_crypto_suite));
      }
      int ssl_cipher_suite = channel_iter.ssl_cipher_suite;
      if (ssl_cipher_suite != rtc::TLS_NULL_WITH_NULL_NULL &&
          rtc::SSLStreamAdapter::SslCipherSuiteToName(ssl_cipher_suite)
              .length()) {
        channel_report->AddString(
            StatsReport::kStatsValueNameDtlsCipher,
            rtc::SSLStreamAdapter::SslCipherSuiteToName(ssl_cipher_suite));
      }

      int connection_id = 0;
      for (const cricket::ConnectionInfo& info :
               channel_iter.connection_infos) {
        StatsReport* connection_report = AddConnectionInfoReport(
            transport_iter.first, channel_iter.component, connection_id++,
            channel_report->id(), info);
        if (info.best_connection) {
          channel_report->AddId(
              StatsReport::kStatsValueNameSelectedCandidatePairId,
              connection_report->id());
        }
      }
    }
  }
}

void StatsCollector::ExtractBweInfo() {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());

  if (pc_->session()->state() == WebRtcSession::State::STATE_CLOSED)
    return;

  webrtc::Call::Stats call_stats = pc_->session()->GetCallStats();
  cricket::BandwidthEstimationInfo bwe_info;
  bwe_info.available_send_bandwidth = call_stats.send_bandwidth_bps;
  bwe_info.available_recv_bandwidth = call_stats.recv_bandwidth_bps;
  bwe_info.bucket_delay = call_stats.pacer_delay_ms;
  // Fill in target encoder bitrate, actual encoder bitrate, rtx bitrate, etc.
  // TODO(holmer): Also fill this in for audio.
  if (pc_->session()->video_channel()) {
    pc_->session()->video_channel()->FillBitrateInfo(&bwe_info);
  }
  StatsReport::Id report_id(StatsReport::NewBandwidthEstimationId());
  StatsReport* report = reports_.FindOrAddNew(report_id);
  ExtractStats(bwe_info, stats_gathering_started_, report);
}

void StatsCollector::ExtractVoiceInfo() {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());

  if (!pc_->session()->voice_channel()) {
    return;
  }
  cricket::VoiceMediaInfo voice_info;
  if (!pc_->session()->voice_channel()->GetStats(&voice_info)) {
    LOG(LS_ERROR) << "Failed to get voice channel stats.";
    return;
  }

  // TODO(tommi): The above code should run on the worker thread and post the
  // results back to the signaling thread, where we can add data to the reports.
  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  StatsReport::Id transport_id(GetTransportIdFromProxy(
      proxy_to_transport_, pc_->session()->voice_channel()->content_name()));
  if (!transport_id.get()) {
    LOG(LS_ERROR) << "Failed to get transport name for proxy "
                  << pc_->session()->voice_channel()->content_name();
    return;
  }

  ExtractStatsFromList(voice_info.receivers, transport_id, this,
      StatsReport::kReceive);
  ExtractStatsFromList(voice_info.senders, transport_id, this,
      StatsReport::kSend);

  UpdateStatsFromExistingLocalAudioTracks();
}

void StatsCollector::ExtractVideoInfo(
    PeerConnectionInterface::StatsOutputLevel level) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());

  if (!pc_->session()->video_channel())
    return;

  cricket::VideoMediaInfo video_info;
  if (!pc_->session()->video_channel()->GetStats(&video_info)) {
    LOG(LS_ERROR) << "Failed to get video channel stats.";
    return;
  }

  // TODO(tommi): The above code should run on the worker thread and post the
  // results back to the signaling thread, where we can add data to the reports.
  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  StatsReport::Id transport_id(GetTransportIdFromProxy(
      proxy_to_transport_, pc_->session()->video_channel()->content_name()));
  if (!transport_id.get()) {
    LOG(LS_ERROR) << "Failed to get transport name for proxy "
                  << pc_->session()->video_channel()->content_name();
    return;
  }
  ExtractStatsFromList(video_info.receivers, transport_id, this,
      StatsReport::kReceive);
  ExtractStatsFromList(video_info.senders, transport_id, this,
      StatsReport::kSend);
}

void StatsCollector::ExtractSenderInfo() {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());

  for (const auto& sender : pc_->GetSenders()) {
    // TODO(nisse): SSRC == 0 currently means none. Delete check when
    // that is fixed.
    if (!sender->ssrc()) {
      continue;
    }
    const rtc::scoped_refptr<MediaStreamTrackInterface> track(sender->track());
    if (!track || track->kind() != MediaStreamTrackInterface::kVideoKind) {
      continue;
    }
    // Safe, because kind() == kVideoKind implies a subclass of
    // VideoTrackInterface; see mediastreaminterface.h.
    VideoTrackSourceInterface* source =
        static_cast<VideoTrackInterface*>(track.get())->GetSource();

    VideoTrackSourceInterface::Stats stats;
    if (!source->GetStats(&stats)) {
      continue;
    }
    const StatsReport::Id stats_id = StatsReport::NewIdWithDirection(
        StatsReport::kStatsReportTypeSsrc,
        rtc::ToString<uint32_t>(sender->ssrc()), StatsReport::kSend);
    StatsReport* report = reports_.FindOrAddNew(stats_id);
    report->AddInt(StatsReport::kStatsValueNameFrameWidthInput,
                   stats.input_width);
    report->AddInt(StatsReport::kStatsValueNameFrameHeightInput,
                   stats.input_height);
  }
}

void StatsCollector::ExtractDataInfo() {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());

  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  for (const auto& dc : pc_->sctp_data_channels()) {
    StatsReport::Id id(StatsReport::NewTypedIntId(
        StatsReport::kStatsReportTypeDataChannel, dc->id()));
    StatsReport* report = reports_.ReplaceOrAddNew(id);
    report->set_timestamp(stats_gathering_started_);
    report->AddString(StatsReport::kStatsValueNameLabel, dc->label());
    // Filter out the initial id (-1).
    if (dc->id() >= 0) {
      report->AddInt(StatsReport::kStatsValueNameDataChannelId, dc->id());
    }
    report->AddString(StatsReport::kStatsValueNameProtocol, dc->protocol());
    report->AddString(StatsReport::kStatsValueNameState,
                      DataChannelInterface::DataStateString(dc->state()));
  }
}

StatsReport* StatsCollector::GetReport(const StatsReport::StatsType& type,
                                       const std::string& id,
                                       StatsReport::Direction direction) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  RTC_DCHECK(type == StatsReport::kStatsReportTypeSsrc ||
             type == StatsReport::kStatsReportTypeRemoteSsrc);
  return reports_.Find(StatsReport::NewIdWithDirection(type, id, direction));
}

void StatsCollector::UpdateStatsFromExistingLocalAudioTracks() {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  // Loop through the existing local audio tracks.
  for (const auto& it : local_audio_tracks_) {
    AudioTrackInterface* track = it.first;
    uint32_t ssrc = it.second;
    StatsReport* report =
        GetReport(StatsReport::kStatsReportTypeSsrc,
                  rtc::ToString<uint32_t>(ssrc), StatsReport::kSend);
    if (report == NULL) {
      // This can happen if a local audio track is added to a stream on the
      // fly and the report has not been set up yet. Do nothing in this case.
      LOG(LS_ERROR) << "Stats report does not exist for ssrc " << ssrc;
      continue;
    }

    // The same ssrc can be used by both local and remote audio tracks.
    const StatsReport::Value* v =
        report->FindValue(StatsReport::kStatsValueNameTrackId);
    if (!v || v->string_val() != track->id())
      continue;

    report->set_timestamp(stats_gathering_started_);
    UpdateReportFromAudioTrack(track, report);
  }
}

void StatsCollector::UpdateReportFromAudioTrack(AudioTrackInterface* track,
                                                StatsReport* report) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  RTC_DCHECK(track != NULL);

  // Don't overwrite report values if they're not available.
  int signal_level;
  if (track->GetSignalLevel(&signal_level)) {
    RTC_DCHECK_GE(signal_level, 0);
    report->AddInt(StatsReport::kStatsValueNameAudioInputLevel, signal_level);
  }

  auto audio_processor(track->GetAudioProcessor());

  if (audio_processor.get()) {
    AudioProcessorInterface::AudioProcessorStats stats;
    audio_processor->GetStats(&stats);

    SetAudioProcessingStats(
        report, stats.typing_noise_detected, stats.echo_return_loss,
        stats.echo_return_loss_enhancement, stats.echo_delay_median_ms,
        stats.aec_quality_min, stats.echo_delay_std_ms,
        stats.residual_echo_likelihood,
        stats.residual_echo_likelihood_recent_max);

    report->AddFloat(StatsReport::kStatsValueNameAecDivergentFilterFraction,
                     stats.aec_divergent_filter_fraction);
  }
}

bool StatsCollector::GetTrackIdBySsrc(uint32_t ssrc,
                                      std::string* track_id,
                                      StatsReport::Direction direction) {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());
  if (direction == StatsReport::kSend) {
    if (!pc_->session()->GetLocalTrackIdBySsrc(ssrc, track_id)) {
      LOG(LS_WARNING) << "The SSRC " << ssrc
                      << " is not associated with a sending track";
      return false;
    }
  } else {
    RTC_DCHECK(direction == StatsReport::kReceive);
    if (!pc_->session()->GetRemoteTrackIdBySsrc(ssrc, track_id)) {
      LOG(LS_WARNING) << "The SSRC " << ssrc
                      << " is not associated with a receiving track";
      return false;
    }
  }

  return true;
}

void StatsCollector::UpdateTrackReports() {
  RTC_DCHECK(pc_->session()->signaling_thread()->IsCurrent());

  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  for (const auto& entry : track_ids_) {
    StatsReport* report = entry.second;
    report->set_timestamp(stats_gathering_started_);
  }
}

void StatsCollector::ClearUpdateStatsCacheForTest() {
  stats_gathering_started_ = 0;
}

}  // namespace webrtc
