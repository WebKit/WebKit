/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"

#include <memory>

#include "webrtc/base/constructormagic.h"
#include "webrtc/system_wrappers/include/atomic32.h"

namespace webrtc {

SharedDesktopFrame::~SharedDesktopFrame() {}

// static
std::unique_ptr<SharedDesktopFrame> SharedDesktopFrame::Wrap(
    std::unique_ptr<DesktopFrame> desktop_frame) {
  return std::unique_ptr<SharedDesktopFrame>(
      new SharedDesktopFrame(new Core(std::move(desktop_frame))));
}

SharedDesktopFrame* SharedDesktopFrame::Wrap(DesktopFrame* desktop_frame) {
  return Wrap(std::unique_ptr<DesktopFrame>(desktop_frame)).release();
}

DesktopFrame* SharedDesktopFrame::GetUnderlyingFrame() {
  return core_->get();
}

std::unique_ptr<SharedDesktopFrame> SharedDesktopFrame::Share() {
  std::unique_ptr<SharedDesktopFrame> result(new SharedDesktopFrame(core_));
  result->set_dpi(dpi());
  result->set_capture_time_ms(capture_time_ms());
  *result->mutable_updated_region() = updated_region();
  return result;
}

bool SharedDesktopFrame::IsShared() {
  return !core_->HasOneRef();
}

SharedDesktopFrame::SharedDesktopFrame(rtc::scoped_refptr<Core> core)
    : DesktopFrame((*core)->size(),
                   (*core)->stride(),
                   (*core)->data(),
                   (*core)->shared_memory()),
      core_(core) {}

}  // namespace webrtc
