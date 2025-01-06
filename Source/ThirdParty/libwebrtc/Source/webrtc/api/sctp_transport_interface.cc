/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/sctp_transport_interface.h"

#include <optional>
#include <utility>

#include "api/dtls_transport_interface.h"
#include "api/scoped_refptr.h"

namespace webrtc {

SctpTransportInformation::SctpTransportInformation(SctpTransportState state)
    : state_(state) {}

SctpTransportInformation::SctpTransportInformation(
    SctpTransportState state,
    rtc::scoped_refptr<DtlsTransportInterface> dtls_transport,
    std::optional<double> max_message_size,
    std::optional<int> max_channels)
    : state_(state),
      dtls_transport_(std::move(dtls_transport)),
      max_message_size_(max_message_size),
      max_channels_(max_channels) {}

SctpTransportInformation::~SctpTransportInformation() {}

}  // namespace webrtc
