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

#include "absl/types/optional.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/test/fake_resource.h"
#include "call/adaptation/test/fake_resource_consumer_configuration.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

// The indices of different resolutions returned by
// AddStandardResolutionConfigurations().
static size_t k1080pIndex = 0;
static size_t k720pIndex = 1;
static size_t k360pIndex = 2;
static size_t k180pIndex = 3;

void ConnectNeighbors(ResourceConsumerConfiguration* upper,
                      ResourceConsumerConfiguration* lower) {
  upper->AddLowerNeighbor(lower);
  lower->AddUpperNeighbor(upper);
}

std::vector<FakeResourceConsumerConfiguration*>
AddStandardResolutionConfigurations(
    NewResourceAdaptationProcessorPoc* processor) {
  std::vector<FakeResourceConsumerConfiguration*> configs;
  configs.push_back(processor->AddConfiguration(
      std::make_unique<FakeResourceConsumerConfiguration>(1920, 1080, 30.0,
                                                          1.0)));
  configs.push_back(processor->AddConfiguration(
      std::make_unique<FakeResourceConsumerConfiguration>(1280, 720, 30.0,
                                                          1.0)));
  configs.push_back(processor->AddConfiguration(
      std::make_unique<FakeResourceConsumerConfiguration>(640, 360, 30.0,
                                                          1.0)));
  configs.push_back(processor->AddConfiguration(
      std::make_unique<FakeResourceConsumerConfiguration>(320, 180, 30.0,
                                                          1.0)));
  for (size_t i = 1; i < configs.size(); ++i) {
    ConnectNeighbors(configs[i - 1], configs[i]);
  }
  return configs;
}

TEST(NewResourceAdaptationProcessorPocTest,
     SingleStreamAndResourceDontAdaptDownWhenStable) {
  NewResourceAdaptationProcessorPoc processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kStable));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "OnlyStream", resolution_configs[k1080pIndex]));
  EXPECT_EQ(absl::nullopt, processor.FindNextConfiguration());
}

TEST(NewResourceAdaptationProcessorPocTest,
     SingleStreamAndResourceAdaptDownOnOveruse) {
  NewResourceAdaptationProcessorPoc processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kOveruse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  auto* consumer = processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "OnlyStream", resolution_configs[k1080pIndex]));
  auto next_config = processor.FindNextConfiguration();
  EXPECT_TRUE(next_config.has_value());
  EXPECT_EQ(consumer, next_config->consumer);
  EXPECT_EQ(resolution_configs[k720pIndex], next_config->configuration);
}

TEST(NewResourceAdaptationProcessorPocTest,
     SingleStreamAndResourceDontAdaptOnOveruseIfMinResolution) {
  NewResourceAdaptationProcessorPoc processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kOveruse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "OnlyStream", resolution_configs.back()));
  EXPECT_EQ(absl::nullopt, processor.FindNextConfiguration());
}

TEST(NewResourceAdaptationProcessorPocTest,
     SingleStreamAndResourceAdaptUpOnUnderuse) {
  NewResourceAdaptationProcessorPoc processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kUnderuse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  auto* consumer = processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "OnlyStream", resolution_configs[k720pIndex]));
  auto next_config = processor.FindNextConfiguration();
  EXPECT_TRUE(next_config.has_value());
  EXPECT_EQ(consumer, next_config->consumer);
  EXPECT_EQ(resolution_configs[k1080pIndex], next_config->configuration);
}

TEST(NewResourceAdaptationProcessorPocTest,
     SingleStreamAndResourceDontAdaptOnUnderuseIfMaxResolution) {
  NewResourceAdaptationProcessorPoc processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kUnderuse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "OnlyStream", resolution_configs[k1080pIndex]));
  EXPECT_EQ(absl::nullopt, processor.FindNextConfiguration());
}

TEST(NewResourceAdaptationProcessorPocTest,
     MultipleStreamsLargestStreamGetsAdaptedDownOnOveruse) {
  NewResourceAdaptationProcessorPoc processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kOveruse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  auto* first_stream = processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "FirstStream", resolution_configs[k1080pIndex]));
  auto* second_stream =
      processor.AddConsumer(std::make_unique<ResourceConsumer>(
          "SecondStream", resolution_configs[k720pIndex]));
  // When the first stream is larger.
  auto next_config = processor.FindNextConfiguration();
  EXPECT_TRUE(next_config.has_value());
  EXPECT_EQ(first_stream, next_config->consumer);
  // When the second stream is larger.
  first_stream->SetConfiguration(resolution_configs[k720pIndex]);
  second_stream->SetConfiguration(resolution_configs[k1080pIndex]);
  next_config = processor.FindNextConfiguration();
  EXPECT_TRUE(next_config.has_value());
  EXPECT_EQ(second_stream, next_config->consumer);
}

