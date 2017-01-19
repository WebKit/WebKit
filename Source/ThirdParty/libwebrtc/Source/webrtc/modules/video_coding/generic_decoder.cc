/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/generic_decoder.h"
#include "webrtc/modules/video_coding/internal_defines.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

VCMDecodedFrameCallback::VCMDecodedFrameCallback(VCMTiming* timing,
                                                 Clock* clock)
    : _critSect(CriticalSectionWrapper::CreateCriticalSection()),
      _clock(clock),
      _receiveCallback(NULL),
      _timing(timing),
      _timestampMap(kDecoderFrameMemoryLength),
      _lastReceivedPictureID(0) {}

VCMDecodedFrameCallback::~VCMDecodedFrameCallback() {
  delete _critSect;
}

void VCMDecodedFrameCallback::SetUserReceiveCallback(
    VCMReceiveCallback* receiveCallback) {
  CriticalSectionScoped cs(_critSect);
  _receiveCallback = receiveCallback;
}

VCMReceiveCallback* VCMDecodedFrameCallback::UserReceiveCallback() {
  CriticalSectionScoped cs(_critSect);
  return _receiveCallback;
}

int32_t VCMDecodedFrameCallback::Decoded(VideoFrame& decodedImage) {
  return Decoded(decodedImage, -1);
}

int32_t VCMDecodedFrameCallback::Decoded(VideoFrame& decodedImage,
                                         int64_t decode_time_ms) {
  TRACE_EVENT_INSTANT1("webrtc", "VCMDecodedFrameCallback::Decoded",
                       "timestamp", decodedImage.timestamp());
  // TODO(holmer): We should improve this so that we can handle multiple
  // callbacks from one call to Decode().
  VCMFrameInformation* frameInfo;
  VCMReceiveCallback* callback;
  {
    CriticalSectionScoped cs(_critSect);
    frameInfo = _timestampMap.Pop(decodedImage.timestamp());
    callback = _receiveCallback;
  }

  if (frameInfo == NULL) {
    LOG(LS_WARNING) << "Too many frames backed up in the decoder, dropping "
                       "this one.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  const int64_t now_ms = _clock->TimeInMilliseconds();
  if (decode_time_ms < 0) {
    decode_time_ms =
        static_cast<int32_t>(now_ms - frameInfo->decodeStartTimeMs);
  }
  _timing->StopDecodeTimer(decodedImage.timestamp(), decode_time_ms, now_ms,
                           frameInfo->renderTimeMs);

  decodedImage.set_render_time_ms(frameInfo->renderTimeMs);
  decodedImage.set_rotation(frameInfo->rotation);
  // TODO(sakal): Investigate why callback is NULL sometimes and replace if
  // statement with a DCHECK.
  if (callback) {
    callback->FrameToRender(decodedImage);
  } else {
    LOG(LS_WARNING) << "No callback, dropping frame.";
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VCMDecodedFrameCallback::ReceivedDecodedReferenceFrame(
    const uint64_t pictureId) {
  CriticalSectionScoped cs(_critSect);
  if (_receiveCallback != NULL) {
    return _receiveCallback->ReceivedDecodedReferenceFrame(pictureId);
  }
  return -1;
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
  CriticalSectionScoped cs(_critSect);
  if (_receiveCallback)
    _receiveCallback->OnDecoderImplementationName(implementation_name);
}

void VCMDecodedFrameCallback::Map(uint32_t timestamp,
                                  VCMFrameInformation* frameInfo) {
  CriticalSectionScoped cs(_critSect);
  _timestampMap.Add(timestamp, frameInfo);
}

int32_t VCMDecodedFrameCallback::Pop(uint32_t timestamp) {
  CriticalSectionScoped cs(_critSect);
  if (_timestampMap.Pop(timestamp) == NULL) {
    return VCM_GENERAL_ERROR;
  }
  return VCM_OK;
}

VCMGenericDecoder::VCMGenericDecoder(VideoDecoder* decoder, bool isExternal)
    : _callback(NULL),
      _frameInfos(),
      _nextFrameInfoIdx(0),
      _decoder(decoder),
      _codecType(kVideoCodecUnknown),
      _isExternal(isExternal),
      _keyFrameDecoded(false) {}

VCMGenericDecoder::~VCMGenericDecoder() {}

int32_t VCMGenericDecoder::InitDecode(const VideoCodec* settings,
                                      int32_t numberOfCores) {
  TRACE_EVENT0("webrtc", "VCMGenericDecoder::InitDecode");
  _codecType = settings->codecType;

  return _decoder->InitDecode(settings, numberOfCores);
}

int32_t VCMGenericDecoder::Decode(const VCMEncodedFrame& frame, int64_t nowMs) {
    TRACE_EVENT1("webrtc", "VCMGenericDecoder::Decode", "timestamp",
                 frame.EncodedImage()._timeStamp);
    _frameInfos[_nextFrameInfoIdx].decodeStartTimeMs = nowMs;
    _frameInfos[_nextFrameInfoIdx].renderTimeMs = frame.RenderTimeMs();
    _frameInfos[_nextFrameInfoIdx].rotation = frame.rotation();
    _callback->Map(frame.TimeStamp(), &_frameInfos[_nextFrameInfoIdx]);

    _nextFrameInfoIdx = (_nextFrameInfoIdx + 1) % kDecoderFrameMemoryLength;
    const RTPFragmentationHeader dummy_header;
    int32_t ret = _decoder->Decode(frame.EncodedImage(), frame.MissingFrame(),
                                   &dummy_header,
                                   frame.CodecSpecific(), frame.RenderTimeMs());

    _callback->OnDecoderImplementationName(_decoder->ImplementationName());
    if (ret < WEBRTC_VIDEO_CODEC_OK) {
        LOG(LS_WARNING) << "Failed to decode frame with timestamp "
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

int32_t VCMGenericDecoder::Release() {
  return _decoder->Release();
}

int32_t VCMGenericDecoder::RegisterDecodeCompleteCallback(
    VCMDecodedFrameCallback* callback) {
  _callback = callback;
  return _decoder->RegisterDecodeCompleteCallback(callback);
}

bool VCMGenericDecoder::External() const {
  return _isExternal;
}

bool VCMGenericDecoder::PrefersLateDecoding() const {
  return _decoder->PrefersLateDecoding();
}

}  // namespace webrtc
