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

#include "absl/strings/string_view.h"
#include "api/make_ref_counted.h"

namespace webrtc {

// static
rtc::scoped_refptr<FakeResource> FakeResource::Create(absl::string_view name) {
  return rtc::make_ref_counted<FakeResource>(name);
}

FakeResource::FakeResource(absl::string_view name)
    : Resource(), name_(name), listener_(nullptr) {}

FakeResource::~FakeResource() {}

void FakeResource::SetUsageState(ResourceUsageState usage_state) {
  if (listener_) {
    listener_->OnResourceUsageStateMeasured(rtc::scoped_refptr<Resource>(this),
                                            usage_state);
  }
}

std::string FakeResource::Name() const {
  return name_;
}

void FakeResource::SetResourceListener(ResourceListener* listener) {
  listener_ = listener;
}

}  // namespace webrtc
