/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_PAYLOAD_TYPE_H_
#define CALL_PAYLOAD_TYPE_H_


#include "absl/strings/string_view.h"
#include "api/payload_type.h"
#include "api/rtc_error.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_parameters.h"
#include "media/base/codec.h"

namespace webrtc {

class PayloadTypePicker;

class PayloadTypeSuggester {
 public:
  virtual ~PayloadTypeSuggester() = default;

  // Suggest a payload type for a given codec on a given media section.
  // Media section is indicated by MID.
  // The function will either return a PT already in use on the connection
  // or a newly suggested one.
  virtual RTCErrorOr<PayloadType> SuggestPayloadType(
      absl::string_view mid,
      const Codec& codec,
      bool pick_from_top_of_range = false) = 0;
  // Register a payload type as mapped to a specific codec for this MID
  // at this time.
  virtual RTCError AddLocalMapping(absl::string_view mid,
                                   PayloadType payload_type,
                                   const Codec& codec) = 0;

  // Suggest an ID for a given RTP header extension on a given media section.
  // The function will either return an ID already in use on the connection
  // or a newly suggested one.
  virtual RTCErrorOr<RtpHeaderExtensionId> SuggestRtpHeaderExtensionId(
      absl::string_view mid,
      const RtpExtension& extension,
      RtpTransceiverIdDomain id_domain) = 0;
  // Register an RTP header extension ID as mapped to a specific extension
  // for this MID at this time.
  [[nodiscard]] virtual RTCError AddRtpHeaderExtensionMapping(
      absl::string_view mid,
      const RtpExtension& extension,
      bool local) = 0;
};

}  // namespace webrtc

#endif  // CALL_PAYLOAD_TYPE_H_
