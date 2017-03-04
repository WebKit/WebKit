/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_SCTP_SCTPTRANSPORTINTERNAL_H_
#define WEBRTC_MEDIA_SCTP_SCTPTRANSPORTINTERNAL_H_

// TODO(deadbeef): Move SCTP code out of media/, and make it not depend on
// anything in media/.

#include <memory>  // for unique_ptr
#include <string>
#include <vector>

#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/thread.h"
// For SendDataParams/ReceiveDataParams.
// TODO(deadbeef): Use something else for SCTP. It's confusing that we use an
// SSRC field for SID.
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/p2p/base/packettransportinternal.h"

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

// Abstract SctpTransport interface for use internally (by
// PeerConnection/WebRtcSession/etc.). Exists to allow mock/fake SctpTransports
// to be created.
class SctpTransportInternal {
 public:
  virtual ~SctpTransportInternal() {}

  // Changes what underlying DTLS channel is uses. Used when switching which
  // bundled transport the SctpTransport uses.
  // Assumes |channel| is non-null.
  virtual void SetTransportChannel(rtc::PacketTransportInternal* channel) = 0;

  // When Start is called, connects as soon as possible; this can be called
  // before DTLS completes, in which case the connection will begin when DTLS
  // completes. This method can be called multiple times, though not if either
  // of the ports are changed.
  //
  // |local_sctp_port| and |remote_sctp_port| are passed along the wire and the
  // listener and connector must be using the same port. They are not related
  // to the ports at the IP level. If set to -1, we default to
  // kSctpDefaultPort.
  //
  // TODO(deadbeef): Add remote max message size as parameter to Start, once we
  // start supporting it.
  // TODO(deadbeef): Support calling Start with different local/remote ports
  // and create a new association? Not clear if this is something we need to
  // support though. See: https://github.com/w3c/webrtc-pc/issues/979
  virtual bool Start(int local_sctp_port, int remote_sctp_port) = 0;

  // NOTE: Initially there was a "Stop" method here, but it was never used, so
  // it was removed.

  // Informs SctpTransport that |sid| will start being used. Returns false if
  // it is impossible to use |sid|, or if it's already in use.
  // Until calling this, can't send data using |sid|.
  // TODO(deadbeef): Actually implement the "returns false if |sid| can't be
  // used" part. See:
  // https://bugs.chromium.org/p/chromium/issues/detail?id=619849
  virtual bool OpenStream(int sid) = 0;
  // The inverse of OpenStream. When this method returns, the reset process may
  // have not finished but it will have begun.
  // TODO(deadbeef): We need a way to tell when it's done. See:
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=4453
  virtual bool ResetStream(int sid) = 0;
  // Send data down this channel (will be wrapped as SCTP packets then given to
  // usrsctp that will then post the network interface).
  // Returns true iff successful data somewhere on the send-queue/network.
  // Uses |params.ssrc| as the SCTP sid.
  virtual bool SendData(const SendDataParams& params,
                        const rtc::CopyOnWriteBuffer& payload,
                        SendDataResult* result = nullptr) = 0;

  // Indicates when the SCTP socket is created and not blocked by congestion
  // control. This changes to false when SDR_BLOCK is returned from SendData,
  // and
  // changes to true when SignalReadyToSendData is fired. The underlying DTLS/
  // ICE channels may be unwritable while ReadyToSendData is true, because data
  // can still be queued in usrsctp.
  virtual bool ReadyToSendData() = 0;

  sigslot::signal0<> SignalReadyToSendData;
  // ReceiveDataParams includes SID, seq num, timestamp, etc. CopyOnWriteBuffer
  // contains message payload.
  sigslot::signal2<const ReceiveDataParams&, const rtc::CopyOnWriteBuffer&>
      SignalDataReceived;
  // Parameter is SID of closed stream.
  sigslot::signal1<int> SignalStreamClosedRemotely;

  // Helper for debugging.
  virtual void set_debug_name_for_testing(const char* debug_name) = 0;
};

// Factory class which can be used to allow fake SctpTransports to be injected
// for testing. Or, theoretically, SctpTransportInternal implementations that
// use something other than usrsctp.
class SctpTransportInternalFactory {
 public:
  virtual ~SctpTransportInternalFactory() {}

  // Create an SCTP transport using |channel| for the underlying transport.
  virtual std::unique_ptr<SctpTransportInternal> CreateSctpTransport(
      rtc::PacketTransportInternal* channel) = 0;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_SCTP_SCTPTRANSPORTINTERNAL_H_
