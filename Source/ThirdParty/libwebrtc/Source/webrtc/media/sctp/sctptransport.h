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

#include <memory>  // for unique_ptr.
#include <set>
#include <string>
#include <vector>

#include "rtc_base/asyncinvoker.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/sigslot.h"
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
//  5.  TransportChannel::SendPacket(wrapped_data)
//  6.  ... across network ... a packet is sent back ...
//  7.  SctpTransport::OnPacketReceived(wrapped_data)
//  8.  usrsctp_conninput(wrapped_data)
// [network thread returns; sctp thread then calls the following]
//  9.  OnSctpInboundData(data)
// [sctp thread returns having async invoked on the network thread]
//  10. SctpTransport::OnInboundPacketFromSctpToChannel(inboundpacket)
//  11. SctpTransport::OnDataFromSctpToChannel(data)
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
  void SetTransportChannel(rtc::PacketTransportInternal* channel) override;
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
  void ConnectTransportChannelSignals();
  void DisconnectTransportChannelSignals();

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
  void OnInboundPacketFromSctpToChannel(const rtc::CopyOnWriteBuffer& buffer,
                                        ReceiveDataParams params,
                                        int flags);
  void OnDataFromSctpToChannel(const ReceiveDataParams& params,
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
  rtc::PacketTransportInternal* transport_channel_;
  bool was_ever_writable_ = false;
  int local_port_ = kSctpDefaultPort;
  int remote_port_ = kSctpDefaultPort;
  struct socket* sock_ = nullptr;  // The socket created by usrsctp_socket(...).

  // Has Start been called? Don't create SCTP socket until it has.
  bool started_ = false;
  // Are we ready to queue data (SCTP socket created, and not blocked due to
  // congestion control)? Different than |transport_channel_|'s "ready to
  // send".
  bool ready_to_send_data_ = false;

  typedef std::set<uint32_t> StreamSet;
  // When a data channel opens a stream, it goes into open_streams_.  When we
  // want to close it, the stream's ID goes into queued_reset_streams_.  When
  // we actually transmit a RE-CONFIG chunk with that stream ID, the ID goes
  // into sent_reset_streams_.  When we get a response RE-CONFIG chunk back
  // acknowledging the reset, we remove the stream ID from
  // sent_reset_streams_.  We use sent_reset_streams_ to differentiate
  // between acknowledgment RE-CONFIG and peer-initiated RE-CONFIGs.
  StreamSet open_streams_;
  StreamSet queued_reset_streams_;
  StreamSet sent_reset_streams_;

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
      rtc::PacketTransportInternal* channel) override {
    return std::unique_ptr<SctpTransportInternal>(
        new SctpTransport(network_thread_, channel));
  }

 private:
  rtc::Thread* network_thread_;
};

}  // namespace cricket

#endif  // MEDIA_SCTP_SCTPTRANSPORT_H_
