/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "webrtc/modules/audio_device/audio_device_buffer.h"

#include "webrtc/base/arraysize.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {

static const char kTimerQueueName[] = "AudioDeviceBufferTimer";

// Time between two sucessive calls to LogStats().
static const size_t kTimerIntervalInSeconds = 10;
static const size_t kTimerIntervalInMilliseconds =
    kTimerIntervalInSeconds * rtc::kNumMillisecsPerSec;
// Min time required to qualify an audio session as a "call". If playout or
// recording has been active for less than this time we will not store any
// logs or UMA stats but instead consider the call as too short.
static const size_t kMinValidCallTimeTimeInSeconds = 10;
static const size_t kMinValidCallTimeTimeInMilliseconds =
    kMinValidCallTimeTimeInSeconds * rtc::kNumMillisecsPerSec;

AudioDeviceBuffer::AudioDeviceBuffer()
    : task_queue_(kTimerQueueName),
      audio_transport_cb_(nullptr),
      rec_sample_rate_(0),
      play_sample_rate_(0),
      rec_channels_(0),
      play_channels_(0),
      playing_(false),
      recording_(false),
      current_mic_level_(0),
      new_mic_level_(0),
      typing_status_(false),
      play_delay_ms_(0),
      rec_delay_ms_(0),
      clock_drift_(0),
      num_stat_reports_(0),
      last_timer_task_time_(0),
      rec_stat_count_(0),
      play_stat_count_(0),
      play_start_time_(0),
      only_silence_recorded_(true),
      log_stats_(false) {
  LOG(INFO) << "AudioDeviceBuffer::ctor";
  playout_thread_checker_.DetachFromThread();
  recording_thread_checker_.DetachFromThread();
}

AudioDeviceBuffer::~AudioDeviceBuffer() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  RTC_DCHECK(!playing_);
  RTC_DCHECK(!recording_);
  LOG(INFO) << "AudioDeviceBuffer::~dtor";
}

int32_t AudioDeviceBuffer::RegisterAudioCallback(
    AudioTransport* audio_callback) {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  LOG(INFO) << __FUNCTION__;
  if (playing_ || recording_) {
    LOG(LS_ERROR) << "Failed to set audio transport since media was active";
    return -1;
  }
  audio_transport_cb_ = audio_callback;
  return 0;
}

void AudioDeviceBuffer::StartPlayout() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  // TODO(henrika): allow for usage of DCHECK(!playing_) here instead. Today the
  // ADM allows calling Start(), Start() by ignoring the second call but it
  // makes more sense to only allow one call.
  if (playing_) {
    return;
  }
  LOG(INFO) << __FUNCTION__;
  playout_thread_checker_.DetachFromThread();
  // Clear members tracking playout stats and do it on the task queue.
  task_queue_.PostTask([this] { ResetPlayStats(); });
  // Start a periodic timer based on task queue if not already done by the
  // recording side.
  if (!recording_) {
    StartPeriodicLogging();
  }
  const int64_t now_time = rtc::TimeMillis();
  // Clear members that are only touched on the main (creating) thread.
  play_start_time_ = now_time;
  playing_ = true;
}

void AudioDeviceBuffer::StartRecording() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  if (recording_) {
    return;
  }
  LOG(INFO) << __FUNCTION__;
  recording_thread_checker_.DetachFromThread();
  // Clear members tracking recording stats and do it on the task queue.
  task_queue_.PostTask([this] { ResetRecStats(); });
  // Start a periodic timer based on task queue if not already done by the
  // playout side.
  if (!playing_) {
    StartPeriodicLogging();
  }
  // Clear members that will be touched on the main (creating) thread.
  rec_start_time_ = rtc::TimeMillis();
  recording_ = true;
  // And finally a member which can be modified on the native audio thread.
  // It is safe to do so since we know by design that the owning ADM has not
  // yet started the native audio recording.
  only_silence_recorded_ = true;
}

void AudioDeviceBuffer::StopPlayout() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  if (!playing_) {
    return;
  }
  LOG(INFO) << __FUNCTION__;
  playing_ = false;
  // Stop periodic logging if no more media is active.
  if (!recording_) {
    StopPeriodicLogging();
  }
  LOG(INFO) << "total playout time: " << rtc::TimeSince(play_start_time_);
}

