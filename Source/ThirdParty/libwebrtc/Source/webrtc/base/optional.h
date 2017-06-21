/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_OPTIONAL_H_
#define WEBRTC_BASE_OPTIONAL_H_

#include <algorithm>
#include <memory>
#include <utility>

#ifdef UNIT_TEST
#include <iomanip>
#include <ostream>
#endif  // UNIT_TEST

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/sanitizer.h"

namespace rtc {

namespace optional_internal {

#if RTC_HAS_ASAN

// This is a non-inlined function. The optimizer can't see inside it.  It
// prevents the compiler from generating optimized code that reads value_ even
// if it is unset. Although safe, this causes memory sanitizers to complain.
void* FunctionThatDoesNothingImpl(void*);

template <typename T>
inline T* FunctionThatDoesNothing(T* x) {
  return reinterpret_cast<T*>(
      FunctionThatDoesNothingImpl(reinterpret_cast<void*>(x)));
}

#else

template <typename T>
inline T* FunctionThatDoesNothing(T* x) { return x; }

#endif

}  // namespace optional_internal

// Simple std::optional-wannabe. It either contains a T or not.
//
// A moved-from Optional<T> may only be destroyed, and assigned to if T allows
// being assigned to after having been moved from. Specifically, you may not
// assume that it just doesn't contain a value anymore.
//
// Examples of good places to use Optional:
//
// - As a class or struct member, when the member doesn't always have a value:
//     struct Prisoner {
//       std::string name;
//       Optional<int> cell_number;  // Empty if not currently incarcerated.
//     };
//
// - As a return value for functions that may fail to return a value on all
//   allowed inputs. For example, a function that searches an array might
//   return an Optional<size_t> (the index where it found the element, or
//   nothing if it didn't find it); and a function that parses numbers might
//   return Optional<double> (the parsed number, or nothing if parsing failed).
//
// Examples of bad places to use Optional:
//
// - As a return value for functions that may fail because of disallowed
//   inputs. For example, a string length function should not return
//   Optional<size_t> so that it can return nothing in case the caller passed
//   it a null pointer; the function should probably use RTC_[D]CHECK instead,
//   and return plain size_t.
//
// - As a return value for functions that may fail to return a value on all
//   allowed inputs, but need to tell the caller what went wrong. Returning
//   Optional<double> when parsing a single number as in the example above
//   might make sense, but any larger parse job is probably going to need to
//   tell the caller what the problem was, not just that there was one.
//
// - As a non-mutable function argument. When you want to pass a value of a
//   type T that can fail to be there, const T* is almost always both fastest
//   and cleanest. (If you're *sure* that the the caller will always already
//   have an Optional<T>, const Optional<T>& is slightly faster than const T*,
//   but this is a micro-optimization. In general, stick to const T*.)
//
// TODO(kwiberg): Get rid of this class when the standard library has
// std::optional (and we're allowed to use it).
template <typename T>
class Optional final {
 public:
  // Construct an empty Optional.
  Optional() : has_value_(false), empty_('\0') {
    PoisonValue();
  }

  // Construct an Optional that contains a value.
  explicit Optional(const T& value) : has_value_(true) {
    new (&value_) T(value);
  }
  explicit Optional(T&& value) : has_value_(true) {
    new (&value_) T(std::move(value));
  }

  // Copy constructor: copies the value from m if it has one.
  Optional(const Optional& m) : has_value_(m.has_value_) {
    if (has_value_)
      new (&value_) T(m.value_);
    else
      PoisonValue();
  }

  // Move constructor: if m has a value, moves the value from m, leaving m
  // still in a state where it has a value, but a moved-from one (the
  // properties of which depends on T; the only general guarantee is that we
  // can destroy m).
  Optional(Optional&& m) : has_value_(m.has_value_) {
    if (has_value_)
      new (&value_) T(std::move(m.value_));
    else
      PoisonValue();
  }

  ~Optional() {
    if (has_value_)
      value_.~T();
    else
      UnpoisonValue();
  }

