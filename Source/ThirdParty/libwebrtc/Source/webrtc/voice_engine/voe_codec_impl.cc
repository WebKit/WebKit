/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_codec_impl.h"

#include "webrtc/base/format_macros.h"
#include "webrtc/modules/audio_coding/include/audio_coding_module.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

VoECodec* VoECodec::GetInterface(VoiceEngine* voiceEngine) {
  if (NULL == voiceEngine) {
    return NULL;
  }
  VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
  s->AddRef();
  return s;
}

VoECodecImpl::VoECodecImpl(voe::SharedData* shared) : _shared(shared) {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoECodecImpl() - ctor");
}

VoECodecImpl::~VoECodecImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "~VoECodecImpl() - dtor");
}

int VoECodecImpl::NumOfCodecs() {
  // Number of supported codecs in the ACM
  uint8_t nSupportedCodecs = AudioCodingModule::NumberOfCodecs();
  return (nSupportedCodecs);
}

int VoECodecImpl::GetCodec(int index, CodecInst& codec) {
  if (AudioCodingModule::Codec(index, &codec) == -1) {
    _shared->SetLastError(VE_INVALID_LISTNR, kTraceError,
                          "GetCodec() invalid index");
    return -1;
  }
  return 0;
}

int VoECodecImpl::SetSendCodec(int channel, const CodecInst& codec) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetSendCodec(channel=%d, codec)", channel);
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "codec: plname=%s, pacsize=%d, plfreq=%d, pltype=%d, "
               "channels=%" PRIuS ", rate=%d",
               codec.plname, codec.pacsize, codec.plfreq, codec.pltype,
               codec.channels, codec.rate);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  // External sanity checks performed outside the ACM
  if ((STR_CASE_CMP(codec.plname, "L16") == 0) && (codec.pacsize >= 960)) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetSendCodec() invalid L16 packet size");
    return -1;
  }
  if (!STR_CASE_CMP(codec.plname, "CN") ||
      !STR_CASE_CMP(codec.plname, "TELEPHONE-EVENT") ||
      !STR_CASE_CMP(codec.plname, "RED")) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetSendCodec() invalid codec name");
    return -1;
  }
  if ((codec.channels != 1) && (codec.channels != 2)) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetSendCodec() invalid number of channels");
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetSendCodec() failed to locate channel");
    return -1;
  }
  if (!AudioCodingModule::IsCodecValid(codec)) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetSendCodec() invalid codec");
    return -1;
  }
  if (channelPtr->SetSendCodec(codec) != 0) {
    _shared->SetLastError(VE_CANNOT_SET_SEND_CODEC, kTraceError,
                          "SetSendCodec() failed to set send codec");
    return -1;
  }

  return 0;
}

int VoECodecImpl::GetSendCodec(int channel, CodecInst& codec) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetSendCodec() failed to locate channel");
    return -1;
  }
  if (channelPtr->GetSendCodec(codec) != 0) {
    _shared->SetLastError(VE_CANNOT_GET_SEND_CODEC, kTraceError,
                          "GetSendCodec() failed to get send codec");
    return -1;
  }
  return 0;
}

int VoECodecImpl::SetBitRate(int channel, int bitrate_bps) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetBitRate(bitrate_bps=%d)", bitrate_bps);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  constexpr int64_t kDefaultProbingIntervalMs = 3000;
  _shared->channel_manager().GetChannel(channel).channel()->SetBitRate(
      bitrate_bps, kDefaultProbingIntervalMs);
  return 0;
}

int VoECodecImpl::GetRecCodec(int channel, CodecInst& codec) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetRecCodec() failed to locate channel");
    return -1;
  }
  return channelPtr->GetRecCodec(codec);
}

int VoECodecImpl::SetRecPayloadType(int channel, const CodecInst& codec) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetRecPayloadType(channel=%d, codec)", channel);
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "codec: plname=%s, plfreq=%d, pltype=%d, channels=%" PRIuS ", "
               "pacsize=%d, rate=%d",
               codec.plname, codec.plfreq, codec.pltype, codec.channels,
               codec.pacsize, codec.rate);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetRecPayloadType() failed to locate channel");
    return -1;
  }
  return channelPtr->SetRecPayloadType(codec);
}

