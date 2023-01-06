//
// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: Int128Impl.h
// -----------------------------------------------------------------------------
//
// This header file defines 128-bit integer types, `uint128` and `Int128Impl`.
//
// TODO(absl-team): This module is inconsistent as many inline `uint128` methods
// are defined in this file, while many inline `Int128Impl` methods are defined in
// the `int128_*_intrinsic.inc` files.
//

// Int128.h and Int128.cpp are derived from abseil-cpp (https://github.com/abseil/abseil-cpp)
// Imported revision is ddb842f583e560bbde497bc96cfebe25f9089e11.
// We apply the following changes.
// 1. Use WTF macros instead of ABSL macros.
// 2. Remove abseil HashTable handling
// 3. Remove __int128_t handling

#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <limits>
#include <utility>
#include <wtf/ArgumentCoder.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/Platform.h>

#if COMPILER(MSVC)
// In very old versions of MSVC and when the /Zc:wchar_t flag is off, wchar_t is
// a typedef for unsigned short.  Otherwise wchar_t is mapped to the __wchar_t
// builtin type.  We need to make sure not to define operator wchar_t()
// alongside operator unsigned short() in these instances.
#define ABSL_INTERNAL_WCHAR_T __wchar_t
#else
#define ABSL_INTERNAL_WCHAR_T wchar_t
#endif

namespace WTF {

class Int128Impl;
class PrintStream;

// UInt128Impl
//
// An unsigned 128-bit integer type. The API is meant to mimic an intrinsic type
// as closely as is practical, including exhibiting undefined behavior in
// analogous cases (e.g. division by zero). This type is intended to be a
// drop-in replacement once C++ supports an intrinsic `uint128_t` type; when
// that occurs, existing well-behaved uses of `UInt128Impl` will continue to work
// using that new type.
//
// Note: code written with this type will continue to compile once `uint128_t`
// is introduced, provided the replacement helper functions
// `UInt128Impl(Low|High)64()` and `MakeUInt128()` are made.
//
// A `UInt128Impl` supports the following:
//
//   * Implicit construction from integral types
//   * Explicit conversion to integral types
//
// However, a `UInt128Impl` differs from intrinsic integral types in the following
// ways:
//
//   * Errors on implicit conversions that do not preserve value (such as
//     loss of precision when converting to float values).
//   * Requires explicit construction from and conversion to floating point
//     types.
//   * Conversion to integral types requires an explicit static_cast() to
//     mimic use of the `-Wnarrowing` compiler flag.
//   * The alignment requirement of `UInt128Impl` may differ from that of an
//     intrinsic 128-bit integer type depending on platform and build
//     configuration.
//
// Example:
//
//     float y = UInt128Max();  // Error. UInt128Impl cannot be implicitly
//                                    // converted to float.
//
//     UInt128Impl v;
//     uint64_t i = v;                         // Error
//     uint64_t i = static_cast<uint64_t>(v);  // OK
//
class alignas(16) UInt128Impl {
 public:
  UInt128Impl() = default;

  // Constructors from arithmetic types
  constexpr UInt128Impl(int v);                 // NOLINT(runtime/explicit)
  constexpr UInt128Impl(unsigned int v);        // NOLINT(runtime/explicit)
  constexpr UInt128Impl(long v);                // NOLINT(runtime/int)
  constexpr UInt128Impl(unsigned long v);       // NOLINT(runtime/int)
  constexpr UInt128Impl(long long v);           // NOLINT(runtime/int)
  constexpr UInt128Impl(unsigned long long v);  // NOLINT(runtime/int)
  constexpr UInt128Impl(Int128Impl v);  // NOLINT(runtime/explicit)
  WTF_EXPORT_PRIVATE explicit UInt128Impl(float v);
  WTF_EXPORT_PRIVATE explicit UInt128Impl(double v);
  WTF_EXPORT_PRIVATE explicit UInt128Impl(long double v);

  // Assignment operators from arithmetic types
  UInt128Impl& operator=(int v);
  UInt128Impl& operator=(unsigned int v);
  UInt128Impl& operator=(long v);                // NOLINT(runtime/int)
  UInt128Impl& operator=(unsigned long v);       // NOLINT(runtime/int)
  UInt128Impl& operator=(long long v);           // NOLINT(runtime/int)
  UInt128Impl& operator=(unsigned long long v);  // NOLINT(runtime/int)
  UInt128Impl& operator=(Int128Impl v);

  // Conversion operators to other arithmetic types
  constexpr explicit operator bool() const;
  constexpr explicit operator char() const;
  constexpr explicit operator signed char() const;
  constexpr explicit operator unsigned char() const;
  constexpr explicit operator char16_t() const;
  constexpr explicit operator char32_t() const;
  constexpr explicit operator ABSL_INTERNAL_WCHAR_T() const;
  constexpr explicit operator short() const;  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned short() const;
  constexpr explicit operator int() const;
  constexpr explicit operator unsigned int() const;
  constexpr explicit operator long() const;  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned long() const;
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator long long() const;
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned long long() const;
  explicit operator float() const;
  explicit operator double() const;
  explicit operator long double() const;

  // Trivial copy constructor, assignment operator and destructor.

  // Arithmetic operators.
  UInt128Impl& operator+=(UInt128Impl other);
  UInt128Impl& operator-=(UInt128Impl other);
  UInt128Impl& operator*=(UInt128Impl other);
  // Long division/modulo for UInt128Impl.
  UInt128Impl& operator/=(UInt128Impl other);
  UInt128Impl& operator%=(UInt128Impl other);
  UInt128Impl operator++(int);
  UInt128Impl operator--(int);
  UInt128Impl& operator<<=(int);
  UInt128Impl& operator>>=(int);
  UInt128Impl& operator&=(UInt128Impl other);
  UInt128Impl& operator|=(UInt128Impl other);
  UInt128Impl& operator^=(UInt128Impl other);
  UInt128Impl& operator++();
  UInt128Impl& operator--();

  // UInt128Low64()
  //
  // Returns the lower 64-bit value of a `UInt128Impl` value.
  friend constexpr uint64_t UInt128Low64(UInt128Impl v);

  // UInt128High64()
  //
  // Returns the higher 64-bit value of a `UInt128Impl` value.
  friend constexpr uint64_t UInt128High64(UInt128Impl v);

