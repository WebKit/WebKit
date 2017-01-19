/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>
#include "webrtc/p2p/base/common.h"
#include "webrtc/p2p/base/transportchannel.h"

namespace cricket {

std::string TransportChannel::ToString() const {
  const char RECEIVING_ABBREV[2] = { '_', 'R' };
  const char WRITABLE_ABBREV[2] = { '_', 'W' };
  std::stringstream ss;
  ss << "Channel[" << transport_name_ << "|" << component_ << "|"
     << RECEIVING_ABBREV[receiving_] << WRITABLE_ABBREV[writable_] << "]";
  return ss.str();
}

void TransportChannel::set_receiving(bool receiving) {
  if (receiving_ == receiving) {
    return;
  }
  receiving_ = receiving;
  SignalReceivingState(this);
}

void TransportChannel::set_writable(bool writable) {
  if (writable_ == writable) {
    return;
  }
  LOG_J(LS_VERBOSE, this) << "set_writable from:" << writable_ << " to "
                          << writable;
  writable_ = writable;
  if (writable_) {
    SignalReadyToSend(this);
  }
  SignalWritableState(this);
}

void TransportChannel::set_dtls_state(DtlsTransportState state) {
  if (dtls_state_ == state) {
    return;
  }
  LOG_J(LS_VERBOSE, this) << "set_dtls_state from:" << dtls_state_ << " to "
                          << state;
  dtls_state_ = state;
  SignalDtlsState(this, state);
}

bool TransportChannel::SetSrtpCryptoSuites(const std::vector<int>& ciphers) {
  return false;
}

// TODO(guoweis): Remove this function once everything is moved away.
bool TransportChannel::SetSrtpCiphers(const std::vector<std::string>& ciphers) {
  std::vector<int> crypto_suites;
  for (const auto cipher : ciphers) {
    crypto_suites.push_back(rtc::SrtpCryptoSuiteFromName(cipher));
  }
  return SetSrtpCryptoSuites(crypto_suites);
}

}  // namespace cricket
