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

#include <iterator>
#include <map>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/cleanup/cleanup.h"
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
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/thread.h"
#include "rtc_base/trace_event.h"

namespace webrtc {
namespace {

// Records the payload types of `content` in `recorder` and reports the ones
// that are in use through `payload_types_in_use`.
RTCError RecordCodecs(const ContentInfo& content,
                      PayloadTypeRecorder& recorder,
                      flat_set<PayloadType>& payload_types_in_use) {
  for (const Codec& codec : content.media_description()->codecs()) {
    RTCError error = recorder.AddMapping(codec.id, codec);
    if (!error.ok()) {
      return error;
    }
    payload_types_in_use.insert(codec.id);
  }
  return RTCError::OK();
}

}  // namespace

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
      payload_type_picker_.SuggestMapping(codec, pick_from_top_of_range);
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
  // The payload types that `description` uses, per recorder. A recorder covers
  // all the media sections of a bundle group, so the payload types of every
  // section are collected before anything is released.
  flat_map<PayloadTypeRecorder*, flat_set<PayloadType>> payload_types_in_use;
  flat_set<PayloadTypeRecorder*> modified_recorders;
  for (const ContentInfo& content : description->contents()) {
    if (!content.rejected) {
      PayloadTypeRecorder& recorder = LookupRecorder(content.mid(), local);
      if (modified_recorders.insert(&recorder).second) {
        recorder.DisallowRedefinition();
      }
    }
  }
  auto reallow_guard = absl::MakeCleanup([&modified_recorders] {
    for (PayloadTypeRecorder* recorder : modified_recorders) {
      recorder->ReallowRedefinition();
    }
  });
  for (const ContentInfo& content : description->contents()) {
    if (content.rejected) {
      continue;
    }
    PayloadTypeRecorder& recorder = LookupRecorder(content.mid(), local);
    RTCError error =
        RecordCodecs(content, recorder, payload_types_in_use[&recorder]);
    if (!error.ok()) {
      return error;
    }
    RecordRtpHeaderExtensions(content);
  }
  // Payload types that `description` does not use are available again. They
  // belong either to codecs that have been negotiated away, or to codecs that
  // were assigned a payload type while an offer was being created but that did
  // not end up in the offer.
  for (auto& [recorder, payload_types] : payload_types_in_use) {
    recorder->RetainOnly(payload_types);
  }
  EraseUnusedRecorders(description);
  payload_type_picker_.ReleaseUnusedPayloadTypes();
  return RTCError::OK();
}

void SdpPayloadTypeSuggester::RecordRtpHeaderExtensions(
    const ContentInfo& content) {
  BundleTypeRecorder& bundle_recorder = LookupBundleRecorder(content.mid());
  for (const auto& extension :
       content.media_description()->rtp_header_extensions()) {
    bundle_recorder.header_extensions().AddMapping(extension.id, extension.uri,
                                                   extension.encrypt);
  }
}

void SdpPayloadTypeSuggester::EraseUnusedRecorders(
    const SessionDescription* absl_nonnull description) {
  // Recorders are created per mid until the bundle group is known, at which
  // point all the mids of the group start sharing the recorder of the first
  // mid. The ones that are left behind, and the ones belonging to media
  // sections that have been rejected, would otherwise hold on to their payload
  // types forever.
  flat_set<std::string> recorders_in_use;
  for (const ContentInfo& content : description->contents()) {
    if (!content.rejected) {
      recorders_in_use.insert(BundleRecorderName(content.mid()));
    }
  }
  for (auto it = recorder_by_mid_.begin(); it != recorder_by_mid_.end();) {
    it = recorders_in_use.contains(it->first) ? std::next(it)
                                              : recorder_by_mid_.erase(it);
  }
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
  std::string transport_mapped_name = BundleRecorderName(mid);
  return recorder_by_mid_
      .try_emplace(std::move(transport_mapped_name), payload_type_picker_, env_)
      .first->second;
}

std::string SdpPayloadTypeSuggester::BundleRecorderName(
    absl::string_view mid) const {
  const ContentGroup* absl_nullable group =
      bundle_manager_.LookupGroupByMid(mid);
  if (group == nullptr) {
    // Not in a group.
    return std::string(mid);
  }
  const std::string* group_name = group->FirstContentName();
  RTC_CHECK(group_name);  // empty groups should be impossible here
  return *group_name;
}

}  // namespace webrtc
