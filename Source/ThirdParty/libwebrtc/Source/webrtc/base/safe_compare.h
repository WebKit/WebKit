/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file defines six functions:
//
//   rtc::safe_cmp::Eq  // ==
//   rtc::safe_cmp::Ne  // !=
//   rtc::safe_cmp::Lt  // <
//   rtc::safe_cmp::Le  // <=
//   rtc::safe_cmp::Gt  // >
//   rtc::safe_cmp::Ge  // >=
//
// They each accept two arguments of arbitrary types, and in almost all cases,
// they simply call the appropriate comparison operator. However, if both
// arguments are integers, they don't compare them using C++'s quirky rules,
// but instead adhere to the true mathematical definitions. It is as if the
// arguments were first converted to infinite-range signed integers, and then
// compared, although of course nothing expensive like that actually takes
// place. In practice, for signed/signed and unsigned/unsigned comparisons and
// some mixed-signed comparisons with a compile-time constant, the overhead is
// zero; in the remaining cases, it is just a few machine instructions (no
// branches).

#ifndef WEBRTC_BASE_SAFE_COMPARE_H_
#define WEBRTC_BASE_SAFE_COMPARE_H_

#include <stddef.h>
#include <stdint.h>

#include <type_traits>
#include <utility>

namespace rtc {
namespace safe_cmp {

namespace safe_cmp_impl {

template <size_t N>
struct LargerIntImpl : std::false_type {};
template <>
struct LargerIntImpl<sizeof(int8_t)> : std::true_type {
  using type = int16_t;
};
template <>
struct LargerIntImpl<sizeof(int16_t)> : std::true_type {
  using type = int32_t;
};
template <>
struct LargerIntImpl<sizeof(int32_t)> : std::true_type {
  using type = int64_t;
};

// LargerInt<T1, T2>::value is true iff there's a signed type that's larger
// than T1 (and no larger than the larger of T2 and int*, for performance
// reasons); and if there is such a type, LargerInt<T1, T2>::type is an alias
// for it.
template <typename T1, typename T2>
struct LargerInt
    : LargerIntImpl<sizeof(T1) < sizeof(T2) || sizeof(T1) < sizeof(int*)
                        ? sizeof(T1)
                        : 0> {};

template <typename T>
inline typename std::make_unsigned<T>::type MakeUnsigned(T a) {
  return static_cast<typename std::make_unsigned<T>::type>(a);
}

// Overload for when both T1 and T2 have the same signedness.
template <typename Op,
          typename T1,
          typename T2,
          typename std::enable_if<std::is_signed<T1>::value ==
                                  std::is_signed<T2>::value>::type* = nullptr>
inline bool Cmp(T1 a, T2 b) {
  return Op::Op(a, b);
}

// Overload for signed - unsigned comparison that can be promoted to a bigger
// signed type.
template <typename Op,
          typename T1,
          typename T2,
          typename std::enable_if<std::is_signed<T1>::value &&
                                  std::is_unsigned<T2>::value &&
                                  LargerInt<T2, T1>::value>::type* = nullptr>
inline bool Cmp(T1 a, T2 b) {
  return Op::Op(a, static_cast<typename LargerInt<T2, T1>::type>(b));
}

// Overload for unsigned - signed comparison that can be promoted to a bigger
// signed type.
template <typename Op,
          typename T1,
          typename T2,
          typename std::enable_if<std::is_unsigned<T1>::value &&
                                  std::is_signed<T2>::value &&
                                  LargerInt<T1, T2>::value>::type* = nullptr>
inline bool Cmp(T1 a, T2 b) {
  return Op::Op(static_cast<typename LargerInt<T1, T2>::type>(a), b);
}

// Overload for signed - unsigned comparison that can't be promoted to a bigger
// signed type.
template <typename Op,
          typename T1,
          typename T2,
          typename std::enable_if<std::is_signed<T1>::value &&
                                  std::is_unsigned<T2>::value &&
                                  !LargerInt<T2, T1>::value>::type* = nullptr>
inline bool Cmp(T1 a, T2 b) {
  return a < 0 ? Op::Op(-1, 0) : Op::Op(safe_cmp_impl::MakeUnsigned(a), b);
}

// Overload for unsigned - signed comparison that can't be promoted to a bigger
// signed type.
template <typename Op,
          typename T1,
          typename T2,
          typename std::enable_if<std::is_unsigned<T1>::value &&
                                  std::is_signed<T2>::value &&
                                  !LargerInt<T1, T2>::value>::type* = nullptr>
inline bool Cmp(T1 a, T2 b) {
  return b < 0 ? Op::Op(0, -1) : Op::Op(a, safe_cmp_impl::MakeUnsigned(b));
}

#define RTC_SAFECMP_MAKE_OP(name, op)      \
  struct name {                            \
    template <typename T1, typename T2>    \
    static constexpr bool Op(T1 a, T2 b) { \
      return a op b;                       \
    }                                      \
  };
RTC_SAFECMP_MAKE_OP(EqOp, ==)
RTC_SAFECMP_MAKE_OP(NeOp, !=)
RTC_SAFECMP_MAKE_OP(LtOp, <)
RTC_SAFECMP_MAKE_OP(LeOp, <=)
RTC_SAFECMP_MAKE_OP(GtOp, >)
RTC_SAFECMP_MAKE_OP(GeOp, >=)
#undef RTC_SAFECMP_MAKE_OP

// Determines if the given type is an enum that converts implicitly to
// an integral type.
template <typename T>
struct IsIntEnum {
 private:
  // This overload is used if the type is an enum, and unary plus
  // compiles and turns it into an integral type.
  template <typename X,
            typename std::enable_if<
                std::is_enum<X>::value &&
                std::is_integral<decltype(+std::declval<X>())>::value>::type* =
                nullptr>
  static int Test(int);

