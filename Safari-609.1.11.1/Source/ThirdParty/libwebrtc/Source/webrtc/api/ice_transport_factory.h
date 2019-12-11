/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ICE_TRANSPORT_FACTORY_H_
#define API_ICE_TRANSPORT_FACTORY_H_

#include "api/async_resolver_factory.h"
#include "api/ice_transport_interface.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace cricket {
class PortAllocator;
}

namespace webrtc {

struct IceTransportInit final {
 public:
  IceTransportInit() = default;
  IceTransportInit(const IceTransportInit&) = delete;
  IceTransportInit(IceTransportInit&&) = default;
  IceTransportInit& operator=(const IceTransportInit&) = delete;
  IceTransportInit& operator=(IceTransportInit&&) = default;

  cricket::PortAllocator* port_allocator() { return port_allocator_; }
  void set_port_allocator(cricket::PortAllocator* port_allocator) {
    port_allocator_ = port_allocator;
  }

  AsyncResolverFactory* async_resolver_factory() {
    return async_resolver_factory_;
  }
  void set_async_resolver_factory(
      AsyncResolverFactory* async_resolver_factory) {
    async_resolver_factory_ = async_resolver_factory;
  }

  RtcEventLog* event_log() { return event_log_; }
  void set_event_log(RtcEventLog* event_log) { event_log_ = event_log; }

 private:
  cricket::PortAllocator* port_allocator_ = nullptr;
  AsyncResolverFactory* async_resolver_factory_ = nullptr;
  RtcEventLog* event_log_ = nullptr;
};

// Static factory for an IceTransport object that can be created
// without using a webrtc::PeerConnection.
// The returned object must be accessed and destroyed on the thread that
// created it.
// The PortAllocator must outlive the created IceTransportInterface object.
// TODO(steveanton): Remove in favor of the overload that takes
// IceTransportInit.
RTC_EXPORT rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
    cricket::PortAllocator* port_allocator);

// Static factory for an IceTransport object that can be created
// without using a webrtc::PeerConnection.
// The returned object must be accessed and destroyed on the thread that
// created it.
// |init.port_allocator()| is required and must outlive the created
//     IceTransportInterface object.
// |init.async_resolver_factory()| and |init.event_log()| are optional, but if
//     provided must outlive the created IceTransportInterface object.
RTC_EXPORT rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
    IceTransportInit);

}  // namespace webrtc

#endif  // API_ICE_TRANSPORT_FACTORY_H_
