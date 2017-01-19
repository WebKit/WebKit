/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_SCTP_SCTPDATAENGINE_H_
#define WEBRTC_MEDIA_SCTP_SCTPDATAENGINE_H_

#include <errno.h>
#include <string>
#include <vector>

namespace cricket {
// Some ERRNO values get re-#defined to WSA* equivalents in some talk/
// headers.  We save the original ones in an enum.
enum PreservedErrno {
  SCTP_EINPROGRESS = EINPROGRESS,
  SCTP_EWOULDBLOCK = EWOULDBLOCK
};
}  // namespace cricket

#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/gtest_prod_util.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/mediaengine.h"

// Defined by "usrsctplib/usrsctp.h"
struct sockaddr_conn;
struct sctp_assoc_change;
struct sctp_stream_reset_event;
// Defined by <sys/socket.h>
struct socket;
namespace cricket {
// The number of outgoing streams that we'll negotiate. Since stream IDs (SIDs)
// are 0-based, the highest usable SID is 1023.
//
// It's recommended to use the maximum of 65535 in:
// https://tools.ietf.org/html/draft-ietf-rtcweb-data-channel-13#section-6.2
// However, we use 1024 in order to save memory. usrsctp allocates 104 bytes
// for each pair of incoming/outgoing streams (on a 64-bit system), so 65535
// streams would waste ~6MB.
//
// Note: "max" and "min" here are inclusive.
constexpr uint16_t kMaxSctpStreams = 1024;
constexpr uint16_t kMaxSctpSid = kMaxSctpStreams - 1;
constexpr uint16_t kMinSctpSid = 0;

// This is the default SCTP port to use. It is passed along the wire and the
// connectee and connector must be using the same port. It is not related to the
// ports at the IP level. (Corresponds to: sockaddr_conn.sconn_port in
// usrsctp.h)
const int kSctpDefaultPort = 5000;

class SctpDataMediaChannel;

// A DataEngine that interacts with usrsctp.
//
// From channel calls, data flows like this:
// [worker thread (although it can in princple be another thread)]
//  1.  SctpDataMediaChannel::SendData(data)
//  2.  usrsctp_sendv(data)
// [worker thread returns; sctp thread then calls the following]
//  3.  OnSctpOutboundPacket(wrapped_data)
// [sctp thread returns having posted a message for the worker thread]
//  4.  SctpDataMediaChannel::OnMessage(wrapped_data)
//  5.  SctpDataMediaChannel::OnPacketFromSctpToNetwork(wrapped_data)
//  6.  NetworkInterface::SendPacket(wrapped_data)
//  7.  ... across network ... a packet is sent back ...
//  8.  SctpDataMediaChannel::OnPacketReceived(wrapped_data)
//  9.  usrsctp_conninput(wrapped_data)
// [worker thread returns; sctp thread then calls the following]
//  10.  OnSctpInboundData(data)
// [sctp thread returns having posted a message fot the worker thread]
//  11. SctpDataMediaChannel::OnMessage(inboundpacket)
//  12. SctpDataMediaChannel::OnInboundPacketFromSctpToChannel(inboundpacket)
//  13. SctpDataMediaChannel::OnDataFromSctpToChannel(data)
//  14. SctpDataMediaChannel::SignalDataReceived(data)
// [from the same thread, methods registered/connected to
//  SctpDataMediaChannel are called with the recieved data]
class SctpDataEngine : public DataEngineInterface, public sigslot::has_slots<> {
 public:
  SctpDataEngine();
  ~SctpDataEngine() override;

  DataMediaChannel* CreateChannel(DataChannelType data_channel_type) override;
  const std::vector<DataCodec>& data_codecs() override { return codecs_; }

