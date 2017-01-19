/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/base/buffer.h"

namespace webrtc {
namespace rtcp {
// Class for building RTCP packets.
//
//  Example:
//  ReportBlock report_block;
//  report_block.SetMediaSsrc(234);
//  report_block.SetFractionLost(10);
//
//  ReceiverReport rr;
//  rr.SetSenderSsrc(123);
//  rr.AddReportBlock(report_block);
//
//  Fir fir;
//  fir.SetSenderSsrc(123);
//  fir.AddRequestTo(234, 56);
//
//  size_t length = 0;                     // Builds an intra frame request
//  uint8_t packet[kPacketSize];           // with sequence number 56.
//  fir.Build(packet, &length, kPacketSize);
//
//  rtc::Buffer packet = fir.Build();      // Returns a RawPacket holding
//                                         // the built rtcp packet.
//
//  CompoundPacket compound;               // Builds a compound RTCP packet with
//  compound.Append(&rr);                  // a receiver report, report block
//  compound.Append(&fir);                 // and fir message.
//  rtc::Buffer packet = compound.Build();

class RtcpPacket {
 public:
  // Callback used to signal that an RTCP packet is ready. Note that this may
  // not contain all data in this RtcpPacket; if a packet cannot fit in
  // max_length bytes, it will be fragmented and multiple calls to this
  // callback will be made.
  class PacketReadyCallback {
   public:
    virtual void OnPacketReady(uint8_t* data, size_t length) = 0;

   protected:
    PacketReadyCallback() {}
    virtual ~PacketReadyCallback() {}
  };

  virtual ~RtcpPacket() {}

  // Convenience method mostly used for test. Creates packet without
  // fragmentation using BlockLength() to allocate big enough buffer.
  rtc::Buffer Build() const;

  // Returns true if call to Create succeeded. Provided buffer reference
  // will be used for all calls to callback.
  bool BuildExternalBuffer(uint8_t* buffer,
                           size_t max_length,
                           PacketReadyCallback* callback) const;

  // Size of this packet in bytes (including headers).
  virtual size_t BlockLength() const = 0;

  // Creates packet in the given buffer at the given position.
  // Calls PacketReadyCallback::OnPacketReady if remaining buffer is too small
  // and assume buffer can be reused after OnPacketReady returns.
  virtual bool Create(uint8_t* packet,
                      size_t* index,
                      size_t max_length,
                      PacketReadyCallback* callback) const = 0;

 protected:
  // Size of the rtcp common header.
  static constexpr size_t kHeaderLength = 4;
  RtcpPacket() {}

  static void CreateHeader(uint8_t count_or_format,
                           uint8_t packet_type,
                           size_t block_length,  // Payload size in 32bit words.
                           uint8_t* buffer,
                           size_t* pos);

  bool OnBufferFull(uint8_t* packet,
                    size_t* index,
                    PacketReadyCallback* callback) const;
  // Size of the rtcp packet as written in header.
  size_t HeaderLength() const;
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_H_
