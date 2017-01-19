/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_CONTROLLER_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_CONTROLLER_H_

#include "webrtc/modules/audio_coding/audio_network_adaptor/controller.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockController : public Controller {
 public:
  virtual ~MockController() { Die(); }
  MOCK_METHOD0(Die, void());
  MOCK_METHOD2(MakeDecision,
               void(const NetworkMetrics& metrics,
                    AudioNetworkAdaptor::EncoderRuntimeConfig* config));
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_CONTROLLER_H_
