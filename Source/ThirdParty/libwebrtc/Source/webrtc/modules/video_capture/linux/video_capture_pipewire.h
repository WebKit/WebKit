/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_PIPEWIRE_H_
#define MODULES_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_PIPEWIRE_H_

#include "modules/video_capture/linux/pipewire_session.h"
#include "modules/video_capture/video_capture_defines.h"
#include "modules/video_capture/video_capture_impl.h"

namespace webrtc {
namespace videocapturemodule {
class VideoCaptureModulePipeWire : public VideoCaptureImpl {
 public:
  explicit VideoCaptureModulePipeWire(VideoCaptureOptions* options);
  ~VideoCaptureModulePipeWire() override;
  int32_t Init(const char* deviceUniqueId);
  int32_t StartCapture(const VideoCaptureCapability& capability) override;
  int32_t StopCapture() override;
  bool CaptureStarted() override;
  int32_t CaptureSettings(VideoCaptureCapability& settings) override;

  static VideoType PipeWireRawFormatToVideoType(uint32_t format);

 private:
  static void OnStreamParamChanged(void* data,
                                   uint32_t id,
                                   const struct spa_pod* format);
  static void OnStreamStateChanged(void* data,
                                   pw_stream_state old_state,
                                   pw_stream_state state,
                                   const char* error_message);

  static void OnStreamProcess(void* data);

  void OnFormatChanged(const struct spa_pod* format);
  void ProcessBuffers();

  rtc::scoped_refptr<PipeWireSession> session_;
  int node_id_;
  VideoCaptureCapability frameInfo_;
  bool started_;

  struct pw_stream* stream_;
  struct spa_hook stream_listener_;
};
}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_PIPEWIRE_H_
