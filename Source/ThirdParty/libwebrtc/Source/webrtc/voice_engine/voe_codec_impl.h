/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_CODEC_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_CODEC_IMPL_H

#include "webrtc/voice_engine/include/voe_codec.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoECodecImpl : public VoECodec {
 public:
  int NumOfCodecs() override;

  int GetCodec(int index, CodecInst& codec) override;

  int SetSendCodec(int channel, const CodecInst& codec) override;

  int GetSendCodec(int channel, CodecInst& codec) override;

  int SetBitRate(int channel, int bitrate_bps) override;

  int GetRecCodec(int channel, CodecInst& codec) override;

  int SetSendCNPayloadType(
      int channel,
      int type,
      PayloadFrequencies frequency = kFreq16000Hz) override;

  int SetRecPayloadType(int channel, const CodecInst& codec) override;

  int GetRecPayloadType(int channel, CodecInst& codec) override;

  int SetFECStatus(int channel, bool enable) override;

  int GetFECStatus(int channel, bool& enabled) override;

  int SetVADStatus(int channel,
                   bool enable,
                   VadModes mode = kVadConventional,
                   bool disableDTX = false) override;

  int GetVADStatus(int channel,
                   bool& enabled,
                   VadModes& mode,
                   bool& disabledDTX) override;

  int SetOpusMaxPlaybackRate(int channel, int frequency_hz) override;

  int SetOpusDtx(int channel, bool enable_dtx) override;

  int GetOpusDtxStatus(int channel, bool* enabled) override;

 protected:
  VoECodecImpl(voe::SharedData* shared);
  ~VoECodecImpl() override;

 private:
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_CODEC_IMPL_H
