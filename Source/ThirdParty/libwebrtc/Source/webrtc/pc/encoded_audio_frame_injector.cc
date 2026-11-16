/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/encoded_audio_frame_injector.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/nullability.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/call/bitrate_allocation.h"
#include "api/encoded_audio_frame_injector_interface.h"
#include "api/environment/environment.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

namespace {
// constants used by ProxyAudioTrack and ProxyAudioEncoder, actual values
// depend on the real encoder and can be different.
constexpr int kAudioSampleRateHz = 48000;
constexpr size_t kAudioNumChannels = 1;
constexpr int kAudioTargetBitrateBps = 64000;
}  // namespace

// A placeholder audio track that receives silence injection to trigger the
// audio engine's processing pipeline when using EncodedAudioFrameInjector.
class ProxyAudioTrack : public AudioTrackInterface {
 public:
  static scoped_refptr<ProxyAudioTrack> Create(
      TaskQueueBase* absl_nonnull worker_thread) {
    return scoped_refptr<ProxyAudioTrack>(
        new RefCountedObject<ProxyAudioTrack>(worker_thread));
  }

  // called on any thread
  void InjectSilence() {
    if (TaskQueueBase::Current() == worker_thread_) {
      InjectSilenceInternal();
    } else {
      worker_thread_->PostTask(
          [scoped_this = scoped_refptr<ProxyAudioTrack>(this)] {
            scoped_this->InjectSilenceInternal();
          });
    }
  }

  // AudioTrackInterface implementation
  AudioSourceInterface* GetSource() const override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    return nullptr;
  }

  void AddSink(AudioTrackSinkInterface* sink) override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    Event event;
    worker_thread_->PostTask(
        [scoped_this = scoped_refptr<ProxyAudioTrack>(this), sink, &event] {
          scoped_this->SetSinkOnWorkerThread(sink);
          event.Set();
        });
    event.Wait(Event::kForever);
  }

  void RemoveSink(AudioTrackSinkInterface* sink) override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    Event event;
    worker_thread_->PostTask(
        [scoped_this = scoped_refptr<ProxyAudioTrack>(this), sink, &event] {
          scoped_this->RemoveSinkOnWorkerThread(sink);
          event.Set();
        });
    event.Wait(Event::kForever);
  }

  std::string kind() const override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    return "audio";
  }
  std::string id() const override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    return "proxy_audio_track";
  }
  bool enabled() const override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    return true;
  }
  bool set_enabled(bool enable) override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    return true;
  }
  TrackState state() const override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    return TrackState::kLive;
  }

  void RegisterObserver(ObserverInterface* observer) override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
  }
  void UnregisterObserver(ObserverInterface* observer) override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
  }

 protected:
  explicit ProxyAudioTrack(TaskQueueBase* absl_nonnull worker_thread)
      : worker_thread_(worker_thread) {
    RTC_DCHECK(worker_thread_);
  }
  ~ProxyAudioTrack() override = default;

 private:
  void SetSinkOnWorkerThread(AudioTrackSinkInterface* sink) {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    sink_ = sink;
  }

  void RemoveSinkOnWorkerThread(AudioTrackSinkInterface* sink) {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    if (sink_ == sink) {
      sink_ = nullptr;
    }
  }

  void InjectSilenceInternal() {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    if (!sink_) {
      return;
    }
    // Generate 10ms of silence
    static constexpr size_t kNumFrames = kAudioSampleRateHz / 100;
    static const int16_t kSilence[kNumFrames] = {0};

    sink_->OnData(kSilence, /*bits_per_sample=*/16, kAudioSampleRateHz,
                  kAudioNumChannels, kNumFrames, TimeMillis());
  }

  RTC_NO_UNIQUE_ADDRESS SequenceChecker signaling_sequence_checker_{
      SequenceChecker::kDetached};
  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_sequence_checker_{
      SequenceChecker::kDetached};
  TaskQueueBase* absl_nonnull const worker_thread_;
  AudioTrackSinkInterface* sink_ RTC_GUARDED_BY(worker_sequence_checker_) =
      nullptr;
};

class ProxyAudioEncoder : public AudioEncoder {
 public:
  explicit ProxyAudioEncoder(scoped_refptr<EncodedAudioFrameInjector> injector)
      : injector_(injector) {
    RTC_DCHECK(injector_);
  }

  ~ProxyAudioEncoder() override {}

  int SampleRateHz() const override { return kAudioSampleRateHz; }
  size_t NumChannels() const override { return kAudioNumChannels; }
  size_t Num10MsFramesInNextPacket() const override { return 1; }
  size_t Max10MsFramesInAPacket() const override { return 1; }
  int GetTargetBitrate() const override { return -1; }
  void Reset() override {}
  std::optional<std::pair<TimeDelta, TimeDelta>> GetFrameLengthRange()
      const override {
    return std::make_pair(TimeDelta::Millis(20), TimeDelta::Millis(20));
  }

  // Called on any thread
  void OnReceivedUplinkAllocation(BitrateAllocationUpdate update) override {
    injector_->InvokeBitrateInfoCallback(update.target_bitrate.bps());
  }