  // MakeUInt128()
  //
  // Constructs a `UInt128Impl` numeric value from two 64-bit unsigned integers.
  // Note that this factory function is the only way to construct a `UInt128Impl`
  // from integer values greater than 2^64.
  //
  // Example:
  //
  //   UInt128Impl big = MakeUInt128(1, 0);
  friend constexpr UInt128Impl MakeUInt128(uint64_t high, uint64_t low);

  // UInt128Max()
  //
  // Returns the highest value for a 128-bit unsigned integer.
  friend constexpr UInt128Impl UInt128Max();

 private:
  constexpr UInt128Impl(uint64_t high, uint64_t low);

  // TODO(strel) Update implementation to use __int128 once all users of
  // UInt128Impl are fixed to not depend on alignof(UInt128Impl) == 8. Also add
  // alignas(16) to class definition to keep alignment consistent across
  // platforms.
#if CPU(LITTLE_ENDIAN)
  uint64_t lo_;
  uint64_t hi_;
#elif CPU(BIG_ENDIAN)
  uint64_t hi_;
  uint64_t lo_;
#else  // byte order
#error "Unsupported byte order: must be little-endian or big-endian."
#endif  // byte order
};

// allow UInt128Impl to be logged
WTF_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, UInt128Impl v);

// TODO(strel) add operator>>(std::istream&, UInt128Impl)

constexpr UInt128Impl UInt128Max() {
  return UInt128Impl((std::numeric_limits<uint64_t>::max)(),
                 (std::numeric_limits<uint64_t>::max)());
}

}  // namespace WTF

// Specialized numeric_limits for UInt128Impl.
namespace std {
template <>
class numeric_limits<WTF::UInt128Impl> {
 public:
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = false;
  static constexpr bool is_integer = true;
  static constexpr bool is_exact = true;
  static constexpr bool has_infinity = false;
  static constexpr bool has_quiet_NaN = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr float_denorm_style has_denorm = denorm_absent;
  static constexpr bool has_denorm_loss = false;
  static constexpr float_round_style round_style = round_toward_zero;
  static constexpr bool is_iec559 = false;
  static constexpr bool is_bounded = true;
  static constexpr bool is_modulo = true;
  static constexpr int digits = 128;
  static constexpr int digits10 = 38;
  static constexpr int max_digits10 = 0;
  static constexpr int radix = 2;
  static constexpr int min_exponent = 0;
  static constexpr int min_exponent10 = 0;
  static constexpr int max_exponent = 0;
  static constexpr int max_exponent10 = 0;
  static constexpr bool traps = numeric_limits<uint64_t>::traps;
  static constexpr bool tinyness_before = false;

  static constexpr WTF::UInt128Impl (min)() { return 0; }
  static constexpr WTF::UInt128Impl lowest() { return 0; }
  static constexpr WTF::UInt128Impl (max)() { return WTF::UInt128Max(); }
  static constexpr WTF::UInt128Impl epsilon() { return 0; }
  static constexpr WTF::UInt128Impl round_error() { return 0; }
  static constexpr WTF::UInt128Impl infinity() { return 0; }
  static constexpr WTF::UInt128Impl quiet_NaN() { return 0; }
  static constexpr WTF::UInt128Impl signaling_NaN() { return 0; }
  static constexpr WTF::UInt128Impl denorm_min() { return 0; }
};
}  // namespace std

namespace WTF {

// Int128Impl
//
// A signed 128-bit integer type. The API is meant to mimic an intrinsic
// integral type as closely as is practical, including exhibiting undefined
// behavior in analogous cases (e.g. division by zero).
//
// An `Int128Impl` supports the following:
//
//   * Implicit construction from integral types
//   * Explicit conversion to integral types
//
// However, an `Int128Impl` differs from intrinsic integral types in the following
// ways:
//
//   * It is not implicitly convertible to other integral types.
//   * Requires explicit construction from and conversion to floating point
//     types.

// The design goal for `Int128Impl` is that it will be compatible with a future
// `int128_t`, if that type becomes a part of the standard.
//
// Example:
//
//     float y = Int128Impl(17);  // Error. Int128Impl cannot be implicitly
//                                  // converted to float.
//
//     Int128Impl v;
//     int64_t i = v;                        // Error
//     int64_t i = static_cast<int64_t>(v);  // OK
//
class alignas(16) Int128Impl {
 public:
  Int128Impl() = default;

  // Constructors from arithmetic types
  constexpr Int128Impl(int v);                 // NOLINT(runtime/explicit)
  constexpr Int128Impl(unsigned int v);        // NOLINT(runtime/explicit)
  constexpr Int128Impl(long v);                // NOLINT(runtime/int)
  constexpr Int128Impl(unsigned long v);       // NOLINT(runtime/int)
  constexpr Int128Impl(long long v);           // NOLINT(runtime/int)
  constexpr Int128Impl(unsigned long long v);  // NOLINT(runtime/int)
  constexpr explicit Int128Impl(UInt128Impl v);
  WTF_EXPORT_PRIVATE explicit Int128Impl(float v);
  WTF_EXPORT_PRIVATE explicit Int128Impl(double v);
  WTF_EXPORT_PRIVATE explicit Int128Impl(long double v);

  // Assignment operators from arithmetic types
  Int128Impl& operator=(int v);
  Int128Impl& operator=(unsigned int v);
  Int128Impl& operator=(long v);                // NOLINT(runtime/int)
  Int128Impl& operator=(unsigned long v);       // NOLINT(runtime/int)
  Int128Impl& operator=(long long v);           // NOLINT(runtime/int)
  Int128Impl& operator=(unsigned long long v);  // NOLINT(runtime/int)

  // Conversion operators to other arithmetic types
  constexpr explicit operator bool() const;
  constexpr explicit operator char() const;
  constexpr explicit operator signed char() const;
  constexpr explicit operator unsigned char() const;
  constexpr explicit operator char16_t() const;
  constexpr explicit operator char32_t() const;
  constexpr explicit operator ABSL_INTERNAL_WCHAR_T() const;
  constexpr explicit operator short() const;  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned short() const;
  constexpr explicit operator int() const;
  constexpr explicit operator unsigned int() const;
  constexpr explicit operator long() const;  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned long() const;
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator long long() const;
  // NOLINTNEXTLINE(runtime/int)
  constexpr explicit operator unsigned long long() const;
  explicit operator float() const;
  explicit operator double() const;
  explicit operator long double() const;

  // Trivial copy constructor, assignment operator and destructor.

