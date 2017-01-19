/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_IMPL_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_IMPL_H_

/*
 * video_capture_impl.h
 */

#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/common_video/rotation.h"
#include "webrtc/modules/video_capture/video_capture.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/video_frame.h"

namespace webrtc
{
class CriticalSectionWrapper;

namespace videocapturemodule {
// Class definitions
class VideoCaptureImpl: public VideoCaptureModule, public VideoCaptureExternal
{
public:

    /*
     *   Create a video capture module object
     *
     *   id              - unique identifier of this video capture module object
     *   deviceUniqueIdUTF8 -  name of the device. Available names can be found by using GetDeviceName
     */
   static rtc::scoped_refptr<VideoCaptureModule> Create(
       const int32_t id,
       const char* deviceUniqueIdUTF8);

    /*
     *   Create a video capture module object used for external capture.
     *
     *   id              - unique identifier of this video capture module object
     *   externalCapture - [out] interface to call when a new frame is captured.
     */
   static rtc::scoped_refptr<VideoCaptureModule> Create(
       const int32_t id,
       VideoCaptureExternal*& externalCapture);

    static DeviceInfo* CreateDeviceInfo(const int32_t id);

    // Helpers for converting between (integral) degrees and
    // VideoRotation values.  Return 0 on success.
    static int32_t RotationFromDegrees(int degrees, VideoRotation* rotation);
    static int32_t RotationInDegrees(VideoRotation rotation, int* degrees);

    //Call backs
    virtual void RegisterCaptureDataCallback(
        VideoCaptureDataCallback& dataCallback);
    virtual void DeRegisterCaptureDataCallback();
    virtual void RegisterCaptureCallback(VideoCaptureFeedBack& callBack);
    virtual void DeRegisterCaptureCallback();

    virtual void SetCaptureDelay(int32_t delayMS);
    virtual int32_t CaptureDelay();
    virtual int32_t SetCaptureRotation(VideoRotation rotation);
    virtual bool SetApplyRotation(bool enable);
    virtual bool GetApplyRotation() {
      return apply_rotation_;
    }

    virtual void EnableFrameRateCallback(const bool enable);
    virtual void EnableNoPictureAlarm(const bool enable);

    virtual const char* CurrentDeviceName() const;

    // Module handling
    virtual int64_t TimeUntilNextProcess();
    virtual void Process();

    // Implement VideoCaptureExternal
    // |capture_time| must be specified in NTP time format in milliseconds.
    virtual int32_t IncomingFrame(uint8_t* videoFrame,
                                  size_t videoFrameLength,
                                  const VideoCaptureCapability& frameInfo,
                                  int64_t captureTime = 0);

    // Platform dependent
    virtual int32_t StartCapture(const VideoCaptureCapability& capability)
    {
        _requestedCapability = capability;
        return -1;
    }
    virtual int32_t StopCapture()   { return -1; }
    virtual bool CaptureStarted() {return false; }
    virtual int32_t CaptureSettings(VideoCaptureCapability& /*settings*/)
    { return -1; }
    VideoCaptureEncodeInterface* GetEncodeInterface(const VideoCodec& /*codec*/)
    { return NULL; }

protected:
    VideoCaptureImpl(const int32_t id);
    virtual ~VideoCaptureImpl();
    int32_t DeliverCapturedFrame(VideoFrame& captureFrame);

    int32_t _id; // Module ID
    char* _deviceUniqueId; // current Device unique name;
    CriticalSectionWrapper& _apiCs;
    int32_t _captureDelay; // Current capture delay. May be changed of platform dependent parts.
    VideoCaptureCapability _requestedCapability; // Should be set by platform dependent code in StartCapture.
private:
    void UpdateFrameCount();
    uint32_t CalculateFrameRate(int64_t now_ns);

    CriticalSectionWrapper& _callBackCs;

    // last time the module process function was called.
    int64_t _lastProcessTimeNanos;
    // last time the frame rate callback function was called.
    int64_t _lastFrameRateCallbackTimeNanos;
    bool _frameRateCallBack; // true if EnableFrameRateCallback
    bool _noPictureAlarmCallBack; //true if EnableNoPictureAlarm
    VideoCaptureAlarm _captureAlarm; // current value of the noPictureAlarm

    int32_t _setCaptureDelay; // The currently used capture delay
    VideoCaptureDataCallback* _dataCallBack;
    VideoCaptureFeedBack* _captureCallBack;

    int64_t _lastProcessFrameTimeNanos;
    // timestamp for local captured frames
    int64_t _incomingFrameTimesNanos[kFrameRateCountHistorySize];
    VideoRotation _rotateFrame;  // Set if the frame should be rotated by the
                                 // capture module.

    // Indicate whether rotation should be applied before delivered externally.
    bool apply_rotation_;
};
}  // namespace videocapturemodule
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_IMPL_H_
