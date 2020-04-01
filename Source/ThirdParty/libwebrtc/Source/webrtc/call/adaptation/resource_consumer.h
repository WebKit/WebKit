/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_CONSUMER_H_
#define CALL_ADAPTATION_RESOURCE_CONSUMER_H_

#include <string>

namespace webrtc {

class ResourceConsumerConfiguration;

// Something which affects resource consumption. Used by the
// ResourceAdaptationProcessor to calculate which configurations to use.
//
// For example, this could represent an encoder, and valid
// ResourceConsumerConfigurations would be encoder settings. How a consumer
// affects a resource is described by the ResourceConsumerConfiguration.
//
// The functionality provided by the base class is a name and pointer to the
// current configuration. How a consumers and configurations affect real parts
// of the system (like actual encoders) is implementation-specific.
class ResourceConsumer {
 public:
  ResourceConsumer(std::string name,
                   ResourceConsumerConfiguration* configuration);
  ~ResourceConsumer();

  std::string name() const;
  ResourceConsumerConfiguration* configuration() const;
  void SetConfiguration(ResourceConsumerConfiguration* configuration);

  std::string ToString() const;

 private:
  std::string name_;
  ResourceConsumerConfiguration* configuration_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_CONSUMER_H_
