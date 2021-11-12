/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_MEDIA_PROTOCOL_NAMES_H_
#define PC_MEDIA_PROTOCOL_NAMES_H_

#include "absl/strings/string_view.h"

namespace cricket {

// Names or name prefixes of protocols as defined by SDP specifications.
extern const char kMediaProtocolRtpPrefix[];
extern const char kMediaProtocolSctp[];
extern const char kMediaProtocolDtlsSctp[];
extern const char kMediaProtocolUdpDtlsSctp[];
extern const char kMediaProtocolTcpDtlsSctp[];

bool IsDtlsSctp(absl::string_view protocol);
bool IsPlainSctp(absl::string_view protocol);

// Returns true if the given media section protocol indicates use of RTP.
bool IsRtpProtocol(absl::string_view protocol);
// Returns true if the given media section protocol indicates use of SCTP.
bool IsSctpProtocol(absl::string_view protocol);

}  // namespace cricket

#endif  // PC_MEDIA_PROTOCOL_NAMES_H_
