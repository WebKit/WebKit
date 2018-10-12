/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_OBJC_DEVICE_INFO_H_
#define MODULES_VIDEO_CAPTURE_OBJC_DEVICE_INFO_H_

#include "modules/video_capture/device_info_impl.h"

#include <map>
#include <string>

namespace webrtc {
namespace videocapturemodule {
class DeviceInfoIos : public DeviceInfoImpl {
 public:
  DeviceInfoIos();
  ~DeviceInfoIos() override;

  // Implementation of DeviceInfoImpl.
  int32_t Init() override;
  uint32_t NumberOfDevices() override;
  int32_t GetDeviceName(uint32_t deviceNumber,
                        char* deviceNameUTF8,
                        uint32_t deviceNameLength,
                        char* deviceUniqueIdUTF8,
                        uint32_t deviceUniqueIdUTF8Length,
                        char* productUniqueIdUTF8 = 0,
                        uint32_t productUniqueIdUTF8Length = 0) override;

  int32_t NumberOfCapabilities(const char* deviceUniqueIdUTF8) override;

  int32_t GetCapability(const char* deviceUniqueIdUTF8,
                        const uint32_t deviceCapabilityNumber,
                        VideoCaptureCapability& capability) override;

  int32_t DisplayCaptureSettingsDialogBox(const char* deviceUniqueIdUTF8,
                                          const char* dialogTitleUTF8,
                                          void* parentWindow,
                                          uint32_t positionX,
                                          uint32_t positionY) override;

  int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                         VideoRotation& orientation) override;

  int32_t CreateCapabilityMap(const char* device_unique_id_utf8) override;

 private:
  std::map<std::string, VideoCaptureCapabilities> _capabilitiesMap;
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_OBJC_DEVICE_INFO_H_
