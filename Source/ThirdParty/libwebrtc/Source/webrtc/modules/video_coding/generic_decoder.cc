/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/generic_decoder.h"

#include <algorithm>

#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/internal_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

VCMDecodedFrameCallback::VCMDecodedFrameCallback(VCMTiming* timing,
                                                 Clock* clock)
    : _clock(clock),
      _timing(timing),
      _timestampMap(kDecoderFrameMemoryLength),
      _lastReceivedPictureID(0) {
  ntp_offset_ =
      _clock->CurrentNtpInMilliseconds() - _clock->TimeInMilliseconds();
}

VCMDecodedFrameCallback::~VCMDecodedFrameCallback() {
}

void VCMDecodedFrameCallback::SetUserReceiveCallback(
    VCMReceiveCallback* receiveCallback) {
  RTC_DCHECK(construction_thread_.CalledOnValidThread());
  RTC_DCHECK((!_receiveCallback && receiveCallback) ||
             (_receiveCallback && !receiveCallback));
  _receiveCallback = receiveCallback;
}

VCMReceiveCallback* VCMDecodedFrameCallback::UserReceiveCallback() {
  // Called on the decode thread via VCMCodecDataBase::GetDecoder.
  // The callback must always have been set before this happens.
  RTC_DCHECK(_receiveCallback);
  return _receiveCallback;
}

int32_t VCMDecodedFrameCallback::Decoded(VideoFrame& decodedImage) {
  return Decoded(decodedImage, -1);
}

int32_t VCMDecodedFrameCallback::Decoded(VideoFrame& decodedImage,
                                         int64_t decode_time_ms) {
  Decoded(decodedImage,
          decode_time_ms >= 0 ? rtc::Optional<int32_t>(decode_time_ms)
                              : rtc::nullopt,
          rtc::nullopt);
  return WEBRTC_VIDEO_CODEC_OK;
}

void VCMDecodedFrameCallback::Decoded(VideoFrame& decodedImage,
                                      rtc::Optional<int32_t> decode_time_ms,
                                      rtc::Optional<uint8_t> qp) {
  RTC_DCHECK(_receiveCallback) << "Callback must not be null at this point";
  TRACE_EVENT_INSTANT1("webrtc", "VCMDecodedFrameCallback::Decoded",
                       "timestamp", decodedImage.timestamp());
  // TODO(holmer): We should improve this so that we can handle multiple
  // callbacks from one call to Decode().
  VCMFrameInformation* frameInfo;
  {
    rtc::CritScope cs(&lock_);
    frameInfo = _timestampMap.Pop(decodedImage.timestamp());
  }

  if (frameInfo == NULL) {
    RTC_LOG(LS_WARNING) << "Too many frames backed up in the decoder, dropping "
                           "this one.";
    return;
  }

  const int64_t now_ms = _clock->TimeInMilliseconds();
  if (!decode_time_ms) {
    decode_time_ms = now_ms - frameInfo->decodeStartTimeMs;
  }
  _timing->StopDecodeTimer(decodedImage.timestamp(), *decode_time_ms, now_ms,
                           frameInfo->renderTimeMs);

  // Report timing information.
  if (frameInfo->timing.flags != TimingFrameFlags::kInvalid) {
    int64_t capture_time_ms = decodedImage.ntp_time_ms() - ntp_offset_;
    // Convert remote timestamps to local time from ntp timestamps.
    frameInfo->timing.encode_start_ms -= ntp_offset_;
    frameInfo->timing.encode_finish_ms -= ntp_offset_;
    frameInfo->timing.packetization_finish_ms -= ntp_offset_;
    frameInfo->timing.pacer_exit_ms -= ntp_offset_;
    frameInfo->timing.network_timestamp_ms -= ntp_offset_;
    frameInfo->timing.network2_timestamp_ms -= ntp_offset_;

    int64_t sender_delta_ms = 0;
    if (decodedImage.ntp_time_ms() < 0) {
      // Sender clock is not estimated yet. Make sure that sender times are all
      // negative to indicate that. Yet they still should be relatively correct.
      sender_delta_ms =
          std::max({capture_time_ms, frameInfo->timing.encode_start_ms,
                    frameInfo->timing.encode_finish_ms,
                    frameInfo->timing.packetization_finish_ms,
                    frameInfo->timing.pacer_exit_ms,
                    frameInfo->timing.network_timestamp_ms,
                    frameInfo->timing.network2_timestamp_ms}) +
          1;
    }

    TimingFrameInfo timing_frame_info;

    timing_frame_info.capture_time_ms = capture_time_ms - sender_delta_ms;
    timing_frame_info.encode_start_ms =
        frameInfo->timing.encode_start_ms - sender_delta_ms;
    timing_frame_info.encode_finish_ms =
        frameInfo->timing.encode_finish_ms - sender_delta_ms;
    timing_frame_info.packetization_finish_ms =
        frameInfo->timing.packetization_finish_ms - sender_delta_ms;
    timing_frame_info.pacer_exit_ms =
        frameInfo->timing.pacer_exit_ms - sender_delta_ms;
    timing_frame_info.network_timestamp_ms =
        frameInfo->timing.network_timestamp_ms - sender_delta_ms;
    timing_frame_info.network2_timestamp_ms =
        frameInfo->timing.network2_timestamp_ms - sender_delta_ms;
    timing_frame_info.receive_start_ms = frameInfo->timing.receive_start_ms;
    timing_frame_info.receive_finish_ms = frameInfo->timing.receive_finish_ms;
    timing_frame_info.decode_start_ms = frameInfo->decodeStartTimeMs;
    timing_frame_info.decode_finish_ms = now_ms;
    timing_frame_info.render_time_ms = frameInfo->renderTimeMs;
    timing_frame_info.rtp_timestamp = decodedImage.timestamp();
    timing_frame_info.flags = frameInfo->timing.flags;

    _timing->SetTimingFrameInfo(timing_frame_info);
  }

  decodedImage.set_timestamp_us(
      frameInfo->renderTimeMs * rtc::kNumMicrosecsPerMillisec);
  decodedImage.set_rotation(frameInfo->rotation);
  _receiveCallback->FrameToRender(decodedImage, qp, frameInfo->content_type);
}

