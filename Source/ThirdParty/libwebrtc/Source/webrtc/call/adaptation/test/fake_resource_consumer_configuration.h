/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_TEST_FAKE_RESOURCE_CONSUMER_CONFIGURATION_H_
#define CALL_ADAPTATION_TEST_FAKE_RESOURCE_CONSUMER_CONFIGURATION_H_

#include <string>

#include "call/adaptation/resource_consumer_configuration.h"

namespace webrtc {

class FakeResourceConsumerConfiguration : public ResourceConsumerConfiguration {
 public:
  FakeResourceConsumerConfiguration(int width,
                                    int height,
                                    double frame_rate_hz,
                                    double preference);

  std::string Name() const override;
  double Cost() const override;
  double Preference() const override;

 private:
  int width_;
  int height_;
  double frame_rate_hz_;
  double preference_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_TEST_FAKE_RESOURCE_CONSUMER_CONFIGURATION_H_