  // Arithmetic operators
  Int128Impl& operator+=(Int128Impl other);
  Int128Impl& operator-=(Int128Impl other);
  Int128Impl& operator*=(Int128Impl other);
  Int128Impl& operator/=(Int128Impl other);
  Int128Impl& operator%=(Int128Impl other);
  Int128Impl operator++(int);  // postfix increment: i++
  Int128Impl operator--(int);  // postfix decrement: i--
  Int128Impl& operator++();    // prefix increment:  ++i
  Int128Impl& operator--();    // prefix decrement:  --i
  Int128Impl& operator&=(Int128Impl other);
  Int128Impl& operator|=(Int128Impl other);
  Int128Impl& operator^=(Int128Impl other);
  Int128Impl& operator<<=(int amount);
  Int128Impl& operator>>=(int amount);

  // Int128Low64()
  //
  // Returns the lower 64-bit value of a `Int128Impl` value.
  friend constexpr uint64_t Int128Low64(Int128Impl v);

  // Int128High64()
  //
  // Returns the higher 64-bit value of a `Int128Impl` value.
  friend constexpr int64_t Int128High64(Int128Impl v);

  // MakeInt128()
  //
  // Constructs a `Int128Impl` numeric value from two 64-bit integers. Note that
  // signedness is conveyed in the upper `high` value.
  //
  //   (Int128Impl(1) << 64) * high + low
  //
  // Note that this factory function is the only way to construct a `Int128Impl`
  // from integer values greater than 2^64 or less than -2^64.
  //
  // Example:
  //
  //   Int128Impl big = MakeInt128(1, 0);
  //   Int128Impl big_n = MakeInt128(-1, 0);
  friend constexpr Int128Impl MakeInt128(int64_t high, uint64_t low);

  // Int128Max()
  //
  // Returns the maximum value for a 128-bit signed integer.
  friend constexpr Int128Impl Int128Max();

  // Int128Min()
  //
  // Returns the minimum value for a 128-bit signed integer.
  friend constexpr Int128Impl Int128Min();

 private:
  constexpr Int128Impl(int64_t high, uint64_t low);

#if CPU(LITTLE_ENDIAN)
  uint64_t lo_;
  int64_t hi_;
#elif CPU(BIG_ENDIAN)
  int64_t hi_;
  uint64_t lo_;
#else  // byte order
#error "Unsupported byte order: must be little-endian or big-endian."
#endif  // byte order
};

WTF_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, Int128Impl v);

// TODO(absl-team) add operator>>(std::istream&, Int128Impl)

constexpr Int128Impl Int128Max() {
  return Int128Impl((std::numeric_limits<int64_t>::max)(),
                (std::numeric_limits<uint64_t>::max)());
}

constexpr Int128Impl Int128Min() {
  return Int128Impl((std::numeric_limits<int64_t>::min)(), 0);
}

}  // namespace WTF

// Specialized numeric_limits for Int128Impl.
namespace std {
template <>
class numeric_limits<WTF::Int128Impl> {
 public:
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = true;
  static constexpr bool is_integer = true;
  static constexpr bool is_exact = true;
  static constexpr bool has_infinity = false;
  static constexpr bool has_quiet_NaN = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr float_denorm_style has_denorm = denorm_absent;
  static constexpr bool has_denorm_loss = false;
  static constexpr float_round_style round_style = round_toward_zero;
  static constexpr bool is_iec559 = false;
  static constexpr bool is_bounded = true;
  static constexpr bool is_modulo = false;
  static constexpr int digits = 127;
  static constexpr int digits10 = 38;
  static constexpr int max_digits10 = 0;
  static constexpr int radix = 2;
  static constexpr int min_exponent = 0;
  static constexpr int min_exponent10 = 0;
  static constexpr int max_exponent = 0;
  static constexpr int max_exponent10 = 0;
  static constexpr bool traps = numeric_limits<uint64_t>::traps;
  static constexpr bool tinyness_before = false;

  static constexpr WTF::Int128Impl (min)() { return WTF::Int128Min(); }
  static constexpr WTF::Int128Impl lowest() { return WTF::Int128Min(); }
  static constexpr WTF::Int128Impl (max)() { return WTF::Int128Max(); }
  static constexpr WTF::Int128Impl epsilon() { return 0; }
  static constexpr WTF::Int128Impl round_error() { return 0; }
  static constexpr WTF::Int128Impl infinity() { return 0; }
  static constexpr WTF::Int128Impl quiet_NaN() { return 0; }
  static constexpr WTF::Int128Impl signaling_NaN() { return 0; }
  static constexpr WTF::Int128Impl denorm_min() { return 0; }
};
}  // namespace std

// --------------------------------------------------------------------------
//                      Implementation details follow
// --------------------------------------------------------------------------
namespace WTF {

constexpr UInt128Impl MakeUInt128(uint64_t high, uint64_t low) {
  return UInt128Impl(high, low);
}

// Assignment from integer types.

inline UInt128Impl& UInt128Impl::operator=(int v) { return *this = UInt128Impl(v); }

inline UInt128Impl& UInt128Impl::operator=(unsigned int v) {
  return *this = UInt128Impl(v);
}

inline UInt128Impl& UInt128Impl::operator=(long v) {  // NOLINT(runtime/int)
  return *this = UInt128Impl(v);
}

// NOLINTNEXTLINE(runtime/int)
inline UInt128Impl& UInt128Impl::operator=(unsigned long v) {
  return *this = UInt128Impl(v);
}

// NOLINTNEXTLINE(runtime/int)
inline UInt128Impl& UInt128Impl::operator=(long long v) {
  return *this = UInt128Impl(v);
}

// NOLINTNEXTLINE(runtime/int)
inline UInt128Impl& UInt128Impl::operator=(unsigned long long v) {
  return *this = UInt128Impl(v);
}

inline UInt128Impl& UInt128Impl::operator=(Int128Impl v) {
  return *this = UInt128Impl(v);
}

// Arithmetic operators.

constexpr UInt128Impl operator<<(UInt128Impl lhs, int amount);
constexpr UInt128Impl operator>>(UInt128Impl lhs, int amount);
constexpr UInt128Impl operator+(UInt128Impl lhs, UInt128Impl rhs);
constexpr UInt128Impl operator-(UInt128Impl lhs, UInt128Impl rhs);
constexpr UInt128Impl operator*(UInt128Impl lhs, UInt128Impl rhs);
WTF_EXPORT_PRIVATE UInt128Impl operator/(UInt128Impl lhs, UInt128Impl rhs);
WTF_EXPORT_PRIVATE UInt128Impl operator%(UInt128Impl lhs, UInt128Impl rhs);

inline UInt128Impl& UInt128Impl::operator<<=(int amount) {
  *this = *this << amount;
  return *this;
}

inline UInt128Impl& UInt128Impl::operator>>=(int amount) {
  *this = *this >> amount;
  return *this;
}

inline UInt128Impl& UInt128Impl::operator+=(UInt128Impl other) {
  *this = *this + other;
  return *this;
}

inline UInt128Impl& UInt128Impl::operator-=(UInt128Impl other) {
  *this = *this - other;
  return *this;
}

inline UInt128Impl& UInt128Impl::operator*=(UInt128Impl other) {
  *this = *this * other;
  return *this;
}

inline UInt128Impl& UInt128Impl::operator/=(UInt128Impl other) {
  *this = *this / other;
  return *this;
}

inline UInt128Impl& UInt128Impl::operator%=(UInt128Impl other) {
  *this = *this % other;
  return *this;
}

constexpr uint64_t UInt128Low64(UInt128Impl v) { return v.lo_; }

constexpr uint64_t UInt128High64(UInt128Impl v) { return v.hi_; }

// Constructors from integer types.

#if CPU(LITTLE_ENDIAN)

constexpr UInt128Impl::UInt128Impl(uint64_t high, uint64_t low)
    : lo_{low}, hi_{high} {}

constexpr UInt128Impl::UInt128Impl(int v)
    : lo_{static_cast<uint64_t>(v)},
      hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0} {}
