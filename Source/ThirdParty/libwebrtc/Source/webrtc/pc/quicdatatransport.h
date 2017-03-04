/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_QUICDATATRANSPORT_H_
#define WEBRTC_PC_QUICDATATRANSPORT_H_

#include <string>
#include <unordered_map>

#include "webrtc/api/datachannelinterface.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/transportcontroller.h"
#include "webrtc/pc/quicdatachannel.h"

namespace cricket {
class QuicTransportChannel;
class ReliableQuicStream;
}  // namepsace cricket

namespace webrtc {

// QuicDataTransport creates QuicDataChannels for the PeerConnection. It also
// handles QUIC stream demuxing by distributing incoming QUIC streams from the
// QuicTransportChannel among the QuicDataChannels that it has created.
//
// QuicDataTransport reads the data channel ID from the incoming QUIC stream,
// then looks it up in a map of ID => QuicDataChannel. If the data channel
// exists, it sends the QUIC stream to the QuicDataChannel.
class QuicDataTransport : public sigslot::has_slots<> {
 public:
  QuicDataTransport(rtc::Thread* signaling_thread,
                    rtc::Thread* worker_thread,
                    rtc::Thread* network_thread,
                    cricket::TransportController* transport_controller);
  ~QuicDataTransport() override;

  // The QuicDataTransport acts like a BaseChannel with these functions.
  bool SetTransport(const std::string& transport_name);
  const std::string& transport_name() const { return transport_name_; }
  const std::string& content_name() const { return content_name_; }
  void set_content_name(const std::string& content_name) {
    content_name_ = content_name;
  }

  // Creates a QuicDataChannel that uses this QuicDataTransport.
  rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config);

  // Removes a QuicDataChannel with the given ID from the QuicDataTransport's
  // data channel map.
  void DestroyDataChannel(int id);

  // True if the QuicDataTransport has a data channel with the given ID.
  bool HasDataChannel(int id) const;

  // True if the QuicDataTransport has data channels.
  bool HasDataChannels() const;

  cricket::QuicTransportChannel* quic_transport_channel() {
    return quic_transport_channel_;
  }

 private:
  // Sets the QUIC transport channel for the QuicDataChannels and the
  // QuicDataTransport. Returns false if a different QUIC transport channel is
  // already set, the QUIC transport channel cannot be set for any of the
  // QuicDataChannels, or |channel| is NULL.
  bool SetTransportChannel(cricket::QuicTransportChannel* channel);

  // Called from the QuicTransportChannel when a ReliableQuicStream is created
  // to receive incoming data.
  void OnIncomingStream(cricket::ReliableQuicStream* stream);
  // Called from the ReliableQuicStream when the first QUIC stream frame is
  // received for incoming data. The QuicDataTransport reads the data channel ID
  // and message ID from the incoming data, then dispatches the
  // ReliableQuicStream to the QuicDataChannel with the same data channel ID.
  void OnDataReceived(net::QuicStreamId stream_id,
                      const char* data,
                      size_t len);

  cricket::QuicTransportChannel* CreateTransportChannel(
      const std::string& transport_name);
  void DestroyTransportChannel(cricket::TransportChannel* transport_channel);

  // Map of data channel ID => QUIC data channel values.
  std::unordered_map<int, rtc::scoped_refptr<QuicDataChannel>>
      data_channel_by_id_;
  // Map of QUIC stream ID => ReliableQuicStream* values.
  std::unordered_map<net::QuicStreamId, cricket::ReliableQuicStream*>
      quic_stream_by_id_;
  // QuicTransportChannel for sending/receiving data.
  cricket::QuicTransportChannel* quic_transport_channel_ = nullptr;
  // Threads for the QUIC data channel.
  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;

  cricket::TransportController* transport_controller_;
  std::string content_name_;
  std::string transport_name_;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_QUICDATATRANSPORT_H_
