/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_

#include <array>
#include <span>

#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_parameters.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/checks.h"

namespace webrtc {

class RtpHeaderExtensionMap {
 public:
  static constexpr RTPExtensionType kInvalidType = kRtpExtensionNone;
  static constexpr RtpHeaderExtensionId kInvalidId =
      RtpHeaderExtensionId::NotSet();

  RtpHeaderExtensionMap();
  explicit RtpHeaderExtensionMap(bool extmap_allow_mixed);
  explicit RtpHeaderExtensionMap(std::span<const RtpExtension> extensions);

  void Reset(std::span<const RtpExtension> extensions);

  template <typename Extension>
  bool Register(RtpHeaderExtensionId id) {
    return Register(id, Extension::kId, Extension::Uri());
  }
  // Backwards compatibility overloads.
  // TODO: bugs.webrtc.org/514817938 - Remove when downstream is updated.
  template <typename Extension>
  [[deprecated]] ABSL_REFACTOR_INLINE bool Register(int id) {
    return Register<Extension>(RtpHeaderExtensionId(id));
  }
  bool RegisterByType(RtpHeaderExtensionId id, RTPExtensionType type);
  [[deprecated]] ABSL_REFACTOR_INLINE bool RegisterByType(
      int id,
      RTPExtensionType type) {
    return RegisterByType(RtpHeaderExtensionId(id), type);
  }
  bool RegisterByUri(RtpHeaderExtensionId id, absl::string_view uri);
  [[deprecated]] ABSL_REFACTOR_INLINE bool RegisterByUri(
      int id,
      absl::string_view uri) {
    return RegisterByUri(RtpHeaderExtensionId(id), uri);
  }

  bool IsRegistered(RTPExtensionType type) const {
    return GetId(type) != kInvalidId;
  }
  // Return kInvalidType if not found.
  RTPExtensionType GetType(RtpHeaderExtensionId id) const;
  // TODO: bugs.webrtc.org/514817938 - Remove when downstream is updated.
  [[deprecated]] ABSL_REFACTOR_INLINE RTPExtensionType GetType(int id) const {
    return GetType(RtpHeaderExtensionId(id));
  }
  // Return kInvalidId if not found.
  RtpHeaderExtensionId GetId(RTPExtensionType type) const {
    RTC_DCHECK_GT(type, kRtpExtensionNone);
    RTC_DCHECK_LT(type, kRtpExtensionNumberOfExtensions);
    return ids_[type];
  }

  void Deregister(absl::string_view uri);

  // Corresponds to the SDP attribute extmap-allow-mixed, see RFC8285.
  // Set to true if it's allowed to mix one- and two-byte RTP header extensions
  // in the same stream.
  bool ExtmapAllowMixed() const { return extmap_allow_mixed_; }
  void SetExtmapAllowMixed(bool extmap_allow_mixed) {
    extmap_allow_mixed_ = extmap_allow_mixed;
  }

 private:
  bool Register(RtpHeaderExtensionId id,
                RTPExtensionType type,
                absl::string_view uri);

  std::array<RtpHeaderExtensionId, kRtpExtensionNumberOfExtensions> ids_;
  bool extmap_allow_mixed_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_