int VoECodecImpl::GetRecPayloadType(int channel, CodecInst& codec) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetRecPayloadType() failed to locate channel");
    return -1;
  }
  return channelPtr->GetRecPayloadType(codec);
}

int VoECodecImpl::SetSendCNPayloadType(int channel,
                                       int type,
                                       PayloadFrequencies frequency) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetSendCNPayloadType(channel=%d, type=%d, frequency=%d)",
               channel, type, frequency);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (type < 96 || type > 127) {
    // Only allow dynamic range: 96 to 127
    _shared->SetLastError(VE_INVALID_PLTYPE, kTraceError,
                          "SetSendCNPayloadType() invalid payload type");
    return -1;
  }
  if ((frequency != kFreq16000Hz) && (frequency != kFreq32000Hz)) {
    // It is not possible to modify the payload type for CN/8000.
    // We only allow modification of the CN payload type for CN/16000
    // and CN/32000.
    _shared->SetLastError(VE_INVALID_PLFREQ, kTraceError,
                          "SetSendCNPayloadType() invalid payload frequency");
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "SetSendCNPayloadType() failed to locate channel");
    return -1;
  }
  return channelPtr->SetSendCNPayloadType(type, frequency);
}

int VoECodecImpl::SetFECStatus(int channel, bool enable) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetCodecFECStatus(channel=%d, enable=%d)", channel, enable);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "SetCodecFECStatus() failed to locate channel");
    return -1;
  }
  return channelPtr->SetCodecFECStatus(enable);
}

int VoECodecImpl::GetFECStatus(int channel, bool& enabled) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetFECStatus() failed to locate channel");
    return -1;
  }
  enabled = channelPtr->GetCodecFECStatus();
  return 0;
}

int VoECodecImpl::SetVADStatus(int channel,
                               bool enable,
                               VadModes mode,
                               bool disableDTX) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetVADStatus(channel=%i, enable=%i, mode=%i, disableDTX=%i)",
               channel, enable, mode, disableDTX);

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "SetVADStatus failed to locate channel");
    return -1;
  }

  ACMVADMode vadMode(VADNormal);
  switch (mode) {
    case kVadConventional:
      vadMode = VADNormal;
      break;
    case kVadAggressiveLow:
      vadMode = VADLowBitrate;
      break;
    case kVadAggressiveMid:
      vadMode = VADAggr;
      break;
    case kVadAggressiveHigh:
      vadMode = VADVeryAggr;
      break;
  }
  return channelPtr->SetVADStatus(enable, vadMode, disableDTX);
}

int VoECodecImpl::GetVADStatus(int channel,
                               bool& enabled,
                               VadModes& mode,
                               bool& disabledDTX) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetVADStatus failed to locate channel");
    return -1;
  }

  ACMVADMode vadMode;
  int ret = channelPtr->GetVADStatus(enabled, vadMode, disabledDTX);

  if (ret != 0) {
    _shared->SetLastError(VE_INVALID_OPERATION, kTraceError,
                          "GetVADStatus failed to get VAD mode");
    return -1;
  }
  switch (vadMode) {
    case VADNormal:
      mode = kVadConventional;
      break;
    case VADLowBitrate:
      mode = kVadAggressiveLow;
      break;
    case VADAggr:
      mode = kVadAggressiveMid;
      break;
    case VADVeryAggr:
      mode = kVadAggressiveHigh;
      break;
  }

  return 0;
}

int VoECodecImpl::SetOpusMaxPlaybackRate(int channel, int frequency_hz) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetOpusMaxPlaybackRate(channel=%d, frequency_hz=%d)", channel,
               frequency_hz);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "SetOpusMaxPlaybackRate failed to locate channel");
    return -1;
  }
  return channelPtr->SetOpusMaxPlaybackRate(frequency_hz);
}

int VoECodecImpl::SetOpusDtx(int channel, bool enable_dtx) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetOpusDtx(channel=%d, enable_dtx=%d)", channel, enable_dtx);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "SetOpusDtx failed to locate channel");
    return -1;
  }
  return channelPtr->SetOpusDtx(enable_dtx);
}

int VoECodecImpl::GetOpusDtxStatus(int channel, bool* enabled) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetOpusDtx(channel=%d)", channel);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetOpusDtx failed to locate channel");
    return -1;
  }
  return channelPtr->GetOpusDtx(enabled);
}

}  // namespace webrtc
