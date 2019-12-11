/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/statscollector.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "pc/channel.h"
#include "pc/peerconnection.h"
#include "rtc_base/checks.h"
#include "rtc_base/third_party/base64/base64.h"

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
const char* STATSREPORT_ADAPTER_TYPE_WILDCARD = "wildcard";

template <typename ValueType>
struct TypeForAdd {
  const StatsReport::StatsValueName name;
  const ValueType& value;
};

typedef TypeForAdd<bool> BoolForAdd;
typedef TypeForAdd<float> FloatForAdd;
typedef TypeForAdd<int64_t> Int64ForAdd;
typedef TypeForAdd<int> IntForAdd;

StatsReport* AddTrackReport(StatsCollection* reports,
                            const std::string& track_id) {
  // Adds an empty track report.
  StatsReport::Id id(
      StatsReport::NewTypedId(StatsReport::kStatsReportTypeTrack, track_id));
  StatsReport* report = reports->ReplaceOrAddNew(id);
  report->AddString(StatsReport::kStatsValueNameTrackId, track_id);
  return report;
}

template <class Track>
void CreateTrackReport(const Track* track,
                       StatsCollection* reports,
                       TrackIdMap* track_ids) {
  const std::string& track_id = track->id();
  StatsReport* report = AddTrackReport(reports, track_id);
  RTC_DCHECK(report != nullptr);
  (*track_ids)[track_id] = report;
}