constexpr UInt128Impl::UInt128Impl(long v)  // NOLINT(runtime/int)
    : lo_{static_cast<uint64_t>(v)},
      hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0} {}
constexpr UInt128Impl::UInt128Impl(long long v)  // NOLINT(runtime/int)
    : lo_{static_cast<uint64_t>(v)},
      hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0} {}

constexpr UInt128Impl::UInt128Impl(unsigned int v) : lo_{v}, hi_{0} {}
// NOLINTNEXTLINE(runtime/int)
constexpr UInt128Impl::UInt128Impl(unsigned long v) : lo_{v}, hi_{0} {}
// NOLINTNEXTLINE(runtime/int)
constexpr UInt128Impl::UInt128Impl(unsigned long long v) : lo_{v}, hi_{0} {}

constexpr UInt128Impl::UInt128Impl(Int128Impl v)
    : lo_{Int128Low64(v)}, hi_{static_cast<uint64_t>(Int128High64(v))} {}

#elif CPU(BIG_ENDIAN)

constexpr UInt128Impl::UInt128Impl(uint64_t high, uint64_t low)
    : hi_{high}, lo_{low} {}

constexpr UInt128Impl::UInt128Impl(int v)
    : hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0},
      lo_{static_cast<uint64_t>(v)} {}
constexpr UInt128Impl::UInt128Impl(long v)  // NOLINT(runtime/int)
    : hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0},
      lo_{static_cast<uint64_t>(v)} {}
constexpr UInt128Impl::UInt128Impl(long long v)  // NOLINT(runtime/int)
    : hi_{v < 0 ? (std::numeric_limits<uint64_t>::max)() : 0},
      lo_{static_cast<uint64_t>(v)} {}

constexpr UInt128Impl::UInt128Impl(unsigned int v) : hi_{0}, lo_{v} {}
// NOLINTNEXTLINE(runtime/int)
constexpr UInt128Impl::UInt128Impl(unsigned long v) : hi_{0}, lo_{v} {}
// NOLINTNEXTLINE(runtime/int)
constexpr UInt128Impl::UInt128Impl(unsigned long long v) : hi_{0}, lo_{v} {}

constexpr UInt128Impl::UInt128Impl(Int128Impl v)
    : hi_{static_cast<uint64_t>(Int128High64(v))}, lo_{Int128Low64(v)} {}

#else  // byte order
#error "Unsupported byte order: must be little-endian or big-endian."
#endif  // byte order

// Conversion operators to integer types.

constexpr UInt128Impl::operator bool() const { return lo_ || hi_; }

constexpr UInt128Impl::operator char() const { return static_cast<char>(lo_); }

constexpr UInt128Impl::operator signed char() const {
  return static_cast<signed char>(lo_);
}

constexpr UInt128Impl::operator unsigned char() const {
  return static_cast<unsigned char>(lo_);
}

constexpr UInt128Impl::operator char16_t() const {
  return static_cast<char16_t>(lo_);
}

constexpr UInt128Impl::operator char32_t() const {
  return static_cast<char32_t>(lo_);
}

constexpr UInt128Impl::operator ABSL_INTERNAL_WCHAR_T() const {
  return static_cast<ABSL_INTERNAL_WCHAR_T>(lo_);
}

// NOLINTNEXTLINE(runtime/int)
constexpr UInt128Impl::operator short() const { return static_cast<short>(lo_); }

constexpr UInt128Impl::operator unsigned short() const {  // NOLINT(runtime/int)
  return static_cast<unsigned short>(lo_);            // NOLINT(runtime/int)
}

constexpr UInt128Impl::operator int() const { return static_cast<int>(lo_); }

constexpr UInt128Impl::operator unsigned int() const {
  return static_cast<unsigned int>(lo_);
}

// NOLINTNEXTLINE(runtime/int)
constexpr UInt128Impl::operator long() const { return static_cast<long>(lo_); }

constexpr UInt128Impl::operator unsigned long() const {  // NOLINT(runtime/int)
  return static_cast<unsigned long>(lo_);            // NOLINT(runtime/int)
}

constexpr UInt128Impl::operator long long() const {  // NOLINT(runtime/int)
  return static_cast<long long>(lo_);            // NOLINT(runtime/int)
}

constexpr UInt128Impl::operator unsigned long long() const {  // NOLINT(runtime/int)
  return static_cast<unsigned long long>(lo_);            // NOLINT(runtime/int)
}

// Conversion operators to floating point types.

inline UInt128Impl::operator float() const {
  return static_cast<float>(lo_) + std::ldexp(static_cast<float>(hi_), 64);
}

inline UInt128Impl::operator double() const {
  return static_cast<double>(lo_) + std::ldexp(static_cast<double>(hi_), 64);
}

inline UInt128Impl::operator long double() const {
  return static_cast<long double>(lo_) +
         std::ldexp(static_cast<long double>(hi_), 64);
}

// Comparison operators.

constexpr bool operator==(UInt128Impl lhs, UInt128Impl rhs) {
  return (UInt128Low64(lhs) == UInt128Low64(rhs) &&
          UInt128High64(lhs) == UInt128High64(rhs));
}

