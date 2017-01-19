/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/video_capture_impl.h"

#include <stdlib.h>

#include "webrtc/base/refcount.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {
namespace videocapturemodule {
rtc::scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(
    const int32_t id,
    VideoCaptureExternal*& externalCapture) {
  rtc::scoped_refptr<VideoCaptureImpl> implementation(
      new rtc::RefCountedObject<VideoCaptureImpl>(id));
  externalCapture = implementation.get();
  return implementation;
}

const char* VideoCaptureImpl::CurrentDeviceName() const
{
    return _deviceUniqueId;
}

// static
int32_t VideoCaptureImpl::RotationFromDegrees(int degrees,
                                              VideoRotation* rotation) {
  switch (degrees) {
    case 0:
      *rotation = kVideoRotation_0;
      return 0;
    case 90:
      *rotation = kVideoRotation_90;
      return 0;
    case 180:
      *rotation = kVideoRotation_180;
      return 0;
    case 270:
      *rotation = kVideoRotation_270;
      return 0;
    default:
      return -1;;
  }
}

// static
int32_t VideoCaptureImpl::RotationInDegrees(VideoRotation rotation,
                                            int* degrees) {
  switch (rotation) {
    case kVideoRotation_0:
      *degrees = 0;
      return 0;
    case kVideoRotation_90:
      *degrees = 90;
      return 0;
    case kVideoRotation_180:
      *degrees = 180;
      return 0;
    case kVideoRotation_270:
      *degrees = 270;
      return 0;
  }
  return -1;
}

// returns the number of milliseconds until the module want a worker thread to call Process
int64_t VideoCaptureImpl::TimeUntilNextProcess()
{
    CriticalSectionScoped cs(&_callBackCs);
    const int64_t kProcessIntervalMs = 300;
    return kProcessIntervalMs -
           (rtc::TimeNanos() - _lastProcessTimeNanos) /
           rtc::kNumNanosecsPerMillisec;
}

// Process any pending tasks such as timeouts
void VideoCaptureImpl::Process()
{
    CriticalSectionScoped cs(&_callBackCs);

    const int64_t now_ns = rtc::TimeNanos();
    _lastProcessTimeNanos = rtc::TimeNanos();

    // Handle No picture alarm

    if (_lastProcessFrameTimeNanos == _incomingFrameTimesNanos[0] &&
        _captureAlarm != Raised)
    {
        if (_noPictureAlarmCallBack && _captureCallBack)
        {
            _captureAlarm = Raised;
            _captureCallBack->OnNoPictureAlarm(_id, _captureAlarm);
        }
    }
    else if (_lastProcessFrameTimeNanos != _incomingFrameTimesNanos[0] &&
             _captureAlarm != Cleared)
    {
        if (_noPictureAlarmCallBack && _captureCallBack)
        {
            _captureAlarm = Cleared;
            _captureCallBack->OnNoPictureAlarm(_id, _captureAlarm);

        }
    }

    // Handle frame rate callback
    if ((now_ns - _lastFrameRateCallbackTimeNanos) /
        rtc::kNumNanosecsPerMillisec
        > kFrameRateCallbackInterval)
    {
        if (_frameRateCallBack && _captureCallBack)
        {
            const uint32_t frameRate = CalculateFrameRate(now_ns);
            _captureCallBack->OnCaptureFrameRate(_id, frameRate);
        }
        // Can be set by EnableFrameRateCallback
        _lastFrameRateCallbackTimeNanos = now_ns;

    }

    _lastProcessFrameTimeNanos = _incomingFrameTimesNanos[0];
}

VideoCaptureImpl::VideoCaptureImpl(const int32_t id)
    : _id(id),
      _deviceUniqueId(NULL),
      _apiCs(*CriticalSectionWrapper::CreateCriticalSection()),
      _captureDelay(0),
      _requestedCapability(),
      _callBackCs(*CriticalSectionWrapper::CreateCriticalSection()),
      _lastProcessTimeNanos(rtc::TimeNanos()),
      _lastFrameRateCallbackTimeNanos(rtc::TimeNanos()),
      _frameRateCallBack(false),
      _noPictureAlarmCallBack(false),
      _captureAlarm(Cleared),
      _setCaptureDelay(0),
      _dataCallBack(NULL),
      _captureCallBack(NULL),
      _lastProcessFrameTimeNanos(rtc::TimeNanos()),
      _rotateFrame(kVideoRotation_0),
      apply_rotation_(false) {
    _requestedCapability.width = kDefaultWidth;
    _requestedCapability.height = kDefaultHeight;
    _requestedCapability.maxFPS = 30;
    _requestedCapability.rawType = kVideoI420;
    _requestedCapability.codecType = kVideoCodecUnknown;
    memset(_incomingFrameTimesNanos, 0, sizeof(_incomingFrameTimesNanos));
}

VideoCaptureImpl::~VideoCaptureImpl()
{
    DeRegisterCaptureDataCallback();
    DeRegisterCaptureCallback();
    delete &_callBackCs;
    delete &_apiCs;

    if (_deviceUniqueId)
        delete[] _deviceUniqueId;
}

void VideoCaptureImpl::RegisterCaptureDataCallback(
    VideoCaptureDataCallback& dataCallBack) {
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _dataCallBack = &dataCallBack;
}

void VideoCaptureImpl::DeRegisterCaptureDataCallback() {
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _dataCallBack = NULL;
}
void VideoCaptureImpl::RegisterCaptureCallback(VideoCaptureFeedBack& callBack) {

    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _captureCallBack = &callBack;
}
void VideoCaptureImpl::DeRegisterCaptureCallback() {

    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _captureCallBack = NULL;
}
void VideoCaptureImpl::SetCaptureDelay(int32_t delayMS) {
    CriticalSectionScoped cs(&_apiCs);
    _captureDelay = delayMS;
}
int32_t VideoCaptureImpl::CaptureDelay()
{
    CriticalSectionScoped cs(&_apiCs);
    return _setCaptureDelay;
}

int32_t VideoCaptureImpl::DeliverCapturedFrame(VideoFrame& captureFrame) {
  UpdateFrameCount();  // frame count used for local frame rate callback.

  const bool callOnCaptureDelayChanged = _setCaptureDelay != _captureDelay;
  // Capture delay changed
  if (_setCaptureDelay != _captureDelay) {
      _setCaptureDelay = _captureDelay;
  }

  if (_dataCallBack) {
    if (callOnCaptureDelayChanged) {
      _dataCallBack->OnCaptureDelayChanged(_id, _captureDelay);
    }
    _dataCallBack->OnIncomingCapturedFrame(_id, captureFrame);
  }

  return 0;
}

int32_t VideoCaptureImpl::IncomingFrame(
    uint8_t* videoFrame,
    size_t videoFrameLength,
    const VideoCaptureCapability& frameInfo,
    int64_t captureTime/*=0*/)
{
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);

