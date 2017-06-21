/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/coder.h"

#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/audio_format_conversion.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {
namespace {
AudioCodingModule::Config GetAcmConfig(uint32_t id) {
  AudioCodingModule::Config config;
  // This class does not handle muted output.
  config.neteq_config.enable_muted_state = false;
  config.id = id;
  config.decoder_factory = CreateBuiltinAudioDecoderFactory();
  return config;
}
}  // namespace

AudioCoder::AudioCoder(uint32_t instance_id)
    : acm_(AudioCodingModule::Create(GetAcmConfig(instance_id))),
      receive_codec_(),
      encode_timestamp_(0),
      encoded_data_(nullptr),
      encoded_length_in_bytes_(0),
      decode_timestamp_(0) {
  acm_->InitializeReceiver();
  acm_->RegisterTransportCallback(this);
}

AudioCoder::~AudioCoder() {}

int32_t AudioCoder::SetEncodeCodec(const CodecInst& codec_inst) {
  const bool success = codec_manager_.RegisterEncoder(codec_inst) &&
                       codec_manager_.MakeEncoder(&rent_a_codec_, acm_.get());
  return success ? 0 : -1;
}

int32_t AudioCoder::SetDecodeCodec(const CodecInst& codec_inst) {
  if (!acm_->RegisterReceiveCodec(codec_inst.pltype,
                                  CodecInstToSdp(codec_inst))) {
    return -1;
  }
  memcpy(&receive_codec_, &codec_inst, sizeof(CodecInst));
  return 0;
}

int32_t AudioCoder::Decode(AudioFrame* decoded_audio,
                           uint32_t samp_freq_hz,
                           const int8_t* incoming_payload,
                           size_t payload_length) {
  if (payload_length > 0) {
    const uint8_t payload_type = receive_codec_.pltype;
    decode_timestamp_ += receive_codec_.pacsize;
    if (acm_->IncomingPayload((const uint8_t*)incoming_payload, payload_length,
                              payload_type, decode_timestamp_) == -1) {
      return -1;
    }
  }
  bool muted;
  int32_t ret =
      acm_->PlayoutData10Ms((uint16_t)samp_freq_hz, decoded_audio, &muted);
  RTC_DCHECK(!muted);
  return ret;
}

int32_t AudioCoder::PlayoutData(AudioFrame* decoded_audio,
                                uint16_t samp_freq_hz) {
  bool muted;
  int32_t ret = acm_->PlayoutData10Ms(samp_freq_hz, decoded_audio, &muted);
  RTC_DCHECK(!muted);
  return ret;
}

int32_t AudioCoder::Encode(const AudioFrame& audio,
                           int8_t* encoded_data,
                           size_t* encoded_length_in_bytes) {
  // Fake a timestamp in case audio doesn't contain a correct timestamp.
  // Make a local copy of the audio frame since audio is const
  AudioFrame audio_frame;
  audio_frame.CopyFrom(audio);
  audio_frame.timestamp_ = encode_timestamp_;
  encode_timestamp_ += static_cast<uint32_t>(audio_frame.samples_per_channel_);

  // For any codec with a frame size that is longer than 10 ms the encoded
  // length in bytes should be zero until a a full frame has been encoded.
  encoded_length_in_bytes_ = 0;
  encoded_data_ = encoded_data;
  if (acm_->Add10MsData((AudioFrame&)audio_frame) == -1) {
    return -1;
  }

  *encoded_length_in_bytes = encoded_length_in_bytes_;
  return 0;
}

int32_t AudioCoder::SendData(FrameType /* frame_type */,
                             uint8_t /* payload_type */,
                             uint32_t /* time_stamp */,
                             const uint8_t* payload_data,
                             size_t payload_size,
                             const RTPFragmentationHeader* /* fragmentation*/) {
  memcpy(encoded_data_, payload_data, sizeof(uint8_t) * payload_size);
  encoded_length_in_bytes_ = payload_size;
  return 0;
}

}  // namespace webrtc
