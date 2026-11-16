/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/codec_list.h"

#include <cstddef>
#include <map>
#include <span>
#include <utility>
#include <vector>

#include "api/payload_type.h"
#include "api/rtc_error.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"

namespace webrtc {


namespace {

// Checks that no two codecs in `codecs` use the same payload type, and fills
// `pt_to_index` with the payload type of each codec that has one. This
// invariant holds both while a codec list is being built and once it is
// complete.
RTCError CheckPayloadTypesAreUnique(std::span<const Codec> codecs,
                                    std::map<int, int>& pt_to_index) {
  for (size_t i = 0; i < codecs.size(); i++) {
    const Codec& codec = codecs[i];
    if (codec.id != PayloadType::NotSet()) {
      auto [it, success] = pt_to_index.insert({codec.id, static_cast<int>(i)});
      if (!success) {
        RTC_LOG(LS_ERROR) << "Duplicate payload type in codec list, " << codec
                          << " and " << codecs[it->second]
                          << " have the same ID";
        return RTC_LOG_ERROR(RTCError(RTCErrorType::INVALID_PARAMETER)
                             << "Duplicate payload type in codec list");
      }
    }
  }
  return RTCError::OK();
}

// Checks that the codecs referred to by RTX codecs are present in `codecs`.
// This invariant only holds for a complete codec list: while a list is being
// built, an RTX codec can be added before the codec that it refers to.
RTCError CheckReferencedCodecsArePresent(
    std::span<const Codec> codecs,
    const std::map<int, int>& pt_to_index) {
  for (const Codec& codec : codecs) {
    switch (codec.GetResiliencyType()) {
      case Codec::ResiliencyType::kRed:
        // Check that the target codec exists
        break;
      case Codec::ResiliencyType::kRtx: {
        // Check that the target codec exists
        const auto apt_it = codec.params.find(kCodecParamAssociatedPayloadType);
        // Not true - there's a test that deliberately injects a wrong
        // RTX codec (MediaSessionDescriptionFactoryTest.RtxWithoutApt)
        // TODO: https://issues.webrtc.org/384756622 - reject codec earlier and
        // enable check. RTC_DCHECK(apt_it != codec.params.end()); Until that is
        // fixed:
        if (codec.id == PayloadType::NotSet()) {
          // Should not have an apt parameter.
          if (apt_it != codec.params.end()) {
            RTC_LOG(LS_WARNING) << "Surprising condition: RTX codec without "
                                << "PT has an apt parameter";
          }
          // Stop checking the associated PT.
          break;
        }
        if (apt_it == codec.params.end()) {
          RTC_LOG(LS_WARNING) << "Surprising condition: RTX codec without"
                              << " apt parameter: " << codec;
          break;
        }
        int associated_pt;
        if (!(FromString(apt_it->second, &associated_pt))) {
          RTC_LOG(LS_ERROR) << "Non-numeric argument to rtx apt: " << codec
                            << " apt=" << apt_it->second;
          return RTC_LOG_ERROR(RTCError(RTCErrorType::INVALID_PARAMETER)
                               << "Non-numeric argument to rtx apt parameter");
        }
        if (codec.id != PayloadType::NotSet() &&
            pt_to_index.count(associated_pt) != 1) {
          RTC_LOG(LS_WARNING)
              << "Surprising condition: RTX codec APT not found: " << codec
              << " points to a PT that occurs "
              << pt_to_index.count(associated_pt) << " times";
          return RTC_LOG_ERROR(
              RTCError(RTCErrorType::INVALID_PARAMETER)
              << "PT pointed to by rtx apt parameter does not exist");
        }
        // const Codec& referred_codec = codecs[pt_to_index[associated_pt]];
        // Not true:
        // RTC_DCHECK(referred_codec.type == Codec::Type::kVideo);
        // Not true:
        // RTC_DCHECK(referred_codec.GetResiliencyType() ==
        // Codec::ResiliencyType::kNone);
        // TODO: https://issues.webrtc.org/384756623 - figure out if this is
        // expected or not.
        break;
      }
      case Codec::ResiliencyType::kNone:
        break;  // nothing to see here
      default:
        break;  // don't know what to check yet
    }
  }
  return RTCError::OK();
}

RTCError CheckInputConsistency(std::span<const Codec> codecs) {
  std::map<int, int> pt_to_index;
  RTCError error = CheckPayloadTypesAreUnique(codecs, pt_to_index);
  if (!error.ok()) {
    return error;
  }
  return CheckReferencedCodecsArePresent(codecs, pt_to_index);
}

}  // namespace

// static
RTCErrorOr<CodecList> CodecList::Create(std::span<const Codec> codecs) {
  RTCError error = CheckInputConsistency(codecs);
  if (!error.ok()) {
    return error;
  }
  return CodecList(codecs);
}

CodecList::PushResult CodecList::PushIfNotPresent(const Codec& codec) {
  for (const Codec& present_codec : codecs_) {
    if (present_codec.id == codec.id) {
      if (present_codec != codec) {
        RTC_LOG(LS_ERROR) << "Payload type " << codec.id
                          << " is already used by " << present_codec
                          << ", not adding " << codec;
        return PushResult::kConflict;
      }
      return PushResult::kDuplicate;
    }
  }
  push_back(codec);
  return PushResult::kInserted;
}

void CodecList::push_back(const Codec& codec) {
  codecs_.push_back(codec);
  // Only the payload types are checked here. A codec list that is under
  // construction may contain an RTX codec that refers to a codec that has not
  // been added yet; that is checked by CheckConsistency().
#if RTC_DCHECK_IS_ON
  std::map<int, int> pt_to_index;
  RTC_DCHECK(CheckPayloadTypesAreUnique(codecs_, pt_to_index).ok());
#endif
}

void CodecList::CheckConsistency() {
  RTC_DCHECK(CheckInputConsistency(codecs_).ok());
}

RTCErrorOr<std::vector<Codec>> CodecList::Finalize() && {
  std::vector<Codec> codecs = std::move(codecs_);
  codecs_.clear();
  RTCError error = CheckInputConsistency(codecs);
  if (!error.ok()) {
    return error;
  }
  return codecs;
}

}  // namespace webrtc