constexpr bool operator!=(UInt128Impl lhs, UInt128Impl rhs) { return !(lhs == rhs); }

constexpr bool operator<(UInt128Impl lhs, UInt128Impl rhs) {
  return (UInt128High64(lhs) == UInt128High64(rhs))
             ? (UInt128Low64(lhs) < UInt128Low64(rhs))
             : (UInt128High64(lhs) < UInt128High64(rhs));
}

constexpr bool operator>(UInt128Impl lhs, UInt128Impl rhs) { return rhs < lhs; }

constexpr bool operator<=(UInt128Impl lhs, UInt128Impl rhs) { return !(rhs < lhs); }

constexpr bool operator>=(UInt128Impl lhs, UInt128Impl rhs) { return !(lhs < rhs); }

// Unary operators.

constexpr inline UInt128Impl operator+(UInt128Impl val) {
  return val;
}

constexpr inline Int128Impl operator+(Int128Impl val) {
  return val;
}

constexpr UInt128Impl operator-(UInt128Impl val) {
  return MakeUInt128(
      ~UInt128High64(val) + static_cast<unsigned long>(UInt128Low64(val) == 0),
      ~UInt128Low64(val) + 1);
}

constexpr inline bool operator!(UInt128Impl val) {
  return !UInt128High64(val) && !UInt128Low64(val);
}

// Logical operators.

constexpr inline UInt128Impl operator~(UInt128Impl val) {
  return MakeUInt128(~UInt128High64(val), ~UInt128Low64(val));
}

constexpr inline UInt128Impl operator|(UInt128Impl lhs, UInt128Impl rhs) {
  return MakeUInt128(UInt128High64(lhs) | UInt128High64(rhs),
                     UInt128Low64(lhs) | UInt128Low64(rhs));
}

constexpr inline UInt128Impl operator&(UInt128Impl lhs, UInt128Impl rhs) {
  return MakeUInt128(UInt128High64(lhs) & UInt128High64(rhs),
                     UInt128Low64(lhs) & UInt128Low64(rhs));
}

constexpr inline UInt128Impl operator^(UInt128Impl lhs, UInt128Impl rhs) {
  return MakeUInt128(UInt128High64(lhs) ^ UInt128High64(rhs),
                     UInt128Low64(lhs) ^ UInt128Low64(rhs));
}

inline UInt128Impl& UInt128Impl::operator|=(UInt128Impl other) {
  *this = *this | other;
  return *this;
}

inline UInt128Impl& UInt128Impl::operator&=(UInt128Impl other) {
  *this = *this & other;
  return *this;
}

inline UInt128Impl& UInt128Impl::operator^=(UInt128Impl other) {
  *this = *this ^ other;
  return *this;
}

// Arithmetic operators.

constexpr UInt128Impl operator<<(UInt128Impl lhs, int amount) {
  // uint64_t shifts of >= 64 are undefined, so we will need some
  // special-casing.
  return amount >= 64 ? MakeUInt128(UInt128Low64(lhs) << (amount - 64), 0)
         : amount == 0 ? lhs
                       : MakeUInt128((UInt128High64(lhs) << amount) |
                                         (UInt128Low64(lhs) >> (64 - amount)),
                                     UInt128Low64(lhs) << amount);
}

constexpr UInt128Impl operator>>(UInt128Impl lhs, int amount) {
  // uint64_t shifts of >= 64 are undefined, so we will need some
  // special-casing.
  return amount >= 64 ? MakeUInt128(0, UInt128High64(lhs) >> (amount - 64))
         : amount == 0 ? lhs
                       : MakeUInt128(UInt128High64(lhs) >> amount,
                                     (UInt128Low64(lhs) >> amount) |
                                         (UInt128High64(lhs) << (64 - amount)));
}

namespace int128_internal {
constexpr UInt128Impl AddResult(UInt128Impl result, UInt128Impl lhs) {
  // check for carry
  return (UInt128Low64(result) < UInt128Low64(lhs))
             ? MakeUInt128(UInt128High64(result) + 1, UInt128Low64(result))
             : result;
}
}  // namespace int128_internal

constexpr UInt128Impl operator+(UInt128Impl lhs, UInt128Impl rhs) {
  return int128_internal::AddResult(
      MakeUInt128(UInt128High64(lhs) + UInt128High64(rhs),
                  UInt128Low64(lhs) + UInt128Low64(rhs)),
      lhs);
}

namespace int128_internal {
constexpr UInt128Impl SubstructResult(UInt128Impl result, UInt128Impl lhs, UInt128Impl rhs) {
  // check for carry
  return (UInt128Low64(lhs) < UInt128Low64(rhs))
             ? MakeUInt128(UInt128High64(result) - 1, UInt128Low64(result))
             : result;
}
}  // namespace int128_internal

constexpr UInt128Impl operator-(UInt128Impl lhs, UInt128Impl rhs) {
  return int128_internal::SubstructResult(
      MakeUInt128(UInt128High64(lhs) - UInt128High64(rhs),
                  UInt128Low64(lhs) - UInt128Low64(rhs)),
      lhs, rhs);
}

constexpr UInt128Impl operator*(UInt128Impl lhs, UInt128Impl rhs) {
  uint64_t a32 = UInt128Low64(lhs) >> 32;
  uint64_t a00 = UInt128Low64(lhs) & 0xffffffff;
  uint64_t b32 = UInt128Low64(rhs) >> 32;
  uint64_t b00 = UInt128Low64(rhs) & 0xffffffff;
  UInt128Impl result =
      MakeUInt128(UInt128High64(lhs) * UInt128Low64(rhs) +
                      UInt128Low64(lhs) * UInt128High64(rhs) + a32 * b32,
                  a00 * b00);
  UInt128Impl v1 = UInt128Impl(a32 * b00) << 32;
  UInt128Impl v2 = UInt128Impl(a00 * b32) << 32;
  return result + v1 + v2;
}

// Increment/decrement operators.

inline UInt128Impl UInt128Impl::operator++(int) {
  UInt128Impl tmp(*this);
  *this += 1;
  return tmp;
}

inline UInt128Impl UInt128Impl::operator--(int) {
  UInt128Impl tmp(*this);
  *this -= 1;
  return tmp;
}

inline UInt128Impl& UInt128Impl::operator++() {
  *this += 1;
  return *this;
}

inline UInt128Impl& UInt128Impl::operator--() {
  *this -= 1;
  return *this;
}

