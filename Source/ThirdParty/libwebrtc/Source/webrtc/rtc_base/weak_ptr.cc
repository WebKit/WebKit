/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/weak_ptr.h"

// The implementation is borrowed from chromium except that it does not
// implement SupportsWeakPtr.

namespace rtc {
namespace internal {

void WeakReference::Flag::Invalidate() {
  RTC_DCHECK_RUN_ON(&checker_);
  is_valid_ = false;
}

bool WeakReference::Flag::IsValid() const {
  RTC_DCHECK_RUN_ON(&checker_);
  return is_valid_;
}

WeakReference::WeakReference() {}

WeakReference::WeakReference(const RefCountedFlag* flag) : flag_(flag) {}

WeakReference::~WeakReference() {}

WeakReference::WeakReference(WeakReference&& other) = default;

WeakReference::WeakReference(const WeakReference& other) = default;

bool WeakReference::is_valid() const {
  return flag_.get() && flag_->IsValid();
}

WeakReferenceOwner::WeakReferenceOwner() {}

WeakReferenceOwner::~WeakReferenceOwner() {
  Invalidate();
}

WeakReference WeakReferenceOwner::GetRef() const {
  // If we hold the last reference to the Flag then create a new one.
  if (!HasRefs())
    flag_ = new WeakReference::RefCountedFlag();

  return WeakReference(flag_.get());
}

void WeakReferenceOwner::Invalidate() {
  if (flag_.get()) {
    flag_->Invalidate();
    flag_ = nullptr;
  }
}

WeakPtrBase::WeakPtrBase() {}

WeakPtrBase::~WeakPtrBase() {}

WeakPtrBase::WeakPtrBase(const WeakReference& ref) : ref_(ref) {}

}  // namespace internal
}  // namespace rtc
