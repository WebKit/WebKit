/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_CONSUMER_CONFIGURATION_H_
#define CALL_ADAPTATION_RESOURCE_CONSUMER_CONFIGURATION_H_

#include <map>
#include <string>
#include <vector>

namespace webrtc {

class Resource;

// Represents a possible state for a ResourceConsumer. For example, if an
// encoder consumer can have the states "HD" and "VGA", there is one
// ResourceConsumerConfiguration for each state. "HD" is an upper neighbor of
// "VGA" and "VGA" is a lower neighbor of "HD".
class ResourceConsumerConfiguration {
 public:
  virtual ~ResourceConsumerConfiguration();

  const std::vector<ResourceConsumerConfiguration*>& upper_neighbors() const;
  const std::vector<ResourceConsumerConfiguration*>& lower_neighbors() const;
  void AddUpperNeighbor(ResourceConsumerConfiguration* upper_neighbor);
  void AddLowerNeighbor(ResourceConsumerConfiguration* lower_neighbor);

  virtual std::string Name() const = 0;

  // How expensive this configuration is. This is an abstract unit used by the
  // ResourceAdaptationProcessor to compare configurations. When overusing, the
  // consumer with the most expensive configuration will be adapted down. When
  // underusing, the consumer with the least expensive configuration will be
  // adapted up. The cost generally scales with pixels per second. The value
  // must be non-negative.
  virtual double Cost() const = 0;

  // How preferable this configuration is. The is an abstract unit used by the
  // ResourceAdaptationProcessor to compare configurations. When a consumer is
  // reconfigured to a neighbor configuration, the configuration with the
  // highest preference value is preferred. The value must be non-negative.
  virtual double Preference() const = 0;

 private:
  // Configurations we can adapt "up" to when we are in |this| configuration,
  // such as higher resolutions.
  std::vector<ResourceConsumerConfiguration*> upper_neighbors_;
  // Configurations we can adapt "down" to when we are in |this| configuration,
  // such as lower resolutions.
  std::vector<ResourceConsumerConfiguration*> lower_neighbors_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_CONSUMER_CONFIGURATION_H_
