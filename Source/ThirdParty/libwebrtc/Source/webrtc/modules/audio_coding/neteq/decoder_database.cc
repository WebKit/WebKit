/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq/decoder_database.h"

#include <utility>  // pair

#include "webrtc/api/audio_codecs/audio_decoder.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc {

DecoderDatabase::DecoderDatabase(
    const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory)
    : active_decoder_type_(-1),
      active_cng_decoder_type_(-1),
      decoder_factory_(decoder_factory) {}

DecoderDatabase::~DecoderDatabase() = default;

DecoderDatabase::DecoderInfo::DecoderInfo(const SdpAudioFormat& audio_format,
                                          AudioDecoderFactory* factory,
                                          const std::string& codec_name)
    : name_(codec_name),
      audio_format_(audio_format),
      factory_(factory),
      external_decoder_(nullptr),
      cng_decoder_(CngDecoder::Create(audio_format)),
      subtype_(SubtypeFromFormat(audio_format)) {}

DecoderDatabase::DecoderInfo::DecoderInfo(const SdpAudioFormat& audio_format,
                                          AudioDecoderFactory* factory)
    : DecoderInfo(audio_format, factory, audio_format.name) {}

DecoderDatabase::DecoderInfo::DecoderInfo(NetEqDecoder ct,
                                          AudioDecoderFactory* factory)
    : DecoderInfo(*NetEqDecoderToSdpAudioFormat(ct), factory) {}

DecoderDatabase::DecoderInfo::DecoderInfo(const SdpAudioFormat& audio_format,
                                          AudioDecoder* ext_dec,
                                          const std::string& codec_name)
    : name_(codec_name),
      audio_format_(audio_format),
      factory_(nullptr),
      external_decoder_(ext_dec),
      subtype_(Subtype::kNormal) {
  RTC_CHECK(ext_dec);
}

DecoderDatabase::DecoderInfo::DecoderInfo(DecoderInfo&&) = default;
DecoderDatabase::DecoderInfo::~DecoderInfo() = default;

AudioDecoder* DecoderDatabase::DecoderInfo::GetDecoder() const {
  if (subtype_ != Subtype::kNormal) {
    // These are handled internally, so they have no AudioDecoder objects.
    return nullptr;
  }
  if (external_decoder_) {
    RTC_DCHECK(!decoder_);
    RTC_DCHECK(!cng_decoder_);
    return external_decoder_;
  }
  if (!decoder_) {
    // TODO(ossu): Keep a check here for now, since a number of tests create
    // DecoderInfos without factories.
    RTC_DCHECK(factory_);
    decoder_ = factory_->MakeAudioDecoder(audio_format_);
  }
  RTC_DCHECK(decoder_) << "Failed to create: " << audio_format_;
  return decoder_.get();
}

bool DecoderDatabase::DecoderInfo::IsType(const char* name) const {
  return STR_CASE_CMP(audio_format_.name.c_str(), name) == 0;
}

bool DecoderDatabase::DecoderInfo::IsType(const std::string& name) const {
  return IsType(name.c_str());
}

rtc::Optional<DecoderDatabase::DecoderInfo::CngDecoder>
DecoderDatabase::DecoderInfo::CngDecoder::Create(const SdpAudioFormat& format) {
  if (STR_CASE_CMP(format.name.c_str(), "CN") == 0) {
    // CN has a 1:1 RTP clock rate to sample rate ratio.
    const int sample_rate_hz = format.clockrate_hz;
    RTC_DCHECK(sample_rate_hz == 8000 || sample_rate_hz == 16000 ||
               sample_rate_hz == 32000 || sample_rate_hz == 48000);
    return rtc::Optional<DecoderDatabase::DecoderInfo::CngDecoder>(
        {sample_rate_hz});
  } else {
    return rtc::Optional<CngDecoder>();
  }
}

