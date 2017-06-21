/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_ORTC_SESSIONDESCRIPTION_H_
#define WEBRTC_API_ORTC_SESSIONDESCRIPTION_H_

#include <string>
#include <utility>

namespace webrtc {

// A structured representation of an SDP session description.
class SessionDescription {
 public:
  SessionDescription(int64_t session_id, std::string session_version)
      : session_id_(session_id), session_version_(std::move(session_version)) {}

  // https://tools.ietf.org/html/rfc4566#section-5.2
  // o=<username> <sess-id> <sess-version> <nettype> <addrtype>
  //   <unicast-address>
  // session_id_ is the "sess-id" field.
  // session_version_ is the "sess-version" field.
  int64_t session_id() { return session_id_; }
  void set_session_id(int64_t session_id) { session_id_ = session_id; }

  const std::string& session_version() const { return session_version_; }
  void set_session_version(std::string session_version) {
    session_version_ = std::move(session_version);
  }

 private:
  int64_t session_id_;
  std::string session_version_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_ORTC_SESSIONDESCRIPTION_H_
