/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_TEST_TEST_EGL_DMABUF_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_TEST_TEST_EGL_DMABUF_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "modules/desktop_capture/linux/wayland/egl_dmabuf.h"

namespace webrtc {

// Test DMA-BUF modifier constants
// Using vendor ID 0xFE. This is not a standard reserved ID in drm_fourcc.h,
// but it is unlikely to conflict with real hardware vendors currently allocated
// in the 0x00-0x0F range.
constexpr uint64_t kTestFailingModifier = 0xFE00000000000001ULL;
constexpr uint64_t kTestSuccessModifier = 0xFE00000000000002ULL;

// Test EGL DRM device for testing DMA-BUF functionality.
// Simulates DMA-BUF operations without requiring real EGL/GBM.
class TestEglDrmDevice : public EglDrmDevice {
 public:
  explicit TestEglDrmDevice(dev_t device_id = DEVICE_ID_INVALID);
  ~TestEglDrmDevice() override = default;

  bool ImageFromDmaBuf(const DesktopSize& size,
                       uint32_t format,
                       const std::vector<PlaneData>& plane_datas,
                       uint64_t modifier,
                       const DesktopVector& offset,
                       const DesktopSize& buffer_size,
                       uint8_t* data) override;

  std::vector<uint64_t> QueryDmaBufModifiers(uint32_t format) override;
};

// Test EGL DMA-BUF manager for testing.
// Creates a single test device for testing DMA-BUF negotiation and fallback.
class TestEglDmaBuf : public EglDmaBuf {
 public:
  static std::unique_ptr<TestEglDmaBuf> CreateDefault();
  ~TestEglDmaBuf() override = default;

 protected:
  TestEglDmaBuf() = default;
  bool Initialize() override;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_TEST_TEST_EGL_DMABUF_H_
