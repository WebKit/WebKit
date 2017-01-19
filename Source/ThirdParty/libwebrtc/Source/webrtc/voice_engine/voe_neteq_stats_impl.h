/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_NETEQ_STATS_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_NETEQ_STATS_IMPL_H

#include "webrtc/voice_engine/include/voe_neteq_stats.h"

#include "webrtc/common_types.h"
#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoENetEqStatsImpl : public VoENetEqStats {
 public:
  int GetNetworkStatistics(int channel, NetworkStatistics& stats) override;

  int GetDecodingCallStatistics(int channel,
                                AudioDecodingCallStats* stats) const override;

 protected:
  VoENetEqStatsImpl(voe::SharedData* shared);
  ~VoENetEqStatsImpl() override;

 private:
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_NETEQ_STATS_IMPL_H
