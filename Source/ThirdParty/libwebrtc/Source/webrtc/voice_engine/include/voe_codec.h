/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Support of non-default codecs (e.g. iLBC, iSAC, etc.).
//  - Voice Activity Detection (VAD) on a per channel basis.
//  - Possibility to specify how to map received payload types to codecs.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoECodec* codec = VoECodec::GetInterface(voe);
//  base->Init();
//  int num_of_codecs = codec->NumOfCodecs()
//  ...
//  base->Terminate();
//  base->Release();
//  codec->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_CODEC_H
#define WEBRTC_VOICE_ENGINE_VOE_CODEC_H

#include "webrtc/common_types.h"

namespace webrtc {

class VoiceEngine;

class WEBRTC_DLLEXPORT VoECodec {
 public:
  // Factory for the VoECodec sub-API. Increases an internal
  // reference counter if successful. Returns NULL if the API is not
  // supported or if construction fails.
  static VoECodec* GetInterface(VoiceEngine* voiceEngine);

  // Releases the VoECodec sub-API and decreases an internal
  // reference counter. Returns the new reference count. This value should
  // be zero for all sub-API:s before the VoiceEngine object can be safely
  // deleted.
  virtual int Release() = 0;

  // Gets the number of supported codecs.
  virtual int NumOfCodecs() = 0;

  // Get the |codec| information for a specified list |index|.
  virtual int GetCodec(int index, CodecInst& codec) = 0;

  // Sets the |codec| for the |channel| to be used for sending.
  virtual int SetSendCodec(int channel, const CodecInst& codec) = 0;

  // Gets the |codec| parameters for the sending codec on a specified
  // |channel|.
  virtual int GetSendCodec(int channel, CodecInst& codec) = 0;

  // Sets the bitrate on a specified |channel| to the specified value
  // (in bits/sec). If the value is not supported by the codec, the codec will
  // choose an appropriate value.
  // Returns -1 on failure and 0 on success.
  virtual int SetBitRate(int channel, int bitrate_bps) = 0;

  // Gets the currently received |codec| for a specific |channel|.
  virtual int GetRecCodec(int channel, CodecInst& codec) = 0;

  // Sets the dynamic payload type number for a particular |codec| or
  // disables (ignores) a codec for receiving. For instance, when receiving
  // an invite from a SIP-based client, this function can be used to change
  // the dynamic payload type number to match that in the INVITE SDP-
  // message. The utilized parameters in the |codec| structure are:
  // plname, plfreq, pltype and channels.
  virtual int SetRecPayloadType(int channel, const CodecInst& codec) = 0;

  // Gets the actual payload type that is set for receiving a |codec| on a
  // |channel|. The value it retrieves will either be the default payload
  // type, or a value earlier set with SetRecPayloadType().
  virtual int GetRecPayloadType(int channel, CodecInst& codec) = 0;

  // Sets the payload |type| for the sending of SID-frames with background
  // noise estimation during silence periods detected by the VAD.
  virtual int SetSendCNPayloadType(
      int channel,
      int type,
      PayloadFrequencies frequency = kFreq16000Hz) = 0;

  // Sets the codec internal FEC (forward error correction) status for a
  // specified |channel|. Returns 0 if success, and -1 if failed.
  // TODO(minyue): Make SetFECStatus() pure virtual when fakewebrtcvoiceengine
  // in talk is ready.
  virtual int SetFECStatus(int channel, bool enable) { return -1; }

  // Gets the codec internal FEC status for a specified |channel|. Returns 0
  // with the status stored in |enabled| if success, and -1 if encountered
  // error.
  // TODO(minyue): Make GetFECStatus() pure virtual when fakewebrtcvoiceengine
  // in talk is ready.
  virtual int GetFECStatus(int channel, bool& enabled) { return -1; }

  // Sets the VAD/DTX (silence suppression) status and |mode| for a
  // specified |channel|. Disabling VAD (through |enable|) will also disable
  // DTX; it is not necessary to explictly set |disableDTX| in this case.
  virtual int SetVADStatus(int channel,
                           bool enable,
                           VadModes mode = kVadConventional,
                           bool disableDTX = false) = 0;

  // Gets the VAD/DTX status and |mode| for a specified |channel|.
  virtual int GetVADStatus(int channel,
                           bool& enabled,
                           VadModes& mode,
                           bool& disabledDTX) = 0;

  // If send codec is Opus on a specified |channel|, sets the maximum playback
  // rate the receiver will render: |frequency_hz| (in Hz).
  // TODO(minyue): Make SetOpusMaxPlaybackRate() pure virtual when
  // fakewebrtcvoiceengine in talk is ready.
  virtual int SetOpusMaxPlaybackRate(int channel, int frequency_hz) {
    return -1;
  }

  // If send codec is Opus on a specified |channel|, set its DTX. Returns 0 if
  // success, and -1 if failed.
  virtual int SetOpusDtx(int channel, bool enable_dtx) = 0;

  // If send codec is Opus on a specified |channel|, return its DTX status.
  // Returns 0 on success, and -1 if failed.
  // TODO(ivoc): Make GetOpusDtxStatus() pure virtual when all deriving classes
  // are updated.
  virtual int GetOpusDtxStatus(int channel, bool* enabled) { return -1; }

 protected:
  VoECodec() {}
  virtual ~VoECodec() {}
};

}  // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_VOE_CODEC_H
