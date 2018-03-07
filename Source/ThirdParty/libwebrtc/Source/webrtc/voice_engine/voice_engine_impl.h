/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_VOICE_ENGINE_IMPL_H_
#define VOICE_ENGINE_VOICE_ENGINE_IMPL_H_

#include <memory>

#include "system_wrappers/include/atomic32.h"
#include "typedefs.h"  // NOLINT(build/include)
#include "voice_engine/voe_base_impl.h"

namespace webrtc {
namespace voe {
class ChannelProxy;
}  // namespace voe

class VoiceEngineImpl : public voe::SharedData,  // Must be the first base class
                        public VoiceEngine,
                        public VoEBaseImpl {
 public:
  VoiceEngineImpl()
      : SharedData(),
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

#endif  // VOICE_ENGINE_VOICE_ENGINE_IMPL_H_