void AudioDeviceBuffer::StopRecording() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  if (!recording_) {
    return;
  }
  LOG(INFO) << __FUNCTION__;
  recording_ = false;
  // Stop periodic logging if no more media is active.
  if (!playing_) {
    StopPeriodicLogging();
  }
  // Add UMA histogram to keep track of the case when only zeros have been
  // recorded. Measurements (max of absolute level) are taken twice per second,
  // which means that if e.g 10 seconds of audio has been recorded, a total of
  // 20 level estimates must all be identical to zero to trigger the histogram.
  // |only_silence_recorded_| can only be cleared on the native audio thread
  // that drives audio capture but we know by design that the audio has stopped
  // when this method is called, hence there should not be aby conflicts. Also,
  // the fact that |only_silence_recorded_| can be affected during the complete
  // call makes chances of conflicts with potentially one last callback very
  // small.
  const size_t time_since_start = rtc::TimeSince(rec_start_time_);
  if (time_since_start > kMinValidCallTimeTimeInMilliseconds) {
    const int only_zeros = static_cast<int>(only_silence_recorded_);
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.RecordedOnlyZeros", only_zeros);
    LOG(INFO) << "HISTOGRAM(WebRTC.Audio.RecordedOnlyZeros): " << only_zeros;
  }
  LOG(INFO) << "total recording time: " << time_since_start;
}

