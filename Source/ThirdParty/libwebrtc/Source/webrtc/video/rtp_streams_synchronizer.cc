/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/rtp_streams_synchronizer.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/video_coding/video_coding_impl.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/video/stream_synchronization.h"
#include "webrtc/video_frame.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {
namespace {
int UpdateMeasurements(StreamSynchronization::Measurements* stream,
                       RtpRtcp* rtp_rtcp, RtpReceiver* receiver) {
  if (!receiver->Timestamp(&stream->latest_timestamp))
    return -1;
  if (!receiver->LastReceivedTimeMs(&stream->latest_receive_time_ms))
    return -1;

  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  if (rtp_rtcp->RemoteNTP(&ntp_secs, &ntp_frac, nullptr, nullptr,
                          &rtp_timestamp) != 0) {
    return -1;
  }

  bool new_rtcp_sr = false;
  if (!UpdateRtcpList(ntp_secs, ntp_frac, rtp_timestamp, &stream->rtcp,
                      &new_rtcp_sr)) {
    return -1;
  }

  return 0;
}
}  // namespace

RtpStreamsSynchronizer::RtpStreamsSynchronizer(
    vcm::VideoReceiver* video_receiver,
    RtpStreamReceiver* rtp_stream_receiver)
    : clock_(Clock::GetRealTimeClock()),
      video_receiver_(video_receiver),
      video_rtp_receiver_(rtp_stream_receiver->GetRtpReceiver()),
      video_rtp_rtcp_(rtp_stream_receiver->rtp_rtcp()),
      voe_channel_id_(-1),
      voe_sync_interface_(nullptr),
      audio_rtp_receiver_(nullptr),
      audio_rtp_rtcp_(nullptr),
      sync_(),
      last_sync_time_(rtc::TimeNanos()) {
  process_thread_checker_.DetachFromThread();
}

void RtpStreamsSynchronizer::ConfigureSync(int voe_channel_id,
                                           VoEVideoSync* voe_sync_interface) {
  if (voe_channel_id != -1)
    RTC_DCHECK(voe_sync_interface);

  rtc::CritScope lock(&crit_);
  if (voe_channel_id_ == voe_channel_id &&
      voe_sync_interface_ == voe_sync_interface) {
    // This prevents expensive no-ops.
    return;
  }
  voe_channel_id_ = voe_channel_id;
  voe_sync_interface_ = voe_sync_interface;

  audio_rtp_rtcp_ = nullptr;
  audio_rtp_receiver_ = nullptr;
  sync_.reset(nullptr);

  if (voe_channel_id_ != -1) {
    voe_sync_interface_->GetRtpRtcp(voe_channel_id_, &audio_rtp_rtcp_,
                                    &audio_rtp_receiver_);
    RTC_DCHECK(audio_rtp_rtcp_);
    RTC_DCHECK(audio_rtp_receiver_);
    sync_.reset(new StreamSynchronization(video_rtp_rtcp_->SSRC(),
                                          voe_channel_id_));
  }
}

int64_t RtpStreamsSynchronizer::TimeUntilNextProcess() {
  RTC_DCHECK_RUN_ON(&process_thread_checker_);
  const int64_t kSyncIntervalMs = 1000;
  return kSyncIntervalMs -
      (rtc::TimeNanos() - last_sync_time_) / rtc::kNumNanosecsPerMillisec;
}

void RtpStreamsSynchronizer::Process() {
  RTC_DCHECK_RUN_ON(&process_thread_checker_);

  const int current_video_delay_ms = video_receiver_->Delay();
  last_sync_time_ = rtc::TimeNanos();

  rtc::CritScope lock(&crit_);
  if (voe_channel_id_ == -1) {
    return;
  }
  RTC_DCHECK(voe_sync_interface_);
  RTC_DCHECK(sync_.get());

  int audio_jitter_buffer_delay_ms = 0;
  int playout_buffer_delay_ms = 0;
  if (voe_sync_interface_->GetDelayEstimate(voe_channel_id_,
                                            &audio_jitter_buffer_delay_ms,
                                            &playout_buffer_delay_ms) != 0) {
    return;
  }
  const int current_audio_delay_ms = audio_jitter_buffer_delay_ms +
      playout_buffer_delay_ms;

  int64_t last_video_receive_ms = video_measurement_.latest_receive_time_ms;
  if (UpdateMeasurements(&video_measurement_, video_rtp_rtcp_,
                         video_rtp_receiver_) != 0) {
    return;
  }

  if (UpdateMeasurements(&audio_measurement_, audio_rtp_rtcp_,
                         audio_rtp_receiver_) != 0) {
    return;
  }

  if (last_video_receive_ms == video_measurement_.latest_receive_time_ms) {
    // No new video packet has been received since last update.
    return;
  }

  int relative_delay_ms;
  // Calculate how much later or earlier the audio stream is compared to video.
  if (!sync_->ComputeRelativeDelay(audio_measurement_, video_measurement_,
                                   &relative_delay_ms)) {
    return;
  }

  TRACE_COUNTER1("webrtc", "SyncCurrentVideoDelay", current_video_delay_ms);
  TRACE_COUNTER1("webrtc", "SyncCurrentAudioDelay", current_audio_delay_ms);
  TRACE_COUNTER1("webrtc", "SyncRelativeDelay", relative_delay_ms);
  int target_audio_delay_ms = 0;
  int target_video_delay_ms = current_video_delay_ms;
  // Calculate the necessary extra audio delay and desired total video
  // delay to get the streams in sync.
  if (!sync_->ComputeDelays(relative_delay_ms,
                            current_audio_delay_ms,
                            &target_audio_delay_ms,
                            &target_video_delay_ms)) {
    return;
  }

  if (voe_sync_interface_->SetMinimumPlayoutDelay(
      voe_channel_id_, target_audio_delay_ms) == -1) {
    LOG(LS_ERROR) << "Error setting voice delay.";
  }
  video_receiver_->SetMinimumPlayoutDelay(target_video_delay_ms);
}

bool RtpStreamsSynchronizer::GetStreamSyncOffsetInMs(
    const VideoFrame& frame,
    int64_t* stream_offset_ms,
    double* estimated_freq_khz) const {
  rtc::CritScope lock(&crit_);
  if (voe_channel_id_ == -1)
    return false;

  uint32_t playout_timestamp = 0;
  if (voe_sync_interface_->GetPlayoutTimestamp(voe_channel_id_,
                                               playout_timestamp) != 0) {
    return false;
  }

  int64_t latest_audio_ntp;
  if (!RtpToNtpMs(playout_timestamp, audio_measurement_.rtcp,
                  &latest_audio_ntp)) {
    return false;
  }

  int64_t latest_video_ntp;
  if (!RtpToNtpMs(frame.timestamp(), video_measurement_.rtcp,
                  &latest_video_ntp)) {
    return false;
  }

  int64_t time_to_render_ms =
      frame.render_time_ms() - clock_->TimeInMilliseconds();
  if (time_to_render_ms > 0)
    latest_video_ntp += time_to_render_ms;

  *stream_offset_ms = latest_audio_ntp - latest_video_ntp;
  *estimated_freq_khz = video_measurement_.rtcp.params.frequency_khz;
  return true;
}

}  // namespace webrtc
