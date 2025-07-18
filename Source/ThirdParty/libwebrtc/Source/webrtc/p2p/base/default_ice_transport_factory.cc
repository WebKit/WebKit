/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/default_ice_transport_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "api/ice_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "p2p/base/basic_ice_controller.h"
#include "p2p/base/ice_controller_factory_interface.h"
#include "p2p/base/ice_controller_interface.h"
#include "p2p/base/p2p_transport_channel.h"

namespace {

class BasicIceControllerFactory : public webrtc::IceControllerFactoryInterface {
 public:
  std::unique_ptr<webrtc::IceControllerInterface> Create(
      const webrtc::IceControllerFactoryArgs& args) override {
    return std::make_unique<webrtc::BasicIceController>(args);
  }
};

}  // namespace

namespace webrtc {

DefaultIceTransport::DefaultIceTransport(
    std::unique_ptr<P2PTransportChannel> internal)
    : internal_(std::move(internal)) {}

DefaultIceTransport::~DefaultIceTransport() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
}

scoped_refptr<IceTransportInterface>
DefaultIceTransportFactory::CreateIceTransport(
    const std::string& transport_name,
    int component,
    IceTransportInit init) {
  BasicIceControllerFactory factory;
  init.set_ice_controller_factory(&factory);
  return make_ref_counted<DefaultIceTransport>(
      P2PTransportChannel::Create(transport_name, component, std::move(init)));
}

}  // namespace webrtc
