/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_FAKEWEBRTCDEVICEINFO_H_
#define WEBRTC_MEDIA_ENGINE_FAKEWEBRTCDEVICEINFO_H_

#include <vector>

#include "webrtc/base/stringutils.h"
#include "webrtc/media/engine/webrtcvideocapturer.h"

// Fake class for mocking out webrtc::VideoCaptureModule::DeviceInfo.
class FakeWebRtcDeviceInfo : public webrtc::VideoCaptureModule::DeviceInfo {
 public:
  struct Device {
    Device(const std::string& n, const std::string& i) : name(n), id(i) {}
    std::string name;
    std::string id;
    std::string product;
    std::vector<webrtc::VideoCaptureCapability> caps;
  };
  FakeWebRtcDeviceInfo() {}
  void AddDevice(const std::string& device_name, const std::string& device_id) {
    devices_.push_back(Device(device_name, device_id));
  }
  void AddCapability(const std::string& device_id,
                     const webrtc::VideoCaptureCapability& cap) {
    Device* dev = GetDeviceById(
        reinterpret_cast<const char*>(device_id.c_str()));
    if (!dev) return;
    dev->caps.push_back(cap);
  }
  virtual uint32_t NumberOfDevices() {
    return static_cast<int>(devices_.size());
  }
  virtual int32_t GetDeviceName(uint32_t device_num,
                                char* device_name,
                                uint32_t device_name_len,
                                char* device_id,
                                uint32_t device_id_len,
                                char* product_id,
                                uint32_t product_id_len) {
    Device* dev = GetDeviceByIndex(device_num);
    if (!dev) return -1;
    rtc::strcpyn(reinterpret_cast<char*>(device_name), device_name_len,
                       dev->name.c_str());
    rtc::strcpyn(reinterpret_cast<char*>(device_id), device_id_len,
                       dev->id.c_str());
    if (product_id) {
      rtc::strcpyn(reinterpret_cast<char*>(product_id), product_id_len,
                         dev->product.c_str());
    }
    return 0;
  }
  virtual int32_t NumberOfCapabilities(const char* device_id) {
    Device* dev = GetDeviceById(device_id);
    if (!dev) return -1;
    return static_cast<int32_t>(dev->caps.size());
  }
  virtual int32_t GetCapability(const char* device_id,
                                const uint32_t device_cap_num,
                                webrtc::VideoCaptureCapability& cap) {
    Device* dev = GetDeviceById(device_id);
    if (!dev) return -1;
    if (device_cap_num >= dev->caps.size()) return -1;
    cap = dev->caps[device_cap_num];
    return 0;
  }
  virtual int32_t GetOrientation(const char* device_id,
                                 webrtc::VideoRotation& rotation) {
    return -1;  // not implemented
  }
  virtual int32_t GetBestMatchedCapability(
      const char* device_id,
      const webrtc::VideoCaptureCapability& requested,
      webrtc::VideoCaptureCapability& resulting) {
    return -1;  // not implemented
  }
  virtual int32_t DisplayCaptureSettingsDialogBox(
      const char* device_id, const char* dialog_title,
      void* parent, uint32_t x, uint32_t y) {
    return -1;  // not implemented
  }

  Device* GetDeviceByIndex(size_t num) {
    return (num < devices_.size()) ? &devices_[num] : NULL;
  }
  Device* GetDeviceById(const char* device_id) {
    for (size_t i = 0; i < devices_.size(); ++i) {
      if (devices_[i].id == reinterpret_cast<const char*>(device_id)) {
        return &devices_[i];
      }
    }
    return NULL;
  }

 private:
  std::vector<Device> devices_;
};

#endif  // WEBRTC_MEDIA_ENGINE_FAKEWEBRTCDEVICEINFO_H_
