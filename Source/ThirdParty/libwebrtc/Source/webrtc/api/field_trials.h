/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FIELD_TRIALS_H_
#define API_FIELD_TRIALS_H_

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/field_trials_registry.h"
#include "api/field_trials_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_map.h"

namespace webrtc {

// The FieldTrials class is used to inject field trials into webrtc.
//
// Field trials allow webrtc clients (such as Chromium) to turn on feature code
// in binaries out in the field and gather information with that.
//
// They are designed to be easy to use with Chromium field trials and to speed
// up developers by reducing the need to wire up APIs to control whether a
// feature is on/off.
//
// The field trials are injected into objects that use them at creation time.
class FieldTrials : public FieldTrialsRegistry {
 public:
  // Creates field trials from a valid field trial string.
  // Returns nullptr if the string is invalid.
  // E.g., valid string:
  //   "WebRTC-ExperimentFoo/Enabled/WebRTC-ExperimentBar/Enabled100kbps/"
  //   Assigns to group "Enabled" on WebRTC-ExperimentFoo trial
  //   and to group "Enabled100kbps" on WebRTC-ExperimentBar.
  //
  // E.g., invalid string:
  //   "WebRTC-experiment1/Enabled"  (note missing / separator at the end).
  static absl_nullable std::unique_ptr<FieldTrials> Create(absl::string_view s);

  // Creates field trials from a string.
  // It is an error to call the constructor with an invalid field trial string.
  explicit FieldTrials(absl::string_view s);

  FieldTrials(const FieldTrials&);
  FieldTrials(FieldTrials&&);
  FieldTrials& operator=(const FieldTrials&);
  FieldTrials& operator=(FieldTrials&&);

  ~FieldTrials() override = default;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const FieldTrials& self);

  // Merges field trials from the `other` into this.
  //
  // If a key (trial) exists twice with conflicting values (groups), the value
  // in `other` takes precedence.
  void Merge(const FieldTrials& other);

  // Sets value (`group`) for an indvidual `trial`.
  // It is an error to call this function with an invalid `trial` or `group`.
  // Setting empty `group` is valid and removes the `trial`.
  void Set(absl::string_view trial, absl::string_view group);

  // Create a copy of this view.
  std::unique_ptr<FieldTrialsView> CreateCopy() const override {
    // We don't need to reset get_value_called_ on the returned copy
    // since it is a FieldTrialsView that has no mutable methods.
    return std::make_unique<FieldTrials>(*this);
  }

  void AssertGetValueNotCalled() const {
#if RTC_DCHECK_IS_ON
    RTC_DCHECK(!get_value_called_)
        << "FieldTrials are immutable once first Lookup has been performed";
#endif
  }

 private:
  explicit FieldTrials(flat_map<std::string, std::string> key_value_map)
      : key_value_map_(std::move(key_value_map)) {}

  std::string GetValue(absl::string_view key) const override;

#if RTC_DCHECK_IS_ON
  // Keep track of if GetValue() has been called.
  // This is used to enforce immutability by DCHECK:ing
  // that modification are performed once get_value_called_
  // is true.
  mutable std::atomic<bool> get_value_called_ = false;
#endif

  flat_map<std::string, std::string> key_value_map_;
};

template <typename Sink>
void AbslStringify(Sink& sink, const FieldTrials& self) {
  for (const auto& [trial, group] : self.key_value_map_) {
    sink.Append(trial);
    sink.Append("/");
    sink.Append(group);
    // Intentionally output a string that is not a valid field trial string.
    // Stringification is intended only for human readable logs, and is not
    // intended for reusing as `FieldTrials` construction parameter.
    sink.Append("//");
  }
}

}  // namespace webrtc

#endif  // API_FIELD_TRIALS_H_
