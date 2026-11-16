/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PORTAL_PORTAL_GUARD_H_
#define MODULES_PORTAL_PORTAL_GUARD_H_

#include <glib.h>

#include "api/ref_counted_base.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

// Ref-counted guard for safe cross-thread portal access from GDBus callbacks.
// Callbacks lock the mutex and check the portal pointer before use.
// Stop() nulls the pointer under the same mutex, waiting for any in-flight
// callback to finish.
struct PortalGuard : public RefCountedNonVirtual<PortalGuard> {
  Mutex mutex;
  gpointer portal = nullptr;

  gpointer AddRefAndGet() {
    AddRef();
    return this;
  }
};

inline void portal_guard_release(gpointer data) {
  static_cast<PortalGuard*>(data)->Release();
}

// RAII lock for PortalGuard. Use ScopedPortalLock for async callbacks
// (releases the callback's ref) or ScopedPortalSignalLock for signal
// callbacks (ref is owned by the subscription).
enum class RefOwnership {
  kOwnedByCallback,      // Async: callback owns the ref, release on unlock.
  kOwnedBySubscription,  // Signal: subscription owns the ref via
                         // GDestroyNotify.
};

class ScopedPortalLockBase {
 public:
  ScopedPortalLockBase(const ScopedPortalLockBase&) = delete;
  ScopedPortalLockBase& operator=(const ScopedPortalLockBase&) = delete;

  gpointer portal() const { return guard_->portal; }

 protected:
  ScopedPortalLockBase(gpointer user_data, RefOwnership ownership)
      : guard_(static_cast<PortalGuard*>(user_data)), ownership_(ownership) {
    guard_->mutex.Lock();
  }
  ~ScopedPortalLockBase() {
    guard_->mutex.Unlock();
    if (ownership_ == RefOwnership::kOwnedByCallback)
      guard_->Release();
  }

 private:
  PortalGuard* guard_;
  RefOwnership ownership_;
};

// For async callbacks. Releases the ref on destruction because async
// callbacks fire exactly once and own their ref.
class ScopedPortalLock : public ScopedPortalLockBase {
 public:
  explicit ScopedPortalLock(gpointer user_data)
      : ScopedPortalLockBase(user_data, RefOwnership::kOwnedByCallback) {}
};

// For signal callbacks. Does not release the ref because the signal
// subscription owns it. The ref is released by portal_guard_release
// (GDestroyNotify) when the subscription is removed.
class ScopedPortalSignalLock : public ScopedPortalLockBase {
 public:
  explicit ScopedPortalSignalLock(gpointer user_data)
      : ScopedPortalLockBase(user_data, RefOwnership::kOwnedBySubscription) {}
};

}  // namespace webrtc

#endif  // MODULES_PORTAL_PORTAL_GUARD_H_
