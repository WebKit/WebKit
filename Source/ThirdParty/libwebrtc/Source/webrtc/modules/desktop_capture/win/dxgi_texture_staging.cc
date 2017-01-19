/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/dxgi_texture_staging.h"

#include <comdef.h>
#include <unknwn.h>
#include <DXGI.h>
#include <DXGI1_2.h>

#include "webrtc/base/checks.h"
#include "webrtc/system_wrappers/include/logging.h"

using Microsoft::WRL::ComPtr;

namespace webrtc {

DxgiTextureStaging::DxgiTextureStaging(const DesktopRect& desktop_rect,
                                       const D3dDevice& device)
    : DxgiTexture(desktop_rect), device_(device) {}

DxgiTextureStaging::~DxgiTextureStaging() = default;

bool DxgiTextureStaging::InitializeStage(ID3D11Texture2D* texture) {
  RTC_DCHECK(texture);
  D3D11_TEXTURE2D_DESC desc = {0};
  texture->GetDesc(&desc);
  if (static_cast<int>(desc.Width) != desktop_rect().width() ||
      static_cast<int>(desc.Height) != desktop_rect().height()) {
    LOG(LS_ERROR) << "Texture size is not consistent with current DxgiTexture.";
    return false;
  }

  desc.ArraySize = 1;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.MipLevels = 1;
  desc.MiscFlags = 0;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_STAGING;
  if (stage_) {
    AssertStageAndSurfaceAreSameObject();
    D3D11_TEXTURE2D_DESC current_desc;
    stage_->GetDesc(&current_desc);
    if (memcmp(&desc, &current_desc, sizeof(D3D11_TEXTURE2D_DESC)) == 0) {
      return true;
    }

    // The descriptions are not consistent, we need to create a new
    // ID3D11Texture2D instance.
    stage_.Reset();
    surface_.Reset();
  } else {
    RTC_DCHECK(!surface_);
  }

  _com_error error = device_.d3d_device()->CreateTexture2D(
      &desc, nullptr, stage_.GetAddressOf());
  if (error.Error() != S_OK || !stage_) {
    LOG(LS_ERROR) << "Failed to create a new ID3D11Texture2D as stage, error "
                  << error.ErrorMessage() << ", code " << error.Error();
    return false;
  }

  error = stage_.As(&surface_);
  if (error.Error() != S_OK || !surface_) {
    LOG(LS_ERROR) << "Failed to convert ID3D11Texture2D to IDXGISurface, error "
                  << error.ErrorMessage() << ", code " << error.Error();
    return false;
  }

  return true;
}

void DxgiTextureStaging::AssertStageAndSurfaceAreSameObject() {
  ComPtr<IUnknown> left;
  ComPtr<IUnknown> right;
  bool left_result = SUCCEEDED(stage_.As(&left));
  bool right_result = SUCCEEDED(surface_.As(&right));
  RTC_DCHECK(left_result);
  RTC_DCHECK(right_result);
  RTC_DCHECK(left.Get() == right.Get());
}

bool DxgiTextureStaging::CopyFrom(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                                  IDXGIResource* resource,
                                  const DesktopRegion& region) {
  RTC_DCHECK(resource && frame_info.AccumulatedFrames > 0);
  ComPtr<ID3D11Texture2D> texture;
  _com_error error = resource->QueryInterface(
      __uuidof(ID3D11Texture2D),
      reinterpret_cast<void**>(texture.GetAddressOf()));
  if (error.Error() != S_OK || !texture) {
    LOG(LS_ERROR) << "Failed to convert IDXGIResource to ID3D11Texture2D, "
                     "error "
                  << error.ErrorMessage() << ", code " << error.Error();
    return false;
  }

  // AcquireNextFrame returns a CPU inaccessible IDXGIResource, so we need to
  // copy it to a CPU accessible staging ID3D11Texture2D.
  if (!InitializeStage(texture.Get())) {
    return false;
  }

  for (DesktopRegion::Iterator it(region); !it.IsAtEnd(); it.Advance()) {
    DesktopRect rect(it.rect());
    rect.Translate(-desktop_rect().left(), -desktop_rect().top());
    D3D11_BOX box;
    box.left = rect.left();
    box.top = rect.top();
    box.right = rect.right();
    box.bottom = rect.bottom();
    box.front = 0;
    box.back = 1;
    device_.context()->CopySubresourceRegion(
        static_cast<ID3D11Resource*>(stage_.Get()), 0, rect.left(), rect.top(),
        0, static_cast<ID3D11Resource*>(texture.Get()), 0, &box);
  }

  rect_ = {0};
  error = surface_->Map(&rect_, DXGI_MAP_READ);
  if (error.Error() != S_OK) {
    rect_ = {0};
    LOG(LS_ERROR) << "Failed to map the IDXGISurface to a bitmap, error "
                  << error.ErrorMessage() << ", code " << error.Error();
    return false;
  }

  return true;
}

bool DxgiTextureStaging::DoRelease() {
  _com_error error = surface_->Unmap();
  if (error.Error() != S_OK) {
    stage_.Reset();
    surface_.Reset();
  }
  // If using staging mode, we only need to recreate ID3D11Texture2D instance.
  // This will happen during next CopyFrom call. So this function always returns
  // true.
  return true;
}

}  // namespace webrtc
