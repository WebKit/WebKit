/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/rent_a_codec.h"

#include <memory>
#include <utility>

#include "modules/audio_coding/codecs/cng/audio_encoder_cng.h"
#include "modules/audio_coding/codecs/g711/audio_encoder_pcm.h"
#include "rtc_base/logging.h"
#include "modules/audio_coding/codecs/g722/audio_encoder_g722.h"
#ifdef WEBRTC_CODEC_ILBC
#include "modules/audio_coding/codecs/ilbc/audio_encoder_ilbc.h"  // nogncheck
#endif
#ifdef WEBRTC_CODEC_ISACFX
#include "modules/audio_coding/codecs/isac/fix/include/audio_decoder_isacfix.h"  // nogncheck
#include "modules/audio_coding/codecs/isac/fix/include/audio_encoder_isacfix.h"  // nogncheck
#endif
#ifdef WEBRTC_CODEC_ISAC
#include "modules/audio_coding/codecs/isac/main/include/audio_decoder_isac.h"  // nogncheck
#include "modules/audio_coding/codecs/isac/main/include/audio_encoder_isac.h"  // nogncheck
#endif
#ifdef WEBRTC_CODEC_OPUS
#include "modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#endif
#include "modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"
#ifdef WEBRTC_CODEC_RED
#include "modules/audio_coding/codecs/red/audio_encoder_copy_red.h"  // nogncheck
#endif
#include "modules/audio_coding/acm2/acm_codec_database.h"

#if defined(WEBRTC_CODEC_ISACFX) || defined(WEBRTC_CODEC_ISAC)
#include "modules/audio_coding/codecs/isac/locked_bandwidth_info.h"
#endif

