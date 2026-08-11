/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdp_payload_type_suggester.h"

#include <map>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/jsep.h"
#include "api/payload_type.h"
#include "api/rtc_error.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_parameters.h"
#include "call/payload_type.h"
#include "call/payload_type_picker.h"
#include "media/base/codec.h"
#include "media/base/codec_comparators.h"
#include "pc/session_description.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

// Implementation of the SdpPayloadTypeSuggester
RTCErrorOr<PayloadType> SdpPayloadTypeSuggester::SuggestPayloadType(
    absl::string_view mid,
    const Codec& codec,
    bool pick_from_top_of_range) {
  RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS();
  PayloadTypeRecorder& local_recorder = LookupRecorder(mid, /* local= */ true);

  if (pick_from_top_of_range && codec.id.IsSet()) {
    RTCErrorOr<Codec> existing = local_recorder.LookupCodec(codec.id);
    if (existing.ok()) {
      if (MatchesWithReferenceAttributes(existing.value(), codec)) {
        return codec.id;
      }
    } else if (codec.id >= 0 && codec.id <= 127 &&
               !payload_type_picker_.IsSeen(codec.id)) {
      local_recorder.AddMapping(codec.id, codec);
      return codec.id;
    }
  }

  auto local_result = local_recorder.LookupPayloadType(codec);
  if (local_result.ok()) {
    return local_result;
  }

  PayloadTypeRecorder& remote_recorder =
      LookupRecorder(mid, /* local= */ false);
  RTCErrorOr<PayloadType> remote_result =
      remote_recorder.LookupPayloadType(codec);
  if (remote_result.ok()) {
    RTCErrorOr<Codec> local_codec =
        local_recorder.LookupCodec(remote_result.value());
    if (!local_codec.ok()) {
      // Tell the local payload type registry that we've taken this
      RTC_DCHECK(local_codec.error().type() == RTCErrorType::INVALID_PARAMETER);
      AddLocalMapping(mid, remote_result.value(), codec);
      return remote_result;
    }
    // If we get here, PT is already in use, possibly for something else.
    // Fall through to SuggestMapping.
  }
  RTCErrorOr<PayloadType> suggested_result =
      payload_type_picker_.SuggestMapping(codec, &local_recorder,
                                          pick_from_top_of_range);
  if (suggested_result.ok()) {
    local_recorder.AddMapping(suggested_result.value(), codec);
  }
  return suggested_result;
}

RTCError SdpPayloadTypeSuggester::AddLocalMapping(absl::string_view mid,
                                                  PayloadType payload_type,
                                                  const Codec& codec) {
  RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS();
  PayloadTypeRecorder& recorder = LookupRecorder(mid, /* local= */ true);
  return recorder.AddMapping(payload_type, codec);
}

RTCErrorOr<RtpHeaderExtensionId>
SdpPayloadTypeSuggester::SuggestRtpHeaderExtensionId(
    absl::string_view mid,
    const RtpExtension& extension,
    RtpTransceiverIdDomain id_domain) {
  RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS();
  BundleTypeRecorder& bundle_recorder = LookupBundleRecorder(mid);
  RTCErrorOr<RtpHeaderExtensionId> result =
      bundle_recorder.header_extensions().LookupId(extension.uri,
                                                   extension.encrypt);
  if (result.ok()) {
    return result;
  }
  return rtp_header_extension_picker_.SuggestMapping(
      extension.uri, extension.encrypt, extension.id, id_domain,
      &bundle_recorder.header_extensions());
}

RTCError SdpPayloadTypeSuggester::AddRtpHeaderExtensionMapping(
    absl::string_view mid,
    const RtpExtension& extension,
    bool local) {
  RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS();
  BundleTypeRecorder& bundle_recorder = LookupBundleRecorder(mid);
  rtp_header_extension_picker_.AddMapping(extension.id, extension.uri,
                                          extension.encrypt);
  return bundle_recorder.header_extensions().AddMapping(
      extension.id, extension.uri, extension.encrypt);
}

RTCError SdpPayloadTypeSuggester::Update(const SessionDescription* description,
                                         bool local,
                                         SdpType type) {
  bundle_manager_.Update(description, type);
  if (type == SdpType::kAnswer) {
    bundle_manager_.Commit();
  }
  for (const ContentInfo& content : description->contents()) {
    if (content.rejected) {
      continue;
    }
    PayloadTypeRecorder& recorder = LookupRecorder(content.mid(), local);
    recorder.DisallowRedefinition();
    RTCError error;
    for (const Codec& codec : content.media_description()->codecs()) {
      error = recorder.AddMapping(codec.id, codec);
      if (!error.ok()) {
        break;
      }
    }
    recorder.ReallowRedefinition();
    if (!error.ok()) {
      return error;
    }

    BundleTypeRecorder& bundle_recorder = LookupBundleRecorder(content.mid());
    for (const auto& extension :
         content.media_description()->rtp_header_extensions()) {
      bundle_recorder.header_extensions().AddMapping(
          extension.id, extension.uri, extension.encrypt);
    }
    if (type == SdpType::kAnswer) {
      bundle_recorder.header_extensions().Commit();
    }
  }
  return RTCError::OK();
}

PayloadTypeRecorder& SdpPayloadTypeSuggester::LookupRecorder(
    absl::string_view mid,
    bool local) {
  BundleTypeRecorder& recorder = LookupBundleRecorder(mid);
  return local ? recorder.local_payload_types()
               : recorder.remote_payload_types();
}

SdpPayloadTypeSuggester::BundleTypeRecorder&
SdpPayloadTypeSuggester::LookupBundleRecorder(absl::string_view mid) {
  const ContentGroup* group = bundle_manager_.LookupGroupByMid(mid);
  std::string transport_mapped_name;
  if (group) {
    const std::string* group_name = group->FirstContentName();
    RTC_CHECK(group_name);  // empty groups should be impossible here
    transport_mapped_name = *group_name;
  } else {
    // Not in a group.
    transport_mapped_name = mid;
  }
  if (!recorder_by_mid_.contains(transport_mapped_name)) {
    recorder_by_mid_.emplace(std::make_pair(
        transport_mapped_name, BundleTypeRecorder(payload_type_picker_, env_)));
  }
  return recorder_by_mid_.at(transport_mapped_name);
}

}  // namespace webrtc
