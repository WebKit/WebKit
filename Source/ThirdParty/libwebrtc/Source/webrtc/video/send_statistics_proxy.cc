/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/send_statistics_proxy.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
const float kEncodeTimeWeigthFactor = 0.5f;

// Used by histograms. Values of entries should not be changed.
enum HistogramCodecType {
  kVideoUnknown = 0,
  kVideoVp8 = 1,
  kVideoVp9 = 2,
  kVideoH264 = 3,
  kVideoMax = 64,
};

const char* kRealtimePrefix = "WebRTC.Video.";
const char* kScreenPrefix = "WebRTC.Video.Screenshare.";

const char* GetUmaPrefix(VideoEncoderConfig::ContentType content_type) {
  switch (content_type) {
    case VideoEncoderConfig::ContentType::kRealtimeVideo:
      return kRealtimePrefix;
    case VideoEncoderConfig::ContentType::kScreen:
      return kScreenPrefix;
  }
  RTC_NOTREACHED();
  return nullptr;
}

HistogramCodecType PayloadNameToHistogramCodecType(
    const std::string& payload_name) {
  if (payload_name == "VP8") {
    return kVideoVp8;
  } else if (payload_name == "VP9") {
    return kVideoVp9;
  } else if (payload_name == "H264") {
    return kVideoH264;
  } else {
    return kVideoUnknown;
  }
}

void UpdateCodecTypeHistogram(const std::string& payload_name) {
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.Encoder.CodecType",
                            PayloadNameToHistogramCodecType(payload_name),
                            kVideoMax);
}
}  // namespace


const int SendStatisticsProxy::kStatsTimeoutMs = 5000;

SendStatisticsProxy::SendStatisticsProxy(
    Clock* clock,
    const VideoSendStream::Config& config,
    VideoEncoderConfig::ContentType content_type)
    : clock_(clock),
      payload_name_(config.encoder_settings.payload_name),
      rtp_config_(config.rtp),
      content_type_(content_type),
      start_ms_(clock->TimeInMilliseconds()),
      last_sent_frame_timestamp_(0),
      encode_time_(kEncodeTimeWeigthFactor),
      uma_container_(
          new UmaSamplesContainer(GetUmaPrefix(content_type_), stats_, clock)) {
}

SendStatisticsProxy::~SendStatisticsProxy() {
  rtc::CritScope lock(&crit_);
  uma_container_->UpdateHistograms(rtp_config_, stats_);

  int64_t elapsed_sec = (clock_->TimeInMilliseconds() - start_ms_) / 1000;
  RTC_HISTOGRAM_COUNTS_100000("WebRTC.Video.SendStreamLifetimeInSeconds",
                              elapsed_sec);

  if (elapsed_sec >= metrics::kMinRunTimeInSeconds)
    UpdateCodecTypeHistogram(payload_name_);
}

SendStatisticsProxy::UmaSamplesContainer::UmaSamplesContainer(
    const char* prefix,
    const VideoSendStream::Stats& stats,
    Clock* const clock)
    : uma_prefix_(prefix),
      clock_(clock),
      max_sent_width_per_timestamp_(0),
      max_sent_height_per_timestamp_(0),
      input_frame_rate_tracker_(100, 10u),
      sent_frame_rate_tracker_(100, 10u),
      first_rtcp_stats_time_ms_(-1),
      first_rtp_stats_time_ms_(-1),
      start_stats_(stats) {}

SendStatisticsProxy::UmaSamplesContainer::~UmaSamplesContainer() {}

void AccumulateRtxStats(const VideoSendStream::Stats& stats,
                        const std::vector<uint32_t>& rtx_ssrcs,
                        StreamDataCounters* total_rtp_stats,
                        StreamDataCounters* rtx_stats) {
  for (auto it : stats.substreams) {
    if (std::find(rtx_ssrcs.begin(), rtx_ssrcs.end(), it.first) !=
        rtx_ssrcs.end()) {
      rtx_stats->Add(it.second.rtp_stats);
    } else {
      total_rtp_stats->Add(it.second.rtp_stats);
    }
  }
}

