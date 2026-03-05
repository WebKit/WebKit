/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contain functions for parsing and serializing SDP messages.
// Related RFC/draft including:
// * RFC 4566 - SDP
// * RFC 5245 - ICE
// * RFC 3388 - Grouping of Media Lines in SDP
// * RFC 4568 - SDP Security Descriptions for Media Streams
// * draft-lennox-mmusic-sdp-source-selection-02 -
//   Mechanisms for Media Source Selection in SDP

#ifndef API_WEBRTC_SDP_H_
#define API_WEBRTC_SDP_H_

#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/jsep.h"

namespace webrtc {
// Serializes the passed in SessionDescriptionInterface.
// Serialize SessionDescription including candidates if
// SessionDescriptionInterface has candidates.
// jdesc - The SessionDescriptionInterface object to be serialized.
// return - SDP string serialized from the arguments.
std::string SdpSerialize(const SessionDescriptionInterface& jdesc);

// Deserializes the `sdp` to construct a SessionDescriptionInterface object.
// sdp_type - The type of session description object that should be constructed.
// sdp - The SDP string to be Deserialized.
// error - Optional detail error information when parsing fails.
// return - A new session description object if successful, otherwise nullptr.
absl_nullable std::unique_ptr<SessionDescriptionInterface> SdpDeserialize(
    SdpType sdp_type,
    absl::string_view sdp,
    SdpParseError* absl_nullable error = nullptr);

}  // namespace webrtc

#endif  // API_WEBRTC_SDP_H_
