/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_STATS_RTC_STATS_H_
#define API_STATS_RTC_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/system/rtc_export_template.h"

namespace webrtc {

class RTCStatsMemberInterface;

// Abstract base class for RTCStats-derived dictionaries, see
// https://w3c.github.io/webrtc-stats/.
//
// All derived classes must have the following static variable defined:
//   static const char kType[];
// It is used as a unique class identifier and a string representation of the
// class type, see https://w3c.github.io/webrtc-stats/#rtcstatstype-str*.
// Use the `WEBRTC_RTCSTATS_IMPL` macro when implementing subclasses, see macro
// for details.
//
// Derived classes list their dictionary members, RTCStatsMember<T>, as public
// fields, allowing the following:
//
// RTCFooStats foo("fooId", Timestamp::Micros(GetCurrentTime()));
// foo.bar = 42;
// foo.baz = std::vector<std::string>();
// foo.baz->push_back("hello world");
// uint32_t x = *foo.bar;
//
// Pointers to all the members are available with `Members`, allowing iteration:
//
// for (const RTCStatsMemberInterface* member : foo.Members()) {
//   printf("%s = %s\n", member->name(), member->ValueToString().c_str());
// }
class RTC_EXPORT RTCStats {
 public:
  RTCStats(const std::string& id, Timestamp timestamp)
      : id_(id), timestamp_(timestamp) {}

  virtual ~RTCStats() {}

  virtual std::unique_ptr<RTCStats> copy() const = 0;

  const std::string& id() const { return id_; }
  // Time relative to the UNIX epoch (Jan 1, 1970, UTC), in microseconds.
  Timestamp timestamp() const { return timestamp_; }

  // Returns the static member variable `kType` of the implementing class.
  virtual const char* type() const = 0;
  // Returns a vector of pointers to all the `RTCStatsMemberInterface` members
  // of this class. This allows for iteration of members. For a given class,
  // `Members` always returns the same members in the same order.
  std::vector<const RTCStatsMemberInterface*> Members() const;
  // Checks if the two stats objects are of the same type and have the same
  // member values. Timestamps are not compared. These operators are exposed for
  // testing.
  bool operator==(const RTCStats& other) const;
  bool operator!=(const RTCStats& other) const;

  // Creates a JSON readable string representation of the stats
  // object, listing all of its members (names and values).
  std::string ToJson() const;

  // Downcasts the stats object to an `RTCStats` subclass `T`. DCHECKs that the
  // object is of type `T`.
  template <typename T>
  const T& cast_to() const {
    RTC_DCHECK_EQ(type(), T::kType);
    return static_cast<const T&>(*this);
  }

 protected:
  // Gets a vector of all members of this `RTCStats` object, including members
  // derived from parent classes. `additional_capacity` is how many more members
  // shall be reserved in the vector (so that subclasses can allocate a vector
  // with room for both parent and child members without it having to resize).
  virtual std::vector<const RTCStatsMemberInterface*>
  MembersOfThisObjectAndAncestors(size_t additional_capacity) const;

