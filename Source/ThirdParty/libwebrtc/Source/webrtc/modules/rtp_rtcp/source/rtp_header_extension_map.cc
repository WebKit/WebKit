/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"

#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

struct ExtensionInfo {
  RTPExtensionType type;
  const char* uri;
};

template <typename Extension>
constexpr ExtensionInfo CreateExtensionInfo() {
  return {Extension::kId, Extension::kUri};
}

constexpr ExtensionInfo kExtensions[] = {
    CreateExtensionInfo<TransmissionOffset>(),
    CreateExtensionInfo<AudioLevel>(),
    CreateExtensionInfo<AbsoluteSendTime>(),
    CreateExtensionInfo<VideoOrientation>(),
    CreateExtensionInfo<TransportSequenceNumber>(),
    CreateExtensionInfo<PlayoutDelayLimits>(),
    CreateExtensionInfo<VideoContentTypeExtension>(),
    CreateExtensionInfo<VideoTimingExtension>(),
    CreateExtensionInfo<RtpStreamId>(),
    CreateExtensionInfo<RepairedRtpStreamId>(),
    CreateExtensionInfo<RtpMid>(),
    CreateExtensionInfo<RtpGenericFrameDescriptorExtension>(),
};

// Because of kRtpExtensionNone, NumberOfExtension is 1 bigger than the actual
// number of known extensions.
static_assert(arraysize(kExtensions) ==
                  static_cast<int>(kRtpExtensionNumberOfExtensions) - 1,
              "kExtensions expect to list all known extensions");

}  // namespace

constexpr RTPExtensionType RtpHeaderExtensionMap::kInvalidType;
constexpr int RtpHeaderExtensionMap::kInvalidId;
constexpr int RtpHeaderExtensionMap::kMinId;
constexpr int RtpHeaderExtensionMap::kMaxId;

RtpHeaderExtensionMap::RtpHeaderExtensionMap() {
  for (auto& type : types_)
    type = kInvalidType;
  for (auto& id : ids_)
    id = kInvalidId;
}

RtpHeaderExtensionMap::RtpHeaderExtensionMap(
    rtc::ArrayView<const RtpExtension> extensions)
    : RtpHeaderExtensionMap() {
  for (const RtpExtension& extension : extensions)
    RegisterByUri(extension.id, extension.uri);
}

bool RtpHeaderExtensionMap::RegisterByType(int id, RTPExtensionType type) {
  for (const ExtensionInfo& extension : kExtensions)
    if (type == extension.type)
      return Register(id, extension.type, extension.uri);
  RTC_NOTREACHED();
  return false;
}

bool RtpHeaderExtensionMap::RegisterByUri(int id, const std::string& uri) {
  for (const ExtensionInfo& extension : kExtensions)
    if (uri == extension.uri)
      return Register(id, extension.type, extension.uri);
  RTC_LOG(LS_WARNING) << "Unknown extension uri:'" << uri << "', id: " << id
                      << '.';
  return false;
}

size_t RtpHeaderExtensionMap::GetTotalLengthInBytes(
    rtc::ArrayView<const RtpExtensionSize> extensions) const {
  // Header size of the extension block, see RFC3550 Section 5.3.1
  static constexpr size_t kRtpOneByteHeaderLength = 4;
  // Header size of each individual extension, see RFC5285 Section 4.2
  static constexpr size_t kExtensionHeaderLength = 1;
  size_t values_size = 0;
  for (const RtpExtensionSize& extension : extensions) {
    if (IsRegistered(extension.type))
      values_size += extension.value_size + kExtensionHeaderLength;
  }
  if (values_size == 0)
    return 0;
  size_t size = kRtpOneByteHeaderLength + values_size;
  // Round up to the nearest size that is a multiple of 4.
  // Which is same as round down (size + 3).
  return size + 3 - (size + 3) % 4;
}

int32_t RtpHeaderExtensionMap::Deregister(RTPExtensionType type) {
  if (IsRegistered(type)) {
    uint8_t id = GetId(type);
    types_[id] = kInvalidType;
    ids_[type] = kInvalidId;
  }
  return 0;
}

bool RtpHeaderExtensionMap::Register(int id,
                                     RTPExtensionType type,
                                     const char* uri) {
  RTC_DCHECK_GT(type, kRtpExtensionNone);
  RTC_DCHECK_LT(type, kRtpExtensionNumberOfExtensions);

  if (id < kMinId || id > kMaxId) {
    RTC_LOG(LS_WARNING) << "Failed to register extension uri:'" << uri
                        << "' with invalid id:" << id << ".";
    return false;
  }

  if (GetType(id) == type) {  // Same type/id pair already registered.
    RTC_LOG(LS_VERBOSE) << "Reregistering extension uri:'" << uri
                        << "', id:" << id;
    return true;
  }

  if (GetType(id) != kInvalidType) {  // |id| used by another extension type.
    RTC_LOG(LS_WARNING) << "Failed to register extension uri:'" << uri
                        << "', id:" << id
                        << ". Id already in use by extension type "
                        << static_cast<int>(GetType(id));
    return false;
  }
  RTC_DCHECK(!IsRegistered(type));

  types_[id] = type;
  // There is a run-time check above id fits into uint8_t.
  ids_[type] = static_cast<uint8_t>(id);
  return true;
}

}  // namespace webrtc
