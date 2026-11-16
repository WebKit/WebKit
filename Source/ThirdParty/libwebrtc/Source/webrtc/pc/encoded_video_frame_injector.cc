/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/encoded_video_frame_injector.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/nullability.h"
#include "api/encoded_video_frame_injector_interface.h"
#include "api/environment/environment.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

// A placeholder video track that generates black frames to keep the WebRTC
// engine's capture and processing pipeline active when using
// EncodedVideoFrameInjector.
class ProxyVideoTrack : public VideoTrackInterface {
 public:
  static scoped_refptr<ProxyVideoTrack> Create(
      TaskQueueBase* absl_nonnull worker_thread) {
    return scoped_refptr<ProxyVideoTrack>(
        new RefCountedObject<ProxyVideoTrack>(worker_thread));
  }

  // called on any thread
  void InjectBlackFrame(uint16_t width, uint16_t height) {
    if (worker_thread_->IsCurrent()) {
      InjectBlackFrameInternal(width, height);
    } else {
      worker_thread_->PostTask(
          [scoped_this = scoped_refptr<ProxyVideoTrack>(this), width, height] {
            scoped_this->InjectBlackFrameInternal(width, height);
          });
    }
  }

  // webrtc::VideoSourceInterface<webrtc::VideoFrame>
  void AddOrUpdateSink(VideoSinkInterface<VideoFrame>* sink,
                       const VideoSinkWants& wants) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    RTC_DCHECK(sink != nullptr);
    if (std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end()) {
      sinks_.push_back(sink);
    }
  }

  void RemoveSink(VideoSinkInterface<VideoFrame>* sink) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    RTC_DCHECK(sink != nullptr);
    std::erase(sinks_, sink);
  }

  void RequestRefreshFrame() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  }

  // webrtc::VideoTrackInterface / MediaStreamTrackInterface / NotifierInterface
  VideoTrackSourceInterface* GetSource() const override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    return nullptr;
  }

  std::string kind() const override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    return "video";
  }
  std::string id() const override {
    RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
    return "proxy_video_track";
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
  explicit ProxyVideoTrack(TaskQueueBase* absl_nonnull worker_thread)
      : worker_thread_(worker_thread) {
    RTC_DCHECK(worker_thread_);
  }
  ~ProxyVideoTrack() override = default;

 private:
  void InjectBlackFrameInternal(uint16_t width, uint16_t height) {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    if (sinks_.empty()) {
      return;
    }
    if (!black_buffer_ || black_buffer_->width() != width ||
        black_buffer_->height() != height) {
      black_buffer_ = I420Buffer::Create(width, height);
      I420Buffer::SetBlack(black_buffer_.get());
    }
    VideoFrame frame = VideoFrame::Builder()
                           .set_video_frame_buffer(black_buffer_)
                           .set_rotation(kVideoRotation_0)
                           .set_timestamp_us(TimeMicros())
                           .build();
    for (auto* sink : sinks_) {
      sink->OnFrame(frame);
    }
  }

  RTC_NO_UNIQUE_ADDRESS SequenceChecker signaling_sequence_checker_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_sequence_checker_{
      SequenceChecker::kDetached};
  TaskQueueBase* absl_nonnull const worker_thread_;
  std::vector<VideoSinkInterface<VideoFrame>*> sinks_
      RTC_GUARDED_BY(worker_sequence_checker_);
  scoped_refptr<I420Buffer> black_buffer_
      RTC_GUARDED_BY(worker_sequence_checker_);
};

// A placeholder video encoder that discards raw frames and instead passes
// injected encoded video frames to the WebRTC encoded video pipeline.
class ProxyVideoEncoder : public VideoEncoder {
 public:
  explicit ProxyVideoEncoder(scoped_refptr<EncodedVideoFrameInjector> injector)
      : injector_(injector), encoder_queue_(TaskQueueBase::Current()) {
    RTC_DCHECK(injector);
    RTC_DCHECK(encoder_queue_);
  }

