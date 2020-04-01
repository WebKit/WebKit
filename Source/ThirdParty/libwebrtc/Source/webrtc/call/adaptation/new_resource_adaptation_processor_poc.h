/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_NEW_RESOURCE_ADAPTATION_PROCESSOR_POC_H_
#define CALL_ADAPTATION_NEW_RESOURCE_ADAPTATION_PROCESSOR_POC_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/resource_consumer.h"
#include "call/adaptation/resource_consumer_configuration.h"

namespace webrtc {

struct ConsumerConfigurationPair {
  ConsumerConfigurationPair(ResourceConsumer* consumer,
                            ResourceConsumerConfiguration* configuration);

  ResourceConsumer* consumer;
  ResourceConsumerConfiguration* configuration;
};

// Given a set of Resources, ResourceConsumers and
// ResourceConsumerConfigurations, the processor calculates which consumer, if
// any, should be reconfigured and how, in order to adapt to resource
// constraints.
//   Example: "CPU" is a resource, a video stream being encoded is a consumer
// and the encoder setting (e.g. VP8/720p/30fps) is a configuration.
//
// A resource can be "overused", "stable" or "underused". The processor
// maximises quality without overusing any resource as follows:
// 1. If we are "overusing" on any resource, find the most expensive consumer
//    and adapt it one step "down".
// 2. If we are "underusing" on all resources, find the least expensive consumer
//    and adapt it one step "up".
//
// The expensiveness of a consumer is the expensiveness of its current
// configuration and the cost of a configuration is estimated based on pixels
// per second. How a consumer can be reconfigured in terms of one step "up" or
// "down" is expressed as a graph: each configuration has a set of "upper"
// neighbors and "lower" neighbors. When there are multiple options, neighbors
// are chosen based on configuration preferences.
//
// See FindNextConfiguration().
//
// This class owns all resources, consumers and configurations. As long as it is
// alive, raw pointers to these are safe to use.
class NewResourceAdaptationProcessorPoc {
 public:
  const std::vector<std::unique_ptr<Resource>>& resources() const {
    return resources_;
  }
  const std::vector<std::unique_ptr<ResourceConsumerConfiguration>>&
  configurations() const {
    return configurations_;
  }
  const std::vector<std::unique_ptr<ResourceConsumer>>& consumers() const {
    return consumers_;
  }

  // Takes on ownership of the argument. A raw pointer is returned to the object
  // for convenience; it is valid for the lifetime of the
  // NewResourceAdaptationProcessorPoc.
  // T = any subclass of Resource
  template <typename T>
  T* AddResource(std::unique_ptr<T> resource) {
    T* resource_ptr = resource.get();
    resources_.push_back(std::move(resource));
    return resource_ptr;
  }
  // T = any subclass of ResourceConsumerConfiguration
  template <typename T>
  T* AddConfiguration(std::unique_ptr<T> configuration) {
    T* configuration_ptr = configuration.get();
    configurations_.push_back(std::move(configuration));
    return configuration_ptr;
  }
  // T = any subclass of ResourceConsumer
  template <typename T>
  T* AddConsumer(std::unique_ptr<T> consumer) {
    T* consumer_ptr = consumer.get();
    consumers_.push_back(std::move(consumer));
    return consumer_ptr;
  }

  // Based on the current state of the resources and consumers, finds the
  // consumer that should be reconfigured up or down in order to maximies
  // quality without overusing any resources, as described in
  // NewResourceAdaptationProcessorPoc's class description.
  //
  // When this is used in a real system, care needs to be taken for how often
  // FindNextConfiguration() is called. There may be a delay between
  // reconfiguring a consumer and the desired effects being observed on resource
  // usage.
  absl::optional<ConsumerConfigurationPair> FindNextConfiguration();

 private:
  ResourceConsumer* FindMostExpensiveConsumerThatCanBeAdaptedDown();
  ResourceConsumer* FindLeastExpensiveConsumerThatCanBeAdaptedUp();

  std::vector<std::unique_ptr<Resource>> resources_;
  std::vector<std::unique_ptr<ResourceConsumerConfiguration>> configurations_;
  std::vector<std::unique_ptr<ResourceConsumer>> consumers_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_NEW_RESOURCE_ADAPTATION_PROCESSOR_POC_H_
