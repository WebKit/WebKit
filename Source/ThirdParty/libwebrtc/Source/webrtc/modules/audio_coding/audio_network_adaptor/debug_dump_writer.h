/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_DEBUG_DUMP_WRITER_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_DEBUG_DUMP_WRITER_H_

#include <memory>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"

namespace webrtc {

class DebugDumpWriter {
 public:
  static std::unique_ptr<DebugDumpWriter> Create(FILE* file_handle);

  virtual ~DebugDumpWriter() = default;

  virtual void DumpEncoderRuntimeConfig(
      const AudioNetworkAdaptor::EncoderRuntimeConfig& config,
      int64_t timestamp) = 0;

  virtual void DumpNetworkMetrics(const Controller::NetworkMetrics& metrics,
                                  int64_t timestamp) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_DEBUG_DUMP_WRITER_H_
