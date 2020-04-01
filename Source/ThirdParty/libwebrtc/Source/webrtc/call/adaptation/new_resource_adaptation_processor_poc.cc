/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/new_resource_adaptation_processor_poc.h"

#include <limits>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

ResourceConsumerConfiguration* FindMostPreferredConfiguration(
    const std::vector<ResourceConsumerConfiguration*>& configurations) {
  if (configurations.empty())
    return nullptr;
  ResourceConsumerConfiguration* most_preferred_configuration =
      configurations[0];
  double most_preferred_configuration_preference =
      most_preferred_configuration->Preference();
  RTC_DCHECK_GE(most_preferred_configuration_preference, 0.0);
  for (size_t i = 1; i < configurations.size(); ++i) {
    auto* configuration = configurations[i];
    double preference = configuration->Preference();
    RTC_DCHECK_GE(preference, 0.0);
    if (most_preferred_configuration_preference < preference) {
      most_preferred_configuration = configuration;
      most_preferred_configuration_preference = preference;
    }
  }
  return most_preferred_configuration;
}

}  // namespace

ConsumerConfigurationPair::ConsumerConfigurationPair(
    ResourceConsumer* consumer,
    ResourceConsumerConfiguration* configuration)
    : consumer(consumer), configuration(configuration) {}

absl::optional<ConsumerConfigurationPair>
NewResourceAdaptationProcessorPoc::FindNextConfiguration() {
  ResourceUsageState overall_usage = ResourceUsageState::kUnderuse;
  for (auto& resource : resources_) {
    if (resource->usage_state() == ResourceUsageState::kStable) {
      // If any resource is "stable", we are not underusing.
      if (overall_usage == ResourceUsageState::kUnderuse)
        overall_usage = ResourceUsageState::kStable;
    } else if (resource->usage_state() == ResourceUsageState::kOveruse) {
      // If any resource is "overuse", we are overusing.
      overall_usage = ResourceUsageState::kOveruse;
      break;
    }
  }
  // If we are stable we should neither adapt up or down: stay where we are.
  if (overall_usage == ResourceUsageState::kStable)
    return absl::nullopt;
  if (overall_usage == ResourceUsageState::kOveruse) {
    // If we are overusing, we adapt down the most expensive consumer to its
    // most preferred lower neighbor.
    ResourceConsumer* max_cost_consumer =
        FindMostExpensiveConsumerThatCanBeAdaptedDown();
    if (!max_cost_consumer)
      return absl::nullopt;
    ResourceConsumerConfiguration* next_configuration =
        FindMostPreferredConfiguration(
            max_cost_consumer->configuration()->lower_neighbors());
    RTC_DCHECK(next_configuration);
    return ConsumerConfigurationPair(max_cost_consumer, next_configuration);
  } else {
    RTC_DCHECK_EQ(overall_usage, ResourceUsageState::kUnderuse);
    // If we are underusing, we adapt up the least expensive consumer to its
    // most preferred upper neighbor.
    ResourceConsumer* min_cost_consumer =
        FindLeastExpensiveConsumerThatCanBeAdaptedUp();
    if (!min_cost_consumer)
      return absl::nullopt;
    ResourceConsumerConfiguration* next_configuration =
        FindMostPreferredConfiguration(
            min_cost_consumer->configuration()->upper_neighbors());
    RTC_DCHECK(next_configuration);
    return ConsumerConfigurationPair(min_cost_consumer, next_configuration);
  }
}

ResourceConsumer* NewResourceAdaptationProcessorPoc::
    FindMostExpensiveConsumerThatCanBeAdaptedDown() {
  ResourceConsumer* max_cost_consumer = nullptr;
  double max_cost = -1.0;
  for (auto& consumer : consumers_) {
    if (consumer->configuration()->lower_neighbors().empty())
      continue;
    double cost = consumer->configuration()->Cost();
    if (max_cost < cost) {
      max_cost_consumer = consumer.get();
      max_cost = cost;
    }
  }
  return max_cost_consumer;
}

ResourceConsumer* NewResourceAdaptationProcessorPoc::
    FindLeastExpensiveConsumerThatCanBeAdaptedUp() {
  ResourceConsumer* min_cost_consumer = nullptr;
  double min_cost = std::numeric_limits<double>::infinity();
  for (auto& consumer : consumers_) {
    if (consumer->configuration()->upper_neighbors().empty())
      continue;
    double cost = consumer->configuration()->Cost();
    if (min_cost > cost) {
      min_cost_consumer = consumer.get();
      min_cost = cost;
    }
  }
  return min_cost_consumer;
}

}  // namespace webrtc