DecoderDatabase::DecoderInfo::Subtype
DecoderDatabase::DecoderInfo::SubtypeFromFormat(const SdpAudioFormat& format) {
  if (STR_CASE_CMP(format.name.c_str(), "CN") == 0) {
    return Subtype::kComfortNoise;
  } else if (STR_CASE_CMP(format.name.c_str(), "telephone-event") == 0) {
    return Subtype::kDtmf;
  } else if (STR_CASE_CMP(format.name.c_str(), "red") == 0) {
    return Subtype::kRed;
  }

  return Subtype::kNormal;
}

bool DecoderDatabase::Empty() const { return decoders_.empty(); }

int DecoderDatabase::Size() const { return static_cast<int>(decoders_.size()); }

void DecoderDatabase::Reset() {
  decoders_.clear();
  active_decoder_type_ = -1;
  active_cng_decoder_type_ = -1;
}

int DecoderDatabase::RegisterPayload(uint8_t rtp_payload_type,
                                     NetEqDecoder codec_type,
                                     const std::string& name) {
  if (rtp_payload_type > 0x7F) {
    return kInvalidRtpPayloadType;
  }
  // kCodecArbitrary is only supported through InsertExternal.
  if (codec_type == NetEqDecoder::kDecoderArbitrary ||
      !CodecSupported(codec_type)) {
    return kCodecNotSupported;
  }
  const auto opt_format = NetEqDecoderToSdpAudioFormat(codec_type);
  if (!opt_format) {
    return kCodecNotSupported;
  }
  DecoderInfo info(*opt_format, decoder_factory_, name);
  auto ret =
      decoders_.insert(std::make_pair(rtp_payload_type, std::move(info)));
  if (ret.second == false) {
    // Database already contains a decoder with type |rtp_payload_type|.
    return kDecoderExists;
  }
  return kOK;
}

int DecoderDatabase::RegisterPayload(int rtp_payload_type,
                                     const SdpAudioFormat& audio_format) {
  if (rtp_payload_type < 0 || rtp_payload_type > 0x7f) {
    return kInvalidRtpPayloadType;
  }
  const auto ret = decoders_.insert(std::make_pair(
      rtp_payload_type, DecoderInfo(audio_format, decoder_factory_.get())));
  if (ret.second == false) {
    // Database already contains a decoder with type |rtp_payload_type|.
    return kDecoderExists;
  }
  return kOK;
}

int DecoderDatabase::InsertExternal(uint8_t rtp_payload_type,
                                    NetEqDecoder codec_type,
                                    const std::string& codec_name,
                                    AudioDecoder* decoder) {
  if (rtp_payload_type > 0x7F) {
    return kInvalidRtpPayloadType;
  }
  if (!decoder) {
    return kInvalidPointer;
  }

  const auto opt_db_format = NetEqDecoderToSdpAudioFormat(codec_type);
  const SdpAudioFormat format = opt_db_format.value_or({"arbitrary", 0, 0});

  std::pair<DecoderMap::iterator, bool> ret;
  DecoderInfo info(format, decoder, codec_name);
  ret = decoders_.insert(std::make_pair(rtp_payload_type, std::move(info)));
  if (ret.second == false) {
    // Database already contains a decoder with type |rtp_payload_type|.
    return kDecoderExists;
  }
  return kOK;
}

int DecoderDatabase::Remove(uint8_t rtp_payload_type) {
  if (decoders_.erase(rtp_payload_type) == 0) {
    // No decoder with that |rtp_payload_type|.
    return kDecoderNotFound;
  }
  if (active_decoder_type_ == rtp_payload_type) {
    active_decoder_type_ = -1;  // No active decoder.
  }
  if (active_cng_decoder_type_ == rtp_payload_type) {
    active_cng_decoder_type_ = -1;  // No active CNG decoder.
  }
  return kOK;
}

void DecoderDatabase::RemoveAll() {
  decoders_.clear();
  active_decoder_type_ = -1;      // No active decoder.
  active_cng_decoder_type_ = -1;  // No active CNG decoder.
}

const DecoderDatabase::DecoderInfo* DecoderDatabase::GetDecoderInfo(
    uint8_t rtp_payload_type) const {
  DecoderMap::const_iterator it = decoders_.find(rtp_payload_type);
  if (it == decoders_.end()) {
    // Decoder not found.
    return NULL;
  }
  return &it->second;
}

