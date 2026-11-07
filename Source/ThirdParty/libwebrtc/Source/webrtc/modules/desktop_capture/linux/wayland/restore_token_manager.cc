/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/restore_token_manager.h"

#include <string>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/portal/screencast_persist_mode.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

// static
RestoreTokenManager& RestoreTokenManager::GetInstance() {
  static RestoreTokenManager* manager = new RestoreTokenManager();
  return *manager;
}

void RestoreTokenManager::AddToken(
    DesktopCapturer::SourceId id,
    const std::string& token,
    xdg_portal::ScreenCastPersistMode persist_mode) {
  MutexLock lock(&mutex_);
  restore_tokens_[id] = {.token = token, .persist_mode = persist_mode};
}

RestoreTokenManager::Entry RestoreTokenManager::GetEntry(
    DesktopCapturer::SourceId id) {
  MutexLock lock(&mutex_);
  auto it = restore_tokens_.find(id);
  return it == restore_tokens_.end() ? Entry() : it->second;
}

DesktopCapturer::SourceId RestoreTokenManager::GetUnusedId() {
  MutexLock lock(&mutex_);
  return ++last_source_id_;
}

}  // namespace webrtc
