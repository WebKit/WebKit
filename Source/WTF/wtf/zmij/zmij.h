// A double-to-string conversion library: https://github.com/vitaut/zmij/
//
// Copyright (c) 2025 - present, Victor Zverovich
// Distributed under the MIT license (see LICENSE) or alternatively
// the Boost Software License, Version 1.0.

#ifndef ZMIJ_H_
#define ZMIJ_H_

#include <assert.h>  // assert
#include <float.h>   // DBL_MANT_DIG, LDBL_MANT_DIG
#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t
#include <string.h>  // memcpy

#include <limits>       // std::numeric_limits
#include <type_traits>  // std::conditional

#ifdef _MSVC_LANG
#  define ZMIJ_CPLUSPLUS _MSVC_LANG
#else
#  define ZMIJ_CPLUSPLUS __cplusplus
#endif

#if ZMIJ_CPLUSPLUS >= 202002L
#  include <bit>  // std::bit_cast
#endif

#ifdef _MSC_VER
#  include <intrin.h>  // __lzcnt64/_umul128/__umulh
#endif

#ifdef __cpp_lib_is_constant_evaluated
#  define ZMIJ_CONSTEXPR20 constexpr
#else
#  define ZMIJ_CONSTEXPR20
#endif

// Disabling C++14 relaxed constexpr costs ~2x, so it is opt-in, not detected.
#ifndef ZMIJ_USE_CONSTEXPR
#  define ZMIJ_USE_CONSTEXPR 1
#endif
#if ZMIJ_USE_CONSTEXPR
#  define ZMIJ_CONSTEXPR constexpr
#  define ZMIJ_DEFAULT(...) = __VA_ARGS__
#else
#  define ZMIJ_CONSTEXPR
// A default member initializer makes its class a non-aggregate in C++11.
#  define ZMIJ_DEFAULT(...)
#endif

namespace zmij {

enum {
  nonfinite_exp = int(~0u >> 1),
};

// A decimal floating-point number (negative ? -1 : 1) * sig * pow(10, exp).
// If exp is nonfinite_exp then the number is a NaN or an infinity.
template <typename Sig = uint64_t> struct dec_fp {
  Sig sig;  // significand
  int exp;  // exponent
  bool negative;
};

// Floating-point formatting style. Values match std::chars_format, so
// general == fixed | scientific.
enum class format {
  scientific = 1,
  fixed = 2,
  general = 3,
  hex = 4,
};

// Minimum buffer sizes for the shortest `write`, one per floating-point type.
enum {
  float_buffer_size = 17,
  double_buffer_size = 34,
  // Worst case is IEEE binary128: 1 sign + 36 digits + '.' + "e-dddd".
  long_double_buffer_size = 44,
};

namespace detail {

constexpr auto is_constant_evaluated() noexcept -> bool {
#ifdef __cpp_lib_is_constant_evaluated
  return std::is_constant_evaluated();
#else
  return false;
#endif
}

struct uint128 {
  uint64_t lo;
  uint64_t hi;

  uint128() = default;
  constexpr uint128(uint64_t hi, uint64_t lo) noexcept : lo(lo), hi(hi) {}
  constexpr uint128(uint64_t lo) noexcept : lo(lo), hi(0) {}

  explicit constexpr operator uint64_t() const noexcept { return lo; }

  constexpr auto operator+(uint128 rhs) const noexcept -> uint128 {
    return {hi + rhs.hi + (lo + rhs.lo < lo), lo + rhs.lo};
  }
  constexpr auto operator-(uint128 rhs) const noexcept -> uint128 {
    return {hi - rhs.hi - (lo - rhs.lo > lo), lo - rhs.lo};
  }
  constexpr auto operator%(uint32_t d) const noexcept -> uint64_t {
    // (UINT64_MAX % d + 1) % d is 2**64 mod d, which fits in 32 bits.
    return ((hi % d) * ((UINT64_MAX % d + 1) % d) + lo % d) % d;
  }
  constexpr auto operator&(uint128 rhs) const noexcept -> uint128 {
    return {hi & rhs.hi, lo & rhs.lo};
  }
  constexpr auto operator|(uint128 rhs) const noexcept -> uint128 {
    return {hi | rhs.hi, lo | rhs.lo};
  }
  constexpr auto operator^(uint128 rhs) const noexcept -> uint128 {
    return {hi ^ rhs.hi, lo ^ rhs.lo};
  }
  constexpr auto operator<<(int s) const noexcept -> uint128 {
    return s == 0    ? *this
           : s < 64  ? uint128(hi << s | lo >> (64 - s), lo << s)
           : s < 128 ? uint128(lo << (s - 64), 0)
                     : uint128(0, 0);
  }
  constexpr auto operator>>(int s) const noexcept -> uint128 {
    return s == 0    ? *this
           : s < 64  ? uint128(hi >> s, lo >> s | hi << (64 - s))
           : s < 128 ? uint128(0, hi >> (s - 64))
                     : uint128(0, 0);
  }

