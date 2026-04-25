/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_PACKET_TRANSPORT_INTERNAL_H_
#define P2P_BASE_PACKET_TRANSPORT_INTERNAL_H_

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/sequence_checker.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_EXPORT PacketTransportInternal {
 public:
  virtual const std::string& transport_name() const = 0;

  // The transport has been established.
  virtual bool writable() const = 0;

  // The transport has received a packet in the last X milliseconds, here X is
  // configured by each implementation.
  virtual bool receiving() const = 0;

  // Attempts to send the given packet.
  // The return value is < 0 on failure. The return value in failure case is not
  // descriptive. Depending on failure cause and implementation details
  // GetError() returns an descriptive errno.h error value.
  // This mimics posix socket send() or sendto() behavior.
  // TODO(johan): Reliable, meaningful, consistent error codes for all
  // implementations would be nice.
  // TODO(johan): Remove the default argument once channel code is updated.
  virtual int SendPacket(const char* data,
                         size_t len,
                         const AsyncSocketPacketOptions& options,
                         int flags = 0) = 0;

  // Sets a socket option. Note that not all options are
  // supported by all transport types.
  virtual int SetOption(Socket::Option opt, int value) = 0;

  // TODO(pthatcher): Once Chrome's MockPacketTransportInterface implements
  // this, remove the default implementation.
  virtual bool GetOption(Socket::Option opt, int* value);

  // Returns the most recent error that occurred on this channel.
  virtual int GetError() = 0;

  // Returns the current network route with transport overhead.
  // TODO(zhihuang): Make it pure virtual once the Chrome/remoting is updated.
  virtual std::optional<NetworkRoute> network_route() const;

  // Emitted when the writable state, represented by `writable()`, changes.
  void SubscribeWritableState(
      void* tag,
      absl::AnyInvocable<void(PacketTransportInternal*)> callback);
  void UnsubscribeWritableState(void* tag);
  void NotifyWritableState(PacketTransportInternal* packet_transport);

  //  Emitted when the PacketTransportInternal is ready to send packets. "Ready
  //  to send" is more sensitive than the writable state; a transport may be
  //  writable, but temporarily not able to send packets. For example, the
  //  underlying transport's socket buffer may be full, as indicated by
  //  SendPacket's return code and/or GetError.
  void SubscribeReadyToSend(
      void* tag,
      absl::AnyInvocable<void(PacketTransportInternal*)> callback);
  void UnsubscribeReadyToSend(void* tag);
  void NotifyReadyToSend(PacketTransportInternal* packet_transport);

  // Emitted when receiving state changes to true.
  [[deprecated]] void SubscribeReceivingState(
      absl::AnyInvocable<void(PacketTransportInternal*)> callback);
  void SubscribeReceivingState(
      void* tag,
      absl::AnyInvocable<void(PacketTransportInternal*)> callback);
  void NotifyReceivingState(PacketTransportInternal* packet_transport);

  // Callback is invoked each time a packet is received on this channel.
  void RegisterReceivedPacketCallback(
      void* id,
      absl::AnyInvocable<void(PacketTransportInternal*,
                              const ReceivedIpPacket&)> callback);

  void DeregisterReceivedPacketCallback(void* id);

  // Signalled each time a packet is sent on this channel.
  void NotifySentPacket(PacketTransportInternal* transport,
                        const SentPacketInfo& info) {
    sent_packet_callbacks_.Send(transport, info);
  }
  void SubscribeSentPacket(
      void* tag,
      absl::AnyInvocable<void(PacketTransportInternal*, const SentPacketInfo&)>
          callback) {
    sent_packet_callbacks_.AddReceiver(tag, std::move(callback));
  }
  void UnsubscribeSentPacket(void* tag) {
    sent_packet_callbacks_.RemoveReceivers(tag);
  }
  // Signalled when the current network route has changed.
  void SubscribeNetworkRouteChanged(
      void* tag,
      absl::AnyInvocable<void(std::optional<NetworkRoute>)> callback);
  void UnsubscribeNetworkRouteChanged(void* tag);
  void NotifyNetworkRouteChanged(
      std::optional<webrtc::NetworkRoute> network_route);

  // Signalled when the transport is closed.
  void SetOnCloseCallback(absl::AnyInvocable<void() &&> callback);

  virtual ~PacketTransportInternal();

 protected:
  PacketTransportInternal();

  void NotifyPacketReceived(const ReceivedIpPacket& packet);
  void NotifyOnClose();

  SequenceChecker network_checker_{SequenceChecker::kDetached};

 private:
  CallbackList<PacketTransportInternal*, const ReceivedIpPacket&>
      received_packet_callback_list_ RTC_GUARDED_BY(&network_checker_);
  absl::AnyInvocable<void() &&> on_close_;
  CallbackList<PacketTransportInternal*, const SentPacketInfo&>
      sent_packet_callbacks_;
  CallbackList<std::optional<NetworkRoute>> network_route_changed_callbacks_;
  CallbackList<PacketTransportInternal*> writable_state_callbacks_;
  CallbackList<PacketTransportInternal*> ready_to_send_callbacks_;
  CallbackList<PacketTransportInternal*> receiving_state_callbacks_;
};

}  //  namespace webrtc

#endif  // P2P_BASE_PACKET_TRANSPORT_INTERNAL_H_
