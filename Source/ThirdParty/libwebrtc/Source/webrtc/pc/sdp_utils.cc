/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdp_utils.h"

#include <memory>

#include "api/jsep.h"
#include "p2p/base/transport_info.h"
#include "pc/session_description.h"
#include "rtc_base/checks.h"

namespace webrtc {

std::unique_ptr<SessionDescriptionInterface> CloneSessionDescriptionAsType(
    const SessionDescriptionInterface* sdesc,
    SdpType type) {
  RTC_DCHECK(sdesc);
  return SessionDescriptionInterface::Create(
      type, sdesc->description() ? sdesc->description()->Clone() : nullptr,
      sdesc->session_id(), sdesc->session_version());
}

bool SdpContentsAll(SdpContentPredicate pred, const SessionDescription* desc) {
  RTC_DCHECK(desc);
  for (const auto& content : desc->contents()) {
    const auto* transport_info = desc->GetTransportInfoByName(content.mid());
    if (!pred(&content, transport_info)) {
      return false;
    }
  }
  return true;
}

bool SdpContentsNone(SdpContentPredicate pred, const SessionDescription* desc) {
  return SdpContentsAll(
      [pred](const ContentInfo* content_info,
             const TransportInfo* transport_info) {
        return !pred(content_info, transport_info);
      },
      desc);
}

void SdpContentsForEach(SdpContentMutator fn, SessionDescription* desc) {
  RTC_DCHECK(desc);
  for (auto& content : desc->contents()) {
    auto* transport_info = desc->GetTransportInfoByName(content.mid());
    fn(&content, transport_info);
  }
}

}  // namespace webrtc