    const int32_t width = frameInfo.width;
    const int32_t height = frameInfo.height;

    TRACE_EVENT1("webrtc", "VC::IncomingFrame", "capture_time", captureTime);

    if (frameInfo.codecType == kVideoCodecUnknown)
    {
        // Not encoded, convert to I420.
        const VideoType commonVideoType =
                  RawVideoTypeToCommonVideoVideoType(frameInfo.rawType);

        if (frameInfo.rawType != kVideoMJPEG &&
            CalcBufferSize(commonVideoType, width,
                           abs(height)) != videoFrameLength)
        {
            LOG(LS_ERROR) << "Wrong incoming frame length.";
            return -1;
        }

        int stride_y = width;
        int stride_uv = (width + 1) / 2;
        int target_width = width;
        int target_height = height;

        // SetApplyRotation doesn't take any lock. Make a local copy here.
        bool apply_rotation = apply_rotation_;

        if (apply_rotation) {
          // Rotating resolution when for 90/270 degree rotations.
          if (_rotateFrame == kVideoRotation_90 ||
              _rotateFrame == kVideoRotation_270) {
            target_width = abs(height);
            target_height = width;
          }
        }

        // Setting absolute height (in case it was negative).
        // In Windows, the image starts bottom left, instead of top left.
        // Setting a negative source height, inverts the image (within LibYuv).

        // TODO(nisse): Use a pool?
        rtc::scoped_refptr<I420Buffer> buffer = I420Buffer::Create(
            target_width, abs(target_height), stride_y, stride_uv, stride_uv);
        const int conversionResult = ConvertToI420(
            commonVideoType, videoFrame, 0, 0,  // No cropping
            width, height, videoFrameLength,
            apply_rotation ? _rotateFrame : kVideoRotation_0, buffer.get());
        if (conversionResult < 0)
        {
          LOG(LS_ERROR) << "Failed to convert capture frame from type "
                        << frameInfo.rawType << "to I420.";
            return -1;
        }

        VideoFrame captureFrame(
            buffer, 0, rtc::TimeMillis(),
            !apply_rotation ? _rotateFrame : kVideoRotation_0);
        captureFrame.set_ntp_time_ms(captureTime);

        DeliverCapturedFrame(captureFrame);
    }
    else // Encoded format
    {
        assert(false);
        return -1;
    }

