/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/voip_metric.h"

#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rtcp {
// VoIP Metrics Report Block (RFC 3611).
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |     BT=7      |   reserved    |       block length = 8        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                        SSRC of source                         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |   loss rate   | discard rate  | burst density |  gap density  |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |       burst duration          |         gap duration          |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |     round trip delay          |       end system delay        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 20 | signal level  |  noise level  |     RERL      |     Gmin      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 24 |   R factor    | ext. R factor |    MOS-LQ     |    MOS-CQ     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 28 |   RX config   |   reserved    |          JB nominal           |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 32 |          JB maximum           |          JB abs max           |
// 36 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
VoipMetric::VoipMetric() : ssrc_(0) {
  memset(&voip_metric_, 0, sizeof(voip_metric_));
}

void VoipMetric::Parse(const uint8_t* buffer) {
  RTC_DCHECK(buffer[0] == kBlockType);
  // reserved = buffer[1];
  RTC_DCHECK(ByteReader<uint16_t>::ReadBigEndian(&buffer[2]) == kBlockLength);
  ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[4]);
  voip_metric_.lossRate = buffer[8];
  voip_metric_.discardRate = buffer[9];
  voip_metric_.burstDensity = buffer[10];
  voip_metric_.gapDensity = buffer[11];
  voip_metric_.burstDuration = ByteReader<uint16_t>::ReadBigEndian(&buffer[12]);
  voip_metric_.gapDuration = ByteReader<uint16_t>::ReadBigEndian(&buffer[14]);
  voip_metric_.roundTripDelay =
      ByteReader<uint16_t>::ReadBigEndian(&buffer[16]);
  voip_metric_.endSystemDelay =
      ByteReader<uint16_t>::ReadBigEndian(&buffer[18]);
  voip_metric_.signalLevel = buffer[20];
  voip_metric_.noiseLevel = buffer[21];
  voip_metric_.RERL = buffer[22];
  voip_metric_.Gmin = buffer[23];
  voip_metric_.Rfactor = buffer[24];
  voip_metric_.extRfactor = buffer[25];
  voip_metric_.MOSLQ = buffer[26];
  voip_metric_.MOSCQ = buffer[27];
  voip_metric_.RXconfig = buffer[28];
  // reserved = buffer[29];
  voip_metric_.JBnominal = ByteReader<uint16_t>::ReadBigEndian(&buffer[30]);
  voip_metric_.JBmax = ByteReader<uint16_t>::ReadBigEndian(&buffer[32]);
  voip_metric_.JBabsMax = ByteReader<uint16_t>::ReadBigEndian(&buffer[34]);
}

void VoipMetric::Create(uint8_t* buffer) const {
  const uint8_t kReserved = 0;
  buffer[0] = kBlockType;
  buffer[1] = kReserved;
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[2], kBlockLength);
  ByteWriter<uint32_t>::WriteBigEndian(&buffer[4], ssrc_);
  buffer[8] = voip_metric_.lossRate;
  buffer[9] = voip_metric_.discardRate;
  buffer[10] = voip_metric_.burstDensity;
  buffer[11] = voip_metric_.gapDensity;
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[12], voip_metric_.burstDuration);
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[14], voip_metric_.gapDuration);
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[16],
                                       voip_metric_.roundTripDelay);
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[18],
                                       voip_metric_.endSystemDelay);
  buffer[20] = voip_metric_.signalLevel;
  buffer[21] = voip_metric_.noiseLevel;
  buffer[22] = voip_metric_.RERL;
  buffer[23] = voip_metric_.Gmin;
  buffer[24] = voip_metric_.Rfactor;
  buffer[25] = voip_metric_.extRfactor;
  buffer[26] = voip_metric_.MOSLQ;
  buffer[27] = voip_metric_.MOSCQ;
  buffer[28] = voip_metric_.RXconfig;
  buffer[29] = kReserved;
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[30], voip_metric_.JBnominal);
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[32], voip_metric_.JBmax);
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[34], voip_metric_.JBabsMax);
}

}  // namespace rtcp
}  // namespace webrtc
