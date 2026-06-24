/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_PAYLOAD_TYPE_PICKER_H_
#define CALL_PAYLOAD_TYPE_PICKER_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/payload_type.h"
#include "api/rtc_error.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_parameters.h"
#include "call/payload_type.h"
#include "media/base/codec.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/containers/flat_set.h"

namespace webrtc {

class PayloadTypeRecorder;

class PayloadTypePicker final {
 public:
  PayloadTypePicker();
  PayloadTypePicker(const PayloadTypePicker&) = delete;
  PayloadTypePicker& operator=(const PayloadTypePicker&) = delete;
  PayloadTypePicker(PayloadTypePicker&&) = delete;
  PayloadTypePicker& operator=(PayloadTypePicker&&) = delete;
  // Suggest a payload type for the codec.
  // If the excluder maps it to something different, don't suggest it.
  RTCErrorOr<PayloadType> SuggestMapping(Codec codec,
                                         const PayloadTypeRecorder* excluder,
                                         bool pick_from_top_of_range = false);
  RTCError AddMapping(PayloadType payload_type, Codec codec);
  std::optional<Codec> LookupCodec(PayloadType payload_type) const;
  bool IsSeen(PayloadType payload_type) const {
    return seen_payload_types_.contains(payload_type.value());
  }

 private:
  class MapEntry final {
   public:
    MapEntry(PayloadType payload_type, Codec codec)
        : payload_type_(payload_type), codec_(codec) {}
    PayloadType payload_type() const { return payload_type_; }
    Codec codec() const { return codec_; }

   private:
    PayloadType payload_type_;
    Codec codec_;
    template <typename Sink>
    friend void AbslStringify(Sink& sink, const MapEntry& entry) {
      absl::Format(&sink, "%v:%v", entry.payload_type(), entry.codec());
    }
  };
  std::vector<MapEntry> entries_;
  flat_set<PayloadType> seen_payload_types_;
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PayloadTypePicker& picker) {
    sink.Append("Reserved:");
    for (PayloadType pt : picker.seen_payload_types_) {
      absl::Format(&sink, " %v", pt);
    }
    sink.Append("\nEntries:");
    for (const MapEntry& entry : picker.entries_) {
      absl::Format(&sink, "\n %v", entry);
    }
  }
};

class PayloadTypeRecorder final {
 public:
  explicit PayloadTypeRecorder(PayloadTypePicker& suggester)
      : suggester_(suggester) {}
  ~PayloadTypeRecorder() {
    // Ensure consistent use of paired Disallow/ReallowRedefintion calls.
    RTC_DCHECK(disallow_redefinition_level_ == 0);
  }

  RTCError AddMapping(PayloadType payload_type, Codec codec);
  std::vector<std::pair<PayloadType, Codec>> GetMappings() const;
  RTCErrorOr<PayloadType> LookupPayloadType(Codec codec) const;
  RTCErrorOr<Codec> LookupCodec(PayloadType payload_type) const;
  // Redefinition guard.
  // In some scenarios, redefinition must be allowed between one offer/answer
  // set and the next offer/answer set, but within the processing of one
  // SDP, it should never be allowed.
  // Implemented as a stack push/pop for convenience; if Disallow has
  // been called more times than Reallow, redefinition is prohibited.
  void DisallowRedefinition();
  void ReallowRedefinition();
  // Transaction support.
  // Commit() commits previous changes.
  void Commit();
  // Rollback() rolls back to the previous checkpoint.
  void Rollback();

 private:
  PayloadTypePicker& suggester_;
  flat_map<PayloadType, Codec> payload_type_to_codec_;
  flat_map<PayloadType, Codec> checkpoint_payload_type_to_codec_;
  int disallow_redefinition_level_ = 0;
  flat_set<PayloadType> accepted_definitions_;
};

class RtpHeaderExtensionRecorder final {
 public:
  explicit RtpHeaderExtensionRecorder(const Environment& env) : env_(env) {}
  ~RtpHeaderExtensionRecorder() {}

  RTCError AddMapping(RtpHeaderExtensionId id,
                      absl::string_view uri,
                      bool encrypt);
  RTCErrorOr<RtpHeaderExtensionId> LookupId(absl::string_view uri,
                                            bool encrypt) const;

  void Commit();
  void Rollback();

 private:
  const Environment env_;
  // (uri, encrypt) -> id
  flat_map<std::pair<std::string, bool>, RtpHeaderExtensionId> uri_to_id_;
  flat_map<std::pair<std::string, bool>, RtpHeaderExtensionId>
      checkpoint_uri_to_id_;
};

class RtpHeaderExtensionPicker final {
 public:
  RtpHeaderExtensionPicker() {}
  ~RtpHeaderExtensionPicker() {}

  RTCErrorOr<RtpHeaderExtensionId> SuggestMapping(
      absl::string_view uri,
      bool encrypt,
      RtpHeaderExtensionId preferred_id,
      RtpTransceiverIdDomain id_domain,
      const RtpHeaderExtensionRecorder* excluder);
  RTCError AddMapping(RtpHeaderExtensionId id,
                      absl::string_view uri,
                      bool encrypt);

 private:
  struct MapEntry {
    std::string uri;
    bool encrypt;
    RtpHeaderExtensionId id;
  };
  std::vector<MapEntry> entries_;
  flat_set<RtpHeaderExtensionId> seen_ids_;
};

}  // namespace webrtc

#endif  //  CALL_PAYLOAD_TYPE_PICKER_H_
