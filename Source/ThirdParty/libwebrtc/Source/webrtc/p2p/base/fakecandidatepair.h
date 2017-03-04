/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_FAKECANDIDATEPAIR_H_
#define WEBRTC_P2P_BASE_FAKECANDIDATEPAIR_H_

#include <memory>

#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/candidatepairinterface.h"

namespace cricket {

// Fake candidate pair class, which can be passed to BaseChannel for testing
// purposes.
class FakeCandidatePair : public CandidatePairInterface {
 public:
  FakeCandidatePair(const Candidate& local_candidate,
                    const Candidate& remote_candidate)
      : local_candidate_(local_candidate),
        remote_candidate_(remote_candidate) {}
  const Candidate& local_candidate() const override { return local_candidate_; }
  const Candidate& remote_candidate() const override {
    return remote_candidate_;
  }

  static std::unique_ptr<FakeCandidatePair> Create(
      const rtc::SocketAddress& local_address,
      int16_t local_network_id,
      const rtc::SocketAddress& remote_address,
      int16_t remote_network_id) {
    Candidate local_candidate(0, "udp", local_address, 0u, "", "", "local", 0,
                              "foundation", local_network_id, 0);
    Candidate remote_candidate(0, "udp", remote_address, 0u, "", "", "local", 0,
                               "foundation", remote_network_id, 0);
    return std::unique_ptr<FakeCandidatePair>(
        new FakeCandidatePair(local_candidate, remote_candidate));
  }

 private:
  Candidate local_candidate_;
  Candidate remote_candidate_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_FAKECANDIDATEPAIR_H_