int DecoderDatabase::SetActiveDecoder(uint8_t rtp_payload_type,
                                      bool* new_decoder) {
  // Check that |rtp_payload_type| exists in the database.
  const DecoderInfo *info = GetDecoderInfo(rtp_payload_type);
  if (!info) {
    // Decoder not found.
    return kDecoderNotFound;
  }
  RTC_CHECK(!info->IsComfortNoise());
  RTC_DCHECK(new_decoder);
  *new_decoder = false;
  if (active_decoder_type_ < 0) {
    // This is the first active decoder.
    *new_decoder = true;
  } else if (active_decoder_type_ != rtp_payload_type) {
    // Moving from one active decoder to another. Delete the first one.
    const DecoderInfo *old_info = GetDecoderInfo(active_decoder_type_);
    RTC_DCHECK(old_info);
    old_info->DropDecoder();
    *new_decoder = true;
  }
  active_decoder_type_ = rtp_payload_type;
  return kOK;
}

AudioDecoder* DecoderDatabase::GetActiveDecoder() const {
  if (active_decoder_type_ < 0) {
    // No active decoder.
    return NULL;
  }
  return GetDecoder(active_decoder_type_);
}

int DecoderDatabase::SetActiveCngDecoder(uint8_t rtp_payload_type) {
  // Check that |rtp_payload_type| exists in the database.
  const DecoderInfo *info = GetDecoderInfo(rtp_payload_type);
  if (!info) {
    // Decoder not found.
    return kDecoderNotFound;
  }
  if (active_cng_decoder_type_ >= 0 &&
      active_cng_decoder_type_ != rtp_payload_type) {
    // Moving from one active CNG decoder to another. Delete the first one.
    RTC_DCHECK(active_cng_decoder_);
    active_cng_decoder_.reset();
  }
  active_cng_decoder_type_ = rtp_payload_type;
  return kOK;
}

ComfortNoiseDecoder* DecoderDatabase::GetActiveCngDecoder() const {
  if (active_cng_decoder_type_ < 0) {
    // No active CNG decoder.
    return NULL;
  }
  if (!active_cng_decoder_) {
    active_cng_decoder_.reset(new ComfortNoiseDecoder);
  }
  return active_cng_decoder_.get();
}

AudioDecoder* DecoderDatabase::GetDecoder(uint8_t rtp_payload_type) const {
  const DecoderInfo *info = GetDecoderInfo(rtp_payload_type);
  return info ? info->GetDecoder() : nullptr;
}

bool DecoderDatabase::IsType(uint8_t rtp_payload_type, const char* name) const {
  const DecoderInfo* info = GetDecoderInfo(rtp_payload_type);
  return info && info->IsType(name);
}

bool DecoderDatabase::IsType(uint8_t rtp_payload_type,
                             const std::string& name) const {
  return IsType(rtp_payload_type, name.c_str());
}

bool DecoderDatabase::IsComfortNoise(uint8_t rtp_payload_type) const {
  const DecoderInfo *info = GetDecoderInfo(rtp_payload_type);
  return info && info->IsComfortNoise();
}

bool DecoderDatabase::IsDtmf(uint8_t rtp_payload_type) const {
  const DecoderInfo *info = GetDecoderInfo(rtp_payload_type);
  return info && info->IsDtmf();
}

bool DecoderDatabase::IsRed(uint8_t rtp_payload_type) const {
  const DecoderInfo *info = GetDecoderInfo(rtp_payload_type);
  return info && info->IsRed();
}

int DecoderDatabase::CheckPayloadTypes(const PacketList& packet_list) const {
  PacketList::const_iterator it;
  for (it = packet_list.begin(); it != packet_list.end(); ++it) {
    if (!GetDecoderInfo(it->payload_type)) {
      // Payload type is not found.
      LOG(LS_WARNING) << "CheckPayloadTypes: unknown RTP payload type "
                      << static_cast<int>(it->payload_type);
      return kDecoderNotFound;
    }
  }
  return kOK;
}

}  // namespace webrtc
