/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FAKEMETRICSOBSERVER_H_
#define API_FAKEMETRICSOBSERVER_H_

#include <map>
#include <string>
#include <vector>

#include "api/peerconnectioninterface.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class FakeMetricsObserver : public MetricsObserverInterface {
 public:
  FakeMetricsObserver();
  void Reset();

  void IncrementEnumCounter(PeerConnectionEnumCounterType,
                            int counter,
                            int counter_max) override;
  void AddHistogramSample(PeerConnectionMetricsName type,
                          int value) override;

  // Accessors to be used by the tests.
  int GetEnumCounter(PeerConnectionEnumCounterType type, int counter) const;
  int GetHistogramSample(PeerConnectionMetricsName type) const;

 protected:
  ~FakeMetricsObserver() {}

 private:
  rtc::ThreadChecker thread_checker_;
  // The vector contains maps for each counter type. In the map, it's a mapping
  // from individual counter to its count, such that it's memory efficient when
  // comes to sparse enum types, like the SSL ciphers in the IANA registry.
  std::vector<std::map<int, int>> counters_;
  int histogram_samples_[kPeerConnectionMetricsName_Max];
};

}  // namespace webrtc

#endif  // API_FAKEMETRICSOBSERVER_H_