int32_t AudioDeviceBuffer::SetRecordingSampleRate(uint32_t fsHz) {
  RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
  LOG(INFO) << "SetRecordingSampleRate(" << fsHz << ")";
  rec_sample_rate_ = fsHz;
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutSampleRate(uint32_t fsHz) {
  RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
  LOG(INFO) << "SetPlayoutSampleRate(" << fsHz << ")";
  play_sample_rate_ = fsHz;
  return 0;
}

int32_t AudioDeviceBuffer::RecordingSampleRate() const {
  RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
  return rec_sample_rate_;
}

int32_t AudioDeviceBuffer::PlayoutSampleRate() const {
  RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
  return play_sample_rate_;
}

int32_t AudioDeviceBuffer::SetRecordingChannels(size_t channels) {
  RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
  LOG(INFO) << "SetRecordingChannels(" << channels << ")";
  rec_channels_ = channels;
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutChannels(size_t channels) {
  RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
  LOG(INFO) << "SetPlayoutChannels(" << channels << ")";
  play_channels_ = channels;
  return 0;
}

int32_t AudioDeviceBuffer::SetRecordingChannel(
    const AudioDeviceModule::ChannelType channel) {
  LOG(INFO) << "SetRecordingChannel(" << channel << ")";
  LOG(LS_WARNING) << "Not implemented";
  // Add DCHECK to ensure that user does not try to use this API with a non-
  // default parameter.
  RTC_DCHECK_EQ(channel, AudioDeviceModule::kChannelBoth);
  return -1;
}

int32_t AudioDeviceBuffer::RecordingChannel(
    AudioDeviceModule::ChannelType& channel) const {
  LOG(LS_WARNING) << "Not implemented";
  return -1;
}

size_t AudioDeviceBuffer::RecordingChannels() const {
  RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
  return rec_channels_;
}

size_t AudioDeviceBuffer::PlayoutChannels() const {
  RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
  return play_channels_;
}

int32_t AudioDeviceBuffer::SetCurrentMicLevel(uint32_t level) {
#if !defined(WEBRTC_WIN)
  // Windows uses a dedicated thread for volume APIs.
  RTC_DCHECK_RUN_ON(&recording_thread_checker_);
#endif
  current_mic_level_ = level;
  return 0;
}

int32_t AudioDeviceBuffer::SetTypingStatus(bool typing_status) {
  RTC_DCHECK_RUN_ON(&recording_thread_checker_);
  typing_status_ = typing_status;
  return 0;
}

uint32_t AudioDeviceBuffer::NewMicLevel() const {
  RTC_DCHECK_RUN_ON(&recording_thread_checker_);
  return new_mic_level_;
}

void AudioDeviceBuffer::SetVQEData(int play_delay_ms,
                                   int rec_delay_ms,
                                   int clock_drift) {
  RTC_DCHECK_RUN_ON(&recording_thread_checker_);
  play_delay_ms_ = play_delay_ms;
  rec_delay_ms_ = rec_delay_ms;
  clock_drift_ = clock_drift;
}

int32_t AudioDeviceBuffer::StartInputFileRecording(
    const char fileName[kAdmMaxFileNameSize]) {
  LOG(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceBuffer::StopInputFileRecording() {
  LOG(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceBuffer::StartOutputFileRecording(
    const char fileName[kAdmMaxFileNameSize]) {
  LOG(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceBuffer::StopOutputFileRecording() {
  LOG(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceBuffer::SetRecordedBuffer(const void* audio_buffer,
                                             size_t samples_per_channel) {
  RTC_DCHECK_RUN_ON(&recording_thread_checker_);
  // Copy the complete input buffer to the local buffer.
  const size_t old_size = rec_buffer_.size();
  rec_buffer_.SetData(static_cast<const int16_t*>(audio_buffer),
                      rec_channels_ * samples_per_channel);
  // Keep track of the size of the recording buffer. Only updated when the
  // size changes, which is a rare event.
  if (old_size != rec_buffer_.size()) {
    LOG(LS_INFO) << "Size of recording buffer: " << rec_buffer_.size();
  }

  // Derive a new level value twice per second and check if it is non-zero.
  int16_t max_abs = 0;
  RTC_DCHECK_LT(rec_stat_count_, 50);
  if (++rec_stat_count_ >= 50) {
    // Returns the largest absolute value in a signed 16-bit vector.
    max_abs = WebRtcSpl_MaxAbsValueW16(rec_buffer_.data(), rec_buffer_.size());
    rec_stat_count_ = 0;
    // Set |only_silence_recorded_| to false as soon as at least one detection
    // of a non-zero audio packet is found. It can only be restored to true
    // again by restarting the call.
    if (max_abs > 0) {
      only_silence_recorded_ = false;
    }
  }
  // Update recording stats which is used as base for periodic logging of the
  // audio input state.
  UpdateRecStats(max_abs, samples_per_channel);
  return 0;
}

int32_t AudioDeviceBuffer::DeliverRecordedData() {
  RTC_DCHECK_RUN_ON(&recording_thread_checker_);
  if (!audio_transport_cb_) {
    LOG(LS_WARNING) << "Invalid audio transport";
    return 0;
  }
  const size_t frames = rec_buffer_.size() / rec_channels_;
  const size_t bytes_per_frame = rec_channels_ * sizeof(int16_t);
  uint32_t new_mic_level(0);
  uint32_t total_delay_ms = play_delay_ms_ + rec_delay_ms_;
  int32_t res = audio_transport_cb_->RecordedDataIsAvailable(
      rec_buffer_.data(), frames, bytes_per_frame, rec_channels_,
      rec_sample_rate_, total_delay_ms, clock_drift_, current_mic_level_,
      typing_status_, new_mic_level);
  if (res != -1) {
    new_mic_level_ = new_mic_level;
  } else {
    LOG(LS_ERROR) << "RecordedDataIsAvailable() failed";
  }
  return 0;
}

int32_t AudioDeviceBuffer::RequestPlayoutData(size_t samples_per_channel) {
  RTC_DCHECK_RUN_ON(&playout_thread_checker_);
  // The consumer can change the requested size on the fly and we therefore
  // resize the buffer accordingly. Also takes place at the first call to this
  // method.
  const size_t total_samples = play_channels_ * samples_per_channel;
  if (play_buffer_.size() != total_samples) {
    play_buffer_.SetSize(total_samples);
    LOG(LS_INFO) << "Size of playout buffer: " << play_buffer_.size();
  }

  size_t num_samples_out(0);
  // It is currently supported to start playout without a valid audio
  // transport object. Leads to warning and silence.
  if (!audio_transport_cb_) {
    LOG(LS_WARNING) << "Invalid audio transport";
    return 0;
  }

  // Retrieve new 16-bit PCM audio data using the audio transport instance.
  int64_t elapsed_time_ms = -1;
  int64_t ntp_time_ms = -1;
  const size_t bytes_per_frame = play_channels_ * sizeof(int16_t);
  uint32_t res = audio_transport_cb_->NeedMorePlayData(
      samples_per_channel, bytes_per_frame, play_channels_, play_sample_rate_,
      play_buffer_.data(), num_samples_out, &elapsed_time_ms, &ntp_time_ms);
  if (res != 0) {
    LOG(LS_ERROR) << "NeedMorePlayData() failed";
  }

  // Derive a new level value twice per second.
  int16_t max_abs = 0;
  RTC_DCHECK_LT(play_stat_count_, 50);
  if (++play_stat_count_ >= 50) {
    // Returns the largest absolute value in a signed 16-bit vector.
    max_abs =
        WebRtcSpl_MaxAbsValueW16(play_buffer_.data(), play_buffer_.size());
    play_stat_count_ = 0;
  }
  // Update playout stats which is used as base for periodic logging of the
  // audio output state.
  UpdatePlayStats(max_abs, num_samples_out);
  return static_cast<int32_t>(num_samples_out);
}

int32_t AudioDeviceBuffer::GetPlayoutData(void* audio_buffer) {
  RTC_DCHECK_RUN_ON(&playout_thread_checker_);
  RTC_DCHECK_GT(play_buffer_.size(), 0);
  const size_t bytes_per_sample = sizeof(int16_t);
  memcpy(audio_buffer, play_buffer_.data(),
         play_buffer_.size() * bytes_per_sample);
  // Return samples per channel or number of frames.
  return static_cast<int32_t>(play_buffer_.size() / play_channels_);
}

void AudioDeviceBuffer::StartPeriodicLogging() {
  task_queue_.PostTask(rtc::Bind(&AudioDeviceBuffer::LogStats, this,
                                 AudioDeviceBuffer::LOG_START));
}

void AudioDeviceBuffer::StopPeriodicLogging() {
  task_queue_.PostTask(rtc::Bind(&AudioDeviceBuffer::LogStats, this,
                                 AudioDeviceBuffer::LOG_STOP));
}

void AudioDeviceBuffer::LogStats(LogState state) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  int64_t now_time = rtc::TimeMillis();

  if (state == AudioDeviceBuffer::LOG_START) {
    // Reset counters at start. We will not add any logging in this state but
    // the timer will started by posting a new (delayed) task.
    num_stat_reports_ = 0;
    last_timer_task_time_ = now_time;
    log_stats_ = true;
  } else if (state == AudioDeviceBuffer::LOG_STOP) {
    // Stop logging and posting new tasks.
    log_stats_ = false;
  } else if (state == AudioDeviceBuffer::LOG_ACTIVE) {
    // Keep logging unless logging was disabled while task was posted.
  }

  // Avoid adding more logs since we are in STOP mode.
  if (!log_stats_) {
    return;
  }

  int64_t next_callback_time = now_time + kTimerIntervalInMilliseconds;
  int64_t time_since_last = rtc::TimeDiff(now_time, last_timer_task_time_);
  last_timer_task_time_ = now_time;

  Stats stats;
  {
    rtc::CritScope cs(&lock_);
    stats = stats_;
    stats_.max_rec_level = 0;
    stats_.max_play_level = 0;
  }

  // Log the latest statistics but skip the first round just after state was
  // set to LOG_START. Hence, first printed log will be after ~10 seconds.
  if (++num_stat_reports_ > 1 && time_since_last > 0) {
    uint32_t diff_samples = stats.rec_samples - last_stats_.rec_samples;
    float rate = diff_samples / (static_cast<float>(time_since_last) / 1000.0);
    LOG(INFO) << "[REC : " << time_since_last << "msec, "
              << rec_sample_rate_ / 1000 << "kHz] callbacks: "
              << stats.rec_callbacks - last_stats_.rec_callbacks << ", "
              << "samples: " << diff_samples << ", "
              << "rate: " << static_cast<int>(rate + 0.5) << ", "
              << "level: " << stats.max_rec_level;

    diff_samples = stats.play_samples - last_stats_.play_samples;
    rate = diff_samples / (static_cast<float>(time_since_last) / 1000.0);
    LOG(INFO) << "[PLAY: " << time_since_last << "msec, "
              << play_sample_rate_ / 1000 << "kHz] callbacks: "
              << stats.play_callbacks - last_stats_.play_callbacks << ", "
              << "samples: " << diff_samples << ", "
              << "rate: " << static_cast<int>(rate + 0.5) << ", "
              << "level: " << stats.max_play_level;
    last_stats_ = stats;
  }

  int64_t time_to_wait_ms = next_callback_time - rtc::TimeMillis();
  RTC_DCHECK_GT(time_to_wait_ms, 0) << "Invalid timer interval";

  // Keep posting new (delayed) tasks until state is changed to kLogStop.
  task_queue_.PostDelayedTask(rtc::Bind(&AudioDeviceBuffer::LogStats, this,
                                        AudioDeviceBuffer::LOG_ACTIVE),
                              time_to_wait_ms);
}

void AudioDeviceBuffer::ResetRecStats() {
  RTC_DCHECK_RUN_ON(&task_queue_);
  last_stats_.ResetRecStats();
  rtc::CritScope cs(&lock_);
  stats_.ResetRecStats();
}

void AudioDeviceBuffer::ResetPlayStats() {
  RTC_DCHECK_RUN_ON(&task_queue_);
  last_stats_.ResetPlayStats();
  rtc::CritScope cs(&lock_);
  stats_.ResetPlayStats();
}

void AudioDeviceBuffer::UpdateRecStats(int16_t max_abs,
                                       size_t samples_per_channel) {
  RTC_DCHECK_RUN_ON(&recording_thread_checker_);
  rtc::CritScope cs(&lock_);
  ++stats_.rec_callbacks;
  stats_.rec_samples += samples_per_channel;
  if (max_abs > stats_.max_rec_level) {
    stats_.max_rec_level = max_abs;
  }
}

void AudioDeviceBuffer::UpdatePlayStats(int16_t max_abs,
                                        size_t samples_per_channel) {
  RTC_DCHECK_RUN_ON(&playout_thread_checker_);
  rtc::CritScope cs(&lock_);
  ++stats_.play_callbacks;
  stats_.play_samples += samples_per_channel;
  if (max_abs > stats_.max_play_level) {
    stats_.max_play_level = max_abs;
  }
}

}  // namespace webrtc
