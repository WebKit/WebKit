/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_H_

#include <memory>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/shared_memory.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// DesktopFrame represents a video frame captured from the screen.
class DesktopFrame {
 public:
  // DesktopFrame objects always hold RGBA data.
  static const int kBytesPerPixel = 4;

  virtual ~DesktopFrame();

  // Size of the frame.
  const DesktopSize& size() const { return size_; }

  // Distance in the buffer between two neighboring rows in bytes.
  int stride() const { return stride_; }

  // Data buffer used for the frame.
  uint8_t* data() const { return data_; }

  // SharedMemory used for the buffer or NULL if memory is allocated on the
  // heap. The result is guaranteed to be deleted only after the frame is
  // deleted (classes that inherit from DesktopFrame must ensure it).
  SharedMemory* shared_memory() const { return shared_memory_; }

  // Indicates region of the screen that has changed since the previous frame.
  const DesktopRegion& updated_region() const { return updated_region_; }
  DesktopRegion* mutable_updated_region() { return &updated_region_; }

  // DPI of the screen being captured. May be set to zero, e.g. if DPI is
  // unknown.
  const DesktopVector& dpi() const { return dpi_; }
  void set_dpi(const DesktopVector& dpi) { dpi_ = dpi; }

  // Time taken to capture the frame in milliseconds.
  int64_t capture_time_ms() const { return capture_time_ms_; }
  void set_capture_time_ms(int64_t time_ms) { capture_time_ms_ = time_ms; }

  // Copies pixels from a buffer or another frame. |dest_rect| rect must lay
  // within bounds of this frame.
  void CopyPixelsFrom(const uint8_t* src_buffer,
                      int src_stride,
                      const DesktopRect& dest_rect);
  void CopyPixelsFrom(const DesktopFrame& src_frame,
                      const DesktopVector& src_pos,
                      const DesktopRect& dest_rect);

  // A helper to return the data pointer of a frame at the specified position.
  uint8_t* GetFrameDataAtPos(const DesktopVector& pos) const;

  // The DesktopCapturer implementation which generates current DesktopFrame.
  // Not all DesktopCapturer implementations set this field; it's set to
  // kUnknown by default.
  uint32_t capturer_id() const { return capturer_id_; }
  void set_capturer_id(uint32_t capturer_id) {
    capturer_id_ = capturer_id;
  }

 protected:
  DesktopFrame(DesktopSize size,
               int stride,
               uint8_t* data,
               SharedMemory* shared_memory);

  // Ownership of the buffers is defined by the classes that inherit from this
  // class. They must guarantee that the buffer is not deleted before the frame
  // is deleted.
  uint8_t* const data_;
  SharedMemory* const shared_memory_;

 private:
  const DesktopSize size_;
  const int stride_;

  DesktopRegion updated_region_;
  DesktopVector dpi_;
  int64_t capture_time_ms_;
  uint32_t capturer_id_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DesktopFrame);
};

// A DesktopFrame that stores data in the heap.
class BasicDesktopFrame : public DesktopFrame {
 public:
  explicit BasicDesktopFrame(DesktopSize size);
  ~BasicDesktopFrame() override;

  // Creates a BasicDesktopFrame that contains copy of |frame|.
  static DesktopFrame* CopyOf(const DesktopFrame& frame);

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(BasicDesktopFrame);
};

// A DesktopFrame that stores data in shared memory.
class SharedMemoryDesktopFrame : public DesktopFrame {
 public:
  static std::unique_ptr<DesktopFrame> Create(
      DesktopSize size,
      SharedMemoryFactory* shared_memory_factory);

  static std::unique_ptr<DesktopFrame> Create(
      DesktopSize size,
      std::unique_ptr<SharedMemory> shared_memory);

  // Takes ownership of |shared_memory|.
  // TODO(zijiehe): Hide constructors after fake_desktop_capturer.cc has been
  // migrated, Create() is preferred.
  SharedMemoryDesktopFrame(DesktopSize size,
                           int stride,
                           SharedMemory* shared_memory);
  ~SharedMemoryDesktopFrame() override;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(SharedMemoryDesktopFrame);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_H_

