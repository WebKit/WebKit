/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_EGL_DMABUF_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_EGL_DMABUF_H_

#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#include <GL/gl.h>
#include <gbm.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

constexpr dev_t DEVICE_ID_INVALID = static_cast<dev_t>(0);

class EglDrmDevice {
 public:
  struct EGLStruct {
    std::vector<std::string> extensions;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
  };

  struct PlaneData {
    int32_t fd;
    uint32_t stride;
    uint32_t offset;
  };

  EglDrmDevice(EGLDisplay display, dev_t device_id = DEVICE_ID_INVALID);
  EglDrmDevice(std::string render_node, dev_t device_id = DEVICE_ID_INVALID);
  virtual ~EglDrmDevice();

  bool EnsureInitialized();
  bool IsInitialized() const { return initialized_; }
  dev_t GetDeviceId() const { return device_id_; }

  virtual bool ImageFromDmaBuf(const DesktopSize& size,
                               uint32_t format,
                               const std::vector<PlaneData>& plane_datas,
                               uint64_t modifiers,
                               const DesktopVector& offset,
                               const DesktopSize& buffer_size,
                               uint8_t* data);
  virtual std::vector<uint64_t> QueryDmaBufModifiers(uint32_t format);

  void MarkModifierFailed(uint32_t format, uint64_t modifier);
  void MarkModifierFailed(uint64_t modifier);

 private:
  friend class TestEglDrmDevice;

  EGLStruct egl_;
  bool initialized_ = false;
  bool has_image_dma_buf_import_ext_ = false;
  dev_t device_id_ = DEVICE_ID_INVALID;

  struct GbmDeviceDeleter {
    void operator()(gbm_device* device) const {
      if (device) {
        gbm_device_destroy(device);
      }
    }
  };
  std::unique_ptr<gbm_device, GbmDeviceDeleter> gbm_device_;
  int32_t drm_fd_ = -1;
  std::string render_node_;

  GLuint fbo_ = 0;
  GLuint texture_ = 0;

  // Map of format -> failed modifiers that didn't work during import
  // The lock is needed for concurrent read/write in case a frame import
  // fails, we started to negotiate a new format, but meanwhile can still
  // receive a new frame and fail again, leading to again marking modifier
  // as failed.
  Mutex failed_modifiers_lock_;
  flat_map<uint32_t, flat_set<uint64_t>> failed_modifiers_
      RTC_GUARDED_BY(failed_modifiers_lock_);
};

// Base class for EGL DMA-BUF implementations.
// Provides shared device management logic for both real and test
// implementations.
class EglDmaBuf {
 public:
  static std::unique_ptr<EglDmaBuf> CreateDefault();
  virtual ~EglDmaBuf() = default;

  // Returns the DRM device to use for querying DMA-BUF modifiers and importing
  // frames. Device selection follows this priority order:
  //
  // 1. Platform device - created from Wayland platform EGL display during
  //    initialization if EGL platform extensions are available
  // 2. First enumerated device - fallback if platform device creation fails,
  //    uses EGL device enumeration to discover available DRM devices
  // 3. nullptr - if no devices are available
  EglDrmDevice* GetRenderDevice();
  // Returns the DRM device given the id or nullptr in case the device is not
  // found
  EglDrmDevice* GetRenderDevice(dev_t id);
  std::vector<dev_t> GetDevices() const;

  bool SetPreferredRenderDevice(dev_t device_id);

 protected:
  EglDmaBuf() = default;

  // Initializes EGL/DRM devices.
  // Returns true if at least one device is available, false otherwise.
  virtual bool Initialize();

 private:
  friend class TestEglDmaBuf;

  bool CreatePlatformDevice();
  void EnumerateDrmDevices();

  std::map<dev_t, std::unique_ptr<EglDrmDevice>> devices_;
  std::unique_ptr<EglDrmDevice> default_platform_device_;
  dev_t preferred_render_device_id_ = DEVICE_ID_INVALID;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_EGL_DMABUF_H_
