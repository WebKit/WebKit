/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/dxgi_texture_mapping.h"

#include <comdef.h>
#include <DXGI.h>
#include <DXGI1_2.h>

#include "webrtc/base/checks.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

DxgiTextureMapping::DxgiTextureMapping(const DesktopRect& desktop_rect,
                                       IDXGIOutputDuplication* duplication)
    : DxgiTexture(desktop_rect), duplication_(duplication) {
  RTC_DCHECK(duplication_);
}

DxgiTextureMapping::~DxgiTextureMapping() = default;

bool DxgiTextureMapping::CopyFrom(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                                  IDXGIResource* resource,
                                  const DesktopRegion& region) {
  RTC_DCHECK(resource && frame_info.AccumulatedFrames > 0);
  rect_ = {0};
  _com_error error = duplication_->MapDesktopSurface(&rect_);
  if (error.Error() != S_OK) {
    rect_ = {0};
    LOG(LS_ERROR) << "Failed to map the IDXGIOutputDuplication to a bitmap, "
                     "error "
                  << error.ErrorMessage() << ", code " << error.Error();
    return false;
  }

  return true;
}

bool DxgiTextureMapping::DoRelease() {
  _com_error error = duplication_->UnMapDesktopSurface();
  if (error.Error() != S_OK) {
    LOG(LS_ERROR) << "Failed to unmap the IDXGIOutputDuplication, error "
                  << error.ErrorMessage() << ", code " << error.Error();
    return false;
  }
  return true;
}

}  // namespace webrtc