  void OnReceivedTargetAudioBitrate(int target_bps) override {
    injector_->InvokeBitrateInfoCallback(target_bps);
  }

  EncodedInfo EncodeImpl(uint32_t rtp_timestamp,
                         std::span<const int16_t> audio,
                         Buffer* encoded) override {
    RTC_DCHECK_RUN_ON(&encoder_sequence_checker_);

    std::unique_ptr<TransformableAudioFrameInterface> frame =
        injector_->GetNextFrame();

    if (!frame) {
      return EncodedInfo();
    }

    encoded->AppendData(frame->GetData().data(), frame->GetData().size());

    EncodedInfo info;
    info.encoded_bytes = frame->GetData().size();

    RtpTimestampInfo rtp_info = frame->GetRtpTimestampInfo();
    if (std::holds_alternative<RtpTimestampWithOffset>(rtp_info)) {
      info.encoded_timestamp = std::get<RtpTimestampWithOffset>(rtp_info);
    } else {
      info.encoded_timestamp = std::get<RtpTimestampWithoutOffset>(rtp_info);
    }
    info.payload_type = frame->GetPayloadType();
    info.send_even_if_empty = true;
    info.speech = true;
    info.audio_level_dbov_override = frame->AudioLevel();
    info.absolute_capture_timestamp_ms_override =
        frame->AbsoluteCaptureTimestamp();
    info.csrcs_override.emplace(frame->GetContributingSources().begin(),
                                frame->GetContributingSources().end());
    return info;
  }

 private:
  const scoped_refptr<EncodedAudioFrameInjector> injector_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_sequence_checker_{
      SequenceChecker::kDetached};
  RTC_NO_UNIQUE_ADDRESS SequenceChecker encoder_sequence_checker_{
      SequenceChecker::kDetached};
};

class ProxyAudioEncoderFactory : public AudioEncoderFactory {
 public:
  explicit ProxyAudioEncoderFactory(
      scoped_refptr<EncodedAudioFrameInjector> injector)
      : injector_(injector) {}

  ~ProxyAudioEncoderFactory() override = default;

  std::vector<AudioCodecSpec> GetSupportedEncoders() override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return {};
  }

  std::optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return AudioCodecInfo(kAudioSampleRateHz, kAudioNumChannels,
                          kAudioTargetBitrateBps);
  }

  std::unique_ptr<AudioEncoder> Create(const Environment& env,
                                       const SdpAudioFormat& format,
                                       Options options) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return std::make_unique<ProxyAudioEncoder>(injector_);
  }

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_{
      SequenceChecker::kDetached};
  const scoped_refptr<EncodedAudioFrameInjector> injector_;
};

scoped_refptr<EncodedAudioFrameInjector> EncodedAudioFrameInjector::Create(
    TargetBitrateCallback bitrate_callback,
    TaskQueueBase* absl_nonnull worker_thread) {
  return scoped_refptr<EncodedAudioFrameInjector>(
      new RefCountedObject<EncodedAudioFrameInjector>(
          std::move(bitrate_callback), worker_thread));
}

EncodedAudioFrameInjector::EncodedAudioFrameInjector(
    TargetBitrateCallback bitrate_callback,
    TaskQueueBase* absl_nonnull worker_thread)
    : audio_track_(ProxyAudioTrack::Create(worker_thread)),
      bitrate_callback_(std::move(bitrate_callback)) {
  RTC_DCHECK(worker_thread);
}

EncodedAudioFrameInjector::~EncodedAudioFrameInjector() = default;

void EncodedAudioFrameInjector::InjectFrame(
    std::unique_ptr<TransformableAudioFrameInterface> encoded_frame) {
  if (!encoded_frame) {
    return;
  }
  {
    MutexLock lock(&buffered_frames_lock_);
    buffered_frames_.push_back(std::move(encoded_frame));
  }
  audio_track_->InjectSilence();
}

scoped_refptr<AudioTrackInterface> EncodedAudioFrameInjector::GetAudioTrack() {
  RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
  return audio_track_;
}

absl_nonnull scoped_refptr<AudioEncoderFactory>
EncodedAudioFrameInjector::CreateEncoderFactory() {
  RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
  return scoped_refptr<AudioEncoderFactory>(
      new RefCountedObject<ProxyAudioEncoderFactory>(
          scoped_refptr<EncodedAudioFrameInjector>(this)));
}

// Called on any thread
void EncodedAudioFrameInjector::InvokeBitrateInfoCallback(
    int32_t allocated_bitrate) {
  if (bitrate_callback_) {
    bitrate_callback_(allocated_bitrate);
  }
}

std::unique_ptr<TransformableAudioFrameInterface>
EncodedAudioFrameInjector::GetNextFrame() {
  RTC_DCHECK_RUN_ON(&encoder_sequence_checker_);
  MutexLock lock(&buffered_frames_lock_);

  if (buffered_frames_.empty()) {
    return nullptr;
  }
  auto frame = std::move(buffered_frames_.front());
  buffered_frames_.pop_front();
  return frame;
}

}  // namespace webrtc