    return 0;
}

int32_t VideoCaptureImpl::SetCaptureRotation(VideoRotation rotation) {
  CriticalSectionScoped cs(&_apiCs);
  CriticalSectionScoped cs2(&_callBackCs);
  _rotateFrame = rotation;
  return 0;
}

void VideoCaptureImpl::EnableFrameRateCallback(const bool enable) {
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _frameRateCallBack = enable;
    if (enable)
    {
      _lastFrameRateCallbackTimeNanos = rtc::TimeNanos();
    }
}

bool VideoCaptureImpl::SetApplyRotation(bool enable) {
  // We can't take any lock here as it'll cause deadlock with IncomingFrame.

  // The effect of this is the last caller wins.
  apply_rotation_ = enable;
  return true;
}

void VideoCaptureImpl::EnableNoPictureAlarm(const bool enable) {
    CriticalSectionScoped cs(&_apiCs);
    CriticalSectionScoped cs2(&_callBackCs);
    _noPictureAlarmCallBack = enable;
}

void VideoCaptureImpl::UpdateFrameCount()
{
  if (_incomingFrameTimesNanos[0] / rtc::kNumNanosecsPerMicrosec == 0)
    {
        // first no shift
    }
    else
    {
        // shift
        for (int i = (kFrameRateCountHistorySize - 2); i >= 0; i--)
        {
            _incomingFrameTimesNanos[i + 1] = _incomingFrameTimesNanos[i];
        }
    }
    _incomingFrameTimesNanos[0] = rtc::TimeNanos();
}

uint32_t VideoCaptureImpl::CalculateFrameRate(int64_t now_ns)
{
    int32_t num = 0;
    int32_t nrOfFrames = 0;
    for (num = 1; num < (kFrameRateCountHistorySize - 1); num++)
    {
        if (_incomingFrameTimesNanos[num] <= 0 ||
            (now_ns - _incomingFrameTimesNanos[num]) /
            rtc::kNumNanosecsPerMillisec >
                kFrameRateHistoryWindowMs) // don't use data older than 2sec
        {
            break;
        }
        else
        {
            nrOfFrames++;
        }
    }
    if (num > 1)
    {
        int64_t diff = (now_ns - _incomingFrameTimesNanos[num - 1]) /
                       rtc::kNumNanosecsPerMillisec;
        if (diff > 0)
        {
            return uint32_t((nrOfFrames * 1000.0f / diff) + 0.5f);
        }
    }

    return nrOfFrames;
}
}  // namespace videocapturemodule
}  // namespace webrtc
