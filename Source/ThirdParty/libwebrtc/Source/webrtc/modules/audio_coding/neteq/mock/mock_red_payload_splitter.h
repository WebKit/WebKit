/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_RED_PAYLOAD_SPLITTER_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_RED_PAYLOAD_SPLITTER_H_

#include "webrtc/modules/audio_coding/neteq/red_payload_splitter.h"

#include "webrtc/test/gmock.h"

namespace webrtc {

class MockRedPayloadSplitter : public RedPayloadSplitter {
 public:
  MOCK_METHOD1(SplitRed, bool(PacketList* packet_list));
  MOCK_METHOD2(CheckRedPayloads,
               int(PacketList* packet_list,
                   const DecoderDatabase& decoder_database));
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_RED_PAYLOAD_SPLITTER_H_
