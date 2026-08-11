/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/field_trials.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/field_trials_registry.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_map.h"

namespace webrtc {
namespace {

absl::string_view NextKeyOrValue(absl::string_view& s) {
  absl::string_view::size_type separator_pos = s.find('/');
  if (separator_pos == absl::string_view::npos) {
    // Missing separator '/' after field trial key or value.
    return "";
  }
  absl::string_view result = s.substr(0, separator_pos);
  s.remove_prefix(separator_pos + 1);
  return result;
}

bool Parse(absl::string_view s,
           flat_map<std::string, std::string>& key_value_map) {
  while (!s.empty()) {
    absl::string_view key = NextKeyOrValue(s);
    absl::string_view value = NextKeyOrValue(s);
    if (key.empty() || value.empty()) {
      return false;
    }

    auto it = key_value_map.emplace(key, value).first;
    if (it->second != value) {
      // Duplicate trials with different values is not fine.
      return false;
    }
  }
  return true;
}

}  // namespace

absl_nullable std::unique_ptr<FieldTrials> FieldTrials::Create(
    absl::string_view s) {
  flat_map<std::string, std::string> key_value_map;
  if (!Parse(s, key_value_map)) {
    return nullptr;
  }
  // Using `new` to access a private constructor.
  return absl::WrapUnique(new FieldTrials(std::move(key_value_map)));
}

FieldTrials::FieldTrials(absl::string_view s) {
  RTC_CHECK(Parse(s, key_value_map_));
}

FieldTrials::FieldTrials(const FieldTrials& other)
    : FieldTrialsRegistry(other) {
  key_value_map_ = other.key_value_map_;
}

FieldTrials::FieldTrials(FieldTrials&& other) : FieldTrialsRegistry(other) {
  key_value_map_ = std::move(other.key_value_map_);
}

FieldTrials& FieldTrials::operator=(const FieldTrials& other) {
  if (this != &other) {
    AssertGetValueNotCalled();
    FieldTrialsRegistry::operator=(other);
    key_value_map_ = other.key_value_map_;
  }
  return *this;
}

FieldTrials& FieldTrials::operator=(FieldTrials&& other) {
  if (this != &other) {
    AssertGetValueNotCalled();
    FieldTrialsRegistry::operator=(other);
    key_value_map_ = std::move(other.key_value_map_);
  }
  return *this;
}

void FieldTrials::Merge(const FieldTrials& other) {
  AssertGetValueNotCalled();
  for (const auto& [trial, group] : other.key_value_map_) {
    key_value_map_.insert_or_assign(trial, group);
  }
}

void FieldTrials::Set(absl::string_view trial, absl::string_view group) {
  AssertGetValueNotCalled();
  RTC_CHECK(!trial.empty());
  RTC_CHECK_EQ(trial.find('/'), absl::string_view::npos);
  RTC_CHECK_EQ(group.find('/'), absl::string_view::npos);
  if (group.empty()) {
    key_value_map_.erase(trial);
  } else {
    key_value_map_.insert_or_assign(trial, group);
  }
}

std::string FieldTrials::GetValue(absl::string_view key) const {
#if RTC_DCHECK_IS_ON
  get_value_called_ = true;
#endif
  auto it = key_value_map_.find(key);
  if (it != key_value_map_.end()) {
    return it->second;
  }

  return "";
}

}  // namespace webrtc