  constexpr auto operator==(uint128 rhs) const noexcept -> bool {
    return hi == rhs.hi && lo == rhs.lo;
  }
  constexpr auto operator!=(uint128 rhs) const noexcept -> bool {
    return hi != rhs.hi || lo != rhs.lo;
  }
  constexpr auto operator<(uint128 rhs) const noexcept -> bool {
    return hi != rhs.hi ? hi < rhs.hi : lo < rhs.lo;
  }
  constexpr auto operator>(uint128 rhs) const noexcept -> bool {
    return rhs < *this;
  }
  constexpr auto operator<=(uint128 rhs) const noexcept -> bool {
    return !(rhs < *this);
  }
  constexpr auto operator>=(uint128 rhs) const noexcept -> bool {
    return !(*this < rhs);
  }

  auto operator++() noexcept -> uint128& {
    if (++lo == 0) ++hi;
    return *this;
  }
};

#ifdef ZMIJ_USE_INT128
// Use the provided definition.
#elif defined(__SIZEOF_INT128__)
#  define ZMIJ_USE_INT128 1
#else
#  define ZMIJ_USE_INT128 0
#endif

#if ZMIJ_USE_INT128
using uint128_t = unsigned __int128;
#else
using uint128_t = uint128;
#endif  // ZMIJ_USE_INT128

// Computes 128-bit result of multiplication of two 64-bit unsigned integers.
inline ZMIJ_CONSTEXPR auto umul128(uint64_t x, uint64_t y) noexcept
    -> uint128_t {
#if ZMIJ_USE_INT128
  return uint128_t(x) * y;
#else
#  ifdef __cpp_lib_is_constant_evaluated
  if (!std::is_constant_evaluated()) {
#    if defined(_M_AMD64)
    uint64_t hi = 0;
    uint64_t lo = _umul128(x, y, &hi);
    return {hi, lo};
#    elif defined(_M_ARM64)
    return {__umulh(x, y), x * y};
#    endif
  }
#  endif
  uint64_t a = x >> 32;
  uint64_t b = uint32_t(x);
  uint64_t c = y >> 32;
  uint64_t d = uint32_t(y);

  uint64_t ac = a * c;
  uint64_t bc = b * c;
  uint64_t ad = a * d;
  uint64_t bd = b * d;

  uint64_t cs = (bd >> 32) + uint32_t(ad) + uint32_t(bc);  // cross sum
  return {ac + (ad >> 32) + (bc >> 32) + (cs >> 32), (cs << 32) + uint32_t(bd)};
#endif  // ZMIJ_USE_INT128
}

inline ZMIJ_CONSTEXPR auto umul192_hi128(uint64_t x_hi, uint64_t x_lo,
                                         uint64_t y) noexcept -> uint128 {
  uint128_t p = umul128(x_hi, y);
  uint64_t lo = uint64_t(p) + uint64_t(umul128(x_lo, y) >> 64);
  return {uint64_t(p >> 64) + (lo < uint64_t(p)), lo};
}

// Computes the decimal exponent as floor(log10(2**bin_exp)) if regular or
// floor(log10(3/4 * 2**bin_exp)) otherwise, without branching.
constexpr auto compute_dec_exp(int bin_exp, bool regular = true) noexcept
    -> int {
  // 315653 is round(log10(2) * 2**20) and 131072 is -log10(3/4) * 2**20
  // rounded to a power of two.
  return assert(bin_exp >= -1334 && bin_exp <= 2620),
         (bin_exp * 315653 - !regular * 131072) >> 20;
}

constexpr auto ilog2(int n) noexcept -> int {
  return n > 1 ? 1 + ilog2(n >> 1) : 0;
}

template <typename Float> struct float_traits : std::numeric_limits<Float> {
  static_assert(float_traits::is_iec559, "IEEE 754 required");

  // x87 80-bit stores the integer bit explicitly (digits == 64), so its
  // exponent sits one bit higher than the implicit-bit binary32/64/128 layouts.
  static constexpr int num_sig_bits = float_traits::digits - 1;
  static constexpr int num_exp_bits = ilog2(float_traits::max_exponent) + 1;
  static constexpr int exp_shift =
      num_sig_bits + (float_traits::digits == 64 ? 1 : 0);
  static constexpr int num_bits = exp_shift + num_exp_bits + 1;
  static constexpr int exp_mask = (1 << num_exp_bits) - 1;
  static constexpr int exp_bias = (1 << (num_exp_bits - 1)) - 1;
  static constexpr int exp_offset = exp_bias + num_sig_bits;
  static constexpr int min_fixed_dec_exp = -4;
  static constexpr int max_fixed_dec_exp =
      compute_dec_exp(float_traits::digits + 1) - 1;

  using sig_type = typename std::conditional<
      num_bits <= 32, uint32_t,
      typename std::conditional<num_bits <= 64, uint64_t,
                                uint128_t>::type>::type;
  static constexpr sig_type implicit_bit = sig_type(1) << num_sig_bits;

  // Bounds for the exact big-integer path (write_big): the significand times
  // 5**k (k up to exp_offset + num_sig_bits - 1) fits in big_bits bits, hence
  // big_limbs base-2**32 limbs and big_digits decimal digits. 2322/1000 and
  // 30103/100000 over-approximate log2(5) and log10(2); the +2 rounds the digit
  // count up and reserves one carry digit.
  static constexpr int big_bits =
      num_sig_bits + 2 + (exp_offset + num_sig_bits - 1) * 2322 / 1000;
  static constexpr int big_limbs = (big_bits + 31) / 32;
  static constexpr int big_digits = big_bits * 30103 / 100000 + 2;

  static ZMIJ_CONSTEXPR20 auto to_bits(Float value) noexcept -> sig_type {
#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
    if (std::is_constant_evaluated()) {
      if constexpr (sizeof(sig_type) == sizeof(Float))
        return std::bit_cast<sig_type>(value);
    }
#endif
    sig_type bits = sig_type();
    memcpy(&bits, &value, sizeof(value));
    return bits;
  }

  static constexpr auto is_negative(sig_type bits) noexcept -> bool {
    return ((bits >> (num_bits - 1)) & 1) != 0;
  }
  static constexpr auto get_sig(sig_type bits) noexcept -> sig_type {
    return bits & (implicit_bit - 1);
  }
  static constexpr auto get_exp(sig_type bits) noexcept -> int64_t {
    return int64_t(uint64_t(bits >> exp_shift) & unsigned(exp_mask));
  }
  static constexpr auto is_normal(int64_t bin_exp) noexcept -> bool {
    // Fold subnormal/zero (exp 0) and inf/nan (exp exp_mask) into one check.
    return unsigned(bin_exp - 1) < unsigned(exp_mask - 1);
  }
};

#ifndef __cpp_inline_variables
template <typename Float>
constexpr
    typename float_traits<Float>::sig_type float_traits<Float>::implicit_bit;
#endif

// Compressed 128-bit significands of powers of 10.
template <typename = void> struct pow10_data {
  static constexpr uint64_t minor[] = {
      0x8000000000000000, 0xa000000000000000, 0xc800000000000000,
      0xfa00000000000000, 0x9c40000000000000, 0xc350000000000000,
      0xf424000000000000, 0x9896800000000000, 0xbebc200000000000,
      0xee6b280000000000, 0x9502f90000000000, 0xba43b74000000000,
      0xe8d4a51000000000, 0x9184e72a00000000, 0xb5e620f480000000,
      0xe35fa931a0000000, 0x8e1bc9bf04000000, 0xb1a2bc2ec5000000,
      0xde0b6b3a76400000, 0x8ac7230489e80000, 0xad78ebc5ac620000,
      0xd8d726b7177a8000, 0x878678326eac9000, 0xa968163f0a57b400,
      0xd3c21bcecceda100, 0x84595161401484a0, 0xa56fa5b99019a5c8,
      0xcecb8f27f4200f3a,
  };
  static constexpr uint128 major[] = {
      {0xaddcb9e83c6b1793, 0xdf4abe242a1bbf3e},  // -331 (+1 ULP: see fixups)
      {0xaf8e5410288e1b6f, 0x07ecf0ae5ee44dda},  // -303
      {0xb1442798f49ffb4a, 0x99cd11cfdf41779d},  // -275
      {0xb2fe3f0b8599ef07, 0x861fa7e6dcb4aa15},  // -247
      {0xb4bca50b065abe63, 0x0fed077a756b53aa},  // -219
      {0xb67f6455292cbf08, 0x1a3bc84c17b1d543},  // -191
      {0xb84687c269ef3bfb, 0x3d5d514f40eea742},  // -163
      {0xba121a4650e4ddeb, 0x92f34d62616ce413},  // -135
      {0xbbe226efb628afea, 0x890489f70a55368c},  // -107
      {0xbdb6b8e905cb600f, 0x5400e987bbc1c921},  //  -79
      {0xbf8fdb78849a5f96, 0xde98520472bdd034},  //  -51
      {0xc16d9a0095928a27, 0x75b7053c0f178294},  //  -23
      {0xc350000000000000, 0x0000000000000000},  //    5
      {0xc5371912364ce305, 0x6c28000000000000},  //   33
      {0xc722f0ef9d80aad6, 0x424d3ad2b7b97ef6},  //   61
      {0xc913936dd571c84c, 0x03bc3a19cd1e38ea},  //   89
      {0xcb090c8001ab551c, 0x5cadf5bfd3072cc6},  //  117
      {0xcd036837130890a1, 0x36dba887c37a8c10},  //  145
      {0xcf02b2c21207ef2e, 0x94f967e45e03f4bc},  //  173
      {0xd106f86e69d785c7, 0xe13336d701beba52},  //  201
      {0xd31045a8341ca07c, 0x1ede48111209a051},  //  229
      {0xd51ea6fa85785631, 0x552a74227f3ea566},  //  257
      {0xd732290fbacaf133, 0xa97c177947ad4096},  //  285
      {0xd94ad8b1c7380874, 0x18375281ae7822bd},  //  313 (+1 ULP: see fixups)
      {0xdb68c2ca82ed2a05, 0xa67398db9f6820e1},  //  341
  };
  static constexpr uint32_t fixups[] = {
      0x8d8fc810, 0x06100293, 0x19000000, 0x00100000, 0x00000908, 0x00000000,
      0x04e00300, 0x3807e0b2, 0x3d83d793, 0x0006f5cc, 0x00000000, 0xffff0000,
      0x8076337d, 0x4ff45ba0, 0x09405033, 0x034376d9, 0x09000000, 0x4e100501,
      0x076d14dc, 0xf964f45e, 0x0000003d};
};

#ifndef __cpp_inline_variables
template <typename T> constexpr uint64_t pow10_data<T>::minor[];
template <typename T> constexpr uint128 pow10_data<T>::major[];
template <typename T> constexpr uint32_t pow10_data<T>::fixups[];
#endif

// Computes the 128-bit significand of 10**exp rounded down using method by
// Dougall Johnson.
inline ZMIJ_CONSTEXPR auto compute_pow10(int exp) noexcept -> uint128 {
  using data = pow10_data<>;
  constexpr int min_exp = -307;
  constexpr int stride = sizeof(data::minor) / sizeof(*data::minor);
  assert(exp >= min_exp && exp <= 341);
  unsigned i = unsigned(exp - min_exp);
  uint64_t m = data::minor[(i + 24) % stride];
  uint128 h = data::major[(i + 24) / stride];

  uint64_t h1 = uint64_t(umul128(h.lo, m) >> 64);
  uint64_t c0 = h.lo * m;
  uint64_t c1 = h1 + h.hi * m;
  uint64_t c2 = (c1 < h1) + uint64_t(umul128(h.hi, m) >> 64);

  uint128 result = (c2 >> 63) != 0
                       ? uint128{c2, c1}
                       : uint128{c2 << 1 | c1 >> 63, c1 << 1 | c0 >> 63};
  result.lo -= (data::fixups[i >> 5] >> (i & 31)) & 1;
  return result;
}

// Computes a shift so that, after scaling by a power of 10, the intermediate
// result always has a fixed 128-bit fractional part (for float and double).
//
// Different binary exponents can map to the same decimal exponent, but place
// the decimal point at different bit positions. The shift compensates for this.
//
// For example, both 3 * 2**59 and 3 * 2**60 have dec_exp = 2, but dividing by
// 10^dec_exp puts the decimal point in different bit positions:
//   3 * 2**59 / 100 = 1.72...e+16  (needs shift = 1 + 1)
//   3 * 2**60 / 100 = 3.45...e+16  (needs shift = 2 + 1)
inline ZMIJ_CONSTEXPR auto compute_exp_shift(int bin_exp,
                                             int dec_exp) noexcept
    -> signed char {
  assert(dec_exp >= -350 && dec_exp <= 350);
  // log2_pow10_sig = round(log2(10) * 2**log2_pow10_exp) + 1
  constexpr int log2_pow10_sig = 217707, log2_pow10_exp = 16;
  // pow10_bin_exp = floor(log2(10**-dec_exp))
  int pow10_bin_exp = -dec_exp * log2_pow10_sig >> log2_pow10_exp;
  // pow10 = ((pow10_hi << 64) | pow10_lo) * 2**(pow10_bin_exp - 127)
  return static_cast<signed char>(bin_exp + pow10_bin_exp + 1);
}

// Converts the nonzero finite binary value bin_sig * 2**bin_exp using yy.
template <typename Float>
inline ZMIJ_CONSTEXPR20 auto to_decimal(uint64_t bin_sig,
                                        int bin_exp) noexcept -> dec_fp<> {
  assert(bin_sig != 0);
  constexpr uint64_t implicit_bit = float_traits<Float>::implicit_bit;
  bool irregular = bin_sig == implicit_bit;
  int dec_exp = compute_dec_exp(bin_exp, !irregular);
  int shift = compute_exp_shift(bin_exp, dec_exp) - 1;
  uint128 pow10 = compute_pow10(-dec_exp);
  uint128 p = umul192_hi128(pow10.hi, pow10.lo, bin_sig << (shift + 1));

  uint64_t one = p.hi % 10;
  uint64_t ten = p.hi - one;
  uint64_t c = one << 60 | p.lo >> 4;
  uint64_t half_ulp = pow10.hi >> (4 - shift);

  bool round_u1 = false;
  bool round_d0 = false;
  if (irregular) {
    bool round_d1 = (half_ulp >> 1) >= (p.lo >> 4);
    round_u1 = p.lo > (uint64_t(1) << 63) || !round_d1;
    round_d0 = (half_ulp >> 1) >= c;
  } else {
    uint64_t half = uint64_t(1) << 63;
    round_u1 = p.lo == half ? (p.hi & 1) != 0 : p.lo > half;
    round_d0 = half_ulp == c ? (bin_sig & 1) == 0 : c < half_ulp;
  }

  uint64_t t0 = uint64_t(10) << 60;
  uint64_t t1 = c + half_ulp;
  bool even = (bin_sig & 1) == 0;
  bool round_u0 = t0 <= t1;
  // Exact trim-up boundaries round toward the even binary significand.
  if (t1 + 1 == t0 || (dec_exp == 0 && t1 == t0)) round_u0 = even;

  uint64_t dec_one = p.hi + round_u1;
  uint64_t dec_ten = ten + (round_u0 ? 10 : 0);
  return {round_d0 || round_u0 ? dec_ten : dec_one, dec_exp, false};
}

inline ZMIJ_CONSTEXPR20 auto copy_n(char* out, const char* in,
                                    size_t count) noexcept -> char* {
  for (size_t i = 0; i < count; ++i) *out++ = *in++;
  return out;
}

template <typename Float>
inline ZMIJ_CONSTEXPR20 auto write_constexpr(char* out, Float value) noexcept
    -> char* {
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto raw_exp = int(traits::get_exp(bits));
  auto bin_sig = uint64_t(traits::get_sig(bits));

  if (traits::is_negative(bits)) *out++ = '-';
  if (raw_exp == traits::exp_mask)
    return copy_n(out, bin_sig != 0 ? "nan" : "inf", 3);
  if (raw_exp == 0 && bin_sig == 0) {
    *out++ = '0';
    return out;
  }

  int bin_exp = (raw_exp == 0 ? 1 : raw_exp) - traits::exp_offset;
  if (raw_exp != 0) bin_sig |= traits::implicit_bit;
  dec_fp<> dec = to_decimal<Float>(bin_sig, bin_exp);
  while (dec.sig % 10 == 0) {
    dec.sig /= 10;
    ++dec.exp;
  }

  char digits[traits::max_digits10] = {};
  char* first = digits + sizeof(digits);
  do {
    *--first = char('0' + dec.sig % 10);
    dec.sig /= 10;
  } while (dec.sig != 0);
  int num_digits = int(digits + sizeof(digits) - first);
  int lead_exp = dec.exp + num_digits - 1;

  if (lead_exp >= traits::min_fixed_dec_exp &&
      lead_exp <= traits::max_fixed_dec_exp) {
    int point_pos = lead_exp + 1;
    if (point_pos <= 0) {
      *out++ = '0';
      *out++ = '.';
      for (int i = point_pos; i < 0; ++i) *out++ = '0';
    }
    for (int i = 0; i < num_digits; ++i) {
      if (point_pos > 0 && i == point_pos) *out++ = '.';
      *out++ = first[i];
    }
    for (int i = num_digits; i < point_pos; ++i) *out++ = '0';
    return out;
  }

  *out++ = first[0];
  if (num_digits > 1) {
    *out++ = '.';
    out = copy_n(out, first + 1, size_t(num_digits - 1));
  }
  *out++ = 'e';
  *out++ = lead_exp >= 0 ? '+' : '-';
  unsigned abs_exp = unsigned(lead_exp >= 0 ? lead_exp : -lead_exp);
  if (abs_exp >= 100) {
    *out++ = char('0' + abs_exp / 100);
    abs_exp %= 100;
  }
  *out++ = char('0' + abs_exp / 10);
  *out++ = char('0' + abs_exp % 10);
  return out;
}

// Divides x by 10 in place and returns the remainder.
inline auto divmod10(uint128_t& x) noexcept -> uint64_t {
#if ZMIJ_USE_INT128
  uint64_t r = uint64_t(x % 10);
  x /= 10;
  return r;
#else
  auto div = [](uint64_t& w, uint64_t rem) -> uint64_t {
    uint64_t hi = rem << 32 | w >> 32;
    uint64_t lo = (hi % 10) << 32 | uint32_t(w);
    w = (hi / 10) << 32 | (lo / 10);
    return lo % 10;
  };
  return div(x.lo, div(x.hi, 0));
#endif
}

// `buffer` params require at least buffer_sizes<Float> capacity;
// `out`/`n` params write at most `n` characters.

// Converts `value` to the shortest correctly rounded decimal (see to_decimal).
template <typename Float> auto to_decimal(Float value) noexcept -> dec_fp<>;

// Wide-significand variant for long double (see to_decimal).
template <typename Float>
auto to_decimal_big(Float value) noexcept -> dec_fp<uint128_t>;

template <typename Float>
auto write(char* buffer, Float value) noexcept -> char*;

// Writes the shortest decimal representation of `value`, correctly rounded
// (ties to even), into `out`, truncating after `n` chars. Returns the total
// length the result would need; if it exceeds `n` the output was truncated to
// the first `n` chars.
template <typename Float>
auto write_big(char* out, size_t n, Float value) noexcept -> size_t;

// Writes `value` in `fmt` notation with `precision` digits, correctly rounded
// (ties to even), into `out`, truncating after `n` chars. Returns the total
// length the result would need; if it exceeds `n` the output was truncated to
// the first `n` chars, and 0 on allocation failure (only possible for long
// double).
template <typename Float>
auto write_big(char* out, size_t n, Float value, int precision,
               format fmt) noexcept -> size_t;
template <>
inline auto write_big(char* out, size_t n, float value, int precision,
                      format fmt) noexcept -> size_t {
  return write_big(out, n, double(value), precision, fmt);
}

template <typename Float>
auto write_scientific(char* buffer, Float value, int precision) noexcept
    -> char*;

template <typename Float>
auto write_general(char* buffer, Float value, int precision) noexcept -> char*;

template <typename Float>
auto write_fixed(char* buffer, Float value, int precision) noexcept -> char*;

// Writes the decimal exponent as 'e', a sign and at least two digits, up to
// four (e.g. e+05 or e+4932, enough for extended long double).
auto write_big_exp(char* buffer, int dec_exp) noexcept -> char*;

// Writes `value` in hexadecimal floating-point notation (like printf's %a) in
// its shortest form, e.g. -0x1.8p+1. If `prefix` is false the leading "0x" is
// omitted (e.g. -1.8p+1).
template <typename Float>
auto write_hex(char* buffer, Float value, bool prefix = true) noexcept -> char*;

// Writes `value` in hexadecimal floating-point notation with `precision` hex
// digits after the point, correctly rounded (ties to even), e.g. -0x1.80p+1,
// into `out`, truncating after `n` chars. Returns the total length the result
// would need. If `prefix` is false the leading "0x" is omitted.
template <typename Float>
auto write_hex(char* out, size_t n, Float value, int precision,
               bool prefix = true) noexcept -> size_t;

// When long double == double it has no explicit instantiations, so forward the
// long double detail writers to their double counterparts.
#if LDBL_MANT_DIG == DBL_MANT_DIG
// There is no write_big for double, so route shortest through write.
template <>
inline auto write_big(char* out, size_t n, long double value) noexcept
    -> size_t {
  char buffer[double_buffer_size];
  size_t size = size_t(write(buffer, double(value)) - buffer);
  memcpy(out, buffer, size < n ? size : n);
  return size;
}
template <>
inline auto write_big(char* out, size_t n, long double value, int precision,
                      format fmt) noexcept -> size_t {
  return write_big(out, n, double(value), precision, fmt);
}
template <>
inline auto write_hex(char* buffer, long double value, bool prefix) noexcept
    -> char* {
  return write_hex(buffer, double(value), prefix);
}
template <>
inline auto write_hex(char* out, size_t n, long double value, int precision,
                      bool prefix) noexcept -> size_t {
  return write_hex(out, n, double(value), precision, prefix);
}
#endif  // LDBL_MANT_DIG == DBL_MANT_DIG

// Returns the past-the-end pointer after writing min(size, n) chars to `out`.
inline auto clamp_end(char* out, size_t size, size_t n) noexcept -> char* {
  return out + (size < n ? size : n);
}

// Copies the result in [`buffer`, `end`) to `out`, truncating after `n` chars,
// and returns the past-the-end pointer.
inline ZMIJ_CONSTEXPR20 auto copy_clamped(char* out, size_t n,
                                         const char* buffer,
                                         const char* end) noexcept -> char* {
  size_t size = size_t(end - buffer);
  if (is_constant_evaluated())
    return copy_n(out, buffer, size < n ? size : n);
  memcpy(out, buffer, size < n ? size : n);
  return clamp_end(out, size, n);
}

}  // namespace detail

/// Converts `value` into the shortest correctly rounded decimal representation.
/// Usage:
///   auto [sig, exp, negative] = to_decimal(6.62607015e-34);
inline auto to_decimal(float value) noexcept -> dec_fp<> {
  return detail::to_decimal(value);
}
inline auto to_decimal(double value) noexcept -> dec_fp<> {
  return detail::to_decimal(value);
}
inline auto to_decimal(long double value) noexcept
    -> dec_fp<detail::uint128_t> {
#if LDBL_MANT_DIG == DBL_MANT_DIG
  dec_fp<> dec = detail::to_decimal(double(value));
  return {dec.sig, dec.exp, dec.negative};
#else
  return detail::to_decimal_big(value);
#endif
}

/// Buffer sizes for the write* functions, usable in generic code as
/// buffer_sizes<Float>::shortest, ::scientific, ::fixed, and ::hex.
/// `scientific` assumes precision up to 17 and `fixed` up to 18; long double
/// sets its own bounds below. Larger precision must be sized by the caller.
template <typename Float> struct buffer_sizes;

template <> struct buffer_sizes<float> {
  static constexpr size_t shortest = float_buffer_size;  // write
  static constexpr size_t scientific = 24;  // write_scientific (and general)
  static constexpr size_t fixed = 59;       // write_fixed
  static constexpr size_t hex = 16;         // write_hex
};
template <> struct buffer_sizes<double> {
  static constexpr size_t shortest = double_buffer_size;  // write
  static constexpr size_t scientific = 25;  // write_scientific (and general)
  static constexpr size_t fixed = 329;      // write_fixed
  static constexpr size_t hex = 24;         // write_hex
};
// long double: `scientific` is sized to round-trip: precision 35 (scientific)
// or 36 (general). `fixed` is omitted as it would need an impractical buffer,
// so callers must size fixed output themselves.
template <> struct buffer_sizes<long double> {
  static constexpr size_t shortest = long_double_buffer_size;  // write
  static constexpr size_t scientific = 44;  // write_scientific (and general)
  // Worst case is IEEE binary128: 1 sign + "0x1." + 28 digits + "p+16383".
  static constexpr size_t hex = 40;  // write_hex
};

/// Writes the shortest correctly rounded decimal representation of `value` to
/// `out`, without a null terminator.
///
/// Returns a pointer past the last character written; if the representation
/// exceeds `n` characters, only the first `n` are written.
inline ZMIJ_CONSTEXPR20 auto write(char* out, size_t n, double value) noexcept
    -> char* {
  if (detail::is_constant_evaluated()) {
    char buffer[double_buffer_size] = {};
    return detail::copy_clamped(
        out, n, buffer, detail::write_constexpr(buffer, value));
  }
  char buffer[double_buffer_size];
  if (n >= sizeof(buffer)) return detail::write(out, value);
  return detail::copy_clamped(out, n, buffer, detail::write(buffer, value));
}

inline ZMIJ_CONSTEXPR20 auto write(char* out, size_t n, float value) noexcept
    -> char* {
  if (detail::is_constant_evaluated()) {
    char buffer[float_buffer_size] = {};
    return detail::copy_clamped(
        out, n, buffer, detail::write_constexpr(buffer, value));
  }
  char buffer[float_buffer_size];
  if (n >= sizeof(buffer)) return detail::write(out, value);
  return detail::copy_clamped(out, n, buffer, detail::write(buffer, value));
}

inline auto write(char* out, size_t n, long double value) noexcept -> char* {
  if (LDBL_MANT_DIG == DBL_MANT_DIG) return write(out, n, double(value));
  return detail::clamp_end(out, detail::write_big(out, n, value), n);
}

/// Writes `value` in scientific format with `precision` digits after the
/// decimal point (e.g. 1.234e+05) to `out`, without a null terminator, like
/// printf's %e. A negative `precision` defaults to 6.
///
/// Returns a pointer past the last character written; if the representation
/// exceeds `n` characters, only the first `n` are written. The `long double`
/// overload returns nullptr on allocation failure.
inline auto write_scientific(char* out, size_t n, double value,
                             int precision) noexcept -> char* {
  if (precision < 0) precision = 6;
  if (precision >= 18) {
    auto size = detail::write_big(out, n, value, precision, format::scientific);
    return detail::clamp_end(out, size, n);
  }
  char buffer[buffer_sizes<double>::scientific];
  if (n >= sizeof(buffer))
    return detail::write_scientific(out, value, precision + 1);
  return detail::copy_clamped(
      out, n, buffer, detail::write_scientific(buffer, value, precision + 1));
}

inline auto write_scientific(char* out, size_t n, float value,
                             int precision) noexcept -> char* {
  if (precision < 0) precision = 6;
  if (precision >= 18) {
    auto size = detail::write_big(out, n, value, precision, format::scientific);
    return detail::clamp_end(out, size, n);
  }
  char buffer[buffer_sizes<float>::scientific];
  if (n >= sizeof(buffer))
    return detail::write_scientific(out, value, precision + 1);
  return detail::copy_clamped(
      out, n, buffer, detail::write_scientific(buffer, value, precision + 1));
}

inline auto write_scientific(char* out, size_t n, long double value,
                             int precision) noexcept -> char* {
  if (double(value) == value)
    return write_scientific(out, n, double(value), precision);
  if (precision < 0) precision = 6;
  auto size = detail::write_big(out, n, value, precision, format::scientific);
  return size != 0 ? detail::clamp_end(out, size, n) : nullptr;
}

/// Writes `value` in general format with up to `precision` significant digits
/// and no trailing zeros (e.g. 1.5 or 1.5e+20) to `out`, without a null
/// terminator. Fixed notation is used when `value`'s decimal exponent is in
/// [-4, precision), and scientific otherwise. A negative `precision` defaults
/// to 6 and zero is treated as 1, matching printf.
///
/// Returns a pointer past the last character written; if the representation
/// exceeds `n` characters, only the first `n` are written. The `long double`
/// overload returns nullptr on allocation failure.
inline auto write_general(char* out, size_t n, double value,
                          int precision) noexcept -> char* {
  if (precision <= 1) precision = precision < 0 ? 6 : 1;
  if (precision > 18) {
    auto size = detail::write_big(out, n, value, precision, format::general);
    return detail::clamp_end(out, size, n);
  }
  char buffer[buffer_sizes<double>::scientific];
  if (n >= sizeof(buffer)) return detail::write_general(out, value, precision);
  return detail::copy_clamped(out, n, buffer,
                              detail::write_general(buffer, value, precision));
}

inline auto write_general(char* out, size_t n, float value,
                          int precision) noexcept -> char* {
  if (precision <= 1) precision = precision < 0 ? 6 : 1;
  if (precision > 18) {
    auto size = detail::write_big(out, n, value, precision, format::general);
    return detail::clamp_end(out, size, n);
  }
  char buffer[buffer_sizes<float>::scientific];
  if (n >= sizeof(buffer)) return detail::write_general(out, value, precision);
  return detail::copy_clamped(out, n, buffer,
                              detail::write_general(buffer, value, precision));
}

inline auto write_general(char* out, size_t n, long double value,
                          int precision) noexcept -> char* {
  if (double(value) == value)
    return write_general(out, n, double(value), precision);
  if (precision <= 1) precision = precision < 0 ? 6 : 1;
  auto size = detail::write_big(out, n, value, precision, format::general);
  return size != 0 ? detail::clamp_end(out, size, n) : nullptr;
}

/// Writes `value` in fixed notation with exactly `precision` digits after the
/// decimal point (e.g. 1.500) to `out`, without a null terminator. The result
/// is the exact value correctly rounded (ties to even), matching printf's %f.
/// A negative `precision` defaults to 6.
///
/// Returns a pointer past the last character written; if the representation
/// exceeds `n` characters, only the first `n` are written. The `long double`
/// overload returns nullptr on allocation failure.
inline auto write_fixed(char* out, size_t n, double value,
                        int precision) noexcept -> char* {
  if (precision < 0) precision = 6;
  if (precision > 18) {
    auto size = detail::write_big(out, n, value, precision, format::fixed);
    return detail::clamp_end(out, size, n);
  }
  char buffer[buffer_sizes<double>::fixed];
  if (n >= sizeof(buffer)) return detail::write_fixed(out, value, precision);
  return detail::copy_clamped(out, n, buffer,
                              detail::write_fixed(buffer, value, precision));
}

inline auto write_fixed(char* out, size_t n, float value,
                        int precision) noexcept -> char* {
  if (precision < 0) precision = 6;
  if (precision > 18) {
    auto size = detail::write_big(out, n, value, precision, format::fixed);
    return detail::clamp_end(out, size, n);
  }
  char buffer[buffer_sizes<float>::fixed];
  if (n >= sizeof(buffer)) return detail::write_fixed(out, value, precision);
  return detail::copy_clamped(out, n, buffer,
                              detail::write_fixed(buffer, value, precision));
}

inline auto write_fixed(char* out, size_t n, long double value,
                        int precision) noexcept -> char* {
  if (double(value) == value)
    return write_fixed(out, n, double(value), precision);
  if (precision < 0) precision = 6;
  auto size = detail::write_big(out, n, value, precision, format::fixed);
  return size != 0 ? detail::clamp_end(out, size, n) : nullptr;
}

/// Writes `value` in hexadecimal floating-point notation (like printf's %a) in
/// its shortest form (e.g. -0x1.8p+1) to `out`, without a null terminator.
///
/// Returns a pointer past the last character written; if the representation
/// exceeds `n` characters, only the first `n` are written.
inline auto write_hex(char* out, size_t n, double value) noexcept -> char* {
  char buffer[buffer_sizes<double>::hex];
  if (n >= sizeof(buffer)) return detail::write_hex(out, value);
  return detail::copy_clamped(out, n, buffer, detail::write_hex(buffer, value));
}

inline auto write_hex(char* out, size_t n, float value) noexcept -> char* {
  return write_hex(out, n, double(value));
}

inline auto write_hex(char* out, size_t n, long double value) noexcept
    -> char* {
  char buffer[buffer_sizes<long double>::hex];
  if (n >= sizeof(buffer)) return detail::write_hex(out, value);
  return detail::copy_clamped(out, n, buffer, detail::write_hex(buffer, value));
}

}  // namespace zmij

#endif  // ZMIJ_H_
