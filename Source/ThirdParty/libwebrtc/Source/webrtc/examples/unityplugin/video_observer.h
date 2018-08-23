/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_UNITYPLUGIN_VIDEO_OBSERVER_H_
#define EXAMPLES_UNITYPLUGIN_VIDEO_OBSERVER_H_

#include <mutex>

#include "api/mediastreaminterface.h"
#include "api/video/video_sink_interface.h"
#include "examples/unityplugin/unity_plugin_apis.h"

class VideoObserver : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  VideoObserver() {}
  ~VideoObserver() {}
  void SetVideoCallback(I420FRAMEREADY_CALLBACK callback);

 protected:
  // VideoSinkInterface implementation
  void OnFrame(const webrtc::VideoFrame& frame) override;

 private:
  I420FRAMEREADY_CALLBACK OnI420FrameReady = nullptr;
  std::mutex mutex;
};

#endif  // EXAMPLES_UNITYPLUGIN_VIDEO_OBSERVER_H_
