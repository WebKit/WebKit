/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
#define RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_

#include <stdint.h>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include "absl/types/optional.h"

// Field trial parser functionality. Provides funcitonality to parse field trial
// argument strings in key:value format. Each parameter is described using
// key:value, parameters are separated with a ,. Values can't include the comma
// character, since there's no quote facility. For most types, white space is
// ignored. Parameters are declared with a given type for which an
// implementation of ParseTypedParameter should be provided. The
// ParseTypedParameter implementation is given whatever is between the : and the
// ,. FieldTrialOptional will use nullopt if the key is provided without :.

// Example string: "my_optional,my_int:3,my_string:hello"

// For further description of usage and behavior, see the examples in the unit
// tests.

namespace webrtc {
class FieldTrialParameterInterface {
 public:
  virtual ~FieldTrialParameterInterface();

 protected:
  explicit FieldTrialParameterInterface(std::string key);
  friend void ParseFieldTrial(
      std::initializer_list<FieldTrialParameterInterface*> fields,
      std::string raw_string);
  virtual bool Parse(absl::optional<std::string> str_value) = 0;
  std::string Key() const;

 private:
  const std::string key_;
};

// ParseFieldTrial function parses the given string and fills the given fields
// with extrated values if available.
void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> fields,
    std::string raw_string);

// Specialize this in code file for custom types. Should return absl::nullopt if
// the given string cannot be properly parsed.
template <typename T>
absl::optional<T> ParseTypedParameter(std::string);

// This class uses the ParseTypedParameter function to implement a parameter
// implementation with an enforced default value.
template <typename T>
class FieldTrialParameter : public FieldTrialParameterInterface {
 public:
  FieldTrialParameter(std::string key, T default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  T Get() const { return value_; }
  operator T() const { return Get(); }

 protected:
  bool Parse(absl::optional<std::string> str_value) override {
    if (str_value) {
      absl::optional<T> value = ParseTypedParameter<T>(*str_value);
      if (value.has_value()) {
        value_ = value.value();
        return true;
      }
    }
    return false;
  }

 private:
  T value_;
};

class AbstractFieldTrialEnum : public FieldTrialParameterInterface {
 public:
  AbstractFieldTrialEnum(std::string key,
                         int default_value,
                         std::map<std::string, int> mapping);
  ~AbstractFieldTrialEnum() override;
  AbstractFieldTrialEnum(const AbstractFieldTrialEnum&);

 protected:
  bool Parse(absl::optional<std::string> str_value) override;

 protected:
  int value_;
  std::map<std::string, int> enum_mapping_;
  std::set<int> valid_values_;
};

// The FieldTrialEnum class can be used to quickly define a parser for a
// specific enum. It handles values provided as integers and as strings if a
// mapping is provided.
template <typename T>
class FieldTrialEnum : public AbstractFieldTrialEnum {
 public:
  FieldTrialEnum(std::string key,
                 T default_value,
                 std::map<std::string, T> mapping)
      : AbstractFieldTrialEnum(key,
                               static_cast<int>(default_value),
                               ToIntMap(mapping)) {}
  T Get() const { return static_cast<T>(value_); }
  operator T() const { return Get(); }

 private:
  static std::map<std::string, int> ToIntMap(std::map<std::string, T> mapping) {
    std::map<std::string, int> res;
    for (const auto& it : mapping)
      res[it.first] = static_cast<int>(it.second);
    return res;
  }
};

// This class uses the ParseTypedParameter function to implement an optional
// parameter implementation that can default to absl::nullopt.
template <typename T>
class FieldTrialOptional : public FieldTrialParameterInterface {
 public:
  explicit FieldTrialOptional(std::string key)
      : FieldTrialParameterInterface(key) {}
  FieldTrialOptional(std::string key, absl::optional<T> default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  absl::optional<T> Get() const { return value_; }

 protected:
  bool Parse(absl::optional<std::string> str_value) override {
    if (str_value) {
      absl::optional<T> value = ParseTypedParameter<T>(*str_value);
      if (!value.has_value())
        return false;
      value_ = value.value();
    } else {
      value_ = absl::nullopt;
    }
    return true;
  }

 private:
  absl::optional<T> value_;
};

// Equivalent to a FieldTrialParameter<bool> in the case that both key and value
// are present. If key is missing, evaluates to false. If key is present, but no
// explicit value is provided, the flag evaluates to true.
class FieldTrialFlag : public FieldTrialParameterInterface {
 public:
  explicit FieldTrialFlag(std::string key);
  FieldTrialFlag(std::string key, bool default_value);
  bool Get() const;

 protected:
  bool Parse(absl::optional<std::string> str_value) override;

 private:
  bool value_;
};

// Accepts true, false, else parsed with sscanf %i, true if != 0.
extern template class FieldTrialParameter<bool>;
// Interpreted using sscanf %lf.
extern template class FieldTrialParameter<double>;
// Interpreted using sscanf %i.
extern template class FieldTrialParameter<int>;
// Using the given value as is.
extern template class FieldTrialParameter<std::string>;

extern template class FieldTrialOptional<double>;
extern template class FieldTrialOptional<int>;
extern template class FieldTrialOptional<bool>;
extern template class FieldTrialOptional<std::string>;

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
