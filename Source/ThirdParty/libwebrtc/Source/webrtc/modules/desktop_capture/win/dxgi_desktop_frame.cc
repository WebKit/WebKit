/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/dxgi_desktop_frame.h"

#include <comdef.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>

#include <utility>

#include "modules/desktop_capture/win/desktop_capture_utils.h"
#include "rtc_base/logging.h"

using Microsoft::WRL::ComPtr;

namespace webrtc {

namespace {

class DXGIFrameTexture : public FrameTexture {
 public:
  DXGIFrameTexture(Handle handle, ComPtr<IUnknown> prevent_release)
      : FrameTexture(handle), prevent_release_(std::move(prevent_release)) {}

  ~DXGIFrameTexture() override {
    if (handle_ != kInvalidHandle) {
      CloseHandle(handle_);
    }
  }

 private:
  // Prevents the WGC frame pool from recycling the captured texture. Typically
  // holds a reference to the IDirect3D11CaptureFrame. When this pointer (and
  // hence this DXGIFrameTexture) is destroyed, the capture frame is released
  // and the frame pool may reuse the underlying texture.
  ComPtr<IUnknown> prevent_release_;
};

}  // namespace

DXGIDesktopFrame::DXGIDesktopFrame(DesktopSize size,
                                   int stride,
                                   uint8_t* data,
                                   std::unique_ptr<FrameTexture> frame_texture)
    : DesktopFrame(size,
                   stride,
                   FOURCC_ARGB,
                   data,
                   /*shared_memory=*/nullptr,
                   frame_texture.get()),
      owned_frame_texture_(std::move(frame_texture)) {}

// static
std::unique_ptr<DXGIDesktopFrame> DXGIDesktopFrame::Create(
    DesktopSize size,
    ComPtr<ID3D11Texture2D> texture,
    ComPtr<IUnknown> prevent_release) {
  ComPtr<IDXGIResource1> dxgi_resource;
  HRESULT hr = texture.As(&dxgi_resource);
  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "Failed to query IDXGIResource1: "
                      << desktop_capture::utils::ComErrorToString(hr);
    return nullptr;
  }

  HANDLE texture_handle;
  hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ,
                                         nullptr, &texture_handle);
  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "Failed to create shared handle for texture: "
                      << desktop_capture::utils::ComErrorToString(hr);
    return nullptr;
  }

  auto frame_texture = std::make_unique<DXGIFrameTexture>(
      texture_handle, std::move(prevent_release));
  return std::unique_ptr<DXGIDesktopFrame>(
      new DXGIDesktopFrame(size, 0, nullptr, std::move(frame_texture)));
}

}  // namespace webrtc
