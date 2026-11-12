/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_RESTORE_TOKEN_MANAGER_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_RESTORE_TOKEN_MANAGER_H_

#include <string>
#include <unordered_map>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/portal/screencast_persist_mode.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// Process-wide map from the SourceId a PipeWire capturer hands out to the
// ScreenCast portal restore token for that source, so that another capturer
// selecting the same SourceId can restore the session without showing the
// portal dialog again. An embedder that keeps tokens across runs can seed an
// entry with ScreenCastPersistMode::kPersistent, select that SourceId before
// Start(), and read the token back with GetEntry() once the capture has
// started (e.g. in DelegatedSourceListController::Observer::OnSelection()). An
// empty token at that point means the portal issued none, and any token saved
// earlier for this source can no longer be used.
class RTC_EXPORT RestoreTokenManager {
 public:
  struct Entry {
    std::string token;
    xdg_portal::ScreenCastPersistMode persist_mode =
        xdg_portal::ScreenCastPersistMode::kTransient;
  };

  RestoreTokenManager(const RestoreTokenManager& manager) = delete;
  RestoreTokenManager& operator=(const RestoreTokenManager& manager) = delete;

  static RestoreTokenManager& GetInstance();

  // `persist_mode` is what the portal is asked for when `token` is used, so
  // that restoring a persistent token yields a persistent token again. `token`
  // may be empty to only request `persist_mode` for `id`.
  void AddToken(DesktopCapturer::SourceId id,
                const std::string& token,
                xdg_portal::ScreenCastPersistMode persist_mode =
                    xdg_portal::ScreenCastPersistMode::kTransient);
  Entry GetEntry(DesktopCapturer::SourceId id);

  // Returns a source ID which does not have any token associated with it yet.
  DesktopCapturer::SourceId GetUnusedId();

 private:
  RestoreTokenManager() = default;
  ~RestoreTokenManager() = default;

  Mutex mutex_;
  DesktopCapturer::SourceId last_source_id_ RTC_GUARDED_BY(mutex_) = 0;

  std::unordered_map<DesktopCapturer::SourceId, Entry> restore_tokens_
      RTC_GUARDED_BY(mutex_);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_RESTORE_TOKEN_MANAGER_H_