  // Otherwise, this overload is used.
  template <typename>
  static char Test(...);

 public:
  static constexpr bool value =
      std::is_same<decltype(Test<typename std::remove_reference<T>::type>(0)),
                   int>::value;
};

// Determines if the given type is integral, or an enum that
// converts implicitly to an integral type.
template <typename T>
struct IsIntlike {
 private:
  using X = typename std::remove_reference<T>::type;

 public:
  static constexpr bool value =
      std::is_integral<X>::value || IsIntEnum<X>::value;
};

namespace test_enum_intlike {

enum E1 { e1 };
enum { e2 };
enum class E3 { e3 };
struct S {};

static_assert(IsIntEnum<E1>::value, "");
static_assert(IsIntEnum<decltype(e2)>::value, "");
static_assert(!IsIntEnum<E3>::value, "");
static_assert(!IsIntEnum<int>::value, "");
static_assert(!IsIntEnum<float>::value, "");
static_assert(!IsIntEnum<S>::value, "");

static_assert(IsIntlike<E1>::value, "");
static_assert(IsIntlike<decltype(e2)>::value, "");
static_assert(!IsIntlike<E3>::value, "");
static_assert(IsIntlike<int>::value, "");
static_assert(!IsIntlike<float>::value, "");
static_assert(!IsIntlike<S>::value, "");

}  // test_enum_intlike
}  // namespace safe_cmp_impl

#define RTC_SAFECMP_MAKE_FUN(name)                                      \
  template <typename T1, typename T2,                                   \
            typename std::enable_if<                                    \
                safe_cmp_impl::IsIntlike<T1>::value &&                  \
                safe_cmp_impl::IsIntlike<T2>::value>::type* = nullptr>  \
  inline bool name(T1 a, T2 b) {                                        \
    /* Unary plus here turns enums into real integral types. */         \
    return safe_cmp_impl::Cmp<safe_cmp_impl::name##Op>(+a, +b);         \
  }                                                                     \
  template <typename T1, typename T2,                                   \
            typename std::enable_if<                                    \
                !safe_cmp_impl::IsIntlike<T1>::value ||                 \
                !safe_cmp_impl::IsIntlike<T2>::value>::type* = nullptr> \
  inline bool name(T1&& a, T2&& b) {                                    \
    return safe_cmp_impl::name##Op::Op(a, b);                           \
  }
RTC_SAFECMP_MAKE_FUN(Eq)
RTC_SAFECMP_MAKE_FUN(Ne)
RTC_SAFECMP_MAKE_FUN(Lt)
RTC_SAFECMP_MAKE_FUN(Le)
RTC_SAFECMP_MAKE_FUN(Gt)
RTC_SAFECMP_MAKE_FUN(Ge)
#undef RTC_SAFECMP_MAKE_FUN

}  // namespace safe_cmp
}  // namespace rtc

#endif  // WEBRTC_BASE_SAFE_COMPARE_H_