void SendStatisticsProxy::UmaSamplesContainer::UpdateHistograms(
    const VideoSendStream::Config::Rtp& rtp_config,
    const VideoSendStream::Stats& current_stats) {
  RTC_DCHECK(uma_prefix_ == kRealtimePrefix || uma_prefix_ == kScreenPrefix);
  const int kIndex = uma_prefix_ == kScreenPrefix ? 1 : 0;
  int in_width = input_width_counter_.Avg(kMinRequiredMetricsSamples);
  int in_height = input_height_counter_.Avg(kMinRequiredMetricsSamples);
  int in_fps = round(input_frame_rate_tracker_.ComputeTotalRate());
  if (in_width != -1) {
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "InputWidthInPixels",
                                in_width);
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "InputHeightInPixels",
                                in_height);
    RTC_HISTOGRAMS_COUNTS_100(kIndex, uma_prefix_ + "InputFramesPerSecond",
                              in_fps);
  }
  int sent_width = sent_width_counter_.Avg(kMinRequiredMetricsSamples);
  int sent_height = sent_height_counter_.Avg(kMinRequiredMetricsSamples);
  int sent_fps = round(sent_frame_rate_tracker_.ComputeTotalRate());
  if (sent_width != -1) {
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "SentWidthInPixels",
                                sent_width);
    RTC_HISTOGRAMS_COUNTS_10000(kIndex, uma_prefix_ + "SentHeightInPixels",
                                sent_height);
    RTC_HISTOGRAMS_COUNTS_100(kIndex, uma_prefix_ + "SentFramesPerSecond",
                              sent_fps);
  }
  int encode_ms = encode_time_counter_.Avg(kMinRequiredMetricsSamples);
  if (encode_ms != -1) {
    RTC_HISTOGRAMS_COUNTS_1000(kIndex, uma_prefix_ + "EncodeTimeInMs",
                               encode_ms);
  }
  int key_frames_permille =
      key_frame_counter_.Permille(kMinRequiredMetricsSamples);
  if (key_frames_permille != -1) {
    RTC_HISTOGRAMS_COUNTS_1000(kIndex, uma_prefix_ + "KeyFramesSentInPermille",
                               key_frames_permille);
  }
  int quality_limited =
      quality_limited_frame_counter_.Percent(kMinRequiredMetricsSamples);
  if (quality_limited != -1) {
    RTC_HISTOGRAMS_PERCENTAGE(kIndex,
                              uma_prefix_ + "QualityLimitedResolutionInPercent",
                              quality_limited);
  }
  int downscales = quality_downscales_counter_.Avg(kMinRequiredMetricsSamples);
  if (downscales != -1) {
    RTC_HISTOGRAMS_ENUMERATION(
        kIndex, uma_prefix_ + "QualityLimitedResolutionDownscales", downscales,
        20);
  }
  int cpu_limited =
      cpu_limited_frame_counter_.Percent(kMinRequiredMetricsSamples);
  if (cpu_limited != -1) {
    RTC_HISTOGRAMS_PERCENTAGE(
        kIndex, uma_prefix_ + "CpuLimitedResolutionInPercent", cpu_limited);
  }
  int bw_limited =
      bw_limited_frame_counter_.Percent(kMinRequiredMetricsSamples);
  if (bw_limited != -1) {
    RTC_HISTOGRAMS_PERCENTAGE(
        kIndex, uma_prefix_ + "BandwidthLimitedResolutionInPercent",
        bw_limited);
  }
  int num_disabled =
      bw_resolutions_disabled_counter_.Avg(kMinRequiredMetricsSamples);
  if (num_disabled != -1) {
    RTC_HISTOGRAMS_ENUMERATION(
        kIndex, uma_prefix_ + "BandwidthLimitedResolutionsDisabled",
        num_disabled, 10);
  }
  int delay_ms = delay_counter_.Avg(kMinRequiredMetricsSamples);
  if (delay_ms != -1)
    RTC_HISTOGRAMS_COUNTS_100000(kIndex, uma_prefix_ + "SendSideDelayInMs",
                                 delay_ms);

  int max_delay_ms = max_delay_counter_.Avg(kMinRequiredMetricsSamples);
  if (max_delay_ms != -1) {
    RTC_HISTOGRAMS_COUNTS_100000(kIndex, uma_prefix_ + "SendSideDelayMaxInMs",
                                 max_delay_ms);
  }

  for (const auto& it : qp_counters_) {
    int qp_vp8 = it.second.vp8.Avg(kMinRequiredMetricsSamples);
    if (qp_vp8 != -1) {
      int spatial_idx = it.first;
      if (spatial_idx == -1) {
        RTC_HISTOGRAMS_COUNTS_200(kIndex, uma_prefix_ + "Encoded.Qp.Vp8",
                                  qp_vp8);
      } else if (spatial_idx == 0) {
        RTC_HISTOGRAMS_COUNTS_200(kIndex, uma_prefix_ + "Encoded.Qp.Vp8.S0",
                                  qp_vp8);
      } else if (spatial_idx == 1) {
        RTC_HISTOGRAMS_COUNTS_200(kIndex, uma_prefix_ + "Encoded.Qp.Vp8.S1",
                                  qp_vp8);
      } else if (spatial_idx == 2) {
        RTC_HISTOGRAMS_COUNTS_200(kIndex, uma_prefix_ + "Encoded.Qp.Vp8.S2",
                                  qp_vp8);
      } else {
        LOG(LS_WARNING) << "QP stats not recorded for VP8 spatial idx "
                        << spatial_idx;
      }
    }
    int qp_vp9 = it.second.vp9.Avg(kMinRequiredMetricsSamples);
    if (qp_vp9 != -1) {
      int spatial_idx = it.first;
      if (spatial_idx == -1) {
        RTC_HISTOGRAMS_COUNTS_500(kIndex, uma_prefix_ + "Encoded.Qp.Vp9",
                                  qp_vp9);
      } else if (spatial_idx == 0) {
        RTC_HISTOGRAMS_COUNTS_500(kIndex, uma_prefix_ + "Encoded.Qp.Vp9.S0",
                                  qp_vp9);
      } else if (spatial_idx == 1) {
        RTC_HISTOGRAMS_COUNTS_500(kIndex, uma_prefix_ + "Encoded.Qp.Vp9.S1",
                                  qp_vp9);
      } else if (spatial_idx == 2) {
        RTC_HISTOGRAMS_COUNTS_500(kIndex, uma_prefix_ + "Encoded.Qp.Vp9.S2",
                                  qp_vp9);
      } else {
        LOG(LS_WARNING) << "QP stats not recorded for VP9 spatial layer "
                        << spatial_idx;
      }
    }
    int qp_h264 = it.second.h264.Avg(kMinRequiredMetricsSamples);
    if (qp_h264 != -1) {
      int spatial_idx = it.first;
      RTC_DCHECK_EQ(-1, spatial_idx);
      RTC_HISTOGRAMS_COUNTS_100(kIndex, uma_prefix_ + "Encoded.Qp.H264",
                                qp_h264);
    }
  }

  if (first_rtcp_stats_time_ms_ != -1) {
    int64_t elapsed_sec =
        (clock_->TimeInMilliseconds() - first_rtcp_stats_time_ms_) / 1000;
    if (elapsed_sec >= metrics::kMinRunTimeInSeconds) {
      int fraction_lost = report_block_stats_.FractionLostInPercent();
      if (fraction_lost != -1) {
        RTC_HISTOGRAMS_PERCENTAGE(
            kIndex, uma_prefix_ + "SentPacketsLostInPercent", fraction_lost);
      }

      // The RTCP packet type counters, delivered via the
      // RtcpPacketTypeCounterObserver interface, are aggregates over the entire
      // life of the send stream and are not reset when switching content type.
      // For the purpose of these statistics though, we want new counts when
      // switching since we switch histogram name. On every reset of the
      // UmaSamplesContainer, we save the initial state of the counters, so that
      // we can calculate the delta here and aggregate over all ssrcs.
      RtcpPacketTypeCounter counters;
      for (uint32_t ssrc : rtp_config.ssrcs) {
        auto kv = current_stats.substreams.find(ssrc);
        if (kv == current_stats.substreams.end())
          continue;

        RtcpPacketTypeCounter stream_counters =
            kv->second.rtcp_packet_type_counts;
        kv = start_stats_.substreams.find(ssrc);
        if (kv != start_stats_.substreams.end())
          stream_counters.Subtract(kv->second.rtcp_packet_type_counts);

        counters.Add(stream_counters);
      }
      RTC_HISTOGRAMS_COUNTS_10000(kIndex,
                                  uma_prefix_ + "NackPacketsReceivedPerMinute",
                                  counters.nack_packets * 60 / elapsed_sec);
      RTC_HISTOGRAMS_COUNTS_10000(kIndex,
                                  uma_prefix_ + "FirPacketsReceivedPerMinute",
                                  counters.fir_packets * 60 / elapsed_sec);
      RTC_HISTOGRAMS_COUNTS_10000(kIndex,
                                  uma_prefix_ + "PliPacketsReceivedPerMinute",
                                  counters.pli_packets * 60 / elapsed_sec);
      if (counters.nack_requests > 0) {
        RTC_HISTOGRAMS_PERCENTAGE(
            kIndex, uma_prefix_ + "UniqueNackRequestsReceivedInPercent",
            counters.UniqueNackRequestsInPercent());
      }
    }
  }

  if (first_rtp_stats_time_ms_ != -1) {
    int64_t elapsed_sec =
        (clock_->TimeInMilliseconds() - first_rtp_stats_time_ms_) / 1000;
    if (elapsed_sec >= metrics::kMinRunTimeInSeconds) {
      StreamDataCounters rtp;
      StreamDataCounters rtx;
      AccumulateRtxStats(current_stats, rtp_config.rtx.ssrcs, &rtp, &rtx);
      StreamDataCounters start_rtp;
      StreamDataCounters start_rtx;
      AccumulateRtxStats(start_stats_, rtp_config.rtx.ssrcs, &start_rtp,
                         &start_rtx);
      rtp.Subtract(start_rtp);
      rtx.Subtract(start_rtx);
      StreamDataCounters rtp_rtx = rtp;
      rtp_rtx.Add(rtx);

      RTC_HISTOGRAMS_COUNTS_10000(
          kIndex, uma_prefix_ + "BitrateSentInKbps",
          static_cast<int>(rtp_rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAMS_COUNTS_10000(
          kIndex, uma_prefix_ + "MediaBitrateSentInKbps",
          static_cast<int>(rtp.MediaPayloadBytes() * 8 / elapsed_sec / 1000));
      RTC_HISTOGRAMS_COUNTS_10000(
          kIndex, uma_prefix_ + "PaddingBitrateSentInKbps",
          static_cast<int>(rtp_rtx.transmitted.padding_bytes * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAMS_COUNTS_10000(
          kIndex, uma_prefix_ + "RetransmittedBitrateSentInKbps",
          static_cast<int>(rtp_rtx.retransmitted.TotalBytes() * 8 /
                           elapsed_sec / 1000));
      if (!rtp_config.rtx.ssrcs.empty()) {
        RTC_HISTOGRAMS_COUNTS_10000(
            kIndex, uma_prefix_ + "RtxBitrateSentInKbps",
            static_cast<int>(rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                             1000));
      }
      if (rtp_config.flexfec.flexfec_payload_type != -1 ||
          rtp_config.ulpfec.red_payload_type != -1) {
        RTC_HISTOGRAMS_COUNTS_10000(kIndex,
                                    uma_prefix_ + "FecBitrateSentInKbps",
                                    static_cast<int>(rtp_rtx.fec.TotalBytes() *
                                                     8 / elapsed_sec / 1000));
      }
    }
  }
}

void SendStatisticsProxy::OnEncoderReconfigured(
    const VideoEncoderConfig& config,
    uint32_t preferred_bitrate_bps) {
  rtc::CritScope lock(&crit_);
  stats_.preferred_media_bitrate_bps = preferred_bitrate_bps;

  if (content_type_ != config.content_type) {
    uma_container_->UpdateHistograms(rtp_config_, stats_);
    uma_container_.reset(new UmaSamplesContainer(
        GetUmaPrefix(config.content_type), stats_, clock_));
    content_type_ = config.content_type;
  }
}

void SendStatisticsProxy::OnEncoderStatsUpdate(uint32_t framerate,
                                               uint32_t bitrate) {
  rtc::CritScope lock(&crit_);
  stats_.encode_frame_rate = framerate;
  stats_.media_bitrate_bps = bitrate;
}

void SendStatisticsProxy::OnEncodedFrameTimeMeasured(
    int encode_time_ms,
    const CpuOveruseMetrics& metrics) {
  rtc::CritScope lock(&crit_);
  uma_container_->encode_time_counter_.Add(encode_time_ms);
  encode_time_.Apply(1.0f, encode_time_ms);
  stats_.avg_encode_time_ms = round(encode_time_.filtered());
  stats_.encode_usage_percent = metrics.encode_usage_percent;
}

void SendStatisticsProxy::OnSuspendChange(bool is_suspended) {
  rtc::CritScope lock(&crit_);
  stats_.suspended = is_suspended;
}

VideoSendStream::Stats SendStatisticsProxy::GetStats() {
  rtc::CritScope lock(&crit_);
  PurgeOldStats();
  stats_.input_frame_rate =
      round(uma_container_->input_frame_rate_tracker_.ComputeRate());
  return stats_;
}

void SendStatisticsProxy::PurgeOldStats() {
  int64_t old_stats_ms = clock_->TimeInMilliseconds() - kStatsTimeoutMs;
  for (std::map<uint32_t, VideoSendStream::StreamStats>::iterator it =
           stats_.substreams.begin();
       it != stats_.substreams.end(); ++it) {
    uint32_t ssrc = it->first;
    if (update_times_[ssrc].resolution_update_ms <= old_stats_ms) {
      it->second.width = 0;
      it->second.height = 0;
    }
  }
}

VideoSendStream::StreamStats* SendStatisticsProxy::GetStatsEntry(
    uint32_t ssrc) {
  std::map<uint32_t, VideoSendStream::StreamStats>::iterator it =
      stats_.substreams.find(ssrc);
  if (it != stats_.substreams.end())
    return &it->second;

  bool is_media = std::find(rtp_config_.ssrcs.begin(), rtp_config_.ssrcs.end(),
                            ssrc) != rtp_config_.ssrcs.end();
  bool is_flexfec = rtp_config_.flexfec.flexfec_payload_type != -1 &&
                    ssrc == rtp_config_.flexfec.flexfec_ssrc;
  bool is_rtx =
      std::find(rtp_config_.rtx.ssrcs.begin(), rtp_config_.rtx.ssrcs.end(),
                ssrc) != rtp_config_.rtx.ssrcs.end();
  if (!is_media && !is_flexfec && !is_rtx)
    return nullptr;

  // Insert new entry and return ptr.
  VideoSendStream::StreamStats* entry = &stats_.substreams[ssrc];
  entry->is_rtx = is_rtx;

  return entry;
}

void SendStatisticsProxy::OnInactiveSsrc(uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->total_bitrate_bps = 0;
  stats->retransmit_bitrate_bps = 0;
  stats->height = 0;
  stats->width = 0;
}

void SendStatisticsProxy::OnSetEncoderTargetRate(uint32_t bitrate_bps) {
  rtc::CritScope lock(&crit_);
  stats_.target_media_bitrate_bps = bitrate_bps;
}

void SendStatisticsProxy::OnSendEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_info) {
  size_t simulcast_idx = 0;

  rtc::CritScope lock(&crit_);
  ++stats_.frames_encoded;
  if (codec_info) {
    if (codec_info->codecType == kVideoCodecVP8) {
      simulcast_idx = codec_info->codecSpecific.VP8.simulcastIdx;
    } else if (codec_info->codecType == kVideoCodecGeneric) {
      simulcast_idx = codec_info->codecSpecific.generic.simulcast_idx;
    }
    if (codec_info->codec_name) {
      stats_.encoder_implementation_name = codec_info->codec_name;
    }
  }

  if (simulcast_idx >= rtp_config_.ssrcs.size()) {
    LOG(LS_ERROR) << "Encoded image outside simulcast range (" << simulcast_idx
                  << " >= " << rtp_config_.ssrcs.size() << ").";
    return;
  }
  uint32_t ssrc = rtp_config_.ssrcs[simulcast_idx];

  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->width = encoded_image._encodedWidth;
  stats->height = encoded_image._encodedHeight;
  update_times_[ssrc].resolution_update_ms = clock_->TimeInMilliseconds();

  uma_container_->key_frame_counter_.Add(encoded_image._frameType ==
                                         kVideoFrameKey);

  stats_.bw_limited_resolution =
      encoded_image.adapt_reason_.quality_resolution_downscales > 0 ||
      encoded_image.adapt_reason_.bw_resolutions_disabled > 0;

  if (encoded_image.adapt_reason_.quality_resolution_downscales != -1) {
    bool downscaled =
        encoded_image.adapt_reason_.quality_resolution_downscales > 0;
    uma_container_->quality_limited_frame_counter_.Add(downscaled);
    if (downscaled) {
      uma_container_->quality_downscales_counter_.Add(
          encoded_image.adapt_reason_.quality_resolution_downscales);
    }
  }
  if (encoded_image.adapt_reason_.bw_resolutions_disabled != -1) {
    bool bw_limited = encoded_image.adapt_reason_.bw_resolutions_disabled > 0;
    uma_container_->bw_limited_frame_counter_.Add(bw_limited);
    if (bw_limited) {
      uma_container_->bw_resolutions_disabled_counter_.Add(
          encoded_image.adapt_reason_.bw_resolutions_disabled);
    }
  }

  if (encoded_image.qp_ != -1) {
    if (!stats_.qp_sum)
      stats_.qp_sum = rtc::Optional<uint64_t>(0);
    *stats_.qp_sum += encoded_image.qp_;

    if (codec_info) {
      if (codec_info->codecType == kVideoCodecVP8) {
        int spatial_idx = (rtp_config_.ssrcs.size() == 1)
                              ? -1
                              : static_cast<int>(simulcast_idx);
        uma_container_->qp_counters_[spatial_idx].vp8.Add(encoded_image.qp_);
      } else if (codec_info->codecType == kVideoCodecVP9) {
        int spatial_idx =
            (codec_info->codecSpecific.VP9.num_spatial_layers == 1)
                ? -1
                : codec_info->codecSpecific.VP9.spatial_idx;
        uma_container_->qp_counters_[spatial_idx].vp9.Add(encoded_image.qp_);
      } else if (codec_info->codecType == kVideoCodecH264) {
        int spatial_idx = -1;
        uma_container_->qp_counters_[spatial_idx].h264.Add(encoded_image.qp_);
      }
    }
  }

  // TODO(asapersson): This is incorrect if simulcast layers are encoded on
  // different threads and there is no guarantee that one frame of all layers
  // are encoded before the next start.
  if (last_sent_frame_timestamp_ > 0 &&
      encoded_image._timeStamp != last_sent_frame_timestamp_) {
    uma_container_->sent_frame_rate_tracker_.AddSamples(1);
    uma_container_->sent_width_counter_.Add(
        uma_container_->max_sent_width_per_timestamp_);
    uma_container_->sent_height_counter_.Add(
        uma_container_->max_sent_height_per_timestamp_);
    uma_container_->max_sent_width_per_timestamp_ = 0;
    uma_container_->max_sent_height_per_timestamp_ = 0;
  }
  last_sent_frame_timestamp_ = encoded_image._timeStamp;
  uma_container_->max_sent_width_per_timestamp_ =
      std::max(uma_container_->max_sent_width_per_timestamp_,
               static_cast<int>(encoded_image._encodedWidth));
  uma_container_->max_sent_height_per_timestamp_ =
      std::max(uma_container_->max_sent_height_per_timestamp_,
               static_cast<int>(encoded_image._encodedHeight));
}

int SendStatisticsProxy::GetSendFrameRate() const {
  rtc::CritScope lock(&crit_);
  return stats_.encode_frame_rate;
}

void SendStatisticsProxy::OnIncomingFrame(int width, int height) {
  rtc::CritScope lock(&crit_);
  uma_container_->input_frame_rate_tracker_.AddSamples(1);
  uma_container_->input_width_counter_.Add(width);
  uma_container_->input_height_counter_.Add(height);
  uma_container_->cpu_limited_frame_counter_.Add(stats_.cpu_limited_resolution);
}

void SendStatisticsProxy::SetCpuRestrictedResolution(
    bool cpu_restricted_resolution) {
  rtc::CritScope lock(&crit_);
  stats_.cpu_limited_resolution = cpu_restricted_resolution;
}

void SendStatisticsProxy::OnCpuRestrictedResolutionChanged(
    bool cpu_restricted_resolution) {
  rtc::CritScope lock(&crit_);
  stats_.cpu_limited_resolution = cpu_restricted_resolution;
  ++stats_.number_of_cpu_adapt_changes;
}

void SendStatisticsProxy::RtcpPacketTypesCounterUpdated(
    uint32_t ssrc,
    const RtcpPacketTypeCounter& packet_counter) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->rtcp_packet_type_counts = packet_counter;
  if (uma_container_->first_rtcp_stats_time_ms_ == -1)
    uma_container_->first_rtcp_stats_time_ms_ = clock_->TimeInMilliseconds();
}

void SendStatisticsProxy::StatisticsUpdated(const RtcpStatistics& statistics,
                                            uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->rtcp_stats = statistics;
  uma_container_->report_block_stats_.Store(statistics, 0, ssrc);
}

void SendStatisticsProxy::CNameChanged(const char* cname, uint32_t ssrc) {}

void SendStatisticsProxy::DataCountersUpdated(
    const StreamDataCounters& counters,
    uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  RTC_DCHECK(stats) << "DataCountersUpdated reported for unknown ssrc: "
                    << ssrc;

  stats->rtp_stats = counters;
  if (uma_container_->first_rtp_stats_time_ms_ == -1)
    uma_container_->first_rtp_stats_time_ms_ = clock_->TimeInMilliseconds();
}

void SendStatisticsProxy::Notify(uint32_t total_bitrate_bps,
                                 uint32_t retransmit_bitrate_bps,
                                 uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->total_bitrate_bps = total_bitrate_bps;
  stats->retransmit_bitrate_bps = retransmit_bitrate_bps;
}

void SendStatisticsProxy::FrameCountUpdated(const FrameCounts& frame_counts,
                                            uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;

  stats->frame_counts = frame_counts;
}

void SendStatisticsProxy::SendSideDelayUpdated(int avg_delay_ms,
                                               int max_delay_ms,
                                               uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  VideoSendStream::StreamStats* stats = GetStatsEntry(ssrc);
  if (!stats)
    return;
  stats->avg_delay_ms = avg_delay_ms;
  stats->max_delay_ms = max_delay_ms;

  uma_container_->delay_counter_.Add(avg_delay_ms);
  uma_container_->max_delay_counter_.Add(max_delay_ms);
}

void SendStatisticsProxy::SampleCounter::Add(int sample) {
  sum += sample;
  ++num_samples;
}

int SendStatisticsProxy::SampleCounter::Avg(int min_required_samples) const {
  if (num_samples < min_required_samples || num_samples == 0)
    return -1;
  return (sum + (num_samples / 2)) / num_samples;
}

void SendStatisticsProxy::BoolSampleCounter::Add(bool sample) {
  if (sample)
    ++sum;
  ++num_samples;
}

int SendStatisticsProxy::BoolSampleCounter::Percent(
    int min_required_samples) const {
  return Fraction(min_required_samples, 100.0f);
}

int SendStatisticsProxy::BoolSampleCounter::Permille(
    int min_required_samples) const {
  return Fraction(min_required_samples, 1000.0f);
}

int SendStatisticsProxy::BoolSampleCounter::Fraction(
    int min_required_samples, float multiplier) const {
  if (num_samples < min_required_samples || num_samples == 0)
    return -1;
  return static_cast<int>((sum * multiplier / num_samples) + 0.5f);
}
}  // namespace webrtc
