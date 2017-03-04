/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_AUDIO_NETWORK_ADAPTOR_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_AUDIO_NETWORK_ADAPTOR_H_

#include "webrtc/modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockAudioNetworkAdaptor : public AudioNetworkAdaptor {
 public:
  virtual ~MockAudioNetworkAdaptor() { Die(); }
  MOCK_METHOD0(Die, void());

  MOCK_METHOD1(SetUplinkBandwidth, void(int uplink_bandwidth_bps));

  MOCK_METHOD1(SetUplinkPacketLossFraction,
               void(float uplink_packet_loss_fraction));

  MOCK_METHOD1(SetRtt, void(int rtt_ms));

  MOCK_METHOD1(SetTargetAudioBitrate, void(int target_audio_bitrate_bps));

  MOCK_METHOD1(SetOverhead, void(size_t overhead_bytes_per_packet));

  MOCK_METHOD0(GetEncoderRuntimeConfig, EncoderRuntimeConfig());

  MOCK_METHOD1(StartDebugDump, void(FILE* file_handle));

  MOCK_METHOD0(StopDebugDump, void());
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_AUDIO_NETWORK_ADAPTOR_H_
