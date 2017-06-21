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

#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_capture/video_capture.h"
#include "webrtc/modules/video_capture/video_capture_config.h"

namespace webrtc
{

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
       const char* deviceUniqueIdUTF8);

    /*
     *   Create a video capture module object used for external capture.
     *
     *   id              - unique identifier of this video capture module object
     *   externalCapture - [out] interface to call when a new frame is captured.
     */
   static rtc::scoped_refptr<VideoCaptureModule> Create(
       VideoCaptureExternal*& externalCapture);

    static DeviceInfo* CreateDeviceInfo();

    // Helpers for converting between (integral) degrees and
    // VideoRotation values.  Return 0 on success.
    static int32_t RotationFromDegrees(int degrees, VideoRotation* rotation);
    static int32_t RotationInDegrees(VideoRotation rotation, int* degrees);

    //Call backs
    void RegisterCaptureDataCallback(
        rtc::VideoSinkInterface<VideoFrame>* dataCallback) override;
    void DeRegisterCaptureDataCallback() override;

    int32_t SetCaptureRotation(VideoRotation rotation) override;
    bool SetApplyRotation(bool enable) override;
    bool GetApplyRotation() override {
      return apply_rotation_;
    }

    const char* CurrentDeviceName() const override;

    // Implement VideoCaptureExternal
    // |capture_time| must be specified in NTP time format in milliseconds.
    int32_t IncomingFrame(uint8_t* videoFrame,
                          size_t videoFrameLength,
                          const VideoCaptureCapability& frameInfo,
                          int64_t captureTime = 0) override;

    // Platform dependent
    int32_t StartCapture(const VideoCaptureCapability& capability) override
    {
        _requestedCapability = capability;
        return -1;
    }
    int32_t StopCapture() override { return -1; }
    bool CaptureStarted() override {return false; }
    int32_t CaptureSettings(VideoCaptureCapability& /*settings*/) override
    { return -1; }

protected:
    VideoCaptureImpl();
    virtual ~VideoCaptureImpl();
    int32_t DeliverCapturedFrame(VideoFrame& captureFrame);

    char* _deviceUniqueId; // current Device unique name;
    rtc::CriticalSection _apiCs;
    VideoCaptureCapability _requestedCapability; // Should be set by platform dependent code in StartCapture.
private:
    void UpdateFrameCount();
    uint32_t CalculateFrameRate(int64_t now_ns);

    // last time the module process function was called.
    int64_t _lastProcessTimeNanos;
    // last time the frame rate callback function was called.
    int64_t _lastFrameRateCallbackTimeNanos;

    rtc::VideoSinkInterface<VideoFrame>* _dataCallBack;

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