 private:
  const std::vector<DataCodec> codecs_;
};

// TODO(ldixon): Make into a special type of TypedMessageData.
// Holds data to be passed on to a channel.
struct SctpInboundPacket;

class SctpDataMediaChannel : public DataMediaChannel,
                             public rtc::MessageHandler {
 public:
  // DataMessageType is used for the SCTP "Payload Protocol Identifier", as
  // defined in http://tools.ietf.org/html/rfc4960#section-14.4
  //
  // For the list of IANA approved values see:
  // http://www.iana.org/assignments/sctp-parameters/sctp-parameters.xml
  // The value is not used by SCTP itself. It indicates the protocol running
  // on top of SCTP.
  enum PayloadProtocolIdentifier {
    PPID_NONE = 0,  // No protocol is specified.
    // Matches the PPIDs in mozilla source and
    // https://datatracker.ietf.org/doc/draft-ietf-rtcweb-data-protocol Sec. 9
    // They're not yet assigned by IANA.
    PPID_CONTROL = 50,
    PPID_BINARY_PARTIAL = 52,
    PPID_BINARY_LAST = 53,
    PPID_TEXT_PARTIAL = 54,
    PPID_TEXT_LAST = 51
  };

  typedef std::set<uint32_t> StreamSet;

  // Given a thread which will be used to post messages (received data) to this
  // SctpDataMediaChannel instance.
  explicit SctpDataMediaChannel(rtc::Thread* thread);
  virtual ~SctpDataMediaChannel();

  // When SetSend is set to true, connects. When set to false, disconnects.
  // Calling: "SetSend(true); SetSend(false); SetSend(true);" will connect,
  // disconnect, and reconnect.
  virtual bool SetSend(bool send);
  // Unless SetReceive(true) is called, received packets will be discarded.
  virtual bool SetReceive(bool receive);

  virtual bool SetSendParameters(const DataSendParameters& params);
  virtual bool SetRecvParameters(const DataRecvParameters& params);
  virtual bool AddSendStream(const StreamParams& sp);
  virtual bool RemoveSendStream(uint32_t ssrc);
  virtual bool AddRecvStream(const StreamParams& sp);
  virtual bool RemoveRecvStream(uint32_t ssrc);

  // Called when Sctp gets data. The data may be a notification or data for
  // OnSctpInboundData. Called from the worker thread.
  virtual void OnMessage(rtc::Message* msg);
  // Send data down this channel (will be wrapped as SCTP packets then given to
  // sctp that will then post the network interface by OnMessage).
  // Returns true iff successful data somewhere on the send-queue/network.
  virtual bool SendData(const SendDataParams& params,
                        const rtc::CopyOnWriteBuffer& payload,
                        SendDataResult* result = NULL);
  // A packet is received from the network interface. Posted to OnMessage.
  virtual void OnPacketReceived(rtc::CopyOnWriteBuffer* packet,
                                const rtc::PacketTime& packet_time);

  // Exposed to allow Post call from c-callbacks.
  rtc::Thread* worker_thread() const { return worker_thread_; }

  // Many of these things are unused by SCTP, but are needed to fulfill
  // the MediaChannel interface.
  virtual void OnRtcpReceived(rtc::CopyOnWriteBuffer* packet,
                              const rtc::PacketTime& packet_time) {}
  virtual void OnReadyToSend(bool ready) {}
  virtual void OnTransportOverheadChanged(int transport_overhead_per_packet) {}

  void OnSendThresholdCallback();
  // Helper for debugging.
  void set_debug_name_for_testing(const char* debug_name) {
    debug_name_ = debug_name;
  }
  const struct socket* socket() const { return sock_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(SctpDataMediaChannelTest, EngineSignalsRightChannel);
  static int SendThresholdCallback(struct socket* sock, uint32_t sb_free);
  static SctpDataMediaChannel* GetChannelFromSocket(struct socket* sock);

 private:
  sockaddr_conn GetSctpSockAddr(int port);

  bool SetSendCodecs(const std::vector<DataCodec>& codecs);
  bool SetRecvCodecs(const std::vector<DataCodec>& codecs);

  // Creates the socket and connects. Sets sending_ to true.
  bool Connect();
  // Closes the socket. Sets sending_ to false.
  void Disconnect();

  // Returns false when openning the socket failed; when successfull sets
  // sending_ to true
  bool OpenSctpSocket();
  // Sets sending_ to false and sock_ to NULL.
  void CloseSctpSocket();

  // Sends a SCTP_RESET_STREAM for all streams in closing_ssids_.
  bool SendQueuedStreamResets();

  // Adds a stream.
  bool AddStream(const StreamParams &sp);
  // Queues a stream for reset.
  bool ResetStream(uint32_t ssrc);

  // Called by OnMessage to send packet on the network.
  void OnPacketFromSctpToNetwork(rtc::CopyOnWriteBuffer* buffer);
  // Called by OnMessage to decide what to do with the packet.
  void OnInboundPacketFromSctpToChannel(SctpInboundPacket* packet);
  void OnDataFromSctpToChannel(const ReceiveDataParams& params,
                               const rtc::CopyOnWriteBuffer& buffer);
  void OnNotificationFromSctp(const rtc::CopyOnWriteBuffer& buffer);
  void OnNotificationAssocChange(const sctp_assoc_change& change);

  void OnStreamResetEvent(const struct sctp_stream_reset_event* evt);

  // Responsible for marshalling incoming data to the channels listeners, and
  // outgoing data to the network interface.
  rtc::Thread* worker_thread_;
  // The local and remote SCTP port to use. These are passed along the wire
  // and the listener and connector must be using the same port. It is not
  // related to the ports at the IP level.  If set to -1, we default to
  // kSctpDefaultPort.
  int local_port_;
  int remote_port_;
  struct socket* sock_;  // The socket created by usrsctp_socket(...).

  // sending_ is true iff there is a connected socket.
  bool sending_;
  // receiving_ controls whether inbound packets are thrown away.
  bool receiving_;

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
  const char* debug_name_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_SCTP_SCTPDATAENGINE_H_
