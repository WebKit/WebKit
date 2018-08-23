/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_FRAMEWORK_CLASSES_VIDEO_OBJCVIDEOTRACKSOURCE_H_
#define SDK_OBJC_FRAMEWORK_CLASSES_VIDEO_OBJCVIDEOTRACKSOURCE_H_

#import "WebRTC/RTCVideoCapturer.h"

#include "WebRTC/RTCMacros.h"
#include "media/base/adaptedvideotracksource.h"
#include "rtc_base/timestampaligner.h"

RTC_FWD_DECL_OBJC_CLASS(RTCVideoFrame);

@interface RTCObjCVideoSourceAdapter : NSObject<RTCVideoCapturerDelegate>
@end

namespace webrtc {

class ObjCVideoTrackSource : public rtc::AdaptedVideoTrackSource {
 public:
  ObjCVideoTrackSource();
  explicit ObjCVideoTrackSource(RTCObjCVideoSourceAdapter* adapter);

  // This class can not be used for implementing screen casting. Hopefully, this
  // function will be removed before we add that to iOS/Mac.
  bool is_screencast() const override;

  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  absl::optional<bool> needs_denoising() const override;

  SourceState state() const override;

  bool remote() const override;

  void OnCapturedFrame(RTCVideoFrame* frame);

  // Called by RTCVideoSource.
  void OnOutputFormatRequest(int width, int height, int fps);

 private:
  rtc::VideoBroadcaster broadcaster_;
  rtc::TimestampAligner timestamp_aligner_;

  RTCObjCVideoSourceAdapter* adapter_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_FRAMEWORK_CLASSES_VIDEO_OBJCVIDEOTRACKSOURCE_H_
