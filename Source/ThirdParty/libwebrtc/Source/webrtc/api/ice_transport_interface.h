/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ICE_TRANSPORT_INTERFACE_H_
#define API_ICE_TRANSPORT_INTERFACE_H_

#include <string>

#include "api/async_dns_resolver.h"
#include "api/environment/environment.h"
#include "api/local_network_access_permission.h"
#include "api/ref_count.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/scoped_refptr.h"

namespace webrtc {

class ActiveIceControllerFactoryInterface;
class FieldTrialsView;
class IceControllerFactoryInterface;
class IceTransportInternal;
class PortAllocator;

// An ICE transport, as represented to the outside world.
// This object is refcounted, and is therefore alive until the
// last holder has released it.
class IceTransportInterface : public RefCountInterface {
 public:
  // Accessor for the internal representation of an ICE transport.
  // The returned object can only be safely used on the signalling thread.
  // TODO(crbug.com/907849): Add API calls for the functions that have to
  // be exposed to clients, and stop allowing access to the
  // IceTransportInternal API.
  virtual IceTransportInternal* internal() = 0;
};

class IceTransportInit final {
 public:
  explicit IceTransportInit(const Environment& env) : env_(env) {}
  IceTransportInit(const IceTransportInit&) = delete;
  IceTransportInit(IceTransportInit&&) = default;
  IceTransportInit& operator=(const IceTransportInit&) = delete;
  IceTransportInit& operator=(IceTransportInit&&) = default;

  PortAllocator* port_allocator() { return port_allocator_; }
  void set_port_allocator(PortAllocator* port_allocator) {
    port_allocator_ = port_allocator;
  }

  AsyncDnsResolverFactoryInterface* async_dns_resolver_factory() {
    return async_dns_resolver_factory_;
  }
  void set_async_dns_resolver_factory(
      AsyncDnsResolverFactoryInterface* async_dns_resolver_factory) {
    async_dns_resolver_factory_ = async_dns_resolver_factory;
  }

  LocalNetworkAccessPermissionFactoryInterface* lna_permission_factory() {
    return lna_permission_factory_;
  }

  void set_lna_permission_factory(
      LocalNetworkAccessPermissionFactoryInterface* lna_permission_factory) {
    lna_permission_factory_ = lna_permission_factory;
  }

  RtcEventLog* event_log() { return &env_.event_log(); }

  void set_ice_controller_factory(
      IceControllerFactoryInterface* ice_controller_factory) {
    ice_controller_factory_ = ice_controller_factory;
  }
  IceControllerFactoryInterface* ice_controller_factory() {
    return ice_controller_factory_;
  }

  // An active ICE controller actively manages the connection used by an ICE
  // transport, in contrast with a legacy ICE controller that only picks the
  // best connection to use or ping, and lets the transport decide when and
  // whether to switch.
  //
  // Which ICE controller is used is determined as follows:
  //
  //   1. If an active ICE controller factory is supplied, it is used and
  //      the legacy ICE controller factory is not used.
  //   2. If not, a default active ICE controller is used, wrapping over the
  //      supplied or the default legacy ICE controller.
  void set_active_ice_controller_factory(
      ActiveIceControllerFactoryInterface* active_ice_controller_factory) {
    active_ice_controller_factory_ = active_ice_controller_factory;
  }
  ActiveIceControllerFactoryInterface* active_ice_controller_factory() {
    return active_ice_controller_factory_;
  }

  const FieldTrialsView* field_trials() { return &env_.field_trials(); }

  const Environment& env() { return env_; }

 private:
  Environment env_;
  PortAllocator* port_allocator_ = nullptr;
  AsyncDnsResolverFactoryInterface* async_dns_resolver_factory_ = nullptr;
  LocalNetworkAccessPermissionFactoryInterface* lna_permission_factory_ =
      nullptr;
  IceControllerFactoryInterface* ice_controller_factory_ = nullptr;
  ActiveIceControllerFactoryInterface* active_ice_controller_factory_ = nullptr;
  // TODO(https://crbug.com/webrtc/12657): Redesign to have const members.
};

// TODO(qingsi): The factory interface is defined in this file instead of its
// namesake file ice_transport_factory.h to avoid the extra dependency on p2p/
// introduced there by the p2p/-dependent factory methods. Move the factory
// methods to a different file or rename it.
class IceTransportFactory {
 public:
  virtual ~IceTransportFactory() = default;
  // As a refcounted object, the returned ICE transport may outlive the host
  // construct into which its reference is given, e.g. a peer connection. As a
  // result, the returned ICE transport should not hold references to any object
  // that the transport does not own and that has a lifetime bound to the host
  // construct. Also, assumptions on the thread safety of the returned transport
  // should be clarified by implementations. For example, a peer connection
  // requires the returned transport to be constructed and destroyed on the
  // network thread and an ICE transport factory that intends to work with a
  // peer connection should offer transports compatible with these assumptions.
  virtual scoped_refptr<IceTransportInterface> CreateIceTransport(
      const std::string& transport_name,
      int component,
      IceTransportInit init) = 0;
};

}  // namespace webrtc
#endif  // API_ICE_TRANSPORT_INTERFACE_H_
