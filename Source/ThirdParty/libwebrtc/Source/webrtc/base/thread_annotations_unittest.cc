/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/thread_annotations.h"
#include "webrtc/test/gtest.h"

namespace {

class LOCKABLE Lock {
 public:
  void EnterWrite() const EXCLUSIVE_LOCK_FUNCTION() {}
  void EnterRead() const SHARED_LOCK_FUNCTION() {}
  bool TryEnterWrite() const EXCLUSIVE_TRYLOCK_FUNCTION(true) { return true; }
  bool TryEnterRead() const SHARED_TRYLOCK_FUNCTION(true) { return true; }
  void Leave() const UNLOCK_FUNCTION() {}
};

class SCOPED_LOCKABLE ScopeLock {
 public:
  explicit ScopeLock(const Lock& lock) EXCLUSIVE_LOCK_FUNCTION(lock) {}
  ~ScopeLock() UNLOCK_FUNCTION() {}
};

class ThreadSafe {
 public:
  ThreadSafe() {
    pt_protected_by_lock_ = new int;
    pt_protected_by_anything_ = new int;
  }

  ~ThreadSafe() {
    delete pt_protected_by_lock_;
    delete pt_protected_by_anything_;
  }

  void LockInOrder() {
    anylock_.EnterWrite();
    lock_.EnterWrite();
    pt_lock_.EnterWrite();

    pt_lock_.Leave();
    lock_.Leave();
    anylock_.Leave();
  }

  void UnprotectedFunction() LOCKS_EXCLUDED(anylock_, lock_, pt_lock_) {
    // Can access unprotected Value.
    unprotected_ = 15;
    // Can access pointers themself, but not data they point to.
    int* tmp = pt_protected_by_lock_;
    pt_protected_by_lock_ = pt_protected_by_anything_;
    pt_protected_by_anything_ = tmp;
  }

  void ReadProtected() {
    lock_.EnterRead();
    unprotected_ = protected_by_anything_;
    unprotected_ = protected_by_lock_;
    lock_.Leave();

    if (pt_lock_.TryEnterRead()) {
      unprotected_ = *pt_protected_by_anything_;
      unprotected_ = *pt_protected_by_lock_;
      pt_lock_.Leave();
    }

    anylock_.EnterRead();
    unprotected_ = protected_by_anything_;
    unprotected_ = *pt_protected_by_anything_;
    anylock_.Leave();
  }

  void WriteProtected() {
    lock_.EnterWrite();
    protected_by_anything_ = unprotected_;
    protected_by_lock_ = unprotected_;
    lock_.Leave();

    if (pt_lock_.TryEnterWrite()) {
      *pt_protected_by_anything_ = unprotected_;
      *pt_protected_by_lock_ = unprotected_;
      pt_lock_.Leave();
    }

    anylock_.EnterWrite();
    protected_by_anything_ = unprotected_;
    *pt_protected_by_anything_ = unprotected_;
    anylock_.Leave();
  }

  void CallReadProtectedFunction() {
    lock_.EnterRead();
    pt_lock_.EnterRead();
    ReadProtectedFunction();
    pt_lock_.Leave();
    lock_.Leave();
  }

  void CallWriteProtectedFunction() {
    ScopeLock scope_lock(GetLock());
    ScopeLock pt_scope_lock(pt_lock_);
    WriteProtectedFunction();
  }

 private:
  void ReadProtectedFunction() SHARED_LOCKS_REQUIRED(lock_, pt_lock_) {
    unprotected_ = protected_by_lock_;
    unprotected_ = *pt_protected_by_lock_;
  }

  void WriteProtectedFunction() EXCLUSIVE_LOCKS_REQUIRED(lock_, pt_lock_) {
    int x = protected_by_lock_;
    *pt_protected_by_lock_ = x;
    protected_by_lock_ = unprotected_;
  }

  const Lock& GetLock() LOCK_RETURNED(lock_) { return lock_; }

  Lock anylock_ ACQUIRED_BEFORE(lock_);
  Lock lock_;
  Lock pt_lock_ ACQUIRED_AFTER(lock_);

  int unprotected_ = 0;

  int protected_by_lock_ GUARDED_BY(lock_) = 0;
  int protected_by_anything_ GUARDED_VAR = 0;

  int* pt_protected_by_lock_ PT_GUARDED_BY(pt_lock_);
  int* pt_protected_by_anything_ PT_GUARDED_VAR;
};

}  // namespace

TEST(ThreadAnnotationsTest, Test) {
  // This test ensure thread annotations doesn't break compilation.
  // Thus no run-time expectations.
  ThreadSafe t;
  t.LockInOrder();
  t.UnprotectedFunction();
  t.ReadProtected();
  t.WriteProtected();
  t.CallReadProtectedFunction();
  t.CallWriteProtectedFunction();
}
