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

namespace webrtc {

// static
RestoreTokenManager& RestoreTokenManager::GetInstance() {
  static webrtc::RestoreTokenManager* manager = new RestoreTokenManager();
  return *manager;
}

void RestoreTokenManager::AddToken(DesktopCapturer::SourceId id,
                                   const std::string& token) {
  restore_tokens_.insert({id, token});
}

std::string RestoreTokenManager::TakeToken(DesktopCapturer::SourceId id) {
  std::string token = restore_tokens_[id];
  // Remove the token as it cannot be used anymore
  restore_tokens_.erase(id);
  return token;
}

}  // namespace webrtc
