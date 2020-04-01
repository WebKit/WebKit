/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource_consumer.h"

#include <utility>

#include "call/adaptation/resource_consumer_configuration.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

ResourceConsumer::ResourceConsumer(std::string name,
                                   ResourceConsumerConfiguration* configuration)
    : name_(std::move(name)), configuration_(configuration) {
  RTC_DCHECK(!name_.empty());
  RTC_DCHECK(configuration_);
}

ResourceConsumer::~ResourceConsumer() {}

std::string ResourceConsumer::name() const {
  return name_;
}

ResourceConsumerConfiguration* ResourceConsumer::configuration() const {
  return configuration_;
}

void ResourceConsumer::SetConfiguration(
    ResourceConsumerConfiguration* configuration) {
  RTC_DCHECK(configuration);
  configuration_ = configuration;
}

std::string ResourceConsumer::ToString() const {
  rtc::StringBuilder sb;
  sb << name_ << ": " << configuration_->Name();
  return sb.str();
}

}  // namespace webrtc
