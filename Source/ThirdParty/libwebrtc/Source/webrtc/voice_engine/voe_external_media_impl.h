/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_EXTERNAL_MEDIA_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_EXTERNAL_MEDIA_IMPL_H

#include "webrtc/voice_engine/include/voe_external_media.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEExternalMediaImpl : public VoEExternalMedia {
 public:
  int RegisterExternalMediaProcessing(int channel,
                                      ProcessingTypes type,
                                      VoEMediaProcess& processObject) override;

  int DeRegisterExternalMediaProcessing(int channel,
                                        ProcessingTypes type) override;

  int GetAudioFrame(int channel,
                    int desired_sample_rate_hz,
                    AudioFrame* frame) override;

  int SetExternalMixing(int channel, bool enable) override;

 protected:
  VoEExternalMediaImpl(voe::SharedData* shared);
  ~VoEExternalMediaImpl() override;

 private:
  voe::SharedData* shared_;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_EXTERNAL_MEDIA_IMPL_H
