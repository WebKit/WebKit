/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/restore_token_manager.h"

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/portal/screencast_persist_mode.h"
#include "test/gtest.h"

namespace webrtc {

using PersistMode = xdg_portal::ScreenCastPersistMode;

TEST(RestoreTokenManagerTest, UnknownIdIsTransientWithoutToken) {
  RestoreTokenManager& manager = RestoreTokenManager::GetInstance();
  const DesktopCapturer::SourceId id = manager.GetUnusedId();
  EXPECT_EQ(manager.GetEntry(id).token, "");
  EXPECT_EQ(manager.GetEntry(id).persist_mode, PersistMode::kTransient);
}

TEST(RestoreTokenManagerTest, AddTokenDefaultsToTransient) {
  RestoreTokenManager& manager = RestoreTokenManager::GetInstance();
  const DesktopCapturer::SourceId id = manager.GetUnusedId();
  manager.AddToken(id, "token");
  EXPECT_EQ(manager.GetEntry(id).token, "token");
  EXPECT_EQ(manager.GetEntry(id).persist_mode, PersistMode::kTransient);
}

TEST(RestoreTokenManagerTest, KeepsPersistModeAndReplacesToken) {
  RestoreTokenManager& manager = RestoreTokenManager::GetInstance();
  const DesktopCapturer::SourceId id = manager.GetUnusedId();
  manager.AddToken(id, "", PersistMode::kPersistent);
  EXPECT_EQ(manager.GetEntry(id).token, "");
  EXPECT_EQ(manager.GetEntry(id).persist_mode, PersistMode::kPersistent);

  manager.AddToken(id, "renewed", PersistMode::kPersistent);
  EXPECT_EQ(manager.GetEntry(id).token, "renewed");
  EXPECT_EQ(manager.GetEntry(id).persist_mode, PersistMode::kPersistent);
}

}  // namespace webrtc
