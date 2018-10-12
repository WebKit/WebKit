/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_
#define MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_

#include "modules/video_coding/include/video_coding.h"

#include <memory>
#include <string>
#include <vector>

#include "common_video/include/frame_callback.h"
#include "modules/video_coding/decoder_database.h"
#include "modules/video_coding/encoder_database.h"
#include "modules/video_coding/frame_buffer.h"
#include "modules/video_coding/generic_decoder.h"
#include "modules/video_coding/generic_encoder.h"
#include "modules/video_coding/jitter_buffer.h"
#include "modules/video_coding/media_optimization.h"
#include "modules/video_coding/receiver.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/onetimeevent.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class VideoBitrateAllocator;
class VideoBitrateAllocationObserver;

namespace vcm {

class VCMProcessTimer {
 public:
  static const int64_t kDefaultProcessIntervalMs = 1000;

  VCMProcessTimer(int64_t periodMs, Clock* clock)
      : _clock(clock),
        _periodMs(periodMs),
        _latestMs(_clock->TimeInMilliseconds()) {}
  int64_t Period() const;
  int64_t TimeUntilProcess() const;
  void Processed();

 private:
  Clock* _clock;
  int64_t _periodMs;
  int64_t _latestMs;
};

class VideoSender {
 public:
  typedef VideoCodingModule::SenderNackMode SenderNackMode;

  VideoSender(Clock* clock, EncodedImageCallback* post_encode_callback);

  ~VideoSender();

  // Register the send codec to be used.
  // This method must be called on the construction thread.
  int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                            uint32_t numberOfCores,
                            uint32_t maxPayloadSize);

  void RegisterExternalEncoder(VideoEncoder* externalEncoder,
                               bool internalSource);

  // Update the channel parameters based on new rates and rtt. This will also
  // cause an immediate call to VideoEncoder::SetRateAllocation().
  int32_t SetChannelParameters(
      uint32_t target_bitrate_bps,
      uint8_t loss_rate,
      int64_t rtt,
      VideoBitrateAllocator* bitrate_allocator,
      VideoBitrateAllocationObserver* bitrate_updated_callback);

  // Updates the channel parameters with a new bitrate allocation, but using the
  // current targit_bitrate, loss rate and rtt. That is, the distribution or
  // caps may be updated to a change to a new VideoCodec or allocation mode.
  // The new parameters will be stored as pending EncoderParameters, and the
  // encoder will only be updated on the next frame.
  void UpdateChannelParameters(
      VideoBitrateAllocator* bitrate_allocator,
      VideoBitrateAllocationObserver* bitrate_updated_callback);

  int32_t AddVideoFrame(const VideoFrame& videoFrame,
                        const CodecSpecificInfo* codecSpecificInfo);

  int32_t IntraFrameRequest(size_t stream_index);
  int32_t EnableFrameDropper(bool enable);

 private:
  EncoderParameters UpdateEncoderParameters(
      const EncoderParameters& params,
      VideoBitrateAllocator* bitrate_allocator,
      uint32_t target_bitrate_bps);
  void SetEncoderParameters(EncoderParameters params, bool has_internal_source)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(encoder_crit_);

  rtc::CriticalSection encoder_crit_;
  VCMGenericEncoder* _encoder;
  media_optimization::MediaOptimization _mediaOpt;
  VCMEncodedFrameCallback _encodedFrameCallback RTC_GUARDED_BY(encoder_crit_);
  EncodedImageCallback* const post_encode_callback_;
  VCMEncoderDataBase _codecDataBase RTC_GUARDED_BY(encoder_crit_);
  bool frame_dropper_enabled_ RTC_GUARDED_BY(encoder_crit_);

  // Must be accessed on the construction thread of VideoSender.
  VideoCodec current_codec_;
  rtc::SequencedTaskChecker sequenced_checker_;

  rtc::CriticalSection params_crit_;
  EncoderParameters encoder_params_ RTC_GUARDED_BY(params_crit_);
  bool encoder_has_internal_source_ RTC_GUARDED_BY(params_crit_);
  std::vector<FrameType> next_frame_types_ RTC_GUARDED_BY(params_crit_);
};

class VideoReceiver : public Module {
 public:
  VideoReceiver(Clock* clock,
                EventFactory* event_factory,
                VCMTiming* timing,
                NackSender* nack_sender = nullptr,
                KeyFrameRequestSender* keyframe_request_sender = nullptr);
  ~VideoReceiver() override;