int32_t VCMDecodedFrameCallback::ReceivedDecodedReferenceFrame(
    const uint64_t pictureId) {
  return _receiveCallback->ReceivedDecodedReferenceFrame(pictureId);
}

int32_t VCMDecodedFrameCallback::ReceivedDecodedFrame(
    const uint64_t pictureId) {
  _lastReceivedPictureID = pictureId;
  return 0;
}

uint64_t VCMDecodedFrameCallback::LastReceivedPictureID() const {
  return _lastReceivedPictureID;
}

void VCMDecodedFrameCallback::OnDecoderImplementationName(
    const char* implementation_name) {
  _receiveCallback->OnDecoderImplementationName(implementation_name);
}

void VCMDecodedFrameCallback::Map(uint32_t timestamp,
                                  VCMFrameInformation* frameInfo) {
  rtc::CritScope cs(&lock_);
  _timestampMap.Add(timestamp, frameInfo);
}

int32_t VCMDecodedFrameCallback::Pop(uint32_t timestamp) {
  rtc::CritScope cs(&lock_);
  if (_timestampMap.Pop(timestamp) == NULL) {
    return VCM_GENERAL_ERROR;
  }
  return VCM_OK;
}

VCMGenericDecoder::VCMGenericDecoder(std::unique_ptr<VideoDecoder> decoder)
    : VCMGenericDecoder(decoder.release(), false /* isExternal */) {}

VCMGenericDecoder::VCMGenericDecoder(VideoDecoder* decoder, bool isExternal)
    : _callback(NULL),
      _frameInfos(),
      _nextFrameInfoIdx(0),
      decoder_(decoder),
      _codecType(kVideoCodecUnknown),
      _isExternal(isExternal),
      _last_keyframe_content_type(VideoContentType::UNSPECIFIED) {
  RTC_DCHECK(decoder_);
}

VCMGenericDecoder::~VCMGenericDecoder() {
  decoder_->Release();
  if (_isExternal)
    decoder_.release();
  RTC_DCHECK(_isExternal || decoder_);
}

int32_t VCMGenericDecoder::InitDecode(const VideoCodec* settings,
                                      int32_t numberOfCores) {
  TRACE_EVENT0("webrtc", "VCMGenericDecoder::InitDecode");
  _codecType = settings->codecType;

  return decoder_->InitDecode(settings, numberOfCores);
}

int32_t VCMGenericDecoder::Decode(const VCMEncodedFrame& frame, int64_t nowMs) {
    TRACE_EVENT1("webrtc", "VCMGenericDecoder::Decode", "timestamp",
                 frame.EncodedImage()._timeStamp);
    _frameInfos[_nextFrameInfoIdx].decodeStartTimeMs = nowMs;
    _frameInfos[_nextFrameInfoIdx].renderTimeMs = frame.RenderTimeMs();
    _frameInfos[_nextFrameInfoIdx].rotation = frame.rotation();
    _frameInfos[_nextFrameInfoIdx].timing = frame.video_timing();
    // Set correctly only for key frames. Thus, use latest key frame
    // content type. If the corresponding key frame was lost, decode will fail
    // and content type will be ignored.
    if (frame.FrameType() == kVideoFrameKey) {
      _frameInfos[_nextFrameInfoIdx].content_type = frame.contentType();
      _last_keyframe_content_type = frame.contentType();
    } else {
      _frameInfos[_nextFrameInfoIdx].content_type = _last_keyframe_content_type;
    }
    _callback->Map(frame.TimeStamp(), &_frameInfos[_nextFrameInfoIdx]);

    _nextFrameInfoIdx = (_nextFrameInfoIdx + 1) % kDecoderFrameMemoryLength;
    const RTPFragmentationHeader dummy_header;
    int32_t ret = decoder_->Decode(frame.EncodedImage(), frame.MissingFrame(),
                                   &dummy_header,
                                   frame.CodecSpecific(), frame.RenderTimeMs());

    _callback->OnDecoderImplementationName(decoder_->ImplementationName());
    if (ret < WEBRTC_VIDEO_CODEC_OK) {
      RTC_LOG(LS_WARNING) << "Failed to decode frame with timestamp "
                          << frame.TimeStamp() << ", error code: " << ret;
      _callback->Pop(frame.TimeStamp());
      return ret;
    } else if (ret == WEBRTC_VIDEO_CODEC_NO_OUTPUT ||
               ret == WEBRTC_VIDEO_CODEC_REQUEST_SLI) {
        // No output
        _callback->Pop(frame.TimeStamp());
    }
    return ret;
}

int32_t VCMGenericDecoder::RegisterDecodeCompleteCallback(
    VCMDecodedFrameCallback* callback) {
  _callback = callback;
  return decoder_->RegisterDecodeCompleteCallback(callback);
}

bool VCMGenericDecoder::PrefersLateDecoding() const {
  return decoder_->PrefersLateDecoding();
}

}  // namespace webrtc
