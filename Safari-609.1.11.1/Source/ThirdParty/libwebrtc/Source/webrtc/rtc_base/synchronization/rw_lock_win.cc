/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/synchronization/rw_lock_win.h"

#include "rtc_base/logging.h"

namespace webrtc {

typedef void(WINAPI* PInitializeSRWLock)(PSRWLOCK);

typedef void(WINAPI* PAcquireSRWLockExclusive)(PSRWLOCK);
typedef void(WINAPI* PReleaseSRWLockExclusive)(PSRWLOCK);

typedef void(WINAPI* PAcquireSRWLockShared)(PSRWLOCK);
typedef void(WINAPI* PReleaseSRWLockShared)(PSRWLOCK);

PInitializeSRWLock initialize_srw_lock;
PAcquireSRWLockExclusive acquire_srw_lock_exclusive;
PAcquireSRWLockShared acquire_srw_lock_shared;
PReleaseSRWLockShared release_srw_lock_shared;
PReleaseSRWLockExclusive release_srw_lock_exclusive;

RWLockWin::RWLockWin() {
  initialize_srw_lock(&lock_);
}

RWLockWin* RWLockWin::Create() {
  if (!LoadModule()) {
    return NULL;
  }
  return new RWLockWin();
}

void RWLockWin::AcquireLockExclusive() {
  acquire_srw_lock_exclusive(&lock_);
}

void RWLockWin::ReleaseLockExclusive() {
  release_srw_lock_exclusive(&lock_);
}

void RWLockWin::AcquireLockShared() {
  acquire_srw_lock_shared(&lock_);
}

void RWLockWin::ReleaseLockShared() {
  release_srw_lock_shared(&lock_);
}

bool RWLockWin::LoadModule() {
  static bool module_load_attempted = false;
  static bool native_rw_locks_supported = false;
  if (module_load_attempted) {
    return native_rw_locks_supported;
  }
  module_load_attempted = true;
#if !defined(WINUWP)
  // Use native implementation if supported (i.e Vista+)
  static HMODULE library = LoadLibrary(TEXT("Kernel32.dll"));
  if (!library) {
    return false;
  }
  RTC_LOG(LS_VERBOSE) << "Loaded Kernel.dll";

  initialize_srw_lock =
      (PInitializeSRWLock)GetProcAddress(library, "InitializeSRWLock");

  acquire_srw_lock_exclusive = (PAcquireSRWLockExclusive)GetProcAddress(
      library, "AcquireSRWLockExclusive");
  release_srw_lock_exclusive = (PReleaseSRWLockExclusive)GetProcAddress(
      library, "ReleaseSRWLockExclusive");
  acquire_srw_lock_shared =
      (PAcquireSRWLockShared)GetProcAddress(library, "AcquireSRWLockShared");
  release_srw_lock_shared =
      (PReleaseSRWLockShared)GetProcAddress(library, "ReleaseSRWLockShared");

  if (initialize_srw_lock && acquire_srw_lock_exclusive &&
      release_srw_lock_exclusive && acquire_srw_lock_shared &&
      release_srw_lock_shared) {
    RTC_LOG(LS_VERBOSE) << "Loaded Native RW Lock";
    native_rw_locks_supported = true;
  }
#else
  // On WinUWP the symbols loaded from this library are directly present
  // in the headers and thus loading the library is not required (and
  // manually loading libraries is restricted due to WinUWP sandboxing).
  initialize_srw_lock = InitializeSRWLock;
  acquire_srw_lock_exclusive = AcquireSRWLockExclusive;
  release_srw_lock_exclusive = ReleaseSRWLockExclusive;
  acquire_srw_lock_shared = AcquireSRWLockShared;
  release_srw_lock_shared = ReleaseSRWLockShared;

  native_rw_locks_supported = true;
#endif  // !defined(WINUWP)
  return native_rw_locks_supported;
}

}  // namespace webrtc
