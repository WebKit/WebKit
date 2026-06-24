/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DXGI_DESKTOP_FRAME_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DXGI_DESKTOP_FRAME_H_

#include <d3d11.h>
#include <wrl/client.h>

#include <memory>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/frame_texture.h"

namespace webrtc {

// DesktopFrame implementation used by DXGI captures on Windows.
// Frame texture is stored in the handle.
class DXGIDesktopFrame : public DesktopFrame {
 public:
  DXGIDesktopFrame(const DXGIDesktopFrame&) = delete;
  DXGIDesktopFrame& operator=(const DXGIDesktopFrame&) = delete;

  // Creates a DXGIDesktopFrame. If `prevent_release` is provided (typically
  // an IDirect3D11CaptureFrame), it prevents the WGC frame pool from recycling
  // the underlying texture while downstream consumers still reference this
  // frame.
  static std::unique_ptr<DXGIDesktopFrame> Create(
      DesktopSize size,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
      Microsoft::WRL::ComPtr<IUnknown> prevent_release);

 private:
  DXGIDesktopFrame(DesktopSize size,
                   int stride,
                   uint8_t* data,
                   std::unique_ptr<FrameTexture> frame_texture);

  const std::unique_ptr<FrameTexture> owned_frame_texture_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_DXGI_DESKTOP_FRAME_H_
