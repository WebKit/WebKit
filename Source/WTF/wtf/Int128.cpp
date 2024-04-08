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

#include "config.h"
#include <wtf/Int128.h>

#include <stddef.h>
#include <cassert>
#include <iomanip>
#include <ostream>  // NOLINT(readability/streams)
#include <sstream>
#include <string>
#include <type_traits>
#include <wtf/MathExtras.h>
#include <wtf/PrintStream.h>
#include <wtf/Vector.h>
#include <wtf/text/IntegerToStringConversion.h>

namespace WTF {

namespace {

// Returns the 0-based position of the last set bit (i.e., most significant bit)
// in the given UInt128Impl. The argument is not 0.
//
// For example:
//   Given: 5 (decimal) == 101 (binary)
//   Returns: 2
static ALWAYS_INLINE int Fls128(UInt128Impl n) {
  if (uint64_t hi = UInt128High64(n)) {
    ASSERT(hi != 0);
    return 127 - clz(hi);
  }
  const uint64_t low = UInt128Low64(n);
  ASSERT(low != 0);
  return 63 - clz(low);
}

// Long division/modulo for UInt128Impl implemented using the shift-subtract
// division algorithm adapted from:
// https://stackoverflow.com/questions/5386377/division-without-using
static inline void DivModImpl(UInt128Impl dividend, UInt128Impl divisor, UInt128Impl* quotient_ret, UInt128Impl* remainder_ret) {
  assert(divisor != 0);

  if (divisor > dividend) {
    *quotient_ret = 0;
    *remainder_ret = dividend;
    return;
  }

  if (divisor == dividend) {
    *quotient_ret = 1;
    *remainder_ret = 0;
    return;
  }

  UInt128Impl denominator = divisor;
  UInt128Impl quotient = 0;

  // Left aligns the MSB of the denominator and the dividend.
  const int shift = Fls128(dividend) - Fls128(denominator);
  denominator <<= shift;

  // Uses shift-subtract algorithm to divide dividend by denominator. The
  // remainder will be left in dividend.
  for (int i = 0; i <= shift; ++i) {
    quotient <<= 1;
    if (dividend >= denominator) {
      dividend -= denominator;
      quotient |= 1;
    }
    denominator >>= 1;
  }

  *quotient_ret = quotient;
  *remainder_ret = dividend;
}

template <typename T>
static UInt128Impl MakeUInt128FromFloat(T v) {
  static_assert(std::is_floating_point<T>::value);

  // Rounding behavior is towards zero, same as for built-in types.

  // Undefined behavior if v is NaN or cannot fit into UInt128Impl.
  assert(std::isfinite(v) && v > -1 &&
         (std::numeric_limits<T>::max_exponent <= 128 ||
          v < std::ldexp(static_cast<T>(1), 128)));

  if (v >= std::ldexp(static_cast<T>(1), 64)) {
    uint64_t hi = static_cast<uint64_t>(std::ldexp(v, -64));
    uint64_t lo = static_cast<uint64_t>(v - std::ldexp(static_cast<T>(hi), 64));
    return MakeUInt128(hi, lo);
  }

  return MakeUInt128(0, static_cast<uint64_t>(v));
}

}  // namespace

UInt128Impl::UInt128Impl(float v) : UInt128Impl(MakeUInt128FromFloat(v)) {}
UInt128Impl::UInt128Impl(double v) : UInt128Impl(MakeUInt128FromFloat(v)) {}
UInt128Impl::UInt128Impl(long double v) : UInt128Impl(MakeUInt128FromFloat(v)) {}

UInt128Impl operator/(UInt128Impl lhs, UInt128Impl rhs) {
  UInt128Impl quotient = 0;
  UInt128Impl remainder = 0;
  DivModImpl(lhs, rhs, &quotient, &remainder);
  return quotient;
}

UInt128Impl operator%(UInt128Impl lhs, UInt128Impl rhs) {
  UInt128Impl quotient = 0;
  UInt128Impl remainder = 0;
  DivModImpl(lhs, rhs, &quotient, &remainder);
  return remainder;
}

namespace {

static std::string UInt128ToFormattedString(UInt128Impl v, std::ios_base::fmtflags flags) {
  // Select a divisor which is the largest power of the base < 2^64.
  UInt128Impl div;
  int div_base_log;
  switch (flags & std::ios::basefield) {
    case std::ios::hex:
      div = 0x1000000000000000;  // 16^15
      div_base_log = 15;
      break;
    case std::ios::oct:
      div = 01000000000000000000000;  // 8^21
      div_base_log = 21;
      break;
    default:  // std::ios::dec
      div = 10000000000000000000u;  // 10^19
      div_base_log = 19;
      break;
  }

  // Now piece together the UInt128Impl representation from three chunks of the
  // original value, each less than "div" and therefore representable as a
  // uint64_t.
  std::ostringstream os;
  std::ios_base::fmtflags copy_mask =
      std::ios::basefield | std::ios::showbase | std::ios::uppercase;
  os.setf(flags & copy_mask, copy_mask);
  UInt128Impl high = v;
  UInt128Impl low;
  DivModImpl(high, div, &high, &low);
  UInt128Impl mid;
  DivModImpl(high, div, &high, &mid);
  if (UInt128Low64(high) != 0) {
    os << UInt128Low64(high);
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
    os << UInt128Low64(mid);
    os << std::setw(div_base_log);
  } else if (UInt128Low64(mid) != 0) {
    os << UInt128Low64(mid);
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
  }
  os << UInt128Low64(low);
  return os.str();
}

}  // namespace

std::ostream& operator<<(std::ostream& os, UInt128Impl v) {
  std::ios_base::fmtflags flags = os.flags();
  std::string rep = UInt128ToFormattedString(v, flags);

  // Add the requisite padding.
  std::streamsize width = os.width(0);
  if (static_cast<size_t>(width) > rep.size()) {
    std::ios::fmtflags adjustfield = flags & std::ios::adjustfield;
    if (adjustfield == std::ios::left) {
      rep.append(width - rep.size(), os.fill());
    } else if (adjustfield == std::ios::internal &&
               (flags & std::ios::showbase) &&
               (flags & std::ios::basefield) == std::ios::hex && v != 0) {
      rep.insert(2, width - rep.size(), os.fill());
    } else {
      rep.insert(0, width - rep.size(), os.fill());
    }
  }

  return os << rep;
}

namespace {

static UInt128Impl UnsignedAbsoluteValue(Int128Impl v) {
  // Cast to UInt128Impl before possibly negating because -Int128Min() is undefined.
  return Int128High64(v) < 0 ? -UInt128Impl(v) : UInt128Impl(v);
}

}  // namespace

namespace {

template <typename T>
static Int128Impl MakeInt128FromFloat(T v) {
  // Conversion when v is NaN or cannot fit into Int128Impl would be undefined
  // behavior if using an intrinsic 128-bit integer.
  assert(std::isfinite(v) && (std::numeric_limits<T>::max_exponent <= 127 ||
                              (v >= -std::ldexp(static_cast<T>(1), 127) &&
                               v < std::ldexp(static_cast<T>(1), 127))));

  // We must convert the absolute value and then negate as needed, because
  // floating point types are typically sign-magnitude. Otherwise, the
  // difference between the high and low 64 bits when interpreted as two's
  // complement overwhelms the precision of the mantissa.
  UInt128Impl result = v < 0 ? -MakeUInt128FromFloat(-v) : MakeUInt128FromFloat(v);
  return MakeInt128(int128_internal::BitCastToSigned(UInt128High64(result)),
                    UInt128Low64(result));
}

}  // namespace

Int128Impl::Int128Impl(float v) : Int128Impl(MakeInt128FromFloat(v)) {}
Int128Impl::Int128Impl(double v) : Int128Impl(MakeInt128FromFloat(v)) {}
Int128Impl::Int128Impl(long double v) : Int128Impl(MakeInt128FromFloat(v)) {}

Int128Impl operator/(Int128Impl lhs, Int128Impl rhs) {
  assert(lhs != Int128Min() || rhs != -1);  // UB on two's complement.

  UInt128Impl quotient = 0;
  UInt128Impl remainder = 0;
  DivModImpl(UnsignedAbsoluteValue(lhs), UnsignedAbsoluteValue(rhs),
             &quotient, &remainder);
  if ((Int128High64(lhs) < 0) != (Int128High64(rhs) < 0)) quotient = -quotient;
  return MakeInt128(int128_internal::BitCastToSigned(UInt128High64(quotient)),
                    UInt128Low64(quotient));
}

Int128Impl operator%(Int128Impl lhs, Int128Impl rhs) {
  assert(lhs != Int128Min() || rhs != -1);  // UB on two's complement.

  UInt128Impl quotient = 0;
  UInt128Impl remainder = 0;
  DivModImpl(UnsignedAbsoluteValue(lhs), UnsignedAbsoluteValue(rhs),
             &quotient, &remainder);
  if (Int128High64(lhs) < 0) remainder = -remainder;
  return MakeInt128(int128_internal::BitCastToSigned(UInt128High64(remainder)),
                    UInt128Low64(remainder));
}

std::ostream& operator<<(std::ostream& os, Int128Impl v) {
  std::ios_base::fmtflags flags = os.flags();
  std::string rep;

  // Add the sign if needed.
  bool print_as_decimal =
      (flags & std::ios::basefield) == std::ios::dec ||
      (flags & std::ios::basefield) == std::ios_base::fmtflags();
  if (print_as_decimal) {
    if (Int128High64(v) < 0) {
      rep.append("-");
    } else if (flags & std::ios::showpos) {
      rep.append("+");
    }
  }

  rep.append(UInt128ToFormattedString(
      print_as_decimal ? UnsignedAbsoluteValue(v) : UInt128Impl(v), os.flags()));

  // Add the requisite padding.
  std::streamsize width = os.width(0);
  if (static_cast<size_t>(width) > rep.size()) {
    switch (flags & std::ios::adjustfield) {
      case std::ios::left:
        rep.append(width - rep.size(), os.fill());
        break;
      case std::ios::internal:
        if (print_as_decimal && (rep[0] == '+' || rep[0] == '-')) {
          rep.insert(1, width - rep.size(), os.fill());
        } else if ((flags & std::ios::basefield) == std::ios::hex &&
                   (flags & std::ios::showbase) && v != 0) {
          rep.insert(2, width - rep.size(), os.fill());
        } else {
          rep.insert(0, width - rep.size(), os.fill());
        }
        break;
      default:  // std::ios::right
        rep.insert(0, width - rep.size(), os.fill());
        break;
    }
  }

  return os << rep;
}

void printInternal(PrintStream& out, UInt128 value)
{
    auto vector = numberToStringUnsigned<Vector<LChar, 50>>(value);
    vector.append('\0');
    out.printf("%s", bitwise_cast<const char*>(vector.data()));
}

void printInternal(PrintStream& out, Int128 value)
{
    if (value >= 0) {
        printInternal(out, static_cast<UInt128>(value));
        return;
    }
    UInt128 positive;
    if (value == std::numeric_limits<Int128>::min())
        positive = static_cast<UInt128>(0x8000'0000'0000'0000ULL) << 64;
    else
        positive = -value;
    auto vector = numberToStringUnsigned<Vector<LChar, 50>>(positive);
    vector.append('\0');
    out.printf("-%s", bitwise_cast<const char*>(vector.data()));
}

}  // namespace WTF

namespace std {
constexpr bool numeric_limits<WTF::UInt128Impl>::is_specialized;
constexpr bool numeric_limits<WTF::UInt128Impl>::is_signed;
constexpr bool numeric_limits<WTF::UInt128Impl>::is_integer;
constexpr bool numeric_limits<WTF::UInt128Impl>::is_exact;
constexpr bool numeric_limits<WTF::UInt128Impl>::has_infinity;
constexpr bool numeric_limits<WTF::UInt128Impl>::has_quiet_NaN;
constexpr bool numeric_limits<WTF::UInt128Impl>::has_signaling_NaN;
constexpr float_denorm_style numeric_limits<WTF::UInt128Impl>::has_denorm;
constexpr bool numeric_limits<WTF::UInt128Impl>::has_denorm_loss;
constexpr float_round_style numeric_limits<WTF::UInt128Impl>::round_style;
constexpr bool numeric_limits<WTF::UInt128Impl>::is_iec559;
constexpr bool numeric_limits<WTF::UInt128Impl>::is_bounded;
constexpr bool numeric_limits<WTF::UInt128Impl>::is_modulo;
constexpr int numeric_limits<WTF::UInt128Impl>::digits;
constexpr int numeric_limits<WTF::UInt128Impl>::digits10;
constexpr int numeric_limits<WTF::UInt128Impl>::max_digits10;
constexpr int numeric_limits<WTF::UInt128Impl>::radix;
constexpr int numeric_limits<WTF::UInt128Impl>::min_exponent;
constexpr int numeric_limits<WTF::UInt128Impl>::min_exponent10;
constexpr int numeric_limits<WTF::UInt128Impl>::max_exponent;
constexpr int numeric_limits<WTF::UInt128Impl>::max_exponent10;
constexpr bool numeric_limits<WTF::UInt128Impl>::traps;
constexpr bool numeric_limits<WTF::UInt128Impl>::tinyness_before;

constexpr bool numeric_limits<WTF::Int128Impl>::is_specialized;
constexpr bool numeric_limits<WTF::Int128Impl>::is_signed;
constexpr bool numeric_limits<WTF::Int128Impl>::is_integer;
constexpr bool numeric_limits<WTF::Int128Impl>::is_exact;
constexpr bool numeric_limits<WTF::Int128Impl>::has_infinity;
constexpr bool numeric_limits<WTF::Int128Impl>::has_quiet_NaN;
constexpr bool numeric_limits<WTF::Int128Impl>::has_signaling_NaN;
constexpr float_denorm_style numeric_limits<WTF::Int128Impl>::has_denorm;
constexpr bool numeric_limits<WTF::Int128Impl>::has_denorm_loss;
constexpr float_round_style numeric_limits<WTF::Int128Impl>::round_style;
constexpr bool numeric_limits<WTF::Int128Impl>::is_iec559;
constexpr bool numeric_limits<WTF::Int128Impl>::is_bounded;
constexpr bool numeric_limits<WTF::Int128Impl>::is_modulo;
constexpr int numeric_limits<WTF::Int128Impl>::digits;
constexpr int numeric_limits<WTF::Int128Impl>::digits10;
constexpr int numeric_limits<WTF::Int128Impl>::max_digits10;
constexpr int numeric_limits<WTF::Int128Impl>::radix;
constexpr int numeric_limits<WTF::Int128Impl>::min_exponent;
constexpr int numeric_limits<WTF::Int128Impl>::min_exponent10;
constexpr int numeric_limits<WTF::Int128Impl>::max_exponent;
constexpr int numeric_limits<WTF::Int128Impl>::max_exponent10;
constexpr bool numeric_limits<WTF::Int128Impl>::traps;
constexpr bool numeric_limits<WTF::Int128Impl>::tinyness_before;
}  // namespace std
