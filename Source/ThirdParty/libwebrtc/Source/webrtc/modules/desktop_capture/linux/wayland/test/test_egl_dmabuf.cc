/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/test/test_egl_dmabuf.h"

#include <libdrm/drm_fourcc.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>

#include <cstring>
#include <memory>
#include <utility>

#include "modules/portal/pipewire_utils.h"
#include "rtc_base/logging.h"

namespace webrtc {

TestEglDrmDevice::TestEglDrmDevice(dev_t device_id)
    : EglDrmDevice(EGL_NO_DISPLAY, device_id) {
  device_id_ = device_id;
  initialized_ = true;
}

bool TestEglDrmDevice::ImageFromDmaBuf(
    const DesktopSize& size,
    uint32_t format,
    const std::vector<PlaneData>& plane_datas,
    uint64_t modifier,
    const DesktopVector& offset,
    const DesktopSize& buffer_size,
    uint8_t* data) {
  if (modifier == kTestFailingModifier) {
    RTC_LOG(LS_INFO)
        << "TestEglDrmDevice: Simulating import failure for modifier "
        << modifier;
    return false;
  }

  if (plane_datas.empty()) {
    RTC_LOG(LS_ERROR) << "TestEglDrmDevice: No plane data provided";
    return false;
  }

  const PlaneData& plane = plane_datas[0];

  if (plane.fd < 0) {
    RTC_LOG(LS_ERROR) << "TestEglDrmDevice: Invalid file descriptor";
    return false;
  }

  const int kBytesPerPixel = 4;
  const size_t plane_stride = plane.stride;
  const size_t buffer_size_bytes = plane_stride * size.height();

  uint8_t* map = static_cast<uint8_t*>(
      mmap(nullptr, buffer_size_bytes, PROT_READ, MAP_SHARED, plane.fd, 0));
  ScopedBuf scoped_buf;
  scoped_buf.initialize(map, buffer_size_bytes, plane.fd, true);

  if (!scoped_buf) {
    RTC_LOG(LS_ERROR) << "TestEglDrmDevice: Failed to mmap DMA-BUF";
    return false;
  }

  uint8_t* src = scoped_buf.get() + plane.offset;
  const size_t dst_stride = buffer_size.width() * kBytesPerPixel;

  for (int y = 0; y < size.height(); ++y) {
    memcpy(data + y * dst_stride + offset.x() * kBytesPerPixel,
           src + y * plane_stride + offset.x() * kBytesPerPixel,
           size.width() * kBytesPerPixel);
  }

  RTC_LOG(LS_INFO)
      << "TestEglDrmDevice: Successfully read DMA-BUF with modifier "
      << modifier << " (" << size.width() << "x" << size.height()
      << ", stride=" << plane_stride << ")";
  return true;
}

std::vector<uint64_t> TestEglDrmDevice::QueryDmaBufModifiers(uint32_t format) {
  std::vector<uint64_t> modifiers = {
      DRM_FORMAT_MOD_LINEAR,  // Always available
      kTestFailingModifier,   // Modifier that will fail on import
      kTestSuccessModifier    // Test modifier that works
  };

  MutexLock lock(&failed_modifiers_lock_);
  auto it = failed_modifiers_.find(format);
  if (it != failed_modifiers_.end()) {
    const auto& failed = it->second;
    modifiers.erase(std::remove_if(modifiers.begin(), modifiers.end(),
                                   [&failed](uint64_t modifier) {
                                     return failed.find(modifier) !=
                                            failed.end();
                                   }),
                    modifiers.end());
  }

  RTC_LOG(LS_INFO) << "TestEglDrmDevice: Returning " << modifiers.size()
                   << " modifiers for format " << format;
  return modifiers;
}

std::unique_ptr<TestEglDmaBuf> TestEglDmaBuf::CreateDefault() {
  auto instance = std::unique_ptr<TestEglDmaBuf>(new TestEglDmaBuf());
  if (!instance->Initialize()) {
    RTC_LOG(LS_ERROR) << "TestEglDmaBuf initialization failed";
    return nullptr;
  }
  return instance;
}

bool TestEglDmaBuf::Initialize() {
  const dev_t kTestDeviceId = makedev(10, 0);

  auto test_device = std::make_unique<TestEglDrmDevice>(kTestDeviceId);
  devices_[kTestDeviceId] = std::move(test_device);

  RTC_LOG(LS_INFO) << "TestEglDmaBuf: Created test DRM device with ID "
                   << major(kTestDeviceId) << ":" << minor(kTestDeviceId);

  return true;
}

}  // namespace webrtc