template <class TrackVector>
void CreateTrackReports(const TrackVector& tracks,
                        StatsCollection* reports,
                        TrackIdMap* track_ids) {
  for (const auto& track : tracks) {
    CreateTrackReport(track.get(), reports, track_ids);
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
                             const AudioProcessingStats& apm_stats) {
  report->AddBoolean(StatsReport::kStatsValueNameTypingNoiseState,
                     typing_noise_detected);
  if (apm_stats.delay_median_ms) {
    report->AddInt(StatsReport::kStatsValueNameEchoDelayMedian,
                   *apm_stats.delay_median_ms);
  }
  if (apm_stats.delay_standard_deviation_ms) {
    report->AddInt(StatsReport::kStatsValueNameEchoDelayStdDev,
                   *apm_stats.delay_standard_deviation_ms);
  }
  if (apm_stats.echo_return_loss) {
    report->AddInt(StatsReport::kStatsValueNameEchoReturnLoss,
                   *apm_stats.echo_return_loss);
  }
  if (apm_stats.echo_return_loss_enhancement) {
    report->AddInt(StatsReport::kStatsValueNameEchoReturnLossEnhancement,
                   *apm_stats.echo_return_loss_enhancement);
  }
  if (apm_stats.residual_echo_likelihood) {
    report->AddFloat(StatsReport::kStatsValueNameResidualEchoLikelihood,
                     static_cast<float>(*apm_stats.residual_echo_likelihood));
  }
  if (apm_stats.residual_echo_likelihood_recent_max) {
    report->AddFloat(
        StatsReport::kStatsValueNameResidualEchoLikelihoodRecentMax,
        static_cast<float>(*apm_stats.residual_echo_likelihood_recent_max));
  }
  if (apm_stats.divergent_filter_fraction) {
    report->AddFloat(StatsReport::kStatsValueNameAecDivergentFilterFraction,
                     static_cast<float>(*apm_stats.divergent_filter_fraction));
  }
}

void ExtractStats(const cricket::VoiceReceiverInfo& info, StatsReport* report) {
  ExtractCommonReceiveProperties(info, report);
  const FloatForAdd floats[] = {
      {StatsReport::kStatsValueNameExpandRate, info.expand_rate},
      {StatsReport::kStatsValueNameSecondaryDecodedRate,
       info.secondary_decoded_rate},
      {StatsReport::kStatsValueNameSecondaryDiscardedRate,
       info.secondary_discarded_rate},
      {StatsReport::kStatsValueNameSpeechExpandRate, info.speech_expand_rate},
      {StatsReport::kStatsValueNameAccelerateRate, info.accelerate_rate},
      {StatsReport::kStatsValueNamePreemptiveExpandRate,
       info.preemptive_expand_rate},
      {StatsReport::kStatsValueNameTotalAudioEnergy, info.total_output_energy},
      {StatsReport::kStatsValueNameTotalSamplesDuration,
       info.total_output_duration}};

  const IntForAdd ints[] = {
      {StatsReport::kStatsValueNameCurrentDelayMs, info.delay_estimate_ms},
      {StatsReport::kStatsValueNameDecodingCNG, info.decoding_cng},
      {StatsReport::kStatsValueNameDecodingCTN, info.decoding_calls_to_neteq},
      {StatsReport::kStatsValueNameDecodingCTSG,
       info.decoding_calls_to_silence_generator},
      {StatsReport::kStatsValueNameDecodingMutedOutput,
       info.decoding_muted_output},
      {StatsReport::kStatsValueNameDecodingNormal, info.decoding_normal},
      {StatsReport::kStatsValueNameDecodingPLC, info.decoding_plc},
      {StatsReport::kStatsValueNameDecodingPLCCNG, info.decoding_plc_cng},
      {StatsReport::kStatsValueNameJitterBufferMs, info.jitter_buffer_ms},
      {StatsReport::kStatsValueNameJitterReceived, info.jitter_ms},
      {StatsReport::kStatsValueNamePacketsLost, info.packets_lost},
      {StatsReport::kStatsValueNamePacketsReceived, info.packets_rcvd},
      {StatsReport::kStatsValueNamePreferredJitterBufferMs,
       info.jitter_buffer_preferred_ms},
  };

  for (const auto& f : floats)
    report->AddFloat(f.name, f.value);

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
  if (info.audio_level >= 0) {
    report->AddInt(StatsReport::kStatsValueNameAudioOutputLevel,
                   info.audio_level);
  }

  report->AddInt64(StatsReport::kStatsValueNameBytesReceived, info.bytes_rcvd);
  if (info.capture_start_ntp_time_ms >= 0) {
    report->AddInt64(StatsReport::kStatsValueNameCaptureStartNtpTimeMs,
                     info.capture_start_ntp_time_ms);
  }
  report->AddString(StatsReport::kStatsValueNameMediaType, "audio");
}

void ExtractStats(const cricket::VoiceSenderInfo& info, StatsReport* report) {
  ExtractCommonSendProperties(info, report);

  SetAudioProcessingStats(report, info.typing_noise_detected,
                          info.apm_statistics);

  const FloatForAdd floats[] = {
      {StatsReport::kStatsValueNameTotalAudioEnergy, info.total_input_energy},
      {StatsReport::kStatsValueNameTotalSamplesDuration,
       info.total_input_duration}};

  RTC_DCHECK_GE(info.audio_level, 0);
  const IntForAdd ints[] = {
      {StatsReport::kStatsValueNameAudioInputLevel, info.audio_level},
      {StatsReport::kStatsValueNameJitterReceived, info.jitter_ms},
      {StatsReport::kStatsValueNamePacketsLost, info.packets_lost},
      {StatsReport::kStatsValueNamePacketsSent, info.packets_sent},
  };

  for (const auto& f : floats) {
    report->AddFloat(f.name, f.value);
  }

  for (const auto& i : ints) {
    if (i.value >= 0) {
      report->AddInt(i.name, i.value);
    }
  }
  report->AddString(StatsReport::kStatsValueNameMediaType, "audio");
  if (info.ana_statistics.bitrate_action_counter) {
    report->AddInt(StatsReport::kStatsValueNameAnaBitrateActionCounter,
                   *info.ana_statistics.bitrate_action_counter);
  }
  if (info.ana_statistics.channel_action_counter) {
    report->AddInt(StatsReport::kStatsValueNameAnaChannelActionCounter,
                   *info.ana_statistics.channel_action_counter);
  }
  if (info.ana_statistics.dtx_action_counter) {
    report->AddInt(StatsReport::kStatsValueNameAnaDtxActionCounter,
                   *info.ana_statistics.dtx_action_counter);
  }
  if (info.ana_statistics.fec_action_counter) {
    report->AddInt(StatsReport::kStatsValueNameAnaFecActionCounter,
                   *info.ana_statistics.fec_action_counter);
  }
  if (info.ana_statistics.frame_length_increase_counter) {
    report->AddInt(StatsReport::kStatsValueNameAnaFrameLengthIncreaseCounter,
                   *info.ana_statistics.frame_length_increase_counter);
  }
  if (info.ana_statistics.frame_length_decrease_counter) {
    report->AddInt(StatsReport::kStatsValueNameAnaFrameLengthDecreaseCounter,
                   *info.ana_statistics.frame_length_decrease_counter);
  }
  if (info.ana_statistics.uplink_packet_loss_fraction) {
    report->AddFloat(StatsReport::kStatsValueNameAnaUplinkPacketLossFraction,
                     *info.ana_statistics.uplink_packet_loss_fraction);
  }
}

void ExtractStats(const cricket::VideoReceiverInfo& info, StatsReport* report) {
  ExtractCommonReceiveProperties(info, report);
  report->AddString(StatsReport::kStatsValueNameCodecImplementationName,
                    info.decoder_implementation_name);
  report->AddInt64(StatsReport::kStatsValueNameBytesReceived, info.bytes_rcvd);
  if (info.capture_start_ntp_time_ms >= 0) {
    report->AddInt64(StatsReport::kStatsValueNameCaptureStartNtpTimeMs,
                     info.capture_start_ntp_time_ms);
  }
  if (info.qp_sum)
    report->AddInt64(StatsReport::kStatsValueNameQpSum, *info.qp_sum);

  const IntForAdd ints[] = {
      {StatsReport::kStatsValueNameCurrentDelayMs, info.current_delay_ms},
      {StatsReport::kStatsValueNameDecodeMs, info.decode_ms},
      {StatsReport::kStatsValueNameFirsSent, info.firs_sent},
      {StatsReport::kStatsValueNameFrameHeightReceived, info.frame_height},
      {StatsReport::kStatsValueNameFrameRateDecoded, info.framerate_decoded},
      {StatsReport::kStatsValueNameFrameRateOutput, info.framerate_output},
      {StatsReport::kStatsValueNameFrameRateReceived, info.framerate_rcvd},
      {StatsReport::kStatsValueNameFrameWidthReceived, info.frame_width},
      {StatsReport::kStatsValueNameJitterBufferMs, info.jitter_buffer_ms},
      {StatsReport::kStatsValueNameMaxDecodeMs, info.max_decode_ms},
      {StatsReport::kStatsValueNameMinPlayoutDelayMs,
       info.min_playout_delay_ms},
      {StatsReport::kStatsValueNameNacksSent, info.nacks_sent},
      {StatsReport::kStatsValueNamePacketsLost, info.packets_lost},
      {StatsReport::kStatsValueNamePacketsReceived, info.packets_rcvd},
      {StatsReport::kStatsValueNamePlisSent, info.plis_sent},
      {StatsReport::kStatsValueNameRenderDelayMs, info.render_delay_ms},
      {StatsReport::kStatsValueNameTargetDelayMs, info.target_delay_ms},
      {StatsReport::kStatsValueNameFramesDecoded, info.frames_decoded},
  };

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
  report->AddString(StatsReport::kStatsValueNameMediaType, "video");

  if (info.timing_frame_info) {
    report->AddString(StatsReport::kStatsValueNameTimingFrameInfo,
                      info.timing_frame_info->ToString());
  }

  report->AddInt64(StatsReport::kStatsValueNameInterframeDelayMaxMs,
                   info.interframe_delay_max_ms);

  report->AddString(
      StatsReport::kStatsValueNameContentType,
      webrtc::videocontenttypehelpers::ToString(info.content_type));
}

void ExtractStats(const cricket::VideoSenderInfo& info, StatsReport* report) {
  ExtractCommonSendProperties(info, report);

  report->AddString(StatsReport::kStatsValueNameCodecImplementationName,
                    info.encoder_implementation_name);
  report->AddBoolean(StatsReport::kStatsValueNameBandwidthLimitedResolution,
                     (info.adapt_reason & 0x2) > 0);
  report->AddBoolean(StatsReport::kStatsValueNameCpuLimitedResolution,
                     (info.adapt_reason & 0x1) > 0);
  report->AddBoolean(StatsReport::kStatsValueNameHasEnteredLowResolution,
                     info.has_entered_low_resolution);

  if (info.qp_sum)
    report->AddInt(StatsReport::kStatsValueNameQpSum, *info.qp_sum);

  const IntForAdd ints[] = {
      {StatsReport::kStatsValueNameAdaptationChanges, info.adapt_changes},
      {StatsReport::kStatsValueNameAvgEncodeMs, info.avg_encode_ms},
      {StatsReport::kStatsValueNameEncodeUsagePercent,
       info.encode_usage_percent},
      {StatsReport::kStatsValueNameFirsReceived, info.firs_rcvd},
      {StatsReport::kStatsValueNameFrameHeightSent, info.send_frame_height},
      {StatsReport::kStatsValueNameFrameRateInput, info.framerate_input},
      {StatsReport::kStatsValueNameFrameRateSent, info.framerate_sent},
      {StatsReport::kStatsValueNameFrameWidthSent, info.send_frame_width},
      {StatsReport::kStatsValueNameNacksReceived, info.nacks_rcvd},
      {StatsReport::kStatsValueNamePacketsLost, info.packets_lost},
      {StatsReport::kStatsValueNamePacketsSent, info.packets_sent},
      {StatsReport::kStatsValueNamePlisReceived, info.plis_rcvd},
      {StatsReport::kStatsValueNameFramesEncoded, info.frames_encoded},
      {StatsReport::kStatsValueNameHugeFramesSent, info.huge_frames_sent},
  };

  for (const auto& i : ints)
    report->AddInt(i.name, i.value);
  report->AddString(StatsReport::kStatsValueNameMediaType, "video");
  report->AddString(
      StatsReport::kStatsValueNameContentType,
      webrtc::videocontenttypehelpers::ToString(info.content_type));
}

void ExtractStats(const cricket::BandwidthEstimationInfo& info,
                  double stats_gathering_started,
                  StatsReport* report) {
  RTC_DCHECK(report->type() == StatsReport::kStatsReportTypeBwe);

  report->set_timestamp(stats_gathering_started);
  const IntForAdd ints[] = {
      {StatsReport::kStatsValueNameAvailableSendBandwidth,
       info.available_send_bandwidth},
      {StatsReport::kStatsValueNameAvailableReceiveBandwidth,
       info.available_recv_bandwidth},
      {StatsReport::kStatsValueNameTargetEncBitrate, info.target_enc_bitrate},
      {StatsReport::kStatsValueNameActualEncBitrate, info.actual_enc_bitrate},
      {StatsReport::kStatsValueNameRetransmitBitrate, info.retransmit_bitrate},
      {StatsReport::kStatsValueNameTransmitBitrate, info.transmit_bitrate},
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
template <typename T>
void ExtractStatsFromList(const std::vector<T>& data,
                          const StatsReport::Id& transport_id,
                          StatsCollector* collector,
                          StatsReport::Direction direction) {
  for (const auto& d : data) {
    uint32_t ssrc = d.ssrc();
    // Each track can have stats for both local and remote objects.
    // TODO(hta): Handle the case of multiple SSRCs per object.
    StatsReport* report =
        collector->PrepareReport(true, ssrc, transport_id, direction);
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
    case rtc::ADAPTER_TYPE_ANY:
      return STATSREPORT_ADAPTER_TYPE_WILDCARD;
    default:
      RTC_NOTREACHED();
      return "";
  }
}

StatsCollector::StatsCollector(PeerConnectionInternal* pc)
    : pc_(pc), stats_gathering_started_(0) {
  RTC_DCHECK(pc_);
}

StatsCollector::~StatsCollector() {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
}

// Wallclock time in ms.
double StatsCollector::GetTimeNow() {
  return static_cast<double>(rtc::TimeUTCMillis());
}

// Adds a MediaStream with tracks that can be used as a |selector| in a call
// to GetStats.
void StatsCollector::AddStream(MediaStreamInterface* stream) {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
  RTC_DCHECK(stream != NULL);

  CreateTrackReports<AudioTrackVector>(stream->GetAudioTracks(), &reports_,
                                       &track_ids_);
  CreateTrackReports<VideoTrackVector>(stream->GetVideoTracks(), &reports_,
                                       &track_ids_);
}

void StatsCollector::AddTrack(MediaStreamTrackInterface* track) {
  if (track->kind() == MediaStreamTrackInterface::kAudioKind) {
    CreateTrackReport(static_cast<AudioTrackInterface*>(track), &reports_,
                      &track_ids_);
  } else if (track->kind() == MediaStreamTrackInterface::kVideoKind) {
    CreateTrackReport(static_cast<VideoTrackInterface*>(track), &reports_,
                      &track_ids_);
  } else {
    RTC_NOTREACHED() << "Illegal track kind";
  }
}

void StatsCollector::AddLocalAudioTrack(AudioTrackInterface* audio_track,
                                        uint32_t ssrc) {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
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
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
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
      StatsReport::kStatsReportTypeSession, pc_->session_id()));
  if (report)
    reports->push_back(report);

  report = reports_.Find(
      StatsReport::NewTypedId(StatsReport::kStatsReportTypeTrack, track->id()));

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

void StatsCollector::UpdateStats(
    PeerConnectionInterface::StatsOutputLevel level) {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
  double time_now = GetTimeNow();
  // Calls to UpdateStats() that occur less than kMinGatherStatsPeriod number of
  // ms apart will be ignored.
  const double kMinGatherStatsPeriod = 50;
  if (stats_gathering_started_ != 0 &&
      stats_gathering_started_ + kMinGatherStatsPeriod > time_now) {
    return;
  }
  stats_gathering_started_ = time_now;

  // TODO(tommi): All of these hop over to the worker thread to fetch
  // information.  We could use an AsyncInvoker to run all of these and post
  // the information back to the signaling thread where we can create and
  // update stats reports.  That would also clean up the threading story a bit
  // since we'd be creating/updating the stats report objects consistently on
  // the same thread (this class has no locks right now).
  ExtractSessionInfo();
  ExtractBweInfo();
  ExtractMediaInfo();
  ExtractSenderInfo();
  ExtractDataInfo();
  UpdateTrackReports();
}

StatsReport* StatsCollector::PrepareReport(bool local,
                                           uint32_t ssrc,
                                           const StatsReport::Id& transport_id,
                                           StatsReport::Direction direction) {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
  StatsReport::Id id(StatsReport::NewIdWithDirection(
      local ? StatsReport::kStatsReportTypeSsrc
            : StatsReport::kStatsReportTypeRemoteSsrc,
      rtc::ToString(ssrc), direction));
  StatsReport* report = reports_.Find(id);

  // Use the ID of the track that is currently mapped to the SSRC, if any.
  absl::string_view track_id = GetTrackIdBySsrc(ssrc, direction);
  if (track_id.empty()) {
    // The SSRC is not used by any existing track (or lookup failed since the
    // SSRC wasn't signaled in SDP). Try copying the track ID from a previous
    // report: if one exists.
    if (report) {
      const StatsReport::Value* v =
          report->FindValue(StatsReport::kStatsValueNameTrackId);
      if (v) {
        track_id = v->string_val();
      }
    }
  }

  if (!report) {
    report = reports_.InsertNew(id);
  }

  // FYI - for remote reports, the timestamp will be overwritten later.
  report->set_timestamp(stats_gathering_started_);

  report->AddInt64(StatsReport::kStatsValueNameSsrc, ssrc);
  if (!track_id.empty()) {
    report->AddString(StatsReport::kStatsValueNameTrackId,
                      std::string(track_id));
  }
  // Add the mapping of SSRC to transport.
  report->AddId(StatsReport::kStatsValueNameTransportId, transport_id);
  return report;
}

bool StatsCollector::IsValidTrack(const std::string& track_id) {
  return reports_.Find(StatsReport::NewTypedId(
             StatsReport::kStatsReportTypeTrack, track_id)) != nullptr;
}

StatsReport* StatsCollector::AddCertificateReports(
    std::unique_ptr<rtc::SSLCertificateStats> cert_stats) {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());

  StatsReport* first_report = nullptr;
  StatsReport* prev_report = nullptr;
  for (rtc::SSLCertificateStats* stats = cert_stats.get(); stats;
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
    const std::string& content_name,
    int component,
    int connection_id,
    const StatsReport::Id& channel_report_id,
    const cricket::ConnectionInfo& info) {
  StatsReport::Id id(
      StatsReport::NewCandidatePairId(content_name, component, connection_id));
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
  cricket::CandidateStats local_candidate_stats(info.local_candidate);
  cricket::CandidateStats remote_candidate_stats(info.remote_candidate);
  report->AddId(StatsReport::kStatsValueNameLocalCandidateId,
                AddCandidateReport(local_candidate_stats, true)->id());
  report->AddId(StatsReport::kStatsValueNameRemoteCandidateId,
                AddCandidateReport(remote_candidate_stats, false)->id());

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
    const cricket::CandidateStats& candidate_stats,
    bool local) {
  const auto& candidate = candidate_stats.candidate;
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

  if (local && candidate_stats.stun_stats.has_value()) {
    const auto& stun_stats = candidate_stats.stun_stats.value();
    report->AddInt64(StatsReport::kStatsValueNameSentStunKeepaliveRequests,
                     stun_stats.stun_binding_requests_sent);
    report->AddInt64(StatsReport::kStatsValueNameRecvStunKeepaliveResponses,
                     stun_stats.stun_binding_responses_received);
    report->AddFloat(StatsReport::kStatsValueNameStunKeepaliveRttTotal,
                     stun_stats.stun_binding_rtt_ms_total);
    report->AddFloat(StatsReport::kStatsValueNameStunKeepaliveRttSquaredTotal,
                     stun_stats.stun_binding_rtt_ms_squared_total);
  }

  return report;
}

void StatsCollector::ExtractSessionInfo() {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());

  // Extract information from the base session.
  StatsReport::Id id(StatsReport::NewTypedId(
      StatsReport::kStatsReportTypeSession, pc_->session_id()));
  StatsReport* report = reports_.ReplaceOrAddNew(id);
  report->set_timestamp(stats_gathering_started_);
  report->AddBoolean(StatsReport::kStatsValueNameInitiator,
                     pc_->initial_offerer());

  cricket::CandidateStatsList pooled_candidate_stats_list =
      pc_->GetPooledCandidateStats();

  for (const cricket::CandidateStats& stats : pooled_candidate_stats_list) {
    AddCandidateReport(stats, true);
  }

  std::set<std::string> transport_names;
  for (const auto& entry : pc_->GetTransportNamesByMid()) {
    transport_names.insert(entry.second);
  }

  std::map<std::string, cricket::TransportStats> transport_stats_by_name =
      pc_->GetTransportStatsByNames(transport_names);

  for (const auto& entry : transport_stats_by_name) {
    const std::string& transport_name = entry.first;
    const cricket::TransportStats& transport_stats = entry.second;

    // Attempt to get a copy of the certificates from the transport and
    // expose them in stats reports.  All channels in a transport share the
    // same local and remote certificates.
    //
    StatsReport::Id local_cert_report_id, remote_cert_report_id;
    rtc::scoped_refptr<rtc::RTCCertificate> certificate;
    if (pc_->GetLocalCertificate(transport_name, &certificate)) {
      StatsReport* r = AddCertificateReports(
          certificate->GetSSLCertificateChain().GetStats());
      if (r)
        local_cert_report_id = r->id();
    }

    std::unique_ptr<rtc::SSLCertChain> remote_cert_chain =
        pc_->GetRemoteSSLCertChain(transport_name);
    if (remote_cert_chain) {
      StatsReport* r = AddCertificateReports(remote_cert_chain->GetStats());
      if (r)
        remote_cert_report_id = r->id();
    }

    for (const auto& channel_iter : transport_stats.channel_stats) {
      StatsReport::Id id(
          StatsReport::NewComponentId(transport_name, channel_iter.component));
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

      // Collect stats for non-pooled candidates. Note that the reports
      // generated here supersedes the candidate reports generated in
      // AddConnectionInfoReport below, and they may report candidates that are
      // not paired. Also, the candidate report generated in
      // AddConnectionInfoReport do not report port stats like StunStats.
      for (const cricket::CandidateStats& stats :
           channel_iter.candidate_stats_list) {
        AddCandidateReport(stats, true);
      }

      int connection_id = 0;
      for (const cricket::ConnectionInfo& info :
           channel_iter.connection_infos) {
        StatsReport* connection_report = AddConnectionInfoReport(
            transport_name, channel_iter.component, connection_id++,
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
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());

  if (pc_->signaling_state() == PeerConnectionInterface::kClosed)
    return;

  webrtc::Call::Stats call_stats = pc_->GetCallStats();
  cricket::BandwidthEstimationInfo bwe_info;
  bwe_info.available_send_bandwidth = call_stats.send_bandwidth_bps;
  bwe_info.available_recv_bandwidth = call_stats.recv_bandwidth_bps;
  bwe_info.bucket_delay = call_stats.pacer_delay_ms;

  // Fill in target encoder bitrate, actual encoder bitrate, rtx bitrate, etc.
  // TODO(holmer): Also fill this in for audio.
  for (const auto& transceiver : pc_->GetTransceiversInternal()) {
    if (transceiver->media_type() != cricket::MEDIA_TYPE_VIDEO) {
      continue;
    }
    auto* video_channel =
        static_cast<cricket::VideoChannel*>(transceiver->internal()->channel());
    if (!video_channel) {
      continue;
    }
    video_channel->FillBitrateInfo(&bwe_info);
  }

  StatsReport::Id report_id(StatsReport::NewBandwidthEstimationId());
  StatsReport* report = reports_.FindOrAddNew(report_id);
  ExtractStats(bwe_info, stats_gathering_started_, report);
}

namespace {

struct VoiceChannelStatsInfo {
  std::string transport_name;
  cricket::VoiceMediaChannel* voice_media_channel;
  cricket::VoiceMediaInfo voice_media_info;
};

struct VideoChannelStatsInfo {
  std::string transport_name;
  cricket::VideoMediaChannel* video_media_channel;
  cricket::VideoMediaInfo video_media_info;
};

}  // namespace

void StatsCollector::ExtractMediaInfo() {
  RTC_DCHECK_RUN_ON(pc_->signaling_thread());

  std::vector<VoiceChannelStatsInfo> voice_channel_infos;
  std::vector<VideoChannelStatsInfo> video_channel_infos;

  {
    rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;
    for (const auto& transceiver : pc_->GetTransceiversInternal()) {
      if (!transceiver->internal()->channel()) {
        continue;
      }
      cricket::MediaType media_type = transceiver->internal()->media_type();
      if (media_type == cricket::MEDIA_TYPE_AUDIO) {
        auto* voice_channel = static_cast<cricket::VoiceChannel*>(
            transceiver->internal()->channel());
        voice_channel_infos.emplace_back();
        VoiceChannelStatsInfo& info = voice_channel_infos.back();
        info.transport_name = voice_channel->transport_name();
        info.voice_media_channel = voice_channel->media_channel();
      } else {
        RTC_DCHECK_EQ(media_type, cricket::MEDIA_TYPE_VIDEO);
        auto* video_channel = static_cast<cricket::VideoChannel*>(
            transceiver->internal()->channel());
        video_channel_infos.emplace_back();
        VideoChannelStatsInfo& info = video_channel_infos.back();
        info.transport_name = video_channel->transport_name();
        info.video_media_channel = video_channel->media_channel();
      }
    }
  }

  pc_->worker_thread()->Invoke<void>(RTC_FROM_HERE, [&] {
    rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;
    for (auto it = voice_channel_infos.begin(); it != voice_channel_infos.end();
         /* incremented manually */) {
      if (!it->voice_media_channel->GetStats(&it->voice_media_info)) {
        RTC_LOG(LS_ERROR) << "Failed to get voice channel stats";
        it = voice_channel_infos.erase(it);
        continue;
      }
      ++it;
    }
    for (auto it = video_channel_infos.begin(); it != video_channel_infos.end();
         /* incremented manually */) {
      if (!it->video_media_channel->GetStats(&it->video_media_info)) {
        RTC_LOG(LS_ERROR) << "Failed to get video channel stats";
        it = video_channel_infos.erase(it);
        continue;
      }
      ++it;
    }
  });

  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  bool has_remote_audio = false;
  for (const auto& info : voice_channel_infos) {
    StatsReport::Id transport_id = StatsReport::NewComponentId(
        info.transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
    ExtractStatsFromList(info.voice_media_info.receivers, transport_id, this,
                         StatsReport::kReceive);
    ExtractStatsFromList(info.voice_media_info.senders, transport_id, this,
                         StatsReport::kSend);
    if (!info.voice_media_info.receivers.empty()) {
      has_remote_audio = true;
    }
  }
  for (const auto& info : video_channel_infos) {
    StatsReport::Id transport_id = StatsReport::NewComponentId(
        info.transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
    ExtractStatsFromList(info.video_media_info.receivers, transport_id, this,
                         StatsReport::kReceive);
    ExtractStatsFromList(info.video_media_info.senders, transport_id, this,
                         StatsReport::kSend);
  }

  UpdateStatsFromExistingLocalAudioTracks(has_remote_audio);
}

void StatsCollector::ExtractSenderInfo() {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());

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
        StatsReport::kStatsReportTypeSsrc, rtc::ToString(sender->ssrc()),
        StatsReport::kSend);
    StatsReport* report = reports_.FindOrAddNew(stats_id);
    report->AddInt(StatsReport::kStatsValueNameFrameWidthInput,
                   stats.input_width);
    report->AddInt(StatsReport::kStatsValueNameFrameHeightInput,
                   stats.input_height);
  }
}

void StatsCollector::ExtractDataInfo() {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());

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
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
  RTC_DCHECK(type == StatsReport::kStatsReportTypeSsrc ||
             type == StatsReport::kStatsReportTypeRemoteSsrc);
  return reports_.Find(StatsReport::NewIdWithDirection(type, id, direction));
}

void StatsCollector::UpdateStatsFromExistingLocalAudioTracks(
    bool has_remote_tracks) {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
  // Loop through the existing local audio tracks.
  for (const auto& it : local_audio_tracks_) {
    AudioTrackInterface* track = it.first;
    uint32_t ssrc = it.second;
    StatsReport* report = GetReport(StatsReport::kStatsReportTypeSsrc,
                                    rtc::ToString(ssrc), StatsReport::kSend);
    if (report == NULL) {
      // This can happen if a local audio track is added to a stream on the
      // fly and the report has not been set up yet. Do nothing in this case.
      RTC_LOG(LS_ERROR) << "Stats report does not exist for ssrc " << ssrc;
      continue;
    }

    // The same ssrc can be used by both local and remote audio tracks.
    const StatsReport::Value* v =
        report->FindValue(StatsReport::kStatsValueNameTrackId);
    if (!v || v->string_val() != track->id())
      continue;

    report->set_timestamp(stats_gathering_started_);
    UpdateReportFromAudioTrack(track, report, has_remote_tracks);
  }
}

void StatsCollector::UpdateReportFromAudioTrack(AudioTrackInterface* track,
                                                StatsReport* report,
                                                bool has_remote_tracks) {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
  RTC_DCHECK(track != NULL);

  // Don't overwrite report values if they're not available.
  int signal_level;
  if (track->GetSignalLevel(&signal_level)) {
    RTC_DCHECK_GE(signal_level, 0);
    report->AddInt(StatsReport::kStatsValueNameAudioInputLevel, signal_level);
  }

  auto audio_processor(track->GetAudioProcessor());

  if (audio_processor.get()) {
    AudioProcessorInterface::AudioProcessorStatistics stats =
        audio_processor->GetStats(has_remote_tracks);

    SetAudioProcessingStats(report, stats.typing_noise_detected,
                            stats.apm_statistics);
  }
}

absl::string_view StatsCollector::GetTrackIdBySsrc(
    uint32_t ssrc,
    StatsReport::Direction direction) {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());
  absl::string_view track_id;
  if (direction == StatsReport::kSend) {
    track_id = pc_->GetLocalTrackIdBySsrc(ssrc);
  } else {
    RTC_DCHECK_EQ(direction, StatsReport::kReceive);
    track_id = pc_->GetRemoteTrackIdBySsrc(ssrc);
  }
  if (track_id.empty()) {
    RTC_LOG(LS_WARNING) << "The SSRC " << ssrc << " is not associated with a "
                        << (direction == StatsReport::kSend ? "sending"
                                                            : "receiving")
                        << " track";
  }
  return track_id;
}

void StatsCollector::UpdateTrackReports() {
  RTC_DCHECK(pc_->signaling_thread()->IsCurrent());

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
