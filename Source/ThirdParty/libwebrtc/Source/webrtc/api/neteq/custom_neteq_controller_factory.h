/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NETEQ_CUSTOM_NETEQ_CONTROLLER_FACTORY_H_
#define API_NETEQ_CUSTOM_NETEQ_CONTROLLER_FACTORY_H_

#include <memory>

#include "api/environment/environment.h"
#include "api/neteq/delay_manager_factory.h"
#include "api/neteq/neteq_controller.h"
#include "api/neteq/neteq_controller_factory.h"
namespace webrtc {

// This factory can be used to generate NetEqController instances that make
// use of a custom DelayManagerFactory.
class CustomNetEqControllerFactory : public NetEqControllerFactory {
 public:
  explicit CustomNetEqControllerFactory(
      std::unique_ptr<DelayManagerFactory> delay_manager_factory);
  ~CustomNetEqControllerFactory() override;
  CustomNetEqControllerFactory(const CustomNetEqControllerFactory&) = delete;
  CustomNetEqControllerFactory& operator=(const CustomNetEqControllerFactory&) =
      delete;

  std::unique_ptr<NetEqController> Create(
      const Environment& env,
      const NetEqController::Config& config) const override;

 private:
  std::unique_ptr<DelayManagerFactory> delay_manager_factory_;
};

}  // namespace webrtc

#endif  // API_NETEQ_CUSTOM_NETEQ_CONTROLLER_FACTORY_H_