  int InitEncode(const VideoCodec* codec_settings,
                 const VideoEncoder::Settings& settings) override {
    RTC_DCHECK_RUN_ON(encoder_queue_);
    codec_type_ = codec_settings->codecType;

    if (callback_ != nullptr) {
      injector_->RegisterEncoder(this);
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override {
    RTC_DCHECK_RUN_ON(encoder_queue_);
    callback_ = callback;

    injector_->RegisterEncoder(this);
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override {
    RTC_DCHECK_RUN_ON(encoder_queue_);
    injector_->UnregisterEncoder(this);
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Encode(const VideoFrame& frame,
                 const std::vector<VideoFrameType>* frame_types) override {
    RTC_DCHECK_RUN_ON(encoder_queue_);
    if (!frame_types) {
      return WEBRTC_VIDEO_CODEC_OK;
    }

    if (std::find(frame_types->begin(), frame_types->end(),
                  VideoFrameType::kVideoFrameKey) != frame_types->end()) {
      injector_->InvokeKeyFrameCallback();
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

  void SetRates(const RateControlParameters& parameters) override {
    RTC_DCHECK_RUN_ON(encoder_queue_);
    injector_->InvokeBitrateInfoCallback(
        parameters.bitrate.get_sum_bps(),
        static_cast<int32_t>(parameters.bandwidth_allocation.bps()));
  }

  EncoderInfo GetEncoderInfo() const override {
    RTC_DCHECK_RUN_ON(encoder_queue_);
    EncoderInfo info;
    info.implementation_name = "ProxyVideoEncoder";
    return info;
  }

  void InjectEncodedFrameOnQueue(EncodedImage encoded_image) {
    RTC_DCHECK_RUN_ON(encoder_queue_);
    if (!callback_) {
      return;
    }

    CodecSpecificInfo codec_specific_info;
    codec_specific_info.codecType = codec_type_;
    callback_->OnEncodedImage(encoded_image, &codec_specific_info);
  }

  // can be called on any thread
  void InjectEncodedFrame(
      std::unique_ptr<TransformableVideoFrameInterface> frame) {
    if (!frame) {
      return;
    }

    EncodedImage encoded_image;
    encoded_image.SetEncodedData(EncodedImageBuffer::Create(
        frame->GetData().data(), frame->GetData().size()));

    RtpTimestampInfo rtp_info = frame->GetRtpTimestampInfo();
    if (std::holds_alternative<RtpTimestampWithOffset>(rtp_info)) {
      encoded_image.SetRtpTimestamp(std::get<RtpTimestampWithOffset>(rtp_info));
    } else {
      encoded_image.SetRtpTimestamp(
          std::get<RtpTimestampWithoutOffset>(rtp_info));
    }

    std::optional<Timestamp> capture_time = frame->CaptureTime();
    if (capture_time.has_value() && capture_time->IsFinite()) {
      encoded_image.capture_time_ms_ = capture_time->ms();
    }
    encoded_image.set_frame_type(frame->IsKeyFrame()
                                     ? VideoFrameType::kVideoFrameKey
                                     : VideoFrameType::kVideoFrameDelta);

    encoder_queue_->PostTask(
        SafeTask(safety_.flag(),
                 [this, encoded_image = std::move(encoded_image)]() mutable {
                   InjectEncodedFrameOnQueue(std::move(encoded_image));
                 }));
  }

 private:
  const scoped_refptr<EncodedVideoFrameInjector> injector_;
  EncodedImageCallback* callback_ RTC_GUARDED_BY(encoder_queue_) =
      nullptr;  // Safe reference to the Encoder owner

  VideoCodecType codec_type_ RTC_GUARDED_BY(encoder_queue_);

  TaskQueueBase* const encoder_queue_;
  ScopedTaskSafety const safety_;
};

// A video encoder factory that creates ProxyVideoEncoder instances.
// Essentially just a wrapper of the EncodedVideoFrameInjector.
class ProxyVideoEncoderFactory : public VideoEncoderFactory {
 public:
  explicit ProxyVideoEncoderFactory(
      scoped_refptr<EncodedVideoFrameInjector> injector)
      : injector_(injector) {}

  ~ProxyVideoEncoderFactory() override {}

  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return {};
  }

  std::unique_ptr<VideoEncoder> Create(const Environment& env,
                                       const SdpVideoFormat& format) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return std::make_unique<ProxyVideoEncoder>(injector_);
  }

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_{
      SequenceChecker::kDetached};
  const scoped_refptr<EncodedVideoFrameInjector> injector_;
};

scoped_refptr<EncodedVideoFrameInjector> EncodedVideoFrameInjector::Create(
    KeyFrameCallback keyframe_callback,
    BitrateInfoCallback bitrate_callback,
    TaskQueueBase* absl_nonnull worker_thread) {
  return scoped_refptr<EncodedVideoFrameInjector>(
      new RefCountedObject<EncodedVideoFrameInjector>(
          std::move(keyframe_callback), std::move(bitrate_callback),
          worker_thread));
}

EncodedVideoFrameInjector::EncodedVideoFrameInjector(
    KeyFrameCallback keyframe_callback,
    BitrateInfoCallback bitrate_callback,
    TaskQueueBase* absl_nonnull worker_thread)
    : video_track_(ProxyVideoTrack::Create(worker_thread)),
      keyframe_callback_(std::move(keyframe_callback)),
      bitrate_callback_(std::move(bitrate_callback)) {
  RTC_DCHECK(worker_thread);
}

EncodedVideoFrameInjector::~EncodedVideoFrameInjector() {}

// Inject an encoded video frame.
// Requires to inject a raw frame into the track to simulate standard WebRTC
// video capture and processing. This ensures standard pipeline mechanisms like
// bitrate allocation remain active.
// Raw frames will ultimately be passed to the encoder and dropped.
// It is necessary to buffer the first encoded frames as the encoder is not
// allocated until a raw frame is injected. Once the encoder is registered with
// the injector, encoded frames are directly passed to it for injection. It is
// not strictly necessary to keep a 1:1 correlation between raw and encoded
// frames.
void EncodedVideoFrameInjector::InjectFrame(
    std::unique_ptr<TransformableVideoFrameInterface> encoded_frame) {
  if (!encoded_frame) {
    return;
  }

  uint16_t width = encoded_frame->Metadata().GetWidth();
  uint16_t height = encoded_frame->Metadata().GetHeight();
  video_track_->InjectBlackFrame(width, height);

  {
    MutexLock lock(&encoder_lock_);
    if (encoder_) {
      while (!buffered_frames_.empty()) {
        encoder_->InjectEncodedFrame(std::move(buffered_frames_.front()));
        buffered_frames_.pop_front();
      }
      encoder_->InjectEncodedFrame(std::move(encoded_frame));
    } else {
      if (buffered_frames_.size() >= kMaxBufferedFrames) {
        buffered_frames_.pop_front();
      }
      buffered_frames_.push_back(std::move(encoded_frame));
    }
  }
}

scoped_refptr<VideoTrackInterface> EncodedVideoFrameInjector::GetVideoTrack() {
  RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
  return video_track_;
}

absl_nonnull std::unique_ptr<VideoEncoderFactory>
EncodedVideoFrameInjector::CreateEncoderFactory() {
  RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
  return std::make_unique<ProxyVideoEncoderFactory>(
      scoped_refptr<EncodedVideoFrameInjector>(this));
}

void EncodedVideoFrameInjector::RegisterEncoder(ProxyVideoEncoder* encoder) {
  RTC_DCHECK_RUN_ON(&encoder_sequence_checker_);
  MutexLock lock(&encoder_lock_);
  RTC_CHECK(encoder);
  RTC_CHECK(!encoder_ || encoder_ == encoder);

  encoder_ = encoder;
}

void EncodedVideoFrameInjector::InvokeKeyFrameCallback() {
  RTC_DCHECK_RUN_ON(&encoder_sequence_checker_);
  if (keyframe_callback_) {
    keyframe_callback_();
  }
}
void EncodedVideoFrameInjector::InvokeBitrateInfoCallback(
    int32_t allocated_bitrate,
    int32_t available_outgoing_bitrate) {
  RTC_DCHECK_RUN_ON(&encoder_sequence_checker_);
  if (bitrate_callback_) {
    bitrate_callback_(allocated_bitrate, available_outgoing_bitrate);
  }
}

void EncodedVideoFrameInjector::UnregisterEncoder(ProxyVideoEncoder* encoder) {
  RTC_DCHECK_RUN_ON(&encoder_sequence_checker_);
  MutexLock lock(&encoder_lock_);
  RTC_CHECK(encoder_);
  RTC_CHECK(encoder);
  RTC_CHECK(encoder_ == encoder);

  encoder_ = nullptr;
}

}  // namespace webrtc
