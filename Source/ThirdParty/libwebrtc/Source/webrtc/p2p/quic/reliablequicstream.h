/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_QUIC_RELIABLEQUICSTREAM_H_
#define WEBRTC_P2P_QUIC_RELIABLEQUICSTREAM_H_

#include "net/quic/reliable_quic_stream.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/stream.h"

namespace cricket {

// Streams created by QuicSession.
class ReliableQuicStream : public net::ReliableQuicStream,
                           public sigslot::has_slots<> {
 public:
  ReliableQuicStream(net::QuicStreamId id, net::QuicSession* session);

  ~ReliableQuicStream() override;

  // ReliableQuicStream overrides.
  void OnDataAvailable() override;
  void OnClose() override;
  void OnCanWrite() override;

  // Process decrypted data into encrypted QUIC packets, which get sent to the
  // QuicPacketWriter. rtc::SR_BLOCK is returned if the operation blocks instead
  // of writing, in which case the data is queued until OnCanWrite() is called.
  // If |fin| == true, then this stream closes after sending data.
  rtc::StreamResult Write(const char* data, size_t len, bool fin = false);
  // Removes this stream from the QuicSession's stream map.
  void Close();

  // Called when decrypted data is ready to be read.
  sigslot::signal3<net::QuicStreamId, const char*, size_t> SignalDataReceived;
  // Called when the stream is closed.
  sigslot::signal2<net::QuicStreamId, int> SignalClosed;
  // Emits the number of queued bytes that were written by OnCanWrite(), after
  // the stream was previously write blocked.
  sigslot::signal2<net::QuicStreamId, uint64_t> SignalQueuedBytesWritten;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(ReliableQuicStream);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_QUIC_RELIABLEQUICSTREAM_H_
