/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_H_

#include <string>

#include "webrtc/base/array_view.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/checks.h"
#include "webrtc/config.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class RtpHeaderExtensionMap {
 public:
  static constexpr RTPExtensionType kInvalidType = kRtpExtensionNone;
  static constexpr uint8_t kInvalidId = 0;

  RtpHeaderExtensionMap();
  explicit RtpHeaderExtensionMap(rtc::ArrayView<const RtpExtension> extensions);

  template <typename Extension>
  bool Register(uint8_t id) {
    return Register(id, Extension::kId, Extension::kValueSizeBytes,
                    Extension::kUri);
  }
  bool RegisterByType(uint8_t id, RTPExtensionType type);
  bool RegisterByUri(uint8_t id, const std::string& uri);

  bool IsRegistered(RTPExtensionType type) const {
    return GetId(type) != kInvalidId;
  }
  // Return kInvalidType if not found.
  RTPExtensionType GetType(uint8_t id) const {
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

  size_t GetTotalLengthInBytes() const;

  // TODO(danilchap): Remove use of the functions below.
  int32_t Register(RTPExtensionType type, uint8_t id) {
    return RegisterByType(id, type) ? 0 : -1;
  }
  int32_t Deregister(RTPExtensionType type);

 private:
  static constexpr uint8_t kMinId = 1;
  static constexpr uint8_t kMaxId = 14;
  bool Register(uint8_t id,
                RTPExtensionType type,
                size_t value_size,
                const char* uri);

  size_t total_values_size_bytes_ = 0;
  RTPExtensionType types_[kMaxId + 1];
  uint8_t ids_[kRtpExtensionNumberOfExtensions];
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_H_

