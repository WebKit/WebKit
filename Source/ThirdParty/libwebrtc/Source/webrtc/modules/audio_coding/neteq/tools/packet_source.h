/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_PACKET_SOURCE_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_PACKET_SOURCE_H_

#include <bitset>
#include <memory>

#include "modules/audio_coding/neteq/tools/packet.h"
#include "rtc_base/constructormagic.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {
namespace test {

// Interface class for an object delivering RTP packets to test applications.
class PacketSource {
 public:
  PacketSource();
  virtual ~PacketSource();

  // Returns next packet. Returns nullptr if the source is depleted, or if an
  // error occurred.
  virtual std::unique_ptr<Packet> NextPacket() = 0;

  virtual void FilterOutPayloadType(uint8_t payload_type);

  virtual void SelectSsrc(uint32_t ssrc);

 protected:
  std::bitset<128> filter_;  // Payload type is 7 bits in the RFC.
  // If SSRC filtering discards all packet that do not match the SSRC.
  bool use_ssrc_filter_;  // True when SSRC filtering is active.
  uint32_t ssrc_;  // The selected SSRC. All other SSRCs will be discarded.

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(PacketSource);
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_PACKET_SOURCE_H_