  // Copy assignment. Uses T's copy assignment if both sides have a value, T's
  // copy constructor if only the right-hand side has a value.
  Optional& operator=(const Optional& m) {
    if (m.has_value_) {
      if (has_value_) {
        value_ = m.value_;  // T's copy assignment.
      } else {
        UnpoisonValue();
        new (&value_) T(m.value_);  // T's copy constructor.
        has_value_ = true;
      }
    } else {
      reset();
    }
    return *this;
  }

  // Move assignment. Uses T's move assignment if both sides have a value, T's
  // move constructor if only the right-hand side has a value. The state of m
  // after it's been moved from is as for the move constructor.
  Optional& operator=(Optional&& m) {
    if (m.has_value_) {
      if (has_value_) {
        value_ = std::move(m.value_);  // T's move assignment.
      } else {
        UnpoisonValue();
        new (&value_) T(std::move(m.value_));  // T's move constructor.
        has_value_ = true;
      }
    } else {
      reset();
    }
    return *this;
  }

  // Swap the values if both m1 and m2 have values; move the value if only one
  // of them has one.
  friend void swap(Optional& m1, Optional& m2) {
    if (m1.has_value_) {
      if (m2.has_value_) {
        // Both have values: swap.
        using std::swap;
        swap(m1.value_, m2.value_);
      } else {
        // Only m1 has a value: move it to m2.
        m2.UnpoisonValue();
        new (&m2.value_) T(std::move(m1.value_));
        m1.value_.~T();  // Destroy the moved-from value.
        m1.has_value_ = false;
        m2.has_value_ = true;
        m1.PoisonValue();
      }
    } else if (m2.has_value_) {
      // Only m2 has a value: move it to m1.
      m1.UnpoisonValue();
      new (&m1.value_) T(std::move(m2.value_));
      m2.value_.~T();  // Destroy the moved-from value.
      m1.has_value_ = true;
      m2.has_value_ = false;
      m2.PoisonValue();
    }
  }

  // Destroy any contained value. Has no effect if we have no value.
  void reset() {
    if (!has_value_)
      return;
    value_.~T();
    has_value_ = false;
    PoisonValue();
  }

  template <class... Args>
  void emplace(Args&&... args) {
    if (has_value_)
      value_.~T();
    else
      UnpoisonValue();
    new (&value_) T(std::forward<Args>(args)...);
    has_value_ = true;
  }

  // Conversion to bool to test if we have a value.
  explicit operator bool() const { return has_value_; }
  bool has_value() const { return has_value_; }

  // Dereferencing. Only allowed if we have a value.
  const T* operator->() const {
    RTC_DCHECK(has_value_);
    return &value_;
  }
  T* operator->() {
    RTC_DCHECK(has_value_);
    return &value_;
  }
  const T& operator*() const {
    RTC_DCHECK(has_value_);
    return value_;
  }
  T& operator*() {
    RTC_DCHECK(has_value_);
    return value_;
  }
  const T& value() const {
    RTC_DCHECK(has_value_);
    return value_;
  }
  T& value() {
    RTC_DCHECK(has_value_);
    return value_;
  }

  // Dereference with a default value in case we don't have a value.
  const T& value_or(const T& default_val) const {
    // The no-op call prevents the compiler from generating optimized code that
    // reads value_ even if !has_value_, but only if FunctionThatDoesNothing is
    // not completely inlined; see its declaration.).
    return has_value_ ? *optional_internal::FunctionThatDoesNothing(&value_)
                      : default_val;
  }

  // Dereference and move value.
  T MoveValue() {
    RTC_DCHECK(has_value_);
    return std::move(value_);
  }

  // Equality tests. Two Optionals are equal if they contain equivalent values,
  // or if they're both empty.
  friend bool operator==(const Optional& m1, const Optional& m2) {
    return m1.has_value_ && m2.has_value_ ? m1.value_ == m2.value_
                                          : m1.has_value_ == m2.has_value_;
  }
  friend bool operator==(const Optional& opt, const T& value) {
    return opt.has_value_ && opt.value_ == value;
  }
  friend bool operator==(const T& value, const Optional& opt) {
    return opt.has_value_ && value == opt.value_;
  }

