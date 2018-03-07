/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains enums related to IPv4/IPv6 metrics.

#ifndef API_UMAMETRICS_H_
#define API_UMAMETRICS_H_

#include "rtc_base/refcount.h"

namespace webrtc {

// Used to specify which enum counter type we're incrementing in
// MetricsObserverInterface::IncrementEnumCounter.
enum PeerConnectionEnumCounterType {
  kEnumCounterAddressFamily,
  // For the next 2 counters, we track them separately based on the "first hop"
  // protocol used by the local candidate. "First hop" means the local candidate
  // type in the case of non-TURN candidates, and the protocol used to connect
  // to the TURN server in the case of TURN candidates.
  kEnumCounterIceCandidatePairTypeUdp,
  kEnumCounterIceCandidatePairTypeTcp,

  kEnumCounterAudioSrtpCipher,
  kEnumCounterAudioSslCipher,
  kEnumCounterVideoSrtpCipher,
  kEnumCounterVideoSslCipher,
  kEnumCounterDataSrtpCipher,
  kEnumCounterDataSslCipher,
  kEnumCounterDtlsHandshakeError,
  kEnumCounterIceRegathering,
  kEnumCounterIceRestart,
  kPeerConnectionEnumCounterMax
};

// Currently this contains information related to WebRTC network/transport
// information.

// The difference between PeerConnectionEnumCounter and
// PeerConnectionMetricsName is that the "EnumCounter" is only counting the
// occurrences of events, while "Name" has a value associated with it which is
// used to form a histogram.

// This enum is backed by Chromium's histograms.xml,
// chromium/src/tools/metrics/histograms/histograms.xml
// Existing values cannot be re-ordered and new enums must be added
// before kBoundary.
enum PeerConnectionAddressFamilyCounter {
  kPeerConnection_IPv4,
  kPeerConnection_IPv6,
  kBestConnections_IPv4,
  kBestConnections_IPv6,
  kPeerConnectionAddressFamilyCounter_Max,
};

// TODO(guoweis): Keep previous name here until all references are renamed.
#define kBoundary kPeerConnectionAddressFamilyCounter_Max

// TODO(guoweis): Keep previous name here until all references are renamed.
typedef PeerConnectionAddressFamilyCounter PeerConnectionUMAMetricsCounter;

// This enum defines types for UMA samples, which will have a range.
enum PeerConnectionMetricsName {
  kNetworkInterfaces_IPv4,  // Number of IPv4 interfaces.
  kNetworkInterfaces_IPv6,  // Number of IPv6 interfaces.
  kTimeToConnect,           // In milliseconds.
  kLocalCandidates_IPv4,    // Number of IPv4 local candidates.
  kLocalCandidates_IPv6,    // Number of IPv6 local candidates.
  kPeerConnectionMetricsName_Max
};

// TODO(guoweis): Keep previous name here until all references are renamed.
typedef PeerConnectionMetricsName PeerConnectionUMAMetricsName;

// The IceCandidatePairType has the format of
// <local_candidate_type>_<remote_candidate_type>. It is recorded based on the
// type of candidate pair used when the PeerConnection first goes to a completed
// state. When BUNDLE is enabled, only the first transport gets recorded.
enum IceCandidatePairType {
  // HostHost is deprecated. It was replaced with the set of types at the bottom
  // to report private or public host IP address.
  kIceCandidatePairHostHost,
  kIceCandidatePairHostSrflx,
  kIceCandidatePairHostRelay,
  kIceCandidatePairHostPrflx,
  kIceCandidatePairSrflxHost,
  kIceCandidatePairSrflxSrflx,
  kIceCandidatePairSrflxRelay,
  kIceCandidatePairSrflxPrflx,
  kIceCandidatePairRelayHost,
  kIceCandidatePairRelaySrflx,
  kIceCandidatePairRelayRelay,
  kIceCandidatePairRelayPrflx,
  kIceCandidatePairPrflxHost,
  kIceCandidatePairPrflxSrflx,
  kIceCandidatePairPrflxRelay,

  // The following 4 types tell whether local and remote hosts have private or
  // public IP addresses.
  kIceCandidatePairHostPrivateHostPrivate,
  kIceCandidatePairHostPrivateHostPublic,
  kIceCandidatePairHostPublicHostPrivate,
  kIceCandidatePairHostPublicHostPublic,
  kIceCandidatePairMax
};

class MetricsObserverInterface : public rtc::RefCountInterface {
 public:
  // |type| is the type of the enum counter to be incremented. |counter|
  // is the particular counter in that type. |counter_max| is the next sequence
  // number after the highest counter.
  virtual void IncrementEnumCounter(PeerConnectionEnumCounterType,
                                    int,
                                    int) {}

  // This is used to handle sparse counters like SSL cipher suites.
  // TODO(guoweis): Remove the implementation once the dependency's interface
  // definition is updated.
  virtual void IncrementSparseEnumCounter(PeerConnectionEnumCounterType type,
                                          int counter);

  virtual void AddHistogramSample(PeerConnectionMetricsName type,
                                  int value) = 0;
};

typedef MetricsObserverInterface UMAObserver;

}  // namespace webrtc

#endif  // API_UMAMETRICS_H_
