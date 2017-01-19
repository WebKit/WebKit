/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/dxgi_texture.h"

#include "webrtc/modules/desktop_capture/desktop_region.h"

namespace webrtc {

namespace {

class DxgiDesktopFrame : public DesktopFrame {
 public:
  explicit DxgiDesktopFrame(const DxgiTexture& texture)
      : DesktopFrame(texture.desktop_rect().size(),
                     texture.pitch(),
                     texture.bits(),
                     nullptr) {}

  ~DxgiDesktopFrame() override = default;
};

}  // namespace

DxgiTexture::DxgiTexture(const DesktopRect& desktop_rect)
    : desktop_rect_(desktop_rect) {}

DxgiTexture::~DxgiTexture() {}

const DesktopFrame& DxgiTexture::AsDesktopFrame() {
  if (!frame_) {
    frame_.reset(new DxgiDesktopFrame(*this));
  }
  return *frame_;
}

bool DxgiTexture::Release() {
  frame_.reset();
  return DoRelease();
}

}  // namespace webrtc
