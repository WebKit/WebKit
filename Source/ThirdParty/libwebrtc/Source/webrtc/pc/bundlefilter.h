/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_BUNDLEFILTER_H_
#define PC_BUNDLEFILTER_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "media/base/streamparams.h"
#include "rtc_base/basictypes.h"

namespace cricket {

// In case of single RTP session and single transport channel, all session
// (or media) channels share a common transport channel. Hence they all get
// SignalReadPacket when packet received on transport channel. This requires
// cricket::BaseChannel to know all the valid sources, else media channel
// will decode invalid packets.
//
// This class determines whether a packet is destined for cricket::BaseChannel.
// This is only to be used for RTP packets as RTCP packets are not filtered.
// For RTP packets, this is decided based on the payload type.
class BundleFilter {
 public:
  BundleFilter();
  ~BundleFilter();

  // Determines if a RTP packet belongs to valid cricket::BaseChannel.
  bool DemuxPacket(const uint8_t* data, size_t len);

  // Adds the supported payload type.
  void AddPayloadType(int payload_type);

  // Public for unittests.
  bool FindPayloadType(int pl_type) const;
  void ClearAllPayloadTypes();

 private:
  std::set<int> payload_types_;
};

}  // namespace cricket

#endif  // PC_BUNDLEFILTER_H_
