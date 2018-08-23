/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_PROXY_H_
#define MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_PROXY_H_

#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

// Class for temporarily redirecting AECM configuration to a new API.
class EchoControlMobileProxy : public EchoControlMobile {
 public:
  EchoControlMobileProxy(AudioProcessing* audio_processing,
                         EchoControlMobile* echo_control_mobile);
  ~EchoControlMobileProxy() override;

  bool is_enabled() const override;
  RoutingMode routing_mode() const override;
  bool is_comfort_noise_enabled() const override;
  int Enable(bool enable) override;
  int set_routing_mode(RoutingMode mode) override;
  int enable_comfort_noise(bool enable) override;
  int SetEchoPath(const void* echo_path, size_t size_bytes) override;
  int GetEchoPath(void* echo_path, size_t size_bytes) const override;

 private:
  AudioProcessing* audio_processing_;
  EchoControlMobile* echo_control_mobile_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoControlMobileProxy);
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_PROXY_H_
