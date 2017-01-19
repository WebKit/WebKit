/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_CANDIDATEPAIRINTERFACE_H_
#define WEBRTC_P2P_BASE_CANDIDATEPAIRINTERFACE_H_

namespace cricket {

class Candidate;

class CandidatePairInterface {
 public:
  virtual ~CandidatePairInterface() {}

  virtual const Candidate& local_candidate() const = 0;
  virtual const Candidate& remote_candidate() const = 0;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_CANDIDATEPAIRINTERFACE_H_
