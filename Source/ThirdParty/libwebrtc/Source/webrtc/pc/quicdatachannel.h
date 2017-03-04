/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_QUICDATACHANNEL_H_
#define WEBRTC_PC_QUICDATACHANNEL_H_

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "webrtc/api/datachannelinterface.h"
#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/thread.h"

namespace cricket {
class QuicTransportChannel;
class ReliableQuicStream;
class TransportChannel;
}  // namepsace cricket

namespace net {
// TODO(mikescarlett): Make this uint64_t once QUIC uses 64-bit ids.
typedef uint32_t QuicStreamId;
}  // namespace net

namespace rtc {
class CopyOnWriteBuffer;
}  // namespace rtc

namespace webrtc {

// Encodes a QUIC message header with the data channel ID and message ID, then
// stores the result in |header|.
void WriteQuicDataChannelMessageHeader(int data_channel_id,
                                       uint64_t message_id,
                                       rtc::CopyOnWriteBuffer* header);

// Decodes the data channel ID and message ID from the initial data received by
// an incoming QUIC stream. The data channel ID is output to |data_channel_id|,
// the message ID is output to |message_id|, and the number of bytes read is
// output to |bytes_read|. Returns false if either ID cannot be read.
bool ParseQuicDataMessageHeader(const char* data,
                                size_t len,
                                int* data_channel_id,
                                uint64_t* message_id,
                                size_t* bytes_read);

// QuicDataChannel is an implementation of DataChannelInterface based on the
// QUIC protocol. It uses a QuicTransportChannel to establish encryption and
// transfer data, and a QuicDataTransport to receive incoming messages at
// the correct data channel. Currently this class implements unordered, reliable
// delivery and does not send an "OPEN" message.
//
// Each time a message is sent:
//
// - The QuicDataChannel prepends it with the data channel id and message id.
//   The QuicTransportChannel creates a ReliableQuicStream, then the
//   ReliableQuicStream sends the message with a FIN.
//
// - The remote QuicSession creates a ReliableQuicStream to receive the data.
//   The remote QuicDataTransport dispatches the ReliableQuicStream to the
//   QuicDataChannel with the same id as this data channel.
//
// - The remote QuicDataChannel queues data from the ReliableQuicStream. Once
//   it receives a QUIC stream frame with a FIN, it provides the message to the
//   DataChannelObserver.
//
// TODO(mikescarlett): Implement ordered delivery, unreliable delivery, and
// an OPEN message similar to the one for SCTP.
class QuicDataChannel : public rtc::RefCountedObject<DataChannelInterface>,
                        public sigslot::has_slots<> {
 public:
  // Message stores buffered data from the incoming QUIC stream. The QUIC stream
  // is provided so that remaining data can be received from the remote peer.
  struct Message {
    uint64_t id;
    rtc::CopyOnWriteBuffer buffer;
    cricket::ReliableQuicStream* stream;
  };

  QuicDataChannel(rtc::Thread* signaling_thread,
                  rtc::Thread* worker_thread,
                  rtc::Thread* network_thread,
                  const std::string& label,
                  const DataChannelInit& config);
  ~QuicDataChannel() override;

  // DataChannelInterface overrides.
  std::string label() const override { return label_; }
  bool reliable() const override { return true; }
  bool ordered() const override { return false; }
  uint16_t maxRetransmitTime() const override { return -1; }
  uint16_t maxRetransmits() const override { return -1; }
  bool negotiated() const override { return false; }
  int id() const override { return id_; }
  DataState state() const override { return state_; }
  uint64_t buffered_amount() const override { return buffered_amount_; }
  std::string protocol() const override { return protocol_; }
  void RegisterObserver(DataChannelObserver* observer) override;
  void UnregisterObserver() override;
  void Close() override;
  bool Send(const DataBuffer& buffer) override;

  // Called from QuicDataTransport to set the QUIC transport channel that the
  // QuicDataChannel sends messages with. Returns false if a different QUIC
  // transport channel is already set or |channel| is NULL.
  //
  // The QUIC transport channel is not set in the constructor to allow creating
  // the QuicDataChannel before the PeerConnection has a QUIC transport channel,
  // such as before the session description is not set.
  bool SetTransportChannel(cricket::QuicTransportChannel* channel);

  // Called from QuicDataTransport when an incoming ReliableQuicStream is
  // receiving a message received for this data channel. Once this function is
  // called, |message| is owned by the QuicDataChannel and should not be
  // accessed by the QuicDataTransport.
  void OnIncomingMessage(Message&& message);

  // Methods for testing.
  // Gets the number of outgoing QUIC streams with write blocked data that are
  // currently open for this data channel and are not finished writing a
  // message. This is equivalent to the size of |write_blocked_quic_streams_|.
  size_t GetNumWriteBlockedStreams() const;
  // Gets the number of incoming QUIC streams with buffered data that are
  // currently open for this data channel and are not finished receiving a
  // message. This is equivalent to the size of |incoming_quic_messages_|.
  size_t GetNumIncomingStreams() const;

 private:
  // Callbacks from ReliableQuicStream.
  // Called when an incoming QUIC stream in |incoming_quic_messages_| has
  // received a QUIC stream frame.
  void OnDataReceived(net::QuicStreamId stream_id,
                      const char* data,
                      size_t len);
  // Called when a write blocked QUIC stream that has been added to
  // |write_blocked_quic_streams_| is closed.
  void OnWriteBlockedStreamClosed(net::QuicStreamId stream_id, int error);
  // Called when an incoming QUIC stream that has been added to
  // |incoming_quic_messages_| is closed.
  void OnIncomingQueuedStreamClosed(net::QuicStreamId stream_id, int error);
  // Called when a write blocked QUIC stream in |write_blocked_quic_streams_|
  // has written previously queued data.
  void OnQueuedBytesWritten(net::QuicStreamId stream_id,
                            uint64_t queued_bytes_written);

  // Callbacks from |quic_transport_channel_|.
  void OnReadyToSend(cricket::TransportChannel* channel);
  void OnConnectionClosed();

  // Network thread methods.
  // Sends the data buffer to the remote peer using an outgoing QUIC stream.
  // Returns true if the data buffer can be successfully sent, or if it is
  // queued to be sent later.
  bool Send_n(const DataBuffer& buffer);

  // Worker thread methods.
  // Connects the |quic_transport_channel_| signals to this QuicDataChannel,
  // then returns the new QuicDataChannel state.
  DataState SetTransportChannel_w();
  // Closes the QUIC streams associated with this QuicDataChannel.
  void Close_w();
  // Sets |buffered_amount_|.
  void SetBufferedAmount_w(uint64_t buffered_amount);

  // Signaling thread methods.
  // Triggers QuicDataChannelObserver::OnMessage when a message from the remote
  // peer is ready to be read.
  void OnMessage_s(const DataBuffer& received_data);
  // Triggers QuicDataChannel::OnStateChange if the state change is valid.
  // Otherwise does nothing if |state| == |state_| or |state| != kClosed when
  // the data channel is closing.
  void SetState_s(DataState state);
  // Triggers QuicDataChannelObserver::OnBufferedAmountChange when the total
  // buffered data changes for a QUIC stream.
  void OnBufferedAmountChange_s(uint64_t buffered_amount);

  // QUIC transport channel which owns the QUIC session. It is used to create
  // a QUIC stream for sending outgoing messages.
  cricket::QuicTransportChannel* quic_transport_channel_ = nullptr;
  // Signaling thread for DataChannelInterface methods.
  rtc::Thread* const signaling_thread_;
  // Worker thread for |quic_transport_channel_| callbacks.
  rtc::Thread* const worker_thread_;
  // Network thread for sending data and |quic_transport_channel_| callbacks.
  rtc::Thread* const network_thread_;
  rtc::AsyncInvoker invoker_;
  // Map of QUIC stream ID => ReliableQuicStream* for write blocked QUIC
  // streams.
  std::unordered_map<net::QuicStreamId, cricket::ReliableQuicStream*>
      write_blocked_quic_streams_;
  // Map of QUIC stream ID => Message for each incoming QUIC stream.
  std::unordered_map<net::QuicStreamId, Message> incoming_quic_messages_;
  // Handles received data from the remote peer and data channel state changes.
  DataChannelObserver* observer_ = nullptr;
  // QuicDataChannel ID.
  int id_;
  // Connectivity state of the QuicDataChannel.
  DataState state_;
  // Total bytes that are buffered among the QUIC streams.
  uint64_t buffered_amount_;
  // Counter for number of sent messages that is used for message IDs.
  uint64_t next_message_id_;

  // Variables for application use.
  const std::string& label_;
  const std::string& protocol_;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_QUICDATACHANNEL_H_
