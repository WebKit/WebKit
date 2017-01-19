/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DXGI_TEXTURE_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DXGI_TEXTURE_H_

#include <DXGI1_2.h>

#include <memory>

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

class DesktopRegion;

// A texture copied or mapped from a DXGI_OUTDUPL_FRAME_INFO and IDXGIResource.
class DxgiTexture {
 public:
  // Creates a DxgiTexture instance, which represents the DesktopRect area of
  // entire screen -- usually a monitor on the system.
  explicit DxgiTexture(const DesktopRect& desktop_rect);

  virtual ~DxgiTexture();

  // Copies selected regions of a frame represented by frame_info and resource.
  // Returns false if anything wrong.
  virtual bool CopyFrom(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                        IDXGIResource* resource,
                        const DesktopRegion& region) = 0;

  const DesktopRect& desktop_rect() const { return desktop_rect_; }

  uint8_t* bits() const { return static_cast<uint8_t*>(rect_.pBits); }

  int pitch() const { return static_cast<int>(rect_.Pitch); }

  // Releases the resource currently holds by this instance. Returns false if
  // anything wrong, and this instance should be deprecated in this state. bits,
  // pitch and AsDesktopFrame are only valid after a success CopyFrom() call,
  // but before Release() call.
  bool Release();

  // Returns a DesktopFrame snapshot of a DxgiTexture instance. This
  // DesktopFrame is used to copy a DxgiTexture content to another DesktopFrame
  // only. And it should not outlive its DxgiTexture instance.
  const DesktopFrame& AsDesktopFrame();

 protected:
  DXGI_MAPPED_RECT rect_ = {0};

 private:
  virtual bool DoRelease() = 0;

  const DesktopRect desktop_rect_;
  std::unique_ptr<DesktopFrame> frame_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DXGI_TEXTURE_H_