namespace webrtc {
namespace acm2 {

absl::optional<RentACodec::CodecId> RentACodec::CodecIdByParams(
    const char* payload_name,
    int sampling_freq_hz,
    size_t channels) {
  return CodecIdFromIndex(
      ACMCodecDB::CodecId(payload_name, sampling_freq_hz, channels));
}

absl::optional<CodecInst> RentACodec::CodecInstById(CodecId codec_id) {
  absl::optional<int> mi = CodecIndexFromId(codec_id);
  return mi ? absl::optional<CodecInst>(Database()[*mi]) : absl::nullopt;
}

absl::optional<RentACodec::CodecId> RentACodec::CodecIdByInst(
    const CodecInst& codec_inst) {
  return CodecIdFromIndex(ACMCodecDB::CodecNumber(codec_inst));
}

absl::optional<CodecInst> RentACodec::CodecInstByParams(
    const char* payload_name,
    int sampling_freq_hz,
    size_t channels) {
  absl::optional<CodecId> codec_id =
      CodecIdByParams(payload_name, sampling_freq_hz, channels);
  if (!codec_id)
    return absl::nullopt;
  absl::optional<CodecInst> ci = CodecInstById(*codec_id);
  RTC_DCHECK(ci);

  // Keep the number of channels from the function call. For most codecs it
  // will be the same value as in default codec settings, but not for all.
  ci->channels = channels;

  return ci;
}

absl::optional<bool> RentACodec::IsSupportedNumChannels(CodecId codec_id,
                                                        size_t num_channels) {
  auto i = CodecIndexFromId(codec_id);
  return i ? absl::optional<bool>(
                 ACMCodecDB::codec_settings_[*i].channel_support >=
                 num_channels)
           : absl::nullopt;
}

rtc::ArrayView<const CodecInst> RentACodec::Database() {
  return rtc::ArrayView<const CodecInst>(ACMCodecDB::database_,
                                         NumberOfCodecs());
}

absl::optional<NetEqDecoder> RentACodec::NetEqDecoderFromCodecId(
    CodecId codec_id,
    size_t num_channels) {
  absl::optional<int> i = CodecIndexFromId(codec_id);
  if (!i)
    return absl::nullopt;
  const NetEqDecoder ned = ACMCodecDB::neteq_decoders_[*i];
  return (ned == NetEqDecoder::kDecoderOpus && num_channels == 2)
             ? NetEqDecoder::kDecoderOpus_2ch
             : ned;
}

RentACodec::RegistrationResult RentACodec::RegisterCngPayloadType(
    std::map<int, int>* pt_map,
    const CodecInst& codec_inst) {
  if (STR_CASE_CMP(codec_inst.plname, "CN") != 0)
    return RegistrationResult::kSkip;
  switch (codec_inst.plfreq) {
    case 8000:
    case 16000:
    case 32000:
    case 48000:
      (*pt_map)[codec_inst.plfreq] = codec_inst.pltype;
      return RegistrationResult::kOk;
    default:
      return RegistrationResult::kBadFreq;
  }
}

RentACodec::RegistrationResult RentACodec::RegisterRedPayloadType(
    std::map<int, int>* pt_map,
    const CodecInst& codec_inst) {
  if (STR_CASE_CMP(codec_inst.plname, "RED") != 0)
    return RegistrationResult::kSkip;
  switch (codec_inst.plfreq) {
    case 8000:
      (*pt_map)[codec_inst.plfreq] = codec_inst.pltype;
      return RegistrationResult::kOk;
    default:
      return RegistrationResult::kBadFreq;
  }
}

namespace {

// Returns a new speech encoder, or null on error.
// TODO(kwiberg): Don't handle errors here (bug 5033)
std::unique_ptr<AudioEncoder> CreateEncoder(
    const CodecInst& speech_inst,
    const rtc::scoped_refptr<LockedIsacBandwidthInfo>& bwinfo) {
#if defined(WEBRTC_CODEC_ISACFX)
  if (STR_CASE_CMP(speech_inst.plname, "isac") == 0)
    return std::unique_ptr<AudioEncoder>(
        new AudioEncoderIsacFixImpl(speech_inst, bwinfo));
#endif
#if defined(WEBRTC_CODEC_ISAC)
  if (STR_CASE_CMP(speech_inst.plname, "isac") == 0)
    return std::unique_ptr<AudioEncoder>(
        new AudioEncoderIsacFloatImpl(speech_inst, bwinfo));
#endif
#ifdef WEBRTC_CODEC_OPUS
  if (STR_CASE_CMP(speech_inst.plname, "opus") == 0)
    return std::unique_ptr<AudioEncoder>(new AudioEncoderOpusImpl(speech_inst));
#endif
  if (STR_CASE_CMP(speech_inst.plname, "pcmu") == 0)
    return std::unique_ptr<AudioEncoder>(new AudioEncoderPcmU(speech_inst));
  if (STR_CASE_CMP(speech_inst.plname, "pcma") == 0)
    return std::unique_ptr<AudioEncoder>(new AudioEncoderPcmA(speech_inst));
  if (STR_CASE_CMP(speech_inst.plname, "l16") == 0)
    return std::unique_ptr<AudioEncoder>(new AudioEncoderPcm16B(speech_inst));
#ifdef WEBRTC_CODEC_ILBC
  if (STR_CASE_CMP(speech_inst.plname, "ilbc") == 0)
    return std::unique_ptr<AudioEncoder>(new AudioEncoderIlbcImpl(speech_inst));
#endif
  if (STR_CASE_CMP(speech_inst.plname, "g722") == 0)
    return std::unique_ptr<AudioEncoder>(new AudioEncoderG722Impl(speech_inst));
  RTC_LOG_F(LS_ERROR) << "Could not create encoder of type "
                      << speech_inst.plname;
  return std::unique_ptr<AudioEncoder>();
}

std::unique_ptr<AudioEncoder> CreateRedEncoder(
    std::unique_ptr<AudioEncoder> encoder,
    int red_payload_type) {
#ifdef WEBRTC_CODEC_RED
  AudioEncoderCopyRed::Config config;
  config.payload_type = red_payload_type;
  config.speech_encoder = std::move(encoder);
  return std::unique_ptr<AudioEncoder>(
      new AudioEncoderCopyRed(std::move(config)));
#else
  return std::unique_ptr<AudioEncoder>();
#endif
}

std::unique_ptr<AudioEncoder> CreateCngEncoder(
    std::unique_ptr<AudioEncoder> encoder,
    int payload_type,
    ACMVADMode vad_mode) {
  AudioEncoderCng::Config config;
  config.num_channels = encoder->NumChannels();
  config.payload_type = payload_type;
  config.speech_encoder = std::move(encoder);
  switch (vad_mode) {
    case VADNormal:
      config.vad_mode = Vad::kVadNormal;
      break;
    case VADLowBitrate:
      config.vad_mode = Vad::kVadLowBitrate;
      break;
    case VADAggr:
      config.vad_mode = Vad::kVadAggressive;
      break;
    case VADVeryAggr:
      config.vad_mode = Vad::kVadVeryAggressive;
      break;
    default:
      RTC_FATAL();
  }
  return std::unique_ptr<AudioEncoder>(new AudioEncoderCng(std::move(config)));
}

std::unique_ptr<AudioDecoder> CreateIsacDecoder(
    int sample_rate_hz,
    const rtc::scoped_refptr<LockedIsacBandwidthInfo>& bwinfo) {
#if defined(WEBRTC_CODEC_ISACFX)
  return std::unique_ptr<AudioDecoder>(
      new AudioDecoderIsacFixImpl(sample_rate_hz, bwinfo));
#elif defined(WEBRTC_CODEC_ISAC)
  return std::unique_ptr<AudioDecoder>(
      new AudioDecoderIsacFloatImpl(sample_rate_hz, bwinfo));
#else
  RTC_FATAL() << "iSAC is not supported.";
  return std::unique_ptr<AudioDecoder>();
#endif
}

}  // namespace

RentACodec::RentACodec() {
#if defined(WEBRTC_CODEC_ISACFX) || defined(WEBRTC_CODEC_ISAC)
  isac_bandwidth_info_ = new LockedIsacBandwidthInfo;
#endif
}
RentACodec::~RentACodec() = default;

std::unique_ptr<AudioEncoder> RentACodec::RentEncoder(
    const CodecInst& codec_inst) {
  return CreateEncoder(codec_inst, isac_bandwidth_info_);
}

RentACodec::StackParameters::StackParameters() {
  // Register the default payload types for RED and CNG.
  for (const CodecInst& ci : RentACodec::Database()) {
    RentACodec::RegisterCngPayloadType(&cng_payload_types, ci);
    RentACodec::RegisterRedPayloadType(&red_payload_types, ci);
  }
}

RentACodec::StackParameters::~StackParameters() = default;

std::unique_ptr<AudioEncoder> RentACodec::RentEncoderStack(
    StackParameters* param) {
  if (!param->speech_encoder)
    return nullptr;

  if (param->use_codec_fec) {
    // Switch FEC on. On failure, remember that FEC is off.
    if (!param->speech_encoder->SetFec(true))
      param->use_codec_fec = false;
  } else {
    // Switch FEC off. This shouldn't fail.
    const bool success = param->speech_encoder->SetFec(false);
    RTC_DCHECK(success);
  }

  auto pt = [&param](const std::map<int, int>& m) {
    auto it = m.find(param->speech_encoder->SampleRateHz());
    return it == m.end() ? absl::nullopt : absl::optional<int>(it->second);
  };
  auto cng_pt = pt(param->cng_payload_types);
  param->use_cng =
      param->use_cng && cng_pt && param->speech_encoder->NumChannels() == 1;
  auto red_pt = pt(param->red_payload_types);
  param->use_red = param->use_red && red_pt;

  if (param->use_cng || param->use_red) {
    // The RED and CNG encoders need to be in sync with the speech encoder, so
    // reset the latter to ensure its buffer is empty.
    param->speech_encoder->Reset();
  }
  std::unique_ptr<AudioEncoder> encoder_stack =
      std::move(param->speech_encoder);
  if (param->use_red) {
    encoder_stack = CreateRedEncoder(std::move(encoder_stack), *red_pt);
  }
  if (param->use_cng) {
    encoder_stack =
        CreateCngEncoder(std::move(encoder_stack), *cng_pt, param->vad_mode);
  }
  return encoder_stack;
}

std::unique_ptr<AudioDecoder> RentACodec::RentIsacDecoder(int sample_rate_hz) {
  return CreateIsacDecoder(sample_rate_hz, isac_bandwidth_info_);
}

}  // namespace acm2
}  // namespace webrtc
