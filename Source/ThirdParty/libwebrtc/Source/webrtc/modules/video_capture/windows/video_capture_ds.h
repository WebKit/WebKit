/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_VIDEO_CAPTURE_DS_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_VIDEO_CAPTURE_DS_H_

#include "modules/video_capture/video_capture_impl.h"
#include "modules/video_capture/windows/device_info_ds.h"

#define CAPTURE_FILTER_NAME L"VideoCaptureFilter"
#define SINK_FILTER_NAME L"SinkFilter"

namespace webrtc {
namespace videocapturemodule {
// Forward declaraion
class CaptureSinkFilter;

class VideoCaptureDS : public VideoCaptureImpl {
 public:
  VideoCaptureDS();

  virtual int32_t Init(const char* deviceUniqueIdUTF8);

  /*************************************************************************
   *
   *   Start/Stop
   *
   *************************************************************************/
  virtual int32_t StartCapture(const VideoCaptureCapability& capability);
  virtual int32_t StopCapture();

  /**************************************************************************
   *
   *   Properties of the set device
   *
   **************************************************************************/

  virtual bool CaptureStarted();
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings);

 protected:
  virtual ~VideoCaptureDS();

  // Help functions

  int32_t SetCameraOutput(const VideoCaptureCapability& requestedCapability);
  int32_t DisconnectGraph();
  HRESULT ConnectDVCamera();

  DeviceInfoDS _dsInfo;

  IBaseFilter* _captureFilter;
  IGraphBuilder* _graphBuilder;
  IMediaControl* _mediaControl;
  CaptureSinkFilter* _sinkFilter;
  IPin* _inputSendPin;
  IPin* _outputCapturePin;

  // Microsoft DV interface (external DV cameras)
  IBaseFilter* _dvFilter;
  IPin* _inputDvPin;
  IPin* _outputDvPin;
};
}  // namespace videocapturemodule
}  // namespace webrtc
#endif  // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_VIDEO_CAPTURE_DS_H_
