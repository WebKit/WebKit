/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsep_ice_candidate.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/jsep.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
// The sdpMLineIndex property is an unsigned short, a zero based index of the
// m-line associated with the candidate. This function ensures we consistently
// set the property to -1 for out-of-bounds values, to make candidate
// comparisons more robust.
int EnsureValidMLineIndex(int sdp_mline_index) {
  if (sdp_mline_index < 0 ||
      sdp_mline_index > std::numeric_limits<uint16_t>::max())
    return -1;
  return sdp_mline_index;
}
}  // namespace

IceCandidate::IceCandidate(absl::string_view sdp_mid,
                           int sdp_mline_index,
                           const Candidate& candidate)
    : sdp_mid_(sdp_mid),
      sdp_mline_index_(EnsureValidMLineIndex(sdp_mline_index)),
      candidate_(candidate) {}

void IceCandidateCollection::add(std::unique_ptr<IceCandidate> candidate) {
  candidates_.push_back(std::move(candidate));
}

void IceCandidateCollection::add(IceCandidate* candidate) {
  candidates_.push_back(absl::WrapUnique(candidate));
}

const IceCandidate* IceCandidateCollection::at(size_t index) const {
  return candidates_[index].get();
}

bool IceCandidateCollection::HasCandidate(const IceCandidate* candidate) const {
  const auto sdp_mid = candidate->sdp_mid();  // avoid string copy per entry.
  return absl::c_any_of(
      candidates_, [&](const std::unique_ptr<IceCandidate>& entry) {
        if (!entry->candidate().IsEquivalent(candidate->candidate())) {
          return false;
        }
        if (!sdp_mid.empty()) {
          // In this case, we ignore the `sdp_mline_index()` property.
          return sdp_mid == entry->sdp_mid();
        }
        RTC_DCHECK_NE(candidate->sdp_mline_index(), -1);
        return candidate->sdp_mline_index() == entry->sdp_mline_index();
      });
}

size_t JsepCandidateCollection::remove(const IceCandidate* candidate) {
  RTC_DCHECK(candidate);
  auto iter =
      absl::c_find_if(candidates_, [&](const std::unique_ptr<IceCandidate>& c) {
        return c->candidate().MatchesForRemoval(candidate->candidate());
      });
  if (iter != candidates_.end()) {
    candidates_.erase(iter);
    return 1u;
  }
  return 0u;
}

}  // namespace webrtc
