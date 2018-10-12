/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_SCTP_SCTPTRANSPORT_H_
#define MEDIA_SCTP_SCTPTRANSPORT_H_

#include <errno.h>

#include <map>
#include <memory>  // for unique_ptr.
#include <set>
#include <string>
#include <vector>

#include "rtc_base/asyncinvoker.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
// For SendDataParams/ReceiveDataParams.
#include "media/base/mediachannel.h"
#include "media/sctp/sctptransportinternal.h"

// Defined by "usrsctplib/usrsctp.h"
struct sockaddr_conn;
struct sctp_assoc_change;
struct sctp_stream_reset_event;
// Defined by <sys/socket.h>
struct socket;
namespace cricket {

// Holds data to be passed on to a channel.
struct SctpInboundPacket;

// From channel calls, data flows like this:
// [network thread (although it can in princple be another thread)]
//  1.  SctpTransport::SendData(data)
//  2.  usrsctp_sendv(data)
// [network thread returns; sctp thread then calls the following]
//  3.  OnSctpOutboundPacket(wrapped_data)
// [sctp thread returns having async invoked on the network thread]
//  4.  SctpTransport::OnPacketFromSctpToNetwork(wrapped_data)
//  5.  DtlsTransport::SendPacket(wrapped_data)
//  6.  ... across network ... a packet is sent back ...
//  7.  SctpTransport::OnPacketReceived(wrapped_data)
//  8.  usrsctp_conninput(wrapped_data)
// [network thread returns; sctp thread then calls the following]
//  9.  OnSctpInboundData(data)
// [sctp thread returns having async invoked on the network thread]
//  10. SctpTransport::OnInboundPacketFromSctpToTransport(inboundpacket)
//  11. SctpTransport::OnDataFromSctpToTransport(data)
//  12. SctpTransport::SignalDataReceived(data)
// [from the same thread, methods registered/connected to
//  SctpTransport are called with the recieved data]
// TODO(zhihuang): Rename "channel" to "transport" on network-level.
class SctpTransport : public SctpTransportInternal,
                      public sigslot::has_slots<> {
 public:
  // |network_thread| is where packets will be processed and callbacks from
  // this transport will be posted, and is the only thread on which public
  // methods can be called.
  // |channel| is required (must not be null).
  SctpTransport(rtc::Thread* network_thread,
                rtc::PacketTransportInternal* channel);
  ~SctpTransport() override;

  // SctpTransportInternal overrides (see sctptransportinternal.h for comments).
  void SetDtlsTransport(rtc::PacketTransportInternal* transport) override;
  bool Start(int local_port, int remote_port) override;
  bool OpenStream(int sid) override;
  bool ResetStream(int sid) override;
  bool SendData(const SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                SendDataResult* result = nullptr) override;
  bool ReadyToSendData() override;
  void set_debug_name_for_testing(const char* debug_name) override {
    debug_name_ = debug_name;
  }

  // Exposed to allow Post call from c-callbacks.
  // TODO(deadbeef): Remove this or at least make it return a const pointer.
  rtc::Thread* network_thread() const { return network_thread_; }

 private:
  void ConnectTransportSignals();
  void DisconnectTransportSignals();

  // Creates the socket and connects.
  bool Connect();

  // Returns false when opening the socket failed.
  bool OpenSctpSocket();
  // Helpet method to set socket options.
  bool ConfigureSctpSocket();
  // Sets |sock_ |to nullptr.
  void CloseSctpSocket();

  // Sends a SCTP_RESET_STREAM for all streams in closing_ssids_.
  bool SendQueuedStreamResets();

  // Sets the "ready to send" flag and fires signal if needed.
  void SetReadyToSendData();

  // Callbacks from DTLS channel.
  void OnWritableState(rtc::PacketTransportInternal* transport);
  virtual void OnPacketRead(rtc::PacketTransportInternal* transport,
                            const char* data,
                            size_t len,
                            const rtc::PacketTime& packet_time,
                            int flags);

  // Methods related to usrsctp callbacks.
  void OnSendThresholdCallback();
  sockaddr_conn GetSctpSockAddr(int port);

  // Called using |invoker_| to send packet on the network.
  void OnPacketFromSctpToNetwork(const rtc::CopyOnWriteBuffer& buffer);
  // Called using |invoker_| to decide what to do with the packet.
  // The |flags| parameter is used by SCTP to distinguish notification packets
  // from other types of packets.
  void OnInboundPacketFromSctpToTransport(const rtc::CopyOnWriteBuffer& buffer,
                                          ReceiveDataParams params,
                                          int flags);
  void OnDataFromSctpToTransport(const ReceiveDataParams& params,
                                 const rtc::CopyOnWriteBuffer& buffer);
  void OnNotificationFromSctp(const rtc::CopyOnWriteBuffer& buffer);
  void OnNotificationAssocChange(const sctp_assoc_change& change);

  void OnStreamResetEvent(const struct sctp_stream_reset_event* evt);

  // Responsible for marshalling incoming data to the channels listeners, and
  // outgoing data to the network interface.
  rtc::Thread* network_thread_;
  // Helps pass inbound/outbound packets asynchronously to the network thread.
  rtc::AsyncInvoker invoker_;
  // Underlying DTLS channel.
  rtc::PacketTransportInternal* transport_ = nullptr;

  // Track the data received from usrsctp between callbacks until the EOR bit
  // arrives.
  rtc::CopyOnWriteBuffer partial_message_;
  ReceiveDataParams partial_params_;
  int partial_flags_;

  bool was_ever_writable_ = false;
  int local_port_ = kSctpDefaultPort;
  int remote_port_ = kSctpDefaultPort;
  struct socket* sock_ = nullptr;  // The socket created by usrsctp_socket(...).

  // Has Start been called? Don't create SCTP socket until it has.
  bool started_ = false;
  // Are we ready to queue data (SCTP socket created, and not blocked due to
  // congestion control)? Different than |transport_|'s "ready to send".
  bool ready_to_send_data_ = false;

  // Used to keep track of the status of each stream (or rather, each pair of
  // incoming/outgoing streams with matching IDs). It's specifically used to
  // keep track of the status of resets, but more information could be put here
  // later.
  //
  // See datachannel.h for a summary of the closing procedure.
  struct StreamStatus {
    // Closure initiated by application via ResetStream? Note that
    // this may be true while outgoing_reset_initiated is false if the outgoing
    // reset needed to be queued.
    bool closure_initiated = false;
    // Whether we've initiated the outgoing stream reset via
    // SCTP_RESET_STREAMS.
    bool outgoing_reset_initiated = false;
    // Whether usrsctp has indicated that the incoming/outgoing streams have
    // been reset. It's expected that the peer will reset its outgoing stream
    // (our incoming stream) after receiving the reset for our outgoing stream,
    // though older versions of chromium won't do this. See crbug.com/559394
    // for context.
    bool outgoing_reset_complete = false;
    bool incoming_reset_complete = false;

    // Some helper methods to improve code readability.
    bool is_open() const {
      return !closure_initiated && !incoming_reset_complete &&
             !outgoing_reset_complete;
    }
    // We need to send an outgoing reset if the application has closed the data
    // channel, or if we received a reset of the incoming stream from the
    // remote endpoint, indicating the data channel was closed remotely.
    bool need_outgoing_reset() const {
      return (incoming_reset_complete || closure_initiated) &&
             !outgoing_reset_initiated;
    }
    bool reset_complete() const {
      return outgoing_reset_complete && incoming_reset_complete;
    }
  };

  // Entries should only be removed from this map if |reset_complete| is
  // true.
  std::map<uint32_t, StreamStatus> stream_status_by_sid_;

  // A static human-readable name for debugging messages.
  const char* debug_name_ = "SctpTransport";
  // Hides usrsctp interactions from this header file.
  class UsrSctpWrapper;

  RTC_DISALLOW_COPY_AND_ASSIGN(SctpTransport);
};

class SctpTransportFactory : public SctpTransportInternalFactory {
 public:
  explicit SctpTransportFactory(rtc::Thread* network_thread)
      : network_thread_(network_thread) {}

  std::unique_ptr<SctpTransportInternal> CreateSctpTransport(
      rtc::PacketTransportInternal* transport) override {
    return std::unique_ptr<SctpTransportInternal>(
        new SctpTransport(network_thread_, transport));
  }

 private:
  rtc::Thread* network_thread_;
};

}  // namespace cricket

#endif  // MEDIA_SCTP_SCTPTRANSPORT_H_