constexpr Int128Impl MakeInt128(int64_t high, uint64_t low) {
  return Int128Impl(high, low);
}

// Assignment from integer types.
inline Int128Impl& Int128Impl::operator=(int v) {
  return *this = Int128Impl(v);
}

inline Int128Impl& Int128Impl::operator=(unsigned int v) {
  return *this = Int128Impl(v);
}

inline Int128Impl& Int128Impl::operator=(long v) {  // NOLINT(runtime/int)
  return *this = Int128Impl(v);
}

// NOLINTNEXTLINE(runtime/int)
inline Int128Impl& Int128Impl::operator=(unsigned long v) {
  return *this = Int128Impl(v);
}

// NOLINTNEXTLINE(runtime/int)
inline Int128Impl& Int128Impl::operator=(long long v) {
  return *this = Int128Impl(v);
}

// NOLINTNEXTLINE(runtime/int)
inline Int128Impl& Int128Impl::operator=(unsigned long long v) {
  return *this = Int128Impl(v);
}

// Arithmetic operators.
constexpr Int128Impl operator-(Int128Impl v);
constexpr Int128Impl operator+(Int128Impl lhs, Int128Impl rhs);
constexpr Int128Impl operator-(Int128Impl lhs, Int128Impl rhs);
constexpr Int128Impl operator*(Int128Impl lhs, Int128Impl rhs);
WTF_EXPORT_PRIVATE Int128Impl operator/(Int128Impl lhs, Int128Impl rhs);
WTF_EXPORT_PRIVATE Int128Impl operator%(Int128Impl lhs, Int128Impl rhs);
constexpr Int128Impl operator|(Int128Impl lhs, Int128Impl rhs);
constexpr Int128Impl operator&(Int128Impl lhs, Int128Impl rhs);
constexpr Int128Impl operator^(Int128Impl lhs, Int128Impl rhs);
constexpr Int128Impl operator<<(Int128Impl lhs, int amount);
constexpr Int128Impl operator>>(Int128Impl lhs, int amount);

inline Int128Impl& Int128Impl::operator+=(Int128Impl other) {
  *this = *this + other;
  return *this;
}

inline Int128Impl& Int128Impl::operator-=(Int128Impl other) {
  *this = *this - other;
  return *this;
}

inline Int128Impl& Int128Impl::operator*=(Int128Impl other) {
  *this = *this * other;
  return *this;
}

inline Int128Impl& Int128Impl::operator/=(Int128Impl other) {
  *this = *this / other;
  return *this;
}

inline Int128Impl& Int128Impl::operator%=(Int128Impl other) {
  *this = *this % other;
  return *this;
}

inline Int128Impl& Int128Impl::operator|=(Int128Impl other) {
  *this = *this | other;
  return *this;
}

inline Int128Impl& Int128Impl::operator&=(Int128Impl other) {
  *this = *this & other;
  return *this;
}

inline Int128Impl& Int128Impl::operator^=(Int128Impl other) {
  *this = *this ^ other;
  return *this;
}

inline Int128Impl& Int128Impl::operator<<=(int amount) {
  *this = *this << amount;
  return *this;
}

inline Int128Impl& Int128Impl::operator>>=(int amount) {
  *this = *this >> amount;
  return *this;
}

// Forward declaration for comparison operators.
constexpr bool operator!=(Int128Impl lhs, Int128Impl rhs);

namespace int128_internal {

// Casts from unsigned to signed while preserving the underlying binary
// representation.
constexpr int64_t BitCastToSigned(uint64_t v) {
  // Casting an unsigned integer to a signed integer of the same
  // width is implementation defined behavior if the source value would not fit
  // in the destination type. We step around it with a roundtrip bitwise not
  // operation to make sure this function remains constexpr. Clang, GCC, and
  // MSVC optimize this to a no-op on x86-64.
  return v & (uint64_t{1} << 63) ? ~static_cast<int64_t>(~v)
                                 : static_cast<int64_t>(v);
}

}  // namespace int128_internal

// #include "absl/numeric/int128_no_intrinsic.inc"  // IWYU pragma: export

constexpr uint64_t Int128Low64(Int128Impl v) { return v.lo_; }

constexpr int64_t Int128High64(Int128Impl v) { return v.hi_; }

#if CPU(LITTLE_ENDIAN)

constexpr Int128Impl::Int128Impl(int64_t high, uint64_t low) :
    lo_(low), hi_(high) {}

constexpr Int128Impl::Int128Impl(int v)
    : lo_{static_cast<uint64_t>(v)}, hi_{v < 0 ? ~int64_t{0} : 0} {}
constexpr Int128Impl::Int128Impl(long v)  // NOLINT(runtime/int)
    : lo_{static_cast<uint64_t>(v)}, hi_{v < 0 ? ~int64_t{0} : 0} {}
constexpr Int128Impl::Int128Impl(long long v)  // NOLINT(runtime/int)
    : lo_{static_cast<uint64_t>(v)}, hi_{v < 0 ? ~int64_t{0} : 0} {}

constexpr Int128Impl::Int128Impl(unsigned int v) : lo_{v}, hi_{0} {}
// NOLINTNEXTLINE(runtime/int)
constexpr Int128Impl::Int128Impl(unsigned long v) : lo_{v}, hi_{0} {}
// NOLINTNEXTLINE(runtime/int)
constexpr Int128Impl::Int128Impl(unsigned long long v) : lo_{v}, hi_{0} {}

constexpr Int128Impl::Int128Impl(UInt128Impl v)
    : lo_{UInt128Low64(v)}, hi_{static_cast<int64_t>(UInt128High64(v))} {}

#elif CPU(BIG_ENDIAN)

constexpr Int128Impl::Int128Impl(int64_t high, uint64_t low) :
    hi_{high}, lo_{low} {}

constexpr Int128Impl::Int128Impl(int v)
    : hi_{v < 0 ? ~int64_t{0} : 0}, lo_{static_cast<uint64_t>(v)} {}
constexpr Int128Impl::Int128Impl(long v)  // NOLINT(runtime/int)
    : hi_{v < 0 ? ~int64_t{0} : 0}, lo_{static_cast<uint64_t>(v)} {}
constexpr Int128Impl::Int128Impl(long long v)  // NOLINT(runtime/int)
    : hi_{v < 0 ? ~int64_t{0} : 0}, lo_{static_cast<uint64_t>(v)} {}

