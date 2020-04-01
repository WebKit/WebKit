/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource_consumer_configuration.h"

#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

ResourceConsumerConfiguration::~ResourceConsumerConfiguration() {}

const std::vector<ResourceConsumerConfiguration*>&
ResourceConsumerConfiguration::upper_neighbors() const {
  return upper_neighbors_;
}

const std::vector<ResourceConsumerConfiguration*>&
ResourceConsumerConfiguration::lower_neighbors() const {
  return lower_neighbors_;
}

void ResourceConsumerConfiguration::AddUpperNeighbor(
    ResourceConsumerConfiguration* upper_neighbor) {
  upper_neighbors_.push_back(upper_neighbor);
}

void ResourceConsumerConfiguration::AddLowerNeighbor(
    ResourceConsumerConfiguration* lower_neighbor) {
  lower_neighbors_.push_back(lower_neighbor);
}

}  // namespace webrtc
