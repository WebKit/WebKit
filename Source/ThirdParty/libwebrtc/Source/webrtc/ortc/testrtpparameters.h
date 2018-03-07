/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ORTC_TESTRTPPARAMETERS_H_
#define ORTC_TESTRTPPARAMETERS_H_

#include "api/ortc/rtptransportinterface.h"
#include "api/rtpparameters.h"

namespace webrtc {

// Helper methods to create RtpParameters to use for sending/receiving.
//
// "MakeMinimal" methods contain the minimal necessary information for an
// RtpSender or RtpReceiver to function. The "MakeFull" methods are the
// opposite, and include all features that would normally be offered by a
// PeerConnection, and in some cases additional ones.
//
// These methods are intended to be used for end-to-end testing (such as in
// ortcfactory_integrationtest.cc), or unit testing that doesn't care about the
// specific contents of the parameters. Tests should NOT assume that these
// methods will not change; tests that are testing that a specific value in the
// parameters is applied properly should construct the parameters in the test
// itself.

inline RtpTransportParameters MakeRtcpMuxParameters() {
  RtpTransportParameters parameters;
  parameters.rtcp.mux = true;
  return parameters;
}

RtpParameters MakeMinimalOpusParameters();
RtpParameters MakeMinimalIsacParameters();
RtpParameters MakeMinimalOpusParametersWithSsrc(uint32_t ssrc);
RtpParameters MakeMinimalIsacParametersWithSsrc(uint32_t ssrc);

RtpParameters MakeMinimalVp8Parameters();
RtpParameters MakeMinimalVp9Parameters();
RtpParameters MakeMinimalVp8ParametersWithSsrc(uint32_t ssrc);
RtpParameters MakeMinimalVp9ParametersWithSsrc(uint32_t ssrc);

// Will create an encoding with no SSRC (meaning "match first SSRC seen" for a
// receiver, or "pick one automatically" for a sender).
RtpParameters MakeMinimalOpusParametersWithNoSsrc();
RtpParameters MakeMinimalIsacParametersWithNoSsrc();
RtpParameters MakeMinimalVp8ParametersWithNoSsrc();
RtpParameters MakeMinimalVp9ParametersWithNoSsrc();

// Make audio parameters with all the available properties configured and
// features used, and with multiple codecs offered. Obtained by taking a
// snapshot of a default PeerConnection offer (and adding other things, like
// bitrate limit).
RtpParameters MakeFullOpusParameters();
RtpParameters MakeFullIsacParameters();

// Make video parameters with all the available properties configured and
// features used, and with multiple codecs offered. Obtained by taking a
// snapshot of a default PeerConnection offer (and adding other things, like
// bitrate limit).
RtpParameters MakeFullVp8Parameters();
RtpParameters MakeFullVp9Parameters();

}  // namespace webrtc

#endif  // ORTC_TESTRTPPARAMETERS_H_