  friend bool operator!=(const Optional& m1, const Optional& m2) {
    return m1.has_value_ && m2.has_value_ ? m1.value_ != m2.value_
                                          : m1.has_value_ != m2.has_value_;
  }
  friend bool operator!=(const Optional& opt, const T& value) {
    return !opt.has_value_ || opt.value_ != value;
  }
  friend bool operator!=(const T& value, const Optional& opt) {
    return !opt.has_value_ || value != opt.value_;
  }

 private:
  // Tell sanitizers that value_ shouldn't be touched.
  void PoisonValue() {
    rtc::AsanPoison(rtc::MakeArrayView(&value_, 1));
    rtc::MsanMarkUninitialized(rtc::MakeArrayView(&value_, 1));
  }

  // Tell sanitizers that value_ is OK to touch again.
  void UnpoisonValue() {
    rtc::AsanUnpoison(rtc::MakeArrayView(&value_, 1));
  }

  bool has_value_;  // True iff value_ contains a live value.
  union {
    // empty_ exists only to make it possible to initialize the union, even when
    // it doesn't contain any data. If the union goes uninitialized, it may
    // trigger compiler warnings.
    char empty_;
    // By placing value_ in a union, we get to manage its construction and
    // destruction manually: the Optional constructors won't automatically
    // construct it, and the Optional destructor won't automatically destroy
    // it. Basically, this just allocates a properly sized and aligned block of
    // memory in which we can manually put a T with placement new.
    T value_;
  };
};

#ifdef UNIT_TEST
namespace optional_internal {

// Checks if there's a valid PrintTo(const T&, std::ostream*) call for T.
template <typename T>
struct HasPrintTo {
 private:
  struct No {};

  template <typename T2>
  static auto Test(const T2& obj)
      -> decltype(PrintTo(obj, std::declval<std::ostream*>()));

  template <typename>
  static No Test(...);

 public:
  static constexpr bool value =
      !std::is_same<decltype(Test<T>(std::declval<const T&>())), No>::value;
};

// Checks if there's a valid operator<<(std::ostream&, const T&) call for T.
template <typename T>
struct HasOstreamOperator {
 private:
  struct No {};

  template <typename T2>
  static auto Test(const T2& obj)
      -> decltype(std::declval<std::ostream&>() << obj);

  template <typename>
  static No Test(...);

 public:
  static constexpr bool value =
      !std::is_same<decltype(Test<T>(std::declval<const T&>())), No>::value;
};

// Prefer using PrintTo to print the object.
template <typename T>
typename std::enable_if<HasPrintTo<T>::value, void>::type OptionalPrintToHelper(
    const T& value,
    std::ostream* os) {
  PrintTo(value, os);
}

// Fall back to operator<<(std::ostream&, ...) if it exists.
template <typename T>
typename std::enable_if<HasOstreamOperator<T>::value && !HasPrintTo<T>::value,
                        void>::type
OptionalPrintToHelper(const T& value, std::ostream* os) {
  *os << value;
}

inline void OptionalPrintObjectBytes(const unsigned char* bytes,
                                     size_t size,
                                     std::ostream* os) {
  *os << "<optional with " << size << "-byte object [";
  for (size_t i = 0; i != size; ++i) {
    *os << (i == 0 ? "" : ((i & 1) ? "-" : " "));
    *os << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(bytes[i]);
  }
  *os << "]>";
}

// As a final back-up, just print the contents of the objcets byte-wise.
template <typename T>
typename std::enable_if<!HasOstreamOperator<T>::value && !HasPrintTo<T>::value,
                        void>::type
OptionalPrintToHelper(const T& value, std::ostream* os) {
  OptionalPrintObjectBytes(reinterpret_cast<const unsigned char*>(&value),
                           sizeof(value), os);
}

}  // namespace optional_internal

// PrintTo is used by gtest to print out the results of tests. We want to ensure
// the object contained in an Optional can be printed out if it's set, while
// avoiding touching the object's storage if it is undefined.
template <typename T>
void PrintTo(const rtc::Optional<T>& opt, std::ostream* os) {
  if (opt) {
    optional_internal::OptionalPrintToHelper(*opt, os);
  } else {
    *os << "<empty optional>";
  }
}

#endif  // UNIT_TEST

}  // namespace rtc

#endif  // WEBRTC_BASE_OPTIONAL_H_
