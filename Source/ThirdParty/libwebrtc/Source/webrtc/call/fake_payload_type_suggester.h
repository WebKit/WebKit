/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_FAKE_PAYLOAD_TYPE_SUGGESTER_H_
#define CALL_FAKE_PAYLOAD_TYPE_SUGGESTER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/payload_type.h"
#include "api/rtc_error.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_parameters.h"
#include "call/payload_type.h"
#include "call/payload_type_picker.h"
#include "media/base/codec.h"
#include "media/base/codec_comparators.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_map.h"

namespace webrtc {

// Fake payload type suggester, for use in tests.
// It uses a real PayloadTypePicker in order to do consistent PT
// assignment. It keeps per-mid recorders, but has only one PT suggester.
class FakePayloadTypeSuggester : public PayloadTypeSuggester {
 public:
  RTCErrorOr<PayloadType> SuggestPayloadType(
      absl::string_view mid,
      const Codec& codec,
      bool pick_from_top_of_range = false) override {
    PayloadTypeRecorder& recorder = LookupRecorder(mid);
    RTCErrorOr<PayloadType> current_pt = recorder.LookupPayloadType(codec);
    if (current_pt.ok()) {
      return current_pt;
    }
    auto it = fallback_suggestions_.find(
        std::make_pair(std::string(mid), codec.name));
    if (it != fallback_suggestions_.end()) {
      return it->second;
    }

    if (codec.id.IsSet() && !IsPayloadTypeConflict(mid, codec.id, codec)) {
      pt_picker_.AddMapping(codec.id, codec);
      recorder.AddMapping(codec.id, codec);
      return codec.id;
    }

    // There's only one PT picker, but multiple recorders.
    RTCErrorOr<PayloadType> suggested_result =
        pt_picker_.SuggestMapping(codec, &recorder, pick_from_top_of_range);

    if (suggested_result.ok()) {
      pt_picker_.AddMapping(suggested_result.value(), codec);
      recorder.AddMapping(suggested_result.value(), codec);
    }
    return suggested_result;
  }

  // This function is used in tests that make assumptions on what
  // payload types are assigned. It is a temporary measure until
  // those tests can be redesigned.
  void SetSuggestion(absl::string_view mid,
                     const std::string& codec_name,
                     PayloadType suggestion) {
    fallback_suggestions_[std::make_pair(std::string(mid), codec_name)] =
        suggestion;
  }

  RTCError AddLocalMapping(absl::string_view mid,
                           PayloadType payload_type,
                           const Codec& codec) override {
    LookupRecorder(mid).AddMapping(payload_type, codec);
    return pt_picker_.AddMapping(payload_type, codec);
  }

  RTCErrorOr<RtpHeaderExtensionId> SuggestRtpHeaderExtensionId(
      absl::string_view mid,
      const RtpExtension& extension,
      RtpTransceiverIdDomain id_domain) override {
    return rtp_extension_picker_.SuggestMapping(
        extension.uri, extension.encrypt, extension.id, id_domain, nullptr);
  }
  [[nodiscard]] RTCError AddRtpHeaderExtensionMapping(
      absl::string_view mid,
      const RtpExtension& extension,
      bool local) override {
    return rtp_extension_picker_.AddMapping(extension.id, extension.uri,
                                            extension.encrypt);
  }

 private:
  bool IsPayloadTypeConflict(absl::string_view mid,
                             PayloadType payload_type,
                             const Codec& codec) const {
    auto it = recorders_.find(mid);
    if (it != recorders_.end()) {
      RTCErrorOr<Codec> existing = it->second->LookupCodec(payload_type);
      if (existing.ok()) {
        if (!MatchesWithReferenceAttributes(existing.value(), codec)) {
          return true;
        }
      }
    }
    // Also check the global picker
    std::optional<Codec> global_existing = pt_picker_.LookupCodec(payload_type);
    if (global_existing &&
        !MatchesWithReferenceAttributes(*global_existing, codec)) {
      return true;
    }
    return false;
  }

  PayloadTypeRecorder& LookupRecorder(absl::string_view mid) {
    RTC_CHECK(!mid.empty());
    auto it = recorders_.find(mid);
    if (it == recorders_.end()) {
      it = recorders_
               .emplace(std::string(mid),
                        std::make_unique<PayloadTypeRecorder>(pt_picker_))
               .first;
    }
    return *it->second;
  }

  PayloadTypePicker pt_picker_;
  RtpHeaderExtensionPicker rtp_extension_picker_;
  flat_map<std::pair<std::string, std::string>, PayloadType>
      fallback_suggestions_;
  flat_map<std::string, std::unique_ptr<PayloadTypeRecorder>> recorders_;
};

}  // namespace webrtc

#endif  // CALL_FAKE_PAYLOAD_TYPE_SUGGESTER_H_
