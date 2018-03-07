/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_AUDIO_NETWORK_ADAPTOR_H_
#define MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_AUDIO_NETWORK_ADAPTOR_H_

#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "test/gmock.h"

namespace webrtc {

class MockAudioNetworkAdaptor : public AudioNetworkAdaptor {
 public:
  virtual ~MockAudioNetworkAdaptor() { Die(); }
  MOCK_METHOD0(Die, void());

  MOCK_METHOD1(SetUplinkBandwidth, void(int uplink_bandwidth_bps));

  MOCK_METHOD1(SetUplinkPacketLossFraction,
               void(float uplink_packet_loss_fraction));

  MOCK_METHOD1(SetUplinkRecoverablePacketLossFraction,
               void(float uplink_recoverable_packet_loss_fraction));

  MOCK_METHOD1(SetRtt, void(int rtt_ms));

  MOCK_METHOD1(SetTargetAudioBitrate, void(int target_audio_bitrate_bps));

  MOCK_METHOD1(SetOverhead, void(size_t overhead_bytes_per_packet));

  MOCK_METHOD0(GetEncoderRuntimeConfig, AudioEncoderRuntimeConfig());

  MOCK_METHOD1(StartDebugDump, void(FILE* file_handle));

  MOCK_METHOD0(StopDebugDump, void());

  MOCK_CONST_METHOD0(GetStats, ANAStats());
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_AUDIO_NETWORK_ADAPTOR_H_
