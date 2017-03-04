/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOICE_ENGINE_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOICE_ENGINE_IMPL_H

#include <memory>

#include "webrtc/system_wrappers/include/atomic32.h"
#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/voe_base_impl.h"
#include "webrtc/voice_engine/voe_audio_processing_impl.h"
#include "webrtc/voice_engine/voe_codec_impl.h"
#include "webrtc/voice_engine/voe_file_impl.h"
#include "webrtc/voice_engine/voe_hardware_impl.h"
#include "webrtc/voice_engine/voe_neteq_stats_impl.h"
#include "webrtc/voice_engine/voe_network_impl.h"
#include "webrtc/voice_engine/voe_rtp_rtcp_impl.h"
#include "webrtc/voice_engine/voe_volume_control_impl.h"

namespace webrtc {
namespace voe {
class ChannelProxy;
}  // namespace voe

class VoiceEngineImpl : public voe::SharedData,  // Must be the first base class
                        public VoiceEngine,
                        public VoEAudioProcessingImpl,
                        public VoECodecImpl,
                        public VoEFileImpl,
                        public VoEHardwareImpl,
                        public VoENetEqStatsImpl,
                        public VoENetworkImpl,
                        public VoERTP_RTCPImpl,
                        public VoEVolumeControlImpl,
                        public VoEBaseImpl {
 public:
  VoiceEngineImpl()
      : SharedData(),
        VoEAudioProcessingImpl(this),
        VoECodecImpl(this),
        VoEFileImpl(this),
        VoEHardwareImpl(this),
        VoENetEqStatsImpl(this),
        VoENetworkImpl(this),
        VoERTP_RTCPImpl(this),
        VoEVolumeControlImpl(this),
        VoEBaseImpl(this),
        _ref_count(0) {}
  ~VoiceEngineImpl() override { assert(_ref_count.Value() == 0); }

  int AddRef();

  // This implements the Release() method for all the inherited interfaces.
  int Release() override;

  // Backdoor to access a voe::Channel object without a channel ID. This is only
  // to be used while refactoring the VoE API!
  virtual std::unique_ptr<voe::ChannelProxy> GetChannelProxy(int channel_id);

 // This is *protected* so that FakeVoiceEngine can inherit from the class and
 // manipulate the reference count. See: fake_voice_engine.h.
 protected:
  Atomic32 _ref_count;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOICE_ENGINE_IMPL_H
