/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_DEVICE_INFO_LINUX_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_DEVICE_INFO_LINUX_H_

#include "modules/video_capture/device_info_impl.h"
#include "modules/video_capture/video_capture_impl.h"

namespace webrtc
{
namespace videocapturemodule
{
class DeviceInfoLinux: public DeviceInfoImpl
{
public:
    DeviceInfoLinux();
    virtual ~DeviceInfoLinux();
    virtual uint32_t NumberOfDevices();
    virtual int32_t GetDeviceName(
        uint32_t deviceNumber,
        char* deviceNameUTF8,
        uint32_t deviceNameLength,
        char* deviceUniqueIdUTF8,
        uint32_t deviceUniqueIdUTF8Length,
        char* productUniqueIdUTF8=0,
        uint32_t productUniqueIdUTF8Length=0);
    /*
    * Fills the membervariable _captureCapabilities with capabilites for the given device name.
    */
    virtual int32_t CreateCapabilityMap (const char* deviceUniqueIdUTF8);
    virtual int32_t DisplayCaptureSettingsDialogBox(
        const char* /*deviceUniqueIdUTF8*/,
        const char* /*dialogTitleUTF8*/,
        void* /*parentWindow*/,
        uint32_t /*positionX*/,
        uint32_t /*positionY*/) { return -1;}
    int32_t FillCapabilities(int fd);
    int32_t Init();
private:

    bool IsDeviceNameMatches(const char* name, const char* deviceUniqueIdUTF8);
};
}  // namespace videocapturemodule
}  // namespace webrtc
#endif // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_DEVICE_INFO_LINUX_H_