  int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                               int32_t numberOfCores,
                               bool requireKeyFrame);

  void RegisterExternalDecoder(VideoDecoder* externalDecoder,
                               uint8_t payloadType);
  int32_t RegisterReceiveCallback(VCMReceiveCallback* receiveCallback);
  int32_t RegisterReceiveStatisticsCallback(
      VCMReceiveStatisticsCallback* receiveStats);
  int32_t RegisterFrameTypeCallback(VCMFrameTypeCallback* frameTypeCallback);
  int32_t RegisterPacketRequestCallback(VCMPacketRequestCallback* callback);

  int32_t Decode(uint16_t maxWaitTimeMs);

  int32_t Decode(const webrtc::VCMEncodedFrame* frame);

  int32_t IncomingPacket(const uint8_t* incomingPayload,
                         size_t payloadLength,
                         const WebRtcRTPHeader& rtpInfo);
  int32_t SetMinimumPlayoutDelay(uint32_t minPlayoutDelayMs);
  int32_t SetRenderDelay(uint32_t timeMS);
  int32_t Delay() const;

  // DEPRECATED.
  int SetReceiverRobustnessMode(
      VideoCodingModule::ReceiverRobustness robustnessMode,
      VCMDecodeErrorMode errorMode);

  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms);

  void SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode);

  int32_t SetReceiveChannelParameters(int64_t rtt);
  int32_t SetVideoProtection(VCMVideoProtection videoProtection, bool enable);

  int64_t TimeUntilNextProcess() override;
  void Process() override;
  void ProcessThreadAttached(ProcessThread* process_thread) override;

  void TriggerDecoderShutdown();

  // Notification methods that are used to check our internal state and validate
  // threading assumptions. These are called by VideoReceiveStream.
  // See |IsDecoderThreadRunning()| for more details.
  void DecoderThreadStarting();
  void DecoderThreadStopped();

 protected:
  int32_t Decode(const webrtc::VCMEncodedFrame& frame);
  int32_t RequestKeyFrame();

 private:
  // Used for DCHECKing thread correctness.
  // In build where DCHECKs are enabled, will return false before
  // DecoderThreadStarting is called, then true until DecoderThreadStopped
  // is called.
  // In builds where DCHECKs aren't enabled, it will return true.
  bool IsDecoderThreadRunning();

  rtc::ThreadChecker construction_thread_checker_;
  rtc::ThreadChecker decoder_thread_checker_;
  rtc::ThreadChecker module_thread_checker_;
  Clock* const clock_;
  rtc::CriticalSection process_crit_;
  VCMTiming* _timing;
  VCMReceiver _receiver;
  VCMDecodedFrameCallback _decodedFrameCallback;

  // These callbacks are set on the construction thread before being attached
  // to the module thread or decoding started, so a lock is not required.
  VCMFrameTypeCallback* _frameTypeCallback;
  VCMReceiveStatisticsCallback* _receiveStatsCallback;
  VCMPacketRequestCallback* _packetRequestCallback;

  // Used on both the module and decoder thread.
  bool _scheduleKeyRequest RTC_GUARDED_BY(process_crit_);
  bool drop_frames_until_keyframe_ RTC_GUARDED_BY(process_crit_);

  // Modified on the construction thread while not attached to the process
  // thread.  Once attached to the process thread, its value is only read
  // so a lock is not required.
  size_t max_nack_list_size_;

  // Callbacks are set before the decoder thread starts.
  // Once the decoder thread has been started, usage of |_codecDataBase| moves
  // over to the decoder thread.
  VCMDecoderDataBase _codecDataBase;

  VCMProcessTimer _receiveStatsTimer RTC_GUARDED_BY(module_thread_checker_);
  VCMProcessTimer _retransmissionTimer RTC_GUARDED_BY(module_thread_checker_);
  VCMProcessTimer _keyRequestTimer RTC_GUARDED_BY(module_thread_checker_);
  ThreadUnsafeOneTimeEvent first_frame_received_
      RTC_GUARDED_BY(decoder_thread_checker_);
  // Modified on the construction thread. Can be read without a lock and assumed
  // to be non-null on the module and decoder threads.
  ProcessThread* process_thread_ = nullptr;
  bool is_attached_to_process_thread_
      RTC_GUARDED_BY(construction_thread_checker_) = false;
#if RTC_DCHECK_IS_ON
  bool decoder_thread_is_running_ = false;
#endif
};

}  // namespace vcm
}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_