  std::string const id_;
  Timestamp timestamp_;
};

// All `RTCStats` classes should use these macros.
// `WEBRTC_RTCSTATS_DECL` is placed in a public section of the class definition.
// `WEBRTC_RTCSTATS_IMPL` is placed outside the class definition (in a .cc).
//
// These macros declare (in _DECL) and define (in _IMPL) the static `kType` and
// overrides methods as required by subclasses of `RTCStats`: `copy`, `type` and
// `MembersOfThisObjectAndAncestors`. The |...| argument is a list of addresses
// to each member defined in the implementing class. The list must have at least
// one member.
//
// (Since class names need to be known to implement these methods this cannot be
// part of the base `RTCStats`. While these methods could be implemented using
// templates, that would only work for immediate subclasses. Subclasses of
// subclasses also have to override these methods, resulting in boilerplate
// code. Using a macro avoids this and works for any `RTCStats` class, including
// grandchildren.)
//
// Sample usage:
//
// rtcfoostats.h:
//   class RTCFooStats : public RTCStats {
//    public:
//     WEBRTC_RTCSTATS_DECL();
//
//     RTCFooStats(const std::string& id, Timestamp timestamp);
//
//     RTCStatsMember<int32_t> foo;
//     RTCStatsMember<int32_t> bar;
//   };
//
// rtcfoostats.cc:
//   WEBRTC_RTCSTATS_IMPL(RTCFooStats, RTCStats, "foo-stats"
//       &foo,
//       &bar);
//
//   RTCFooStats::RTCFooStats(const std::string& id, Timestamp timestamp)
//       : RTCStats(id, timestamp),
//         foo("foo"),
//         bar("bar") {
//   }
//
#define WEBRTC_RTCSTATS_DECL()                                          \
 protected:                                                             \
  std::vector<const webrtc::RTCStatsMemberInterface*>                   \
  MembersOfThisObjectAndAncestors(size_t local_var_additional_capacity) \
      const override;                                                   \
                                                                        \
 public:                                                                \
  static const char kType[];                                            \
                                                                        \
  std::unique_ptr<webrtc::RTCStats> copy() const override;              \
  const char* type() const override

#define WEBRTC_RTCSTATS_IMPL(this_class, parent_class, type_str, ...)          \
  const char this_class::kType[] = type_str;                                   \
                                                                               \
  std::unique_ptr<webrtc::RTCStats> this_class::copy() const {                 \
    return std::make_unique<this_class>(*this);                                \
  }                                                                            \
                                                                               \
  const char* this_class::type() const {                                       \
    return this_class::kType;                                                  \
  }                                                                            \
                                                                               \
  std::vector<const webrtc::RTCStatsMemberInterface*>                          \
  this_class::MembersOfThisObjectAndAncestors(                                 \
      size_t local_var_additional_capacity) const {                            \
    const webrtc::RTCStatsMemberInterface* local_var_members[] = {             \
        __VA_ARGS__};                                                          \
    size_t local_var_members_count =                                           \
        sizeof(local_var_members) / sizeof(local_var_members[0]);              \
    std::vector<const webrtc::RTCStatsMemberInterface*>                        \
        local_var_members_vec = parent_class::MembersOfThisObjectAndAncestors( \
            local_var_members_count + local_var_additional_capacity);          \
    RTC_DCHECK_GE(                                                             \
        local_var_members_vec.capacity() - local_var_members_vec.size(),       \
        local_var_members_count + local_var_additional_capacity);              \
    local_var_members_vec.insert(local_var_members_vec.end(),                  \
                                 &local_var_members[0],                        \
                                 &local_var_members[local_var_members_count]); \
    return local_var_members_vec;                                              \
  }

// A version of WEBRTC_RTCSTATS_IMPL() where "..." is omitted, used to avoid a
// compile error on windows. This is used if the stats dictionary does not
// declare any members of its own (but perhaps its parent dictionary does).
#define WEBRTC_RTCSTATS_IMPL_NO_MEMBERS(this_class, parent_class, type_str) \
  const char this_class::kType[] = type_str;                                \
                                                                            \
  std::unique_ptr<webrtc::RTCStats> this_class::copy() const {              \
    return std::make_unique<this_class>(*this);                             \
  }                                                                         \
                                                                            \
  const char* this_class::type() const {                                    \
    return this_class::kType;                                               \
  }                                                                         \
                                                                            \
  std::vector<const webrtc::RTCStatsMemberInterface*>                       \
  this_class::MembersOfThisObjectAndAncestors(                              \
      size_t local_var_additional_capacity) const {                         \
    return parent_class::MembersOfThisObjectAndAncestors(0);                \
  }

// Interface for `RTCStats` members, which have a name and a value of a type
// defined in a subclass. Only the types listed in `Type` are supported, these
// are implemented by `RTCStatsMember<T>`. The value of a member may be
// undefined, the value can only be read if `is_defined`.
class RTCStatsMemberInterface {
 public:
  // Member value types.
  enum Type {
    kBool,    // bool
    kInt32,   // int32_t
    kUint32,  // uint32_t
    kInt64,   // int64_t
    kUint64,  // uint64_t
    kDouble,  // double
    kString,  // std::string

    kSequenceBool,    // std::vector<bool>
    kSequenceInt32,   // std::vector<int32_t>
    kSequenceUint32,  // std::vector<uint32_t>
    kSequenceInt64,   // std::vector<int64_t>
    kSequenceUint64,  // std::vector<uint64_t>
    kSequenceDouble,  // std::vector<double>
    kSequenceString,  // std::vector<std::string>

    kMapStringUint64,  // std::map<std::string, uint64_t>
    kMapStringDouble,  // std::map<std::string, double>
  };

  virtual ~RTCStatsMemberInterface() {}

  const char* name() const { return name_; }
  virtual Type type() const = 0;
  virtual bool is_sequence() const = 0;
  virtual bool is_string() const = 0;
  virtual bool is_defined() const = 0;
  // Type and value comparator. The names are not compared. These operators are
  // exposed for testing.
  bool operator==(const RTCStatsMemberInterface& other) const {
    return IsEqual(other);
  }
  bool operator!=(const RTCStatsMemberInterface& other) const {
    return !(*this == other);
  }
  virtual std::string ValueToString() const = 0;
  // This is the same as ValueToString except for kInt64 and kUint64 types,
  // where the value is represented as a double instead of as an integer.
  // Since JSON stores numbers as floating point numbers, very large integers
  // cannot be accurately represented, so we prefer to display them as doubles
  // instead.
  virtual std::string ValueToJson() const = 0;

  template <typename T>
  const T& cast_to() const {
    RTC_DCHECK_EQ(type(), T::StaticType());
    return static_cast<const T&>(*this);
  }

 protected:
  explicit RTCStatsMemberInterface(const char* name) : name_(name) {}

