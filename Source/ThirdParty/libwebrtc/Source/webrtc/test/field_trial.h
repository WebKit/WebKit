/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FIELD_TRIAL_H_
#define TEST_FIELD_TRIAL_H_

#include <map>
#include <string>

namespace webrtc {
namespace test {

// Parses enabled field trials from a string config, such as the one passed
// to chrome's argument --force-fieldtrials and initializes webrtc::field_trial
// with such a config.
//  E.g.:
//    "WebRTC-experimentFoo/Enabled/WebRTC-experimentBar/Enabled100kbps/"
//    Assigns the process to group "Enabled" on WebRTCExperimentFoo trial
//    and to group "Enabled100kbps" on WebRTCExperimentBar.
//
//  E.g. invalid config:
//    "WebRTC-experiment1/Enabled"  (note missing / separator at the end).
//
// Note: This method crashes with an error message if an invalid config is
// passed to it. That can be used to find out if a binary is parsing the flags.
void ValidateFieldTrialsStringOrDie(const std::string& config);

// This class is used to override field-trial configs within specific tests.
// After this class goes out of scope previous field trials will be restored.
class ScopedFieldTrials {
 public:
  explicit ScopedFieldTrials(const std::string& config);
  ~ScopedFieldTrials();

 private:
  std::string current_field_trials_;
  const char* previous_field_trials_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FIELD_TRIAL_H_
