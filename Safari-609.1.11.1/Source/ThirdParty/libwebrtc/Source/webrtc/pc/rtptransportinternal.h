/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTPTRANSPORTINTERNAL_H_
#define PC_RTPTRANSPORTINTERNAL_H_

#include <string>

#include "api/ortc/srtptransportinterface.h"
#include "call/rtp_demuxer.h"
#include "p2p/base/icetransportinternal.h"
#include "pc/sessiondescription.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/sslstreamadapter.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace rtc {
class CopyOnWriteBuffer;
struct PacketOptions;
}  // namespace rtc

namespace webrtc {

// This represents the internal interface beneath SrtpTransportInterface;
// it is not accessible to API consumers but is accessible to internal classes
// in order to send and receive RTP and RTCP packets belonging to a single RTP
// session. Additional convenience and configuration methods are also provided.
class RtpTransportInternal : public SrtpTransportInterface,
                             public sigslot::has_slots<> {
 public:
  virtual void SetRtcpMuxEnabled(bool enable) = 0;

  // TODO(zstein): Remove PacketTransport setters. Clients should pass these
  // in to constructors instead and construct a new RtpTransportInternal instead
  // of updating them.
  virtual bool rtcp_mux_enabled() const = 0;

  virtual rtc::PacketTransportInternal* rtp_packet_transport() const = 0;
  virtual void SetRtpPacketTransport(rtc::PacketTransportInternal* rtp) = 0;

  virtual rtc::PacketTransportInternal* rtcp_packet_transport() const = 0;
  virtual void SetRtcpPacketTransport(rtc::PacketTransportInternal* rtcp) = 0;

  virtual bool IsReadyToSend() const = 0;

  // Called whenever a transport's ready-to-send state changes. The argument
  // is true if all used transports are ready to send. This is more specific
  // than just "writable"; it means the last send didn't return ENOTCONN.
  sigslot::signal1<bool> SignalReadyToSend;

  // Called whenever an RTCP packet is received. There is no equivalent signal
  // for RTP packets because they would be forwarded to the BaseChannel through
  // the RtpDemuxer callback.
  sigslot::signal2<rtc::CopyOnWriteBuffer*, int64_t> SignalRtcpPacketReceived;

  // Called whenever the network route of the P2P layer transport changes.
  // The argument is an optional network route.
  sigslot::signal1<absl::optional<rtc::NetworkRoute>> SignalNetworkRouteChanged;

  // Called whenever a transport's writable state might change. The argument is
  // true if the transport is writable, otherwise it is false.
  sigslot::signal1<bool> SignalWritableState;

  sigslot::signal1<const rtc::SentPacket&> SignalSentPacket;

  virtual bool IsWritable(bool rtcp) const = 0;

  // TODO(zhihuang): Pass the |packet| by copy so that the original data
  // wouldn't be modified.
  virtual bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                             const rtc::PacketOptions& options,
                             int flags) = 0;

  virtual bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                              const rtc::PacketOptions& options,
                              int flags) = 0;

  // This method updates the RTP header extension map so that the RTP transport
  // can parse the received packets and identify the MID. This is called by the
  // BaseChannel when setting the content description.
  //
  // TODO(zhihuang): Merging and replacing following methods handling header
  // extensions with SetParameters:
  //   UpdateRtpHeaderExtensionMap,
  //   UpdateSendEncryptedHeaderExtensionIds,
  //   UpdateRecvEncryptedHeaderExtensionIds,
  //   CacheRtpAbsSendTimeHeaderExtension,
  virtual void UpdateRtpHeaderExtensionMap(
      const cricket::RtpHeaderExtensions& header_extensions) = 0;

  virtual bool IsSrtpActive() const = 0;

  virtual bool RegisterRtpDemuxerSink(const RtpDemuxerCriteria& criteria,
                                      RtpPacketSinkInterface* sink) = 0;

  virtual bool UnregisterRtpDemuxerSink(RtpPacketSinkInterface* sink) = 0;
};

}  // namespace webrtc

#endif  // PC_RTPTRANSPORTINTERNAL_H_
