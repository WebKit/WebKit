/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_DEBUG_DUMP_WRITER_H_
#define MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_DEBUG_DUMP_WRITER_H_

#include "modules/audio_coding/audio_network_adaptor/debug_dump_writer.h"
#include "test/gmock.h"

namespace webrtc {

class MockDebugDumpWriter : public DebugDumpWriter {
 public:
  virtual ~MockDebugDumpWriter() { Die(); }
  MOCK_METHOD0(Die, void());

  MOCK_METHOD2(DumpEncoderRuntimeConfig,
               void(const AudioEncoderRuntimeConfig& config,
                    int64_t timestamp));
  MOCK_METHOD2(DumpNetworkMetrics,
               void(const Controller::NetworkMetrics& metrics,
                    int64_t timestamp));
#if WEBRTC_ENABLE_PROTOBUF
  MOCK_METHOD2(DumpControllerManagerConfig,
               void(const audio_network_adaptor::config::ControllerManager&
                        controller_manager_config,
                    int64_t timestamp));
#endif
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_DEBUG_DUMP_WRITER_H_
