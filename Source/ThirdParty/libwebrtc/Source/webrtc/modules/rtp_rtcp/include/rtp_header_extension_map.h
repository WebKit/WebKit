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

#include <string>

#include "api/array_view.h"
#include "api/rtpparameters.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/basictypes.h"
#include "rtc_base/checks.h"

namespace webrtc {

struct RtpExtensionSize {
  RTPExtensionType type;
  uint8_t value_size;
};

class RtpHeaderExtensionMap {
 public:
  static constexpr RTPExtensionType kInvalidType = kRtpExtensionNone;
  static constexpr int kInvalidId = 0;

  RtpHeaderExtensionMap();
  explicit RtpHeaderExtensionMap(rtc::ArrayView<const RtpExtension> extensions);

  template <typename Extension>
  bool Register(int id) {
    return Register(id, Extension::kId, Extension::kUri);
  }
  bool RegisterByType(int id, RTPExtensionType type);
  bool RegisterByUri(int id, const std::string& uri);

  bool IsRegistered(RTPExtensionType type) const {
    return GetId(type) != kInvalidId;
  }
  // Return kInvalidType if not found.
  RTPExtensionType GetType(int id) const {
    RTC_DCHECK_GE(id, kMinId);
    RTC_DCHECK_LE(id, kMaxId);
    return types_[id];
  }
  // Return kInvalidId if not found.
  uint8_t GetId(RTPExtensionType type) const {
    RTC_DCHECK_GT(type, kRtpExtensionNone);
    RTC_DCHECK_LT(type, kRtpExtensionNumberOfExtensions);
    return ids_[type];
  }

  size_t GetTotalLengthInBytes(
      rtc::ArrayView<const RtpExtensionSize> extensions) const;

  // TODO(danilchap): Remove use of the functions below.
  int32_t Register(RTPExtensionType type, int id) {
    return RegisterByType(id, type) ? 0 : -1;
  }
  int32_t Deregister(RTPExtensionType type);

 private:
  static constexpr int kMinId = 1;
  static constexpr int kMaxId = 14;
  bool Register(int id, RTPExtensionType type, const char* uri);

  RTPExtensionType types_[kMaxId + 1];
  uint8_t ids_[kRtpExtensionNumberOfExtensions];
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_