  virtual bool IsEqual(const RTCStatsMemberInterface& other) const = 0;

  const char* const name_;
};

// Template implementation of `RTCStatsMemberInterface`.
// The supported types are the ones described by
// `RTCStatsMemberInterface::Type`.
template <typename T>
class RTCStatsMember : public RTCStatsMemberInterface {
 public:
  explicit RTCStatsMember(const char* name)
      : RTCStatsMemberInterface(name), value_() {}
  RTCStatsMember(const char* name, const T& value)
      : RTCStatsMemberInterface(name), value_(value) {}
  RTCStatsMember(const char* name, T&& value)
      : RTCStatsMemberInterface(name), value_(std::move(value)) {}
  explicit RTCStatsMember(const RTCStatsMember<T>& other)
      : RTCStatsMemberInterface(other.name_), value_(other.value_) {}
  explicit RTCStatsMember(RTCStatsMember<T>&& other)
      : RTCStatsMemberInterface(other.name_), value_(std::move(other.value_)) {}

  static Type StaticType();
  Type type() const override { return StaticType(); }
  bool is_sequence() const override;
  bool is_string() const override;
  bool is_defined() const override { return value_.has_value(); }
  std::string ValueToString() const override;
  std::string ValueToJson() const override;

  template <typename U>
  inline T ValueOrDefault(U default_value) const {
    return value_.value_or(default_value);
  }

  // Assignment operators.
  T& operator=(const T& value) {
    value_ = value;
    return value_.value();
  }
  T& operator=(const T&& value) {
    value_ = std::move(value);
    return value_.value();
  }

  // Getter methods that look the same as absl::optional<T>. Please prefer these
  // in order to unblock replacing RTCStatsMember<T> with absl::optional<T> in
  // the future (https://crbug.com/webrtc/15164).
  bool has_value() const { return value_.has_value(); }
  const T& value() const { return value_.value(); }
  T& value() { return value_.value(); }
  T& operator*() {
    RTC_DCHECK(value_);
    return *value_;
  }
  const T& operator*() const {
    RTC_DCHECK(value_);
    return *value_;
  }
  T* operator->() {
    RTC_DCHECK(value_);
    return &(*value_);
  }
  const T* operator->() const {
    RTC_DCHECK(value_);
    return &(*value_);
  }

 protected:
  bool IsEqual(const RTCStatsMemberInterface& other) const override {
    if (type() != other.type())
      return false;
    const RTCStatsMember<T>& other_t =
        static_cast<const RTCStatsMember<T>&>(other);
    return value_ == other_t.value_;
  }

 private:
  absl::optional<T> value_;
};

#if !defined(WEBRTC_WEBKIT_BUILD)
#define WEBRTC_DECLARE_RTCSTATSMEMBER_AS_EXTERN(T)                          \
  extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)             \
      RTCStatsMember<T>
#else
#define WEBRTC_DECLARE_RTCSTATSMEMBER_AS_EXTERN(T)
#endif // WEBRTC_WEBKIT_BUILD

namespace rtc_stats_internal {

typedef std::map<std::string, uint64_t> MapStringUint64;
typedef std::map<std::string, double> MapStringDouble;

}  // namespace rtc_stats_internal

#define WEBRTC_DECLARE_RTCSTATSMEMBER(T)                                    \
  template <>                                                               \
  RTC_EXPORT RTCStatsMemberInterface::Type RTCStatsMember<T>::StaticType(); \
  template <>                                                               \
  RTC_EXPORT bool RTCStatsMember<T>::is_sequence() const;                   \
  template <>                                                               \
  RTC_EXPORT bool RTCStatsMember<T>::is_string() const;                     \
  template <>                                                               \
  RTC_EXPORT std::string RTCStatsMember<T>::ValueToString() const;          \
  template <>                                                               \
  RTC_EXPORT std::string RTCStatsMember<T>::ValueToJson() const;            \
  WEBRTC_DECLARE_RTCSTATSMEMBER_AS_EXTERN(T)

WEBRTC_DECLARE_RTCSTATSMEMBER(bool);
WEBRTC_DECLARE_RTCSTATSMEMBER(int32_t);
WEBRTC_DECLARE_RTCSTATSMEMBER(uint32_t);
WEBRTC_DECLARE_RTCSTATSMEMBER(int64_t);
WEBRTC_DECLARE_RTCSTATSMEMBER(uint64_t);
WEBRTC_DECLARE_RTCSTATSMEMBER(double);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::string);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<bool>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<int32_t>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<uint32_t>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<int64_t>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<uint64_t>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<double>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<std::string>);
WEBRTC_DECLARE_RTCSTATSMEMBER(rtc_stats_internal::MapStringUint64);
WEBRTC_DECLARE_RTCSTATSMEMBER(rtc_stats_internal::MapStringDouble);

}  // namespace webrtc

#endif  // API_STATS_RTC_STATS_H_