constexpr Int128Impl::Int128Impl(unsigned int v) : hi_{0}, lo_{v} {}
// NOLINTNEXTLINE(runtime/int)
constexpr Int128Impl::Int128Impl(unsigned long v) : hi_{0}, lo_{v} {}
// NOLINTNEXTLINE(runtime/int)
constexpr Int128Impl::Int128Impl(unsigned long long v) : hi_{0}, lo_{v} {}

constexpr Int128Impl::Int128Impl(UInt128Impl v)
    : hi_{static_cast<int64_t>(UInt128High64(v))}, lo_{UInt128Low64(v)} {}

#else  // byte order
#error "Unsupported byte order: must be little-endian or big-endian."
#endif  // byte order

constexpr Int128Impl::operator bool() const { return lo_ || hi_; }

constexpr Int128Impl::operator char() const {
  // NOLINTNEXTLINE(runtime/int)
  return static_cast<char>(static_cast<long long>(*this));
}

constexpr Int128Impl::operator signed char() const {
  // NOLINTNEXTLINE(runtime/int)
  return static_cast<signed char>(static_cast<long long>(*this));
}

constexpr Int128Impl::operator unsigned char() const {
  return static_cast<unsigned char>(lo_);
}

constexpr Int128Impl::operator char16_t() const {
  return static_cast<char16_t>(lo_);
}

constexpr Int128Impl::operator char32_t() const {
  return static_cast<char32_t>(lo_);
}

constexpr Int128Impl::operator ABSL_INTERNAL_WCHAR_T() const {
  // NOLINTNEXTLINE(runtime/int)
  return static_cast<ABSL_INTERNAL_WCHAR_T>(static_cast<long long>(*this));
}

constexpr Int128Impl::operator short() const {  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  return static_cast<short>(static_cast<long long>(*this));
}

constexpr Int128Impl::operator unsigned short() const {  // NOLINT(runtime/int)
  return static_cast<unsigned short>(lo_);           // NOLINT(runtime/int)
}

constexpr Int128Impl::operator int() const {
  // NOLINTNEXTLINE(runtime/int)
  return static_cast<int>(static_cast<long long>(*this));
}

constexpr Int128Impl::operator unsigned int() const {
  return static_cast<unsigned int>(lo_);
}

constexpr Int128Impl::operator long() const {  // NOLINT(runtime/int)
  // NOLINTNEXTLINE(runtime/int)
  return static_cast<long>(static_cast<long long>(*this));
}

constexpr Int128Impl::operator unsigned long() const {  // NOLINT(runtime/int)
  return static_cast<unsigned long>(lo_);           // NOLINT(runtime/int)
}

constexpr Int128Impl::operator long long() const {  // NOLINT(runtime/int)
  // We don't bother checking the value of hi_. If *this < 0, lo_'s high bit
  // must be set in order for the value to fit into a long long. Conversely, if
  // lo_'s high bit is set, *this must be < 0 for the value to fit.
  return int128_internal::BitCastToSigned(lo_);
}

constexpr Int128Impl::operator unsigned long long() const {  // NOLINT(runtime/int)
  return static_cast<unsigned long long>(lo_);           // NOLINT(runtime/int)
}

inline Int128Impl::operator float() const {
  // We must convert the absolute value and then negate as needed, because
  // floating point types are typically sign-magnitude. Otherwise, the
  // difference between the high and low 64 bits when interpreted as two's
  // complement overwhelms the precision of the mantissa.
  //
  // Also check to make sure we don't negate Int128Min()
  return hi_ < 0 && *this != Int128Min()
             ? -static_cast<float>(-*this)
             : static_cast<float>(lo_) +
                   std::ldexp(static_cast<float>(hi_), 64);
}

inline Int128Impl::operator double() const {
  // See comment in Int128Impl::operator float() above.
  return hi_ < 0 && *this != Int128Min()
             ? -static_cast<double>(-*this)
             : static_cast<double>(lo_) +
                   std::ldexp(static_cast<double>(hi_), 64);
}

inline Int128Impl::operator long double() const {
  // See comment in Int128Impl::operator float() above.
  return hi_ < 0 && *this != Int128Min()
             ? -static_cast<long double>(-*this)
             : static_cast<long double>(lo_) +
                   std::ldexp(static_cast<long double>(hi_), 64);
}

// Comparison operators.

constexpr bool operator==(Int128Impl lhs, Int128Impl rhs) {
  return (Int128Low64(lhs) == Int128Low64(rhs) &&
          Int128High64(lhs) == Int128High64(rhs));
}

constexpr bool operator!=(Int128Impl lhs, Int128Impl rhs) { return !(lhs == rhs); }

constexpr bool operator<(Int128Impl lhs, Int128Impl rhs) {
  return (Int128High64(lhs) == Int128High64(rhs))
             ? (Int128Low64(lhs) < Int128Low64(rhs))
             : (Int128High64(lhs) < Int128High64(rhs));
}

constexpr bool operator>(Int128Impl lhs, Int128Impl rhs) {
  return (Int128High64(lhs) == Int128High64(rhs))
             ? (Int128Low64(lhs) > Int128Low64(rhs))
             : (Int128High64(lhs) > Int128High64(rhs));
}

constexpr bool operator<=(Int128Impl lhs, Int128Impl rhs) { return !(lhs > rhs); }

constexpr bool operator>=(Int128Impl lhs, Int128Impl rhs) { return !(lhs < rhs); }

// Unary operators.

constexpr Int128Impl operator-(Int128Impl v) {
  return MakeInt128(~Int128High64(v) + (Int128Low64(v) == 0),
                    ~Int128Low64(v) + 1);
}

constexpr bool operator!(Int128Impl v) {
  return !Int128Low64(v) && !Int128High64(v);
}

constexpr Int128Impl operator~(Int128Impl val) {
  return MakeInt128(~Int128High64(val), ~Int128Low64(val));
}

// Arithmetic operators.

namespace int128_internal {
constexpr Int128Impl SignedAddResult(Int128Impl result, Int128Impl lhs) {
  // check for carry
  return (Int128Low64(result) < Int128Low64(lhs))
             ? MakeInt128(Int128High64(result) + 1, Int128Low64(result))
             : result;
}
}  // namespace int128_internal
constexpr Int128Impl operator+(Int128Impl lhs, Int128Impl rhs) {
  return int128_internal::SignedAddResult(
      MakeInt128(Int128High64(lhs) + Int128High64(rhs),
                 Int128Low64(lhs) + Int128Low64(rhs)),
      lhs);
}

