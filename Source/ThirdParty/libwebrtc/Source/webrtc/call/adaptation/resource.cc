/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource.h"

#include "absl/algorithm/container.h"
#include "rtc_base/checks.h"

namespace webrtc {

ResourceListener::~ResourceListener() {}

Resource::Resource() : usage_state_(ResourceUsageState::kStable) {}

Resource::~Resource() {
  RTC_DCHECK(listeners_.empty());
}

void Resource::RegisterListener(ResourceListener* listener) {
  RTC_DCHECK(listener);
  RTC_DCHECK(absl::c_find(listeners_, listener) == listeners_.end())
      << "ResourceListener was added twice.";
  listeners_.push_back(listener);
}

void Resource::UnregisterListener(ResourceListener* listener) {
  RTC_DCHECK(listener);
  auto it = absl::c_find(listeners_, listener);
  if (it != listeners_.end())
    listeners_.erase(it);
}

ResourceUsageState Resource::usage_state() const {
  return usage_state_;
}

ResourceListenerResponse Resource::OnResourceUsageStateMeasured(
    ResourceUsageState usage_state) {
  ResourceListenerResponse response = ResourceListenerResponse::kNothing;
  usage_state_ = usage_state;
  for (auto* listener : listeners_) {
    ResourceListenerResponse listener_response =
        listener->OnResourceUsageStateMeasured(*this);
    if (listener_response != ResourceListenerResponse::kNothing)
      response = listener_response;
  }
  return response;
}

}  // namespace webrtc
