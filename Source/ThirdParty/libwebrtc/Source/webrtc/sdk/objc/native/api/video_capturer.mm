/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/native/api/video_capturer.h"

#include "absl/memory/memory.h"
#include "api/videosourceproxy.h"
#include "sdk/objc/native/src/objc_video_track_source.h"

namespace webrtc {

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> ObjCToNativeVideoCapturer(
    RTCVideoCapturer *objc_video_capturer,
    rtc::Thread *signaling_thread,
    rtc::Thread *worker_thread) {
  RTCObjCVideoSourceAdapter *adapter = [[RTCObjCVideoSourceAdapter alloc] init];
  rtc::scoped_refptr<webrtc::ObjCVideoTrackSource> objc_video_track_source(
      new rtc::RefCountedObject<webrtc::ObjCVideoTrackSource>(adapter));
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source =
      webrtc::VideoTrackSourceProxy::Create(
          signaling_thread, worker_thread, objc_video_track_source);

  objc_video_capturer.delegate = adapter;

  return video_source;
}

}  // namespace webrtc
