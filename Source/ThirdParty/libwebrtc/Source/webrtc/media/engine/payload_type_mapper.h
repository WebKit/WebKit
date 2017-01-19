/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_PAYLOAD_TYPE_MAPPER_H_
#define WEBRTC_MEDIA_ENGINE_PAYLOAD_TYPE_MAPPER_H_

#include <map>
#include <set>

#include "webrtc/base/optional.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/modules/audio_coding/codecs/audio_format.h"

namespace cricket {

class PayloadTypeMapper {
 public:
  PayloadTypeMapper();
  ~PayloadTypeMapper();

  // Finds the current payload type for |format| or assigns a new one, if no
  // current mapping exists. Will return an empty value if it was unable to
  // create a mapping, i.e. if all dynamic payload type ids have been used up.
  rtc::Optional<int> GetMappingFor(const webrtc::SdpAudioFormat& format);

  // Finds the current payload type for |format|, if any. Returns an empty value
  // if no payload type mapping exists for the format.
  rtc::Optional<int> FindMappingFor(const webrtc::SdpAudioFormat& format) const;

  // Like GetMappingFor, but fills in an AudioCodec structure with the necessary
  // information instead.
  rtc::Optional<AudioCodec> ToAudioCodec(const webrtc::SdpAudioFormat& format);

 private:
  struct SdpAudioFormatOrdering {
    bool operator()(const webrtc::SdpAudioFormat& a,
                    const webrtc::SdpAudioFormat& b) const;
  };

  int next_unused_payload_type_;
  int max_payload_type_;
  std::map<webrtc::SdpAudioFormat, int, SdpAudioFormatOrdering> mappings_;
  std::set<int> used_payload_types_;
};

}  // namespace cricket
#endif  // WEBRTC_MEDIA_ENGINE_PAYLOAD_TYPE_MAPPER_H_