TEST(NewResourceAdaptationProcessorPocTest,
     MultipleStreamsSmallestStreamGetsAdaptedUpOnUnderuse) {
  NewResourceAdaptationProcessorPoc processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kUnderuse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  auto* first_stream = processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "FirstStream", resolution_configs[k360pIndex]));
  auto* second_stream =
      processor.AddConsumer(std::make_unique<ResourceConsumer>(
          "SecondStream", resolution_configs[k180pIndex]));
  // When the first stream is larger.
  auto next_config = processor.FindNextConfiguration();
  EXPECT_TRUE(next_config.has_value());
  EXPECT_EQ(second_stream, next_config->consumer);
  // When the second stream is larger.
  first_stream->SetConfiguration(resolution_configs[k180pIndex]);
  second_stream->SetConfiguration(resolution_configs[k360pIndex]);
  next_config = processor.FindNextConfiguration();
  EXPECT_TRUE(next_config.has_value());
  EXPECT_EQ(first_stream, next_config->consumer);
}

// If both streams are equally valid to adapt down, the first one is preferred.
TEST(NewResourceAdaptationProcessorPocTest,
     MultipleStreamsAdaptFirstStreamWhenBothStreamsHaveSameCost) {
  NewResourceAdaptationProcessorPoc processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kOveruse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  auto* first_stream = processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "FirstStream", resolution_configs[k720pIndex]));
  processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "SecondStream", resolution_configs[k720pIndex]));
  auto next_config = processor.FindNextConfiguration();
  EXPECT_TRUE(next_config.has_value());
  EXPECT_EQ(first_stream, next_config->consumer);
}

TEST(NewResourceAdaptationProcessorPocTest,
     MultipleResourcesAdaptDownIfAnyIsOverused) {
  NewResourceAdaptationProcessorPoc processor;
  auto* first_resource = processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kOveruse));
  auto* second_resource = processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kStable));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "OnlyStream", resolution_configs[k1080pIndex]));
  // When the first resource is overused.
  EXPECT_TRUE(processor.FindNextConfiguration().has_value());
  // When the second resource is overused.
  first_resource->set_usage_state(ResourceUsageState::kStable);
  second_resource->set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_TRUE(processor.FindNextConfiguration().has_value());
}

TEST(NewResourceAdaptationProcessorPocTest,
     MultipleResourcesAdaptUpIfAllAreUnderused) {
  NewResourceAdaptationProcessorPoc processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kUnderuse));
  auto* second_resource = processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kStable));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "OnlyStream", resolution_configs[k720pIndex]));
  // When only the first resource is underused.
  EXPECT_EQ(absl::nullopt, processor.FindNextConfiguration());
  // When all resources are underused.
  second_resource->set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_TRUE(processor.FindNextConfiguration().has_value());
}

TEST(NewResourceAdaptationProcessorPocTest,
     HighestPreferredNeighborIsPickedWhenAdapting) {
  NewResourceAdaptationProcessorPoc processor;
  // Set up the following graph, where (#) is the preference.
  //
  //    Downward arrows          Upward arrows
  //
  //   a(1) -----> b(2)       a(1) <----- b(2)
  //    |        ^  |          ^        /  ^
  //    |     /     |          |     /     |
  //    v  /        v          |  v        |
  //   c(1.5) ---> d(2)       c(1.5) <--- d(2)
  //
  auto* a = processor.AddConfiguration(
      std::make_unique<FakeResourceConsumerConfiguration>(1, 1, 1, 1.0));
  auto* b = processor.AddConfiguration(
      std::make_unique<FakeResourceConsumerConfiguration>(1, 1, 1, 2.0));
  auto* c = processor.AddConfiguration(
      std::make_unique<FakeResourceConsumerConfiguration>(1, 1, 1, 1.5));
  auto* d = processor.AddConfiguration(
      std::make_unique<FakeResourceConsumerConfiguration>(1, 1, 1, 2.0));
  ConnectNeighbors(a, b);
  ConnectNeighbors(a, c);
  ConnectNeighbors(b, d);
  ConnectNeighbors(c, b);
  ConnectNeighbors(c, d);

  auto* resource = processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kOveruse));
  auto* consumer = processor.AddConsumer(
      std::make_unique<ResourceConsumer>("OnlyStream", a));

  // We should expect adapting down: a -> b -> d
  EXPECT_EQ(b, processor.FindNextConfiguration()->configuration);
  consumer->SetConfiguration(b);
  EXPECT_EQ(d, processor.FindNextConfiguration()->configuration);
  consumer->SetConfiguration(d);

  // We should expect to adapt up: d -> b -> c -> a
  resource->set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(b, processor.FindNextConfiguration()->configuration);
  consumer->SetConfiguration(b);
  EXPECT_EQ(c, processor.FindNextConfiguration()->configuration);
  consumer->SetConfiguration(c);
  EXPECT_EQ(a, processor.FindNextConfiguration()->configuration);
}

}  // namespace webrtc
