/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/field_trial_parser.h"

#include <algorithm>
#include <map>
#include <type_traits>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

int FindOrEnd(std::string str, size_t start, char delimiter) {
  size_t pos = str.find(delimiter, start);
  pos = (pos == std::string::npos) ? str.length() : pos;
  return static_cast<int>(pos);
}
}  // namespace

FieldTrialParameterInterface::FieldTrialParameterInterface(std::string key)
    : key_(key) {}
FieldTrialParameterInterface::~FieldTrialParameterInterface() {
  RTC_DCHECK(used_) << "Field trial parameter with key: '" << key_
                    << "' never used.";
}
std::string FieldTrialParameterInterface::Key() const {
  return key_;
}

void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> fields,
    std::string trial_string) {
  std::map<std::string, FieldTrialParameterInterface*> field_map;
  for (FieldTrialParameterInterface* field : fields) {
    field->MarkAsUsed();
    field_map[field->Key()] = field;
  }
  size_t i = 0;
  while (i < trial_string.length()) {
    int val_end = FindOrEnd(trial_string, i, ',');
    int colon_pos = FindOrEnd(trial_string, i, ':');
    int key_end = std::min(val_end, colon_pos);
    int val_begin = key_end + 1;
    std::string key = trial_string.substr(i, key_end - i);
    absl::optional<std::string> opt_value;
    if (val_end >= val_begin)
      opt_value = trial_string.substr(val_begin, val_end - val_begin);
    i = val_end + 1;
    auto field = field_map.find(key);
    if (field != field_map.end()) {
      if (!field->second->Parse(std::move(opt_value))) {
        RTC_LOG(LS_WARNING) << "Failed to read field with key: '" << key
                            << "' in trial: \"" << trial_string << "\"";
      }
    } else {
      RTC_LOG(LS_INFO) << "No field with key: '" << key
                       << "' (found in trial: \"" << trial_string << "\")";
    }
  }
}

template <>
absl::optional<bool> ParseTypedParameter<bool>(std::string str) {
  if (str == "true" || str == "1") {
    return true;
  } else if (str == "false" || str == "0") {
    return false;
  }
  return absl::nullopt;
}

template <>
absl::optional<double> ParseTypedParameter<double>(std::string str) {
  double value;
  char unit[2]{0, 0};
  if (sscanf(str.c_str(), "%lf%1s", &value, unit) >= 1) {
    if (unit[0] == '%')
      return value / 100;
    return value;
  } else {
    return absl::nullopt;
  }
}

template <>
absl::optional<int> ParseTypedParameter<int>(std::string str) {
  int value;
  if (sscanf(str.c_str(), "%i", &value) == 1) {
    return value;
  } else {
    return absl::nullopt;
  }
}

template <>
absl::optional<std::string> ParseTypedParameter<std::string>(std::string str) {
  return std::move(str);
}

FieldTrialFlag::FieldTrialFlag(std::string key) : FieldTrialFlag(key, false) {}

FieldTrialFlag::FieldTrialFlag(std::string key, bool default_value)
    : FieldTrialParameterInterface(key), value_(default_value) {}

bool FieldTrialFlag::Get() const {
  return value_;
}

webrtc::FieldTrialFlag::operator bool() const {
  return value_;
}

bool FieldTrialFlag::Parse(absl::optional<std::string> str_value) {
  // Only set the flag if there is no argument provided.
  if (str_value) {
    absl::optional<bool> opt_value = ParseTypedParameter<bool>(*str_value);
    if (!opt_value)
      return false;
    value_ = *opt_value;
  } else {
    value_ = true;
  }
  return true;
}

AbstractFieldTrialEnum::AbstractFieldTrialEnum(
    std::string key,
    int default_value,
    std::map<std::string, int> mapping)
    : FieldTrialParameterInterface(key),
      value_(default_value),
      enum_mapping_(mapping) {
  for (auto& key_val : enum_mapping_)
    valid_values_.insert(key_val.second);
}
AbstractFieldTrialEnum::AbstractFieldTrialEnum(const AbstractFieldTrialEnum&) =
    default;
AbstractFieldTrialEnum::~AbstractFieldTrialEnum() = default;

bool AbstractFieldTrialEnum::Parse(absl::optional<std::string> str_value) {
  if (str_value) {
    auto it = enum_mapping_.find(*str_value);
    if (it != enum_mapping_.end()) {
      value_ = it->second;
      return true;
    }
    absl::optional<int> value = ParseTypedParameter<int>(*str_value);
    if (value.has_value() &&
        (valid_values_.find(*value) != valid_values_.end())) {
      value_ = *value;
      return true;
    }
  }
  return false;
}

template class FieldTrialParameter<bool>;
template class FieldTrialParameter<double>;
template class FieldTrialParameter<int>;
template class FieldTrialParameter<std::string>;

template class FieldTrialOptional<double>;
template class FieldTrialOptional<int>;
template class FieldTrialOptional<bool>;
template class FieldTrialOptional<std::string>;

}  // namespace webrtc