namespace int128_internal {
constexpr Int128Impl SignedSubstructResult(Int128Impl result, Int128Impl lhs, Int128Impl rhs) {
  // check for carry
  return (Int128Low64(lhs) < Int128Low64(rhs))
             ? MakeInt128(Int128High64(result) - 1, Int128Low64(result))
             : result;
}
}  // namespace int128_internal
constexpr Int128Impl operator-(Int128Impl lhs, Int128Impl rhs) {
  return int128_internal::SignedSubstructResult(
      MakeInt128(Int128High64(lhs) - Int128High64(rhs),
                 Int128Low64(lhs) - Int128Low64(rhs)),
      lhs, rhs);
}

constexpr Int128Impl operator*(Int128Impl lhs, Int128Impl rhs) {
  return MakeInt128(
      int128_internal::BitCastToSigned(UInt128High64(UInt128Impl(lhs) * rhs)),
      UInt128Low64(UInt128Impl(lhs) * rhs));
}

inline Int128Impl Int128Impl::operator++(int) {
  Int128Impl tmp(*this);
  *this += 1;
  return tmp;
}

inline Int128Impl Int128Impl::operator--(int) {
  Int128Impl tmp(*this);
  *this -= 1;
  return tmp;
}

inline Int128Impl& Int128Impl::operator++() {
  *this += 1;
  return *this;
}

inline Int128Impl& Int128Impl::operator--() {
  *this -= 1;
  return *this;
}

constexpr Int128Impl operator|(Int128Impl lhs, Int128Impl rhs) {
  return MakeInt128(Int128High64(lhs) | Int128High64(rhs),
                    Int128Low64(lhs) | Int128Low64(rhs));
}

constexpr Int128Impl operator&(Int128Impl lhs, Int128Impl rhs) {
  return MakeInt128(Int128High64(lhs) & Int128High64(rhs),
                    Int128Low64(lhs) & Int128Low64(rhs));
}

constexpr Int128Impl operator^(Int128Impl lhs, Int128Impl rhs) {
  return MakeInt128(Int128High64(lhs) ^ Int128High64(rhs),
                    Int128Low64(lhs) ^ Int128Low64(rhs));
}

constexpr Int128Impl operator<<(Int128Impl lhs, int amount) {
  // int64_t shifts of >= 64 are undefined, so we need some special-casing.
  return amount >= 64
             ? MakeInt128(
                   static_cast<int64_t>(Int128Low64(lhs) << (amount - 64)), 0)
         : amount == 0
             ? lhs
             : MakeInt128(
                   (Int128High64(lhs) << amount) |
                       static_cast<int64_t>(Int128Low64(lhs) >> (64 - amount)),
                   Int128Low64(lhs) << amount);
}

constexpr Int128Impl operator>>(Int128Impl lhs, int amount) {
  // int64_t shifts of >= 64 are undefined, so we need some special-casing.
  // The (Int128High64(lhs) >> 32) >> 32 "trick" causes the the most significant
  // int64 to be inititialized with all zeros or all ones correctly. It takes
  // into account whether the number is negative or positive, and whether the
  // current architecture does arithmetic or logical right shifts for negative
  // numbers.
  return amount >= 64
             ? MakeInt128(
                   (Int128High64(lhs) >> 32) >> 32,
                   static_cast<uint64_t>(Int128High64(lhs) >> (amount - 64)))
         : amount == 0
             ? lhs
             : MakeInt128(Int128High64(lhs) >> amount,
                          (Int128Low64(lhs) >> amount) |
                              (static_cast<uint64_t>(Int128High64(lhs))
                               << (64 - amount)));
}

#if HAVE(INT128_T)
using UInt128 = __uint128_t;
using Int128 = __int128_t;
#else
using UInt128 = UInt128Impl;
using Int128 = Int128Impl;
#endif

template<> struct DefaultHash<UInt128> {
    static unsigned hash(const UInt128& i) { return pairIntHash(intHash(static_cast<uint64_t>(i >> 64)), intHash(static_cast<uint64_t>(i))); }
    static bool equal(const UInt128& a, const UInt128& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};
template<> struct DefaultHash<Int128> {
    static unsigned hash(const UInt128& i) { return pairIntHash(intHash(static_cast<uint64_t>(i >> 64)), intHash(static_cast<uint64_t>(i))); }
    static bool equal(const Int128& a, const Int128& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<UInt128> : GenericHashTraits<UInt128> {
    static constexpr bool emptyValueIsZero = true;
    static void constructDeletedValue(UInt128& slot) { slot = static_cast<UInt128>(-1); }
    static bool isDeletedValue(UInt128 value) { return value == static_cast<UInt128>(-1); }
};

template<> struct HashTraits<Int128> : GenericHashTraits<Int128> {
    static constexpr bool emptyValueIsZero = true;
    static void constructDeletedValue(Int128& slot) { slot = static_cast<Int128>(-1); }
    static bool isDeletedValue(Int128 value) { return value == static_cast<Int128>(-1); }
};

WTF_EXPORT_PRIVATE void printInternal(PrintStream&, UInt128);
WTF_EXPORT_PRIVATE void printInternal(PrintStream&, Int128);

}  // namespace WTF

namespace IPC {
template<> struct ArgumentCoder<WTF::UInt128> {
    template<typename Encoder> static void encode(Encoder& encoder, const WTF::UInt128& i)
    {
        encoder << static_cast<uint64_t>(i >> 64);
        encoder << static_cast<uint64_t>(i);
    }
    template<typename Decoder> static std::optional<WTF::UInt128> decode(Decoder& decoder)
    {
        std::optional<uint64_t> high;
        decoder >> high;
        if (!high)
            return std::nullopt;

        std::optional<uint64_t> low;
        decoder >> low;
        if (!low)
            return std::nullopt;

        return (static_cast<WTF::UInt128>(*high) << 64) | *low;
    }
};
template<> struct ArgumentCoder<WTF::Int128> {
    template<typename Encoder> static void encode(Encoder& encoder, const WTF::Int128& i)
    {
        encoder << static_cast<int64_t>(i >> 64);
        encoder << static_cast<int64_t>(i);
    }
    template<typename Decoder> static std::optional<WTF::Int128> decode(Decoder& decoder)
    {
        std::optional<int64_t> high;
        decoder >> high;
        if (!high)
            return std::nullopt;

        std::optional<uint64_t> low;
        decoder >> low;
        if (!low)
            return std::nullopt;

        return (static_cast<WTF::Int128>(*high) << 64) | *low;
    }
};
}

using WTF::Int128;
using WTF::UInt128;
