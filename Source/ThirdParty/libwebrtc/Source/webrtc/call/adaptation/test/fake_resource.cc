/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/test/fake_resource.h"

#include <algorithm>
#include <utility>

#include "rtc_base/ref_counted_object.h"

namespace webrtc {

// static
rtc::scoped_refptr<FakeResource> FakeResource::Create(std::string name) {
  return rtc::make_ref_counted<FakeResource>(name);
}

FakeResource::FakeResource(std::string name)
    : Resource(), name_(std::move(name)), listener_(nullptr) {}

FakeResource::~FakeResource() {}

void FakeResource::SetUsageState(ResourceUsageState usage_state) {
  if (listener_) {
    listener_->OnResourceUsageStateMeasured(this, usage_state);
  }
}

std::string FakeResource::Name() const {
  return name_;
}

void FakeResource::SetResourceListener(ResourceListener* listener) {
  listener_ = listener;
}

}  // namespace webrtc
