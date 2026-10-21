// A double-to-string conversion library: https://github.com/vitaut/zmij/
//
// Copyright (c) 2025 - present, Victor Zverovich
// Distributed under the MIT license (see LICENSE) or alternatively
// the Boost Software License, Version 1.0.

#include "zmij.h"

#include <assert.h>  // assert
#include <float.h>   // DBL_MANT_DIG, LDBL_MANT_DIG
#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t
#include <stdlib.h>  // malloc, free
#include <string.h>  // memcpy

#include <limits>       // std::numeric_limits
#include <type_traits>  // std::conditional

#ifndef ZMIJ_USE_SIMD
#  define ZMIJ_USE_SIMD 1
#endif

#ifdef ZMIJ_USE_NEON
// Use the provided definition.
#elif defined(__ARM_NEON) || defined(_M_ARM64)
#  define ZMIJ_USE_NEON ZMIJ_USE_SIMD
#else
#  define ZMIJ_USE_NEON 0
#endif
#if ZMIJ_USE_NEON
#  include <arm_neon.h>
#endif

#ifdef ZMIJ_USE_SSE
// Use the provided definition.
#elif defined(__SSE2__)
#  define ZMIJ_USE_SSE ZMIJ_USE_SIMD
#elif defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP == 2)
#  define ZMIJ_USE_SSE ZMIJ_USE_SIMD
#else
#  define ZMIJ_USE_SSE 0
#endif
#if ZMIJ_USE_SSE
#  include <immintrin.h>
#endif

#ifdef ZMIJ_USE_SSE4_1
// Use the provided definition.
static_assert(!ZMIJ_USE_SSE4_1 || ZMIJ_USE_SSE);
#elif defined(__SSE4_1__) || defined(__AVX__)
// On MSVC there's no way to check for SSE4.1 specifically so check __AVX__.
#  define ZMIJ_USE_SSE4_1 ZMIJ_USE_SSE
#else
#  define ZMIJ_USE_SSE4_1 0
#endif

#ifdef __aarch64__
#  define ZMIJ_AARCH64 1
#else
#  define ZMIJ_AARCH64 0
#endif

#ifdef __x86_64__
#  define ZMIJ_X86_64 1
#else
#  define ZMIJ_X86_64 0
#endif

#ifdef __clang__
#  define ZMIJ_CLANG 1
#else
#  define ZMIJ_CLANG 0
#endif

#ifdef _MSC_VER
#  define ZMIJ_MSC_VER _MSC_VER
#else
#  define ZMIJ_MSC_VER 0
#endif

#if defined(__has_builtin) && !defined(ZMIJ_NO_BUILTINS)
#  define ZMIJ_HAS_BUILTIN(x) __has_builtin(x)
#else
#  define ZMIJ_HAS_BUILTIN(x) 0
#endif
#ifdef __has_attribute
#  define ZMIJ_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#  define ZMIJ_HAS_ATTRIBUTE(x) 0
#endif
#ifdef __has_cpp_attribute
#  define ZMIJ_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#  define ZMIJ_HAS_CPP_ATTRIBUTE(x) 0
#endif

#if ZMIJ_HAS_CPP_ATTRIBUTE(likely) && ZMIJ_HAS_CPP_ATTRIBUTE(unlikely)
#  define ZMIJ_UNLIKELY unlikely
#else
#  define ZMIJ_UNLIKELY
#endif

#if ZMIJ_HAS_CPP_ATTRIBUTE(maybe_unused)
#  define ZMIJ_MAYBE_UNUSED maybe_unused
#else
#  define ZMIJ_MAYBE_UNUSED
#endif

#ifdef ZMIJ_OPTIMIZE_SIZE
// Use the provided definition.
#elif defined(__OPTIMIZE_SIZE__)
#  define ZMIJ_OPTIMIZE_SIZE 1
#else
#  define ZMIJ_OPTIMIZE_SIZE 0
#endif
#ifndef ZMIJ_USE_EXP_STRING_TABLE
#  define ZMIJ_USE_EXP_STRING_TABLE ZMIJ_OPTIMIZE_SIZE == 0
#endif

#if ZMIJ_HAS_ATTRIBUTE(always_inline) && !ZMIJ_OPTIMIZE_SIZE
#  define ZMIJ_INLINE __attribute__((always_inline)) inline
#elif ZMIJ_MSC_VER
#  define ZMIJ_INLINE __forceinline
#else
#  define ZMIJ_INLINE inline
#endif

#ifdef __GNUC__
#  define ZMIJ_ASM(x) asm x
#else
#  define ZMIJ_ASM(x)
#endif

// Declares struct members that must live in memory on ARM64 but are encoded as
// immediates in the x64 assembly.
#ifdef ZMIJ_CONST_DECL
// Use the provided definition.
#elif ZMIJ_AARCH64
#  define ZMIJ_CONST_DECL
#else
#  define ZMIJ_CONST_DECL static constexpr
#endif

namespace {

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
constexpr bool is_big_endian = true;
#else
constexpr bool is_big_endian = false;
#endif

inline auto bswap64(uint64_t x) noexcept -> uint64_t {
#if ZMIJ_HAS_BUILTIN(__builtin_bswap64)
  return __builtin_bswap64(x);
#elif ZMIJ_MSC_VER
  return _byteswap_uint64(x);
#else
  return ((x & 0xff00000000000000) >> 56) | ((x & 0x00ff000000000000) >> 40) |
         ((x & 0x0000ff0000000000) >> 24) | ((x & 0x000000ff00000000) >> +8) |
         ((x & 0x00000000ff000000) << +8) | ((x & 0x0000000000ff0000) << 24) |
         ((x & 0x000000000000ff00) << 40) | ((x & 0x00000000000000ff) << 56);
#endif
}

inline auto clz(uint64_t x) noexcept -> int {
  assert(x != 0);
#if ZMIJ_HAS_BUILTIN(__builtin_clzll)
  return __builtin_clzll(x);
#elif defined(_M_AMD64) && defined(__AVX2__)
  // Use lzcnt only on AVX2-capable CPUs that have this BMI instruction.
  return __lzcnt64(x);
#elif defined(_M_AMD64) || defined(_M_ARM64)
  unsigned long idx;
  _BitScanReverse64(&idx, x);  // Fallback to the BSR instruction.
  return 63 - idx;
#elif ZMIJ_MSC_VER
  // Fallback to the 32-bit BSR instruction.
  unsigned long idx;
  if (_BitScanReverse(&idx, uint32_t(x >> 32))) return 31 - idx;
  _BitScanReverse(&idx, uint32_t(x));
  return 63 - idx;
#else
  int n = 64;
  for (; x > 0; x >>= 1) --n;
  return n;
#endif
}

inline auto ctz(uint64_t x) noexcept -> int {
  assert(x != 0);
#if ZMIJ_HAS_BUILTIN(__builtin_ctzll)
  return __builtin_ctzll(x);
#elif defined(_M_AMD64) || defined(_M_ARM64)
  unsigned long idx;
  _BitScanForward64(&idx, x);
  return idx;
#elif ZMIJ_MSC_VER
  unsigned long idx;
  if (_BitScanForward(&idx, uint32_t(x))) return idx;
  _BitScanForward(&idx, uint32_t(x >> 32));
  return idx + 32;
#else
  int n = 0;
  for (; (x & 1) == 0; x >>= 1) ++n;
  return n;
#endif
}

// Returns true_value if condition != 0, else false_value, without branching.
ZMIJ_INLINE auto select(uint64_t condition, int64_t true_value,
                        int64_t false_value) -> int64_t {
  // Clang can figure it out on its own.
  if (!ZMIJ_X86_64 || ZMIJ_CLANG) return condition ? true_value : false_value;
  ZMIJ_ASM(
      volatile("test %2, %2\n\t"
               "cmovne %1, %0\n\t" :  //
               "+r"(false_value) : "r"(true_value),
               "r"(condition) : "cc"));
  return false_value;
}

using zmij::detail::compute_dec_exp;
using zmij::detail::compute_exp_shift;
using zmij::detail::float_traits;
using zmij::detail::umul128;
using zmij::detail::umul192_hi128;
using zmij::detail::uint128;
using zmij::detail::uint128_t;

#if ZMIJ_USE_INT128 && defined(__APPLE__)
constexpr bool use_umul128_hi64 = true;  // Use umul128_hi64 for division.
#else
constexpr bool use_umul128_hi64 = false;
#endif

constexpr auto umul128_hi64(uint64_t x, uint64_t y) noexcept -> uint64_t {
  return uint64_t(umul128(x, y) >> 64);
}

// Returns (x * y + c) >> 64.
inline auto umul128_add_hi64(uint64_t x, uint64_t y, uint64_t c) noexcept
    -> uint64_t {
#if ZMIJ_USE_INT128
  return uint64_t((uint128_t(x) * y + c) >> 64);
#else
  auto p = umul128(x, y);
  return p.hi + (p.lo + c < p.lo);
#endif
}

// Computes a power of ten on the fly (no tables): 10**k = 5**k * 2**k. We raise
// 5 (or 1/5 for k < 0) to the k as a fixed-width significand * 2**exp, top bit
// set in work_limbs little-endian limbs, folding 2**k into the exponent.
namespace pow10 {
constexpr int work_bits = 320;                  // internal working precision
constexpr int work_limbs = work_bits / 64;      // working limbs (5)
constexpr int result_bits = 256;                // bits kept in a returned power
constexpr int result_limbs = result_bits / 64;  // result limbs (4)
constexpr int max_exp = 6000;

// out[0..na+nb) = a[0..na) * b[0..nb), base 2**64, little endian.
void mul(const uint64_t* a, int na, const uint64_t* b, int nb,
         uint64_t* out) noexcept {
  memset(out, 0, size_t(na + nb) * sizeof(*out));
  for (int i = 0; i < na; ++i) {
    uint64_t carry = 0;
    for (int j = 0; j < nb; ++j) {
      uint128_t p = umul128(a[i], b[j]);
      uint64_t lo = uint64_t(p), hi = uint64_t(p >> 64);
      uint64_t s = out[i + j] + lo;
      uint64_t c = uint64_t(s < lo);
      s += carry;
      c += uint64_t(s < carry);
      out[i + j] = s;
      carry = hi + c;
    }
    out[i + nb] = carry;  // this limb is still zero: prior rows stop at i+nb-1
  }
}

// out[0..outn) = bits [shift, shift + 64*outn) of a[0..n), truncating low bits.
void extract(const uint64_t* a, int n, int shift, uint64_t* out,
             int outn) noexcept {
  int ws = shift >> 6, bs = shift & 63;
  for (int i = 0; i < outn; ++i) {
    uint64_t lo = ws + i < n ? a[ws + i] : 0;
    uint64_t hi = ws + i + 1 < n ? a[ws + i + 1] : 0;
    out[i] = bs != 0 ? (lo >> bs | hi << (64 - bs)) : lo;
  }
}

// r = a * b kept to work_bits (floor); returns the extra binary exponent shift.
auto fmul(const uint64_t* a, const uint64_t* b, uint64_t* r) noexcept -> int {
  uint64_t prod[2 * work_limbs];
  mul(a, work_limbs, b, work_limbs, prod);
  // Both operands are normalized, so the product's MSB is bit 638 or 639.
  int shift = work_bits - 1 + int(prod[2 * work_limbs - 1] >> 63);
  extract(prod, 2 * work_limbs, shift, r, work_limbs);
  return shift;
}

// Computes base**n (n >= 0) to work_bits, truncating after each multiply, and
// returns rexp with r * 2**rexp <= base**n (a lower approximation). Squares
// base in place, so it is clobbered on return.
auto fpow(uint64_t* base, int base_exp, int n, uint64_t* r) noexcept -> int {
  memset(r, 0, work_limbs * sizeof(*r));
  r[work_limbs - 1] = uint64_t(1) << 63;  // 1.0
  int rexp = 1 - work_bits;
  while (n != 0) {
    if ((n & 1) != 0) rexp += base_exp + fmul(r, base, r);
    n >>= 1;
    if (n != 0) base_exp = 2 * base_exp + fmul(base, base, base);
  }
  return rexp;
}

// Returns e such that m[0..result_limbs) = floor(10**x / 2**e), with m
// normalized to result_bits (top bit set). Requires |x| <= max_exp.
auto compute(int x, uint64_t* m) noexcept -> int {
  assert(x >= -max_exp && x <= max_exp);
  uint64_t p[work_limbs];
  int pe;
  if (x >= 0) {
    uint64_t five[work_limbs] = {};
    five[work_limbs - 1] = uint64_t(5) << 61;  // value 5, top bit set
    pe = fpow(five, 3 - work_bits, x, p);
  } else {
    uint64_t inv5[work_limbs];
    for (auto& limb : inv5) limb = 0xccccccccccccccccu;  // floor(2**322 / 5)
    pe = fpow(inv5, -work_bits - 2, -x, p);  // 1/5, exponent -(work_bits + 2)
  }
  // Truncate p to result_bits; guard bits absorb fpow rounding to exact floor.
  for (int k = 0; k < result_limbs; ++k)
    m[k] = p[k + (work_limbs - result_limbs)];
  return pe + (work_limbs - result_limbs) * 64 + x;  // 10**x = 5**x * 2**x
}
}  // namespace pow10

// Returns the 128-bit `value` shifted right by `n` bits (n >= 1), rounded to
// nearest with ties to even.
auto shr_round_even(uint128_t value, int n) noexcept -> uint128_t {
  if (n >= 128) {  // everything shifts out; 2**127 is the only in-range tie
    uint128_t half = uint128_t(1) << 127;
    return n == 128 && half < value ? uint128_t(1) : uint128_t(0);
  }
  uint128_t q = value >> n;
  uint128_t rem = (value << (128 - n)) >> (128 - n);  // discarded low n bits
  uint128_t half = uint128_t(1) << (n - 1);
  if (half < rem || ((uint64_t(q) & 1) && !(rem < half))) ++q;
  return q;
}

// Returns x / 10 for x <= 2**62.
ZMIJ_INLINE auto div10(uint64_t x) noexcept -> uint64_t {
  assert(x <= (1ull << 62));
  // ceil(2**64 / 10) computed as (1 << 63) / 5 + 1 to avoid int128.
  constexpr uint64_t div10_sig64 = (1ull << 63) / 5 + 1;
  return ZMIJ_USE_INT128 ? umul128_hi64(x, div10_sig64) : x / 10;
}

constexpr uint64_t pow10s[] = {
    1,
    10,
    100,
    1000,
    10000,
    100000,
    1000000,
    10000000,
    100000000,
    1000000000,
    10000000000,
    100000000000,
    1000000000000,
    10000000000000,
    100000000000000,
    1000000000000000,
    10000000000000000,
    100000000000000000,
    1000000000000000000,
};

// 128-bit significands of powers of 10 rounded down.
struct pow10_significand_table {
  static constexpr bool compress =
      ZMIJ_OPTIMIZE_SIZE != 0 || ZMIJ_USE_CONSTEXPR == 0;
  static constexpr bool split_tables = !compress && ZMIJ_AARCH64 != 0;
  static constexpr int num_pow10s = 649;
  uint64_t data[compress ? 1 : num_pow10s * 2] = {};

  static ZMIJ_CONSTEXPR auto make() -> pow10_significand_table {
    pow10_significand_table t;
    for (int i = 0; i < num_pow10s && !compress; ++i) {
      uint128 result = zmij::detail::compute_pow10(i - 307);
      if (split_tables) {
        t.data[num_pow10s - i - 1] = result.hi;
        t.data[num_pow10s * 2 - i - 1] = result.lo;
      } else {
        t.data[i * 2] = result.hi;
        t.data[i * 2 + 1] = result.lo;
      }
    }
    return t;
  }

  ZMIJ_CONSTEXPR20 auto operator[](int dec_exp) const noexcept -> uint128 {
    constexpr int dec_exp_min = -307;
    int i = dec_exp - dec_exp_min;
    if (compress) return zmij::detail::compute_pow10(dec_exp);
    if (!split_tables) {
      const uint64_t* p = data + i * 2;
      return {p[0], p[1]};
    }
    // The caller passes -e - 1 as dec_exp, so ~dec_exp recovers e. Picking the
    // base so that e itself is the index lets both loads share sxtw addressing.
    const uint64_t* p = data + num_pow10s + dec_exp_min;
    if (!zmij::detail::is_constant_evaluated()) ZMIJ_ASM(("" : "+r"(p)));
    return {p[~dec_exp], p[~dec_exp + num_pow10s]};
  }
};

struct exp_shift_table {
  static constexpr bool enable = ZMIJ_OPTIMIZE_SIZE == 0 && ZMIJ_USE_CONSTEXPR;
  // extra_shift in [3, 10]: >= 3 keeps shift non-negative, <= 10 keeps
  // bin_sig << shift in 64 bits (11 overflows the irregular 2**52 << 12).
  // 9 is fastest: extra_shift + 1 == 10 reuses the base-10 digit constant.
  static constexpr int extra_shift = 9;
  unsigned char data[enable ? float_traits<double>::exp_mask + 1 : 1] = {};

  static ZMIJ_CONSTEXPR auto make() -> exp_shift_table {
    exp_shift_table t;
    for (int raw_exp = 0; raw_exp < int(sizeof(t.data)) && enable; ++raw_exp) {
      int bin_exp = raw_exp - float_traits<double>::exp_offset;
      if (raw_exp == 0) ++bin_exp;
      int dec_exp = compute_dec_exp(bin_exp);
      t.data[raw_exp] = compute_exp_shift(bin_exp, dec_exp + 1) + extra_shift;
    }
    return t;
  }
};

// An optional table of precomputed exponent strings for exponential notation.
// Each entry packs "e+dd" or "e+ddd" into a uint64_t with the length in byte 7.
struct exp_string_table {
  static constexpr bool enable =
      ZMIJ_USE_EXP_STRING_TABLE && ZMIJ_USE_CONSTEXPR;
  using traits = float_traits<double>;
  static constexpr int min_dec_exp =
      traits::min_exponent10 - traits::max_digits10;
  static constexpr int offset = -min_dec_exp;
  uint64_t data[enable ? traits::max_exponent10 - min_dec_exp + 1 : 1] = {};

  static ZMIJ_CONSTEXPR auto make() -> exp_string_table {
    exp_string_table t;
    for (int e = min_dec_exp; e <= traits::max_exponent10 && enable; ++e) {
      uint64_t abs_e = e >= 0 ? e : -e;
      uint64_t bc = abs_e % 100;
      uint64_t val = ((bc % 10 + '0') << 8) | (bc / 10 + '0');
      if (uint64_t a = abs_e / 100) val = (val << 8) | (a + '0');
      uint64_t len = 4 + (abs_e >= 100);
      t.data[e + offset] =
          (len << 48) | (val << 16) | (uint64_t(e >= 0 ? '+' : '-') << 8) | 'e';
    }
    return t;
  }
};

// Shuffle vectors to build float strings in both fixed and exponential
// notation.
//
// Byte positions in the source register assembled by write_float_simd:
//   bytes [0, exp_pos):              BCD ASCII digits (reversed)
//   bytes [exp_pos, exp_pos + 4):    exponent string "e±NN"
//   byte  last_digit_pos:            rounded last digit
//   byte  point_pos:                 '.'
//   byte  zero_pos:                  '0', repeated by the shuffle for fixed
//                                    notation's leading zeros and its
//                                    integer-part padding
//
// The shuffle length (max 14) is stored in byte 15; the corresponding output
// byte is past the string and ignored by the caller.
//
// Entries cover both output shapes so that choosing between them is a clamped
// index rather than a branch: slots [0, sci_slot) hold the fixed layout for
// each dec_exp in [min_fixed_dec_exp, max_fixed_dec_exp], and sci_slot holds
// the exponential layout. Keep it branchless: floats reach the fixed range
// often enough that predicting the shape costs more than emitting it.
struct float_shuffle_table {
  static constexpr bool enable =
      (ZMIJ_USE_SSE4_1 || ZMIJ_USE_NEON) && exp_string_table::enable;
  using traits = float_traits<float>;
  static constexpr unsigned char exp_pos = 8;
  static constexpr unsigned char last_digit_pos = 12;
  static constexpr unsigned char point_pos = 13;
  static constexpr unsigned char zero_pos = 14;
  static constexpr int num_bcd_digits = 8;
  // A rounded last digit makes the shuffle span the whole padded BCD, so those
  // variants collapse: sig_index is num_bcd_digits rather than num_digits - 1.
  static constexpr int num_variants = (num_bcd_digits + 1) * 2;
  static constexpr int sci_slot =
      traits::max_fixed_dec_exp - traits::min_fixed_dec_exp + 1;
  static constexpr int num_slots = sci_slot + 1;
  static constexpr int num_shuffles = num_slots * num_variants;
  alignas(16) unsigned char data[enable ? num_shuffles * 16 : 1] = {};

  struct entry {
    const unsigned char* shuffle;
    unsigned char length;
  };

  // A `dec_exp` outside the fixed range clamps onto the scientific slot.
  ZMIJ_CONSTEXPR auto get_entry(int dec_exp, int num_digits,
                                bool has_last_digit,
                                bool has_extra_digit) const noexcept -> entry {
    unsigned rel = unsigned(dec_exp - traits::min_fixed_dec_exp);
    unsigned slot = rel < unsigned(sci_slot) ? rel : unsigned(sci_slot);
    int sig_index = has_last_digit ? num_bcd_digits : num_digits - 1;
    int idx = int(slot) * num_variants + sig_index * 2 + has_extra_digit;
    return entry{&data[idx * 16], data[idx * 16 + 15]};
  }

  // Source-register positions of the significand digits, msb first.
  struct sig_shuffle {
    unsigned char pos[traits::max_digits10];
    int len;
  };

  static ZMIJ_CONSTEXPR auto make_sig_shuffle(int num_digits,
                                              bool has_last_digit,
                                              bool has_extra_digit) noexcept
      -> sig_shuffle {
    sig_shuffle s = {};
    int leading_pos = has_extra_digit ? 7 : 6;
    // Always the full padded BCD in the significand plus a last-digit char;
    // for !has_extra_digit its leading '0' is shown.
    s.len =
        (has_last_digit ? num_bcd_digits : num_digits - 1) + has_extra_digit;
    if (s.len < 1) s.len = 1;
    for (int j = 0; j < s.len - has_last_digit; ++j) s.pos[j] = leading_pos - j;
    if (has_last_digit) s.pos[s.len - 1] = last_digit_pos;
    return s;
  }

  static ZMIJ_CONSTEXPR auto make() -> float_shuffle_table {
    float_shuffle_table t;
    for (int idx = 0; idx < num_variants && enable; ++idx) {
      int sig_index = idx >> 1;
      sig_shuffle s = make_sig_shuffle(sig_index + 1,
                                       sig_index == num_bcd_digits,
                                       (idx & 1) != 0);
      for (int slot = 0; slot < num_slots; ++slot) {
        unsigned char* out = &t.data[(slot * num_variants + idx) * 16];
        // Shuffle high bit: output 0.
        for (int i = 0; i < 16; ++i) out[i] = 0x80;
        int dec_exp = slot + traits::min_fixed_dec_exp;
        int len = 0;

        // Every shape is an integer part, '.', a fraction, and for the
        // exponential slot a trailing exponent. Only the split point differs;
        // when it falls left of the digits there is no integer digit, just a
        // '0', and the shortfall becomes leading zeros in the fraction.
        int int_digits = slot == sci_slot ? 1 : dec_exp + 1;
        int split = int_digits > 0 ? int_digits : 0;
        int lead_zeros = split - int_digits;
        int int_sig = split < s.len ? split : s.len;  // integer digits present
        if (int_digits < 1) out[len++] = zero_pos;
        for (int j = 0; j < int_sig; ++j) out[len++] = s.pos[j];
        for (int z = int_sig; z < split; ++z) out[len++] = zero_pos;
        // A significand that ends at or before the split leaves no fraction,
        // so the point is dropped: "5e+02", not "5.0e+02".
        if (split < s.len) {
          out[len++] = point_pos;
          for (int z = 0; z < lead_zeros; ++z) out[len++] = zero_pos;
          for (int j = split; j < s.len; ++j) out[len++] = s.pos[j];
        }
        if (slot == sci_slot) {
          for (int i = 0; i < 4; ++i) out[len++] = exp_pos + i;
        }
        out[15] = len;
      }
    }
    return t;
  }
};

constexpr auto fixed_entry_align() noexcept -> int {
  // 64 aligns entry to a cache line; 32 makes indexing use `lsl #5` rather
  // than `umaddl`.
  return ZMIJ_USE_SSE4_1 ? 64 : ZMIJ_AARCH64 && !ZMIJ_OPTIMIZE_SIZE ? 32 : 1;
}

// Per-decimal-exponent buffer layout for branchless fixed-notation output.
// Each entry holds the byte positions of the leading zeros, decimal point,
// and end of output, indexed by the decimal exponent (dec_exp).
struct fixed_layout_table {
  static constexpr bool enable = ZMIJ_USE_CONSTEXPR != 0;
  using traits = float_traits<double>;
  static constexpr int num_entries =
      traits::max_fixed_dec_exp - traits::min_fixed_dec_exp + 1;

  struct alignas(fixed_entry_align()) entry {
#if ZMIJ_USE_SSE4_1
    // pshufb table mapping BCD bytes to their output slots; the decimal-point
    // slot (if any) holds a zero-marker (high bit set). Indexed by extra_digit.
    // Read via aligned load (_mm_load_si128), so must be 16-byte aligned.
    alignas(16) unsigned char shuffle[2][16];
#endif
    // Byte offset past leading "0.00..." before first significant digit.
    unsigned char start_pos;
    unsigned char point_pos;
    // Start position for shifting digits right by one to insert the point.
    unsigned char shift_pos;
#if ZMIJ_USE_SSE4_1
    // Buffer-relative position of the last_digit byte, indexed by
    // has_extra_digit. Only used for bcd_size == 16 (doubles).
    unsigned char last_digit_pos[2];
#endif
    // Offset past the end of fixed-notation output, indexed by sig length - 1.
    unsigned char end_pos[traits::max_digits10];
  };
  entry data[enable ? num_entries : 1] = {};

  static ZMIJ_CONSTEXPR auto compute(int dec_exp) noexcept -> entry {
    entry e = {};

    e.start_pos = dec_exp < -0 ? 1 - dec_exp : 0;
    e.point_pos = dec_exp >= 0 ? 1 + dec_exp : 1;
    e.shift_pos = e.point_pos + (dec_exp >= 0);

#if ZMIJ_USE_SSE4_1
    constexpr int bcd_size = 16;
    for (int extra = 0; extra < 2; ++extra) {
      int len = bcd_size + extra - 1;
      e.last_digit_pos[extra] = len + (0 <= dec_exp && dec_exp < len);
    }

    // Build the shuffle tables from natural-order BCD (to_digits puts BCD[k]
    // at byte k). extra_digit == 0 drops the leading zero, so digits start at
    // BCD[!extra]; point_slot gets a pshufb zero-marker (0xFF) for the point.
    int point_slot = (dec_exp >= 0 && dec_exp <= 14) ? 1 + dec_exp : 128;
    for (int extra = 0; extra < 2; ++extra) {
      unsigned char bcd_idx = !extra;
      for (int i = 0; i < bcd_size; ++i)
        e.shuffle[extra][i] = i == point_slot ? 0xFF : bcd_idx++;
    }
#endif  // ZMIJ_USE_SSE4_1

    for (int n = 1; n <= traits::max_digits10; ++n) {
      int end_pos = n;
      if (dec_exp >= 0) end_pos = n > dec_exp + 1 ? n + 1 : dec_exp + 1;
      e.end_pos[n - 1] = end_pos;
    }
    return e;
  }

  static ZMIJ_CONSTEXPR auto make() -> fixed_layout_table {
    fixed_layout_table t;
    for (int dec_exp = traits::min_fixed_dec_exp;
         dec_exp <= traits::max_fixed_dec_exp && enable; ++dec_exp)
      t.data[dec_exp - traits::min_fixed_dec_exp] = compute(dec_exp);
    return t;
  }

  ZMIJ_CONSTEXPR auto get(int dec_exp, entry& scratch) const noexcept
      -> const entry& {
    constexpr auto min = traits::min_fixed_dec_exp;
    assert(dec_exp >= min && dec_exp <= traits::max_fixed_dec_exp);
    if (!enable) return scratch = compute(dec_exp);
    return data[unsigned(dec_exp - min)];
  }
};

inline auto count_trailing_nonzeros(uint64_t x) noexcept -> int {
  // We count the number of bytes until there are only zeros left.
  // The code is equivalent to
  //   return 8 - clz(x) / 8
  // but if the BSR instruction is emitted (as gcc on x64 does with
  // default settings), subtracting the constant before dividing allows
  // the compiler to combine it with the subtraction which it inserts
  // due to BSR counting in the opposite direction.
  //
  // Additionally, the BSR instruction requires a zero check.  Since the
  // high bit is unused we can avoid the zero check by shifting the
  // datum left by one and inserting a sentinel bit at the end. This can
  // be faster than the automatically inserted range check.
  if (is_big_endian) x = bswap64(x);
  return (size_t(70) - clz((x << 1) | 1)) / 8;  // size_t for native arithmetic
}

// Converts value in the range [0, 100) to a string. GCC generates a bit better
// code when value is pointer-size (https://www.godbolt.org/z/5fEPMT1cc).
inline auto digits2(size_t value) noexcept -> const char* {
  // Align data since unaligned access may be slower when crossing a
  // hardware-specific boundary.
  alignas(2) static const char data[] =
      "0001020304050607080910111213141516171819"
      "2021222324252627282930313233343536373839"
      "4041424344454647484950515253545556575859"
      "6061626364656667686970717273747576777879"
      "8081828384858687888990919293949596979899";
  return &data[value * 2];
}

// Writes chars `a` and `b` to `out` and returns the position after them.
ZMIJ_INLINE auto write2(char* out, char a, char b) noexcept -> char* {
  uint16_t v = uint16_t(uint8_t(a) | uint8_t(b) << 8);
  if (is_big_endian) v = uint16_t(v << 8 | v >> 8);
  memcpy(out, &v, 2);
  return out + 2;
}

constexpr int div10k_exp = 40;
constexpr uint32_t div10k_sig = uint32_t((1ull << div10k_exp) / 10000 + 1);
constexpr uint32_t neg10k = uint32_t((1ull << 32) - 10000);

constexpr int div100_exp = 19;
constexpr uint32_t div100_sig = (1 << div100_exp) / 100 + 1;
constexpr uint32_t neg100 = (1 << 16) - 100;

constexpr int div10_exp = 10;
constexpr uint32_t div10_sig = (1 << div10_exp) / 10 + 1;
constexpr uint32_t neg10 = (1 << 8) - 10;

constexpr uint64_t zeros = 0x0101010101010101u * '0';

struct data {
  static constexpr auto splat64(uint64_t x) -> uint128 { return {x, x}; }
  static constexpr auto splat32(uint32_t x) -> uint128 {
    return splat64(uint64_t(x) << 32 | x);
  }
  static constexpr auto splat16(uint16_t x) -> uint128 {
    return splat32(uint32_t(x) << 16 | x);
  }
  static constexpr auto pack8(uint8_t a, uint8_t b, uint8_t c, uint8_t d,  //
                              uint8_t e, uint8_t f, uint8_t g, uint8_t h)
      -> uint64_t {
    using u64 = uint64_t;
    return u64(h) << 56 | u64(g) << 48 | u64(f) << 40 | u64(e) << 32 |
           u64(d) << 24 | u64(c) << 16 | u64(b) << +8 | u64(a);
  }

  // Keep first: arm64 folds a register index into ldrb only at offset zero.
  exp_shift_table exp_shifts =
      exp_shift_table::enable ? exp_shift_table::make() : exp_shift_table();

  ZMIJ_CONST_DECL uint64_t threshold = 1e15;
  // +6 is needed for boundary cases found by verify.py.
  ZMIJ_CONST_DECL uint64_t biased_half = (uint64_t(1) << 63) + 6;

#if ZMIJ_USE_NEON
  static constexpr int32_t neg10k = 0x10000 - 10000;

  using int32x4 =
      typename std::conditional<ZMIJ_MSC_VER != 0, int32_t[4], int32x4_t>::type;
  using int16x8 =
      typename std::conditional<ZMIJ_MSC_VER != 0, int16_t[8], int16x8_t>::type;

  uint64_t mul_const = 0xabcc77118461cefd;
  uint64_t hundred_million = 100000000;
  int32x4 multipliers32 = {div10k_sig, neg10k, div100_sig << 12, neg100};
  int16x8 multipliers16 = {0xce0, neg10};
#elif ZMIJ_USE_SSE
  // Ordered so the values used to format floats fit in a single cache line.
  // Read via aligned load (_mm_load_si128), so must be 16-byte aligned.
  alignas(16) uint128 div100 = splat32(div100_sig);
  uint128 div10 = splat16((1 << 16) / 10 + 1);
#  if ZMIJ_USE_SSE4_1
  uint128 neg100 = splat32(::neg100);
  uint128 neg10 = splat16((1 << 8) - 10);
  uint128 bswap = uint128{pack8(7, 6, 5, 4, 3, 2, 1, 0),
                          pack8(15, 14, 13, 12, 11, 10, 9, 8)};
#  else
  uint128 hundred = splat32(100);
  uint128 moddiv10 = splat16(10 * (1 << 8) - 1);
#  endif  // ZMIJ_USE_SSE4_1
  uint128 div10k = splat64(div10k_sig);
  uint128 neg10k = splat64(::neg10k);
  uint128 zeros = splat64(::zeros);
#endif    // ZMIJ_USE_SSE

  exp_string_table exp_strings =
      exp_string_table::enable ? exp_string_table::make() : exp_string_table();
  alignas(64) pow10_significand_table pow10_significands =
      pow10_significand_table::compress ? pow10_significand_table()
                                        : pow10_significand_table::make();
  fixed_layout_table fixed_layouts = fixed_layout_table::enable
                                         ? fixed_layout_table::make()
                                         : fixed_layout_table();
  float_shuffle_table float_shuffles = float_shuffle_table::enable
                                           ? float_shuffle_table::make()
                                           : float_shuffle_table();

  // Shuffle indices for SIMD digit shift. Offset 0 = identity, offset 1 =
  // shift left by 1 (drops the leading '0' of a 16-digit significand).
  unsigned char shift_shuffle[17] = {0, 1,  2,  3,  4,  5,  6,  7, 8,
                                     9, 10, 11, 12, 13, 14, 15, 0};
};
alignas(64) constexpr data static_data;

#if ZMIJ_USE_NEON  // An optimized version for NEON by Dougall Johnson.

// Converts four numbers < 10000, one in each 32-bit lane, to BCD digits.
ZMIJ_INLINE auto to_bcd_4x4(int32x4_t efgh_abcd_mnop_ijkl,
                            const data& d) noexcept -> uint8x16_t {
  // Compiler barrier, or clang breaks the subsequent MLA into UADDW + MUL.
  ZMIJ_ASM(("" : "+w"(efgh_abcd_mnop_ijkl)));

  int32x4_t ef_ab_mn_ij =
      vqdmulhq_n_s32(efgh_abcd_mnop_ijkl, d.multipliers32[2]);
  int16x8_t gh_ef_cd_ab_op_mn_kl_ij = vreinterpretq_s16_s32(
      vmlaq_n_s32(efgh_abcd_mnop_ijkl, ef_ab_mn_ij, d.multipliers32[3]));
  int16x8_t high_10s =
      vqdmulhq_n_s16(gh_ef_cd_ab_op_mn_kl_ij, d.multipliers16[0]);
  return vreinterpretq_u8_s16(
      vmlaq_n_s16(gh_ef_cd_ab_op_mn_kl_ij, high_10s, d.multipliers16[1]));
}

ZMIJ_INLINE auto to_unshuffled_digits(uint64_t value, const data& d)
    -> uint8x16_t {
  uint64_t hundred_million = d.hundred_million;

  // Compiler barrier, or clang narrows the load to 32-bit and unpairs it.
  ZMIJ_ASM(("" : "+r"(hundred_million)));

  // abcdefgh = value / 100000000, ijklmnop = value % 100000000.
  uint64_t abcdefgh = uint64_t(umul128(value, d.mul_const) >> 90);
  uint64_t ijklmnop = value - abcdefgh * hundred_million;

  uint64x1_t ijklmnop_abcdefgh_64 = {ijklmnop << 32 | abcdefgh};
  int32x2_t abcdefgh_ijklmnop = vreinterpret_s32_u64(ijklmnop_abcdefgh_64);

  int32x2_t abcd_ijkl = vreinterpret_s32_u32(
      vshr_n_u32(vreinterpret_u32_s32(
                     vqdmulh_n_s32(abcdefgh_ijklmnop, d.multipliers32[0])),
                 9));
  int32x2_t efgh_abcd_mnop_ijkl_32 =
      vmla_n_s32(abcdefgh_ijklmnop, abcd_ijkl, d.multipliers32[1]);

  int32x4_t efgh_abcd_mnop_ijkl = vreinterpretq_s32_u32(
      vshll_n_u16(vreinterpret_u16_s32(efgh_abcd_mnop_ijkl_32), 0));
  return to_bcd_4x4(efgh_abcd_mnop_ijkl, d);
}

#elif ZMIJ_USE_SSE

using m128ptr = const __m128i*;

// Converts four numbers < 10000, one in each 32-bit lane, to BCD digits.
// Digits in each 32-bit lane will be in order for SSE2, reversed for SSE4.1.
ZMIJ_INLINE auto to_bcd_4x4(__m128i y, const data& d) noexcept -> __m128i {
  const __m128i div100 = _mm_load_si128(m128ptr(&d.div100));
  const __m128i div10 = _mm_load_si128(m128ptr(&d.div10));
#  if ZMIJ_USE_SSE4_1
  const __m128i neg100 = _mm_load_si128(m128ptr(&d.neg100));
  const __m128i neg10 = _mm_load_si128(m128ptr(&d.neg10));

  // _mm_mullo_epi32 is SSE 4.1
  __m128i z = _mm_add_epi64(
      y,
      _mm_mullo_epi32(neg100, _mm_srli_epi32(_mm_mulhi_epu16(y, div100), 3)));
  return _mm_add_epi16(z, _mm_mullo_epi16(neg10, _mm_mulhi_epu16(z, div10)));
#  else
  const __m128i hundred = _mm_load_si128(m128ptr(&d.hundred));
  const __m128i moddiv10 = _mm_load_si128(m128ptr(&d.moddiv10));

  __m128i y_div_100 = _mm_srli_epi16(_mm_mulhi_epu16(y, div100), 3);
  __m128i y_mod_100 = _mm_sub_epi16(y, _mm_mullo_epi16(y_div_100, hundred));
  __m128i z = _mm_or_si128(_mm_slli_epi32(y_mod_100, 16), y_div_100);
  return _mm_sub_epi16(_mm_slli_epi16(z, 8),
                       _mm_mullo_epi16(moddiv10, _mm_mulhi_epu16(z, div10)));
#  endif  // ZMIJ_USE_SSE4_1
}

#endif  // ZMIJ_USE_SSE

struct bcd_result {
  uint64_t bcd;
  int len;
};

auto to_bcd8(uint64_t abcdefgh) noexcept -> bcd_result {
  if (!ZMIJ_USE_SSE && !ZMIJ_USE_NEON) {
    // An optimization from Xiang JunBo.
    // Three steps BCD. Base 10000 -> base 100 -> base 10.
    // div and mod are evaluated simultaneously as, e.g.
    //   (abcdefgh / 10000) << 32 + (abcdefgh % 10000)
    //      == abcdefgh + (2**32 - 10000) * (abcdefgh / 10000)))
    // where the division on the RHS is implemented by the multiply + shift
    // trick and the fractional bits are masked away.
    uint64_t abcd_efgh =
        abcdefgh + neg10k * ((abcdefgh * div10k_sig) >> div10k_exp);
    uint64_t ab_cd_ef_gh =
        abcd_efgh +
        neg100 * (((abcd_efgh * div100_sig) >> div100_exp) & 0x7f0000007f);
    uint64_t a_b_c_d_e_f_g_h =
        ab_cd_ef_gh +
        neg10 * (((ab_cd_ef_gh * div10_sig) >> div10_exp) & 0xf000f000f000f);
    uint64_t bcd = is_big_endian ? a_b_c_d_e_f_g_h : bswap64(a_b_c_d_e_f_g_h);
    return {bcd, count_trailing_nonzeros(bcd)};
  }

  const auto* d = &static_data;
  ZMIJ_ASM(("" : "+r"(d)));  // Load constants from memory.

#if ZMIJ_USE_NEON
  uint64_t abcd_efgh_64 =
      abcdefgh + neg10k * ((abcdefgh * div10k_sig) >> div10k_exp);
  int32x4_t abcd_efgh = vcombine_s32(
      vreinterpret_s32_u64(vcreate_u64(abcd_efgh_64)), vdup_n_s32(0));
  uint8x16_t digits_128 = to_bcd_4x4(abcd_efgh, *d);
  uint8x8_t digits = vget_low_u8(digits_128);
  uint64_t bcd = vget_lane_u64(vreinterpret_u64_u8(vrev64_u8(digits)), 0);
  return {bcd, count_trailing_nonzeros(bcd)};
#elif ZMIJ_USE_SSE4_1
  uint64_t abcd_efgh =
      abcdefgh + neg10k * ((abcdefgh * div10k_sig) >> div10k_exp);
  uint64_t unshuffled_bcd =
      _mm_cvtsi128_si64(to_bcd_4x4(_mm_set_epi64x(0, abcd_efgh), *d));
  int len = unshuffled_bcd ? 8 - ctz(unshuffled_bcd) / 8 : 0;
  return {bswap64(unshuffled_bcd), len};
#elif ZMIJ_USE_SSE
  // Evaluate the 4-digit limbs and arrange them such that we get a result which
  // is in the correct order.
  uint64_t abcd_efgh =
      (abcdefgh << 32) -
      uint64_t((10000ull << 32) - 1) * ((abcdefgh * div10k_sig) >> div10k_exp);
  __m128i v = to_bcd_4x4(_mm_set_epi64x(0, abcd_efgh), *d);
#  if defined(__x86_64__) || defined(_M_X64)
  uint64_t bcd = _mm_cvtsi128_si64(v);
#  else
  uint64_t bcd = uint64_t(_mm_cvtsi128_si32(_mm_srli_si128(v, 4))) << 32 |
                 uint32_t(_mm_cvtsi128_si32(v));
#  endif
  return {bcd, count_trailing_nonzeros(bcd)};
#endif  // ZMIJ_USE_SSE
}

template <int num_bits> struct dec_digits {
  // `unshuffled` is the byte-reversed BCD vector used by write_float_simd.
#if ZMIJ_USE_NEON
  uint8x16_t unshuffled;
#elif ZMIJ_USE_SSE4_1
  __m128i unshuffled;
#endif
  uint64_t digits;
  int num_digits;
};

template <> struct dec_digits<64> {
#if ZMIJ_USE_NEON
  using digits_type = uint16x8_t;
#elif ZMIJ_USE_SSE
  using digits_type = __m128i;
#else
  using digits_type = uint128;
#endif
  digits_type digits;
  int num_digits;
};

// Converts a significand to decimal digits, removing trailing zeros. value has
// up to 17 decimal digits (16-17 for normals) for double (num_bits == 64) and
// up to 9 digits (8-9 for normals) for float.
template <int num_bits>
ZMIJ_INLINE auto to_digits(uint64_t value, const data& d) noexcept
    -> dec_digits<num_bits> {
#if !ZMIJ_USE_NEON && !ZMIJ_USE_SSE
  uint32_t hi = uint32_t(value / 100000000);
  uint32_t lo = uint32_t(value % 100000000);
  auto hi_bcd = to_bcd8(hi);
  if (lo == 0) return {{zeros, hi_bcd.bcd + zeros}, hi_bcd.len};
  auto lo_bcd = to_bcd8(lo);
  return {{lo_bcd.bcd + zeros, hi_bcd.bcd + zeros}, 8 + lo_bcd.len};
#elif ZMIJ_USE_NEON
  auto unshuffled_digits = to_unshuffled_digits(value, d);
  uint8x16_t digits = vrev64q_u8(unshuffled_digits);
  uint16x8_t str = vaddq_u16(vreinterpretq_u16_u8(digits),
                             vreinterpretq_u16_s8(vdupq_n_s8('0')));
  uint16x8_t is_not_zero =
      vreinterpretq_u16_u8(vcgtzq_s8(vreinterpretq_s8_u8(digits)));
  uint64_t nonzero_mask =
      vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(is_not_zero, 4)), 0);
  return {str, 16 - (clz(nonzero_mask) >> 2)};
#else  // ZMIJ_USE_SSE
  uint32_t hi = uint32_t(value / 100000000);
  uint32_t lo = uint32_t(value % 100000000);

  const __m128i div10k = _mm_load_si128(m128ptr(&d.div10k));
  const __m128i neg10k = _mm_load_si128(m128ptr(&d.neg10k));
  __m128i x = _mm_set_epi64x(hi, lo);
  __m128i y = _mm_add_epi64(
      x, _mm_mul_epu32(neg10k,
                       _mm_srli_epi64(_mm_mul_epu32(x, div10k), div10k_exp)));

  // Shuffle to ensure correctly ordered result from SSE2 path.
  if (!ZMIJ_USE_SSE4_1) y = _mm_shuffle_epi32(y, _MM_SHUFFLE(0, 1, 2, 3));

  __m128i bcd = to_bcd_4x4(y, d);
  const __m128i zeros = _mm_load_si128(m128ptr(&d.zeros));

  // Computed against current bcd (rather than the post-bswap bcd) so the mask
  // is derived in parallel with the shuffle on the SSE4.1 path.
  uint64_t mask = _mm_movemask_epi8(_mm_cmpgt_epi8(bcd, _mm_setzero_si128()));
  // Trailing zeros are in the low bits for SSE4.1, the high bits for SSE2.
  int len = ZMIJ_USE_SSE4_1 ? 16 - ctz(mask) : 64 - clz(mask);
#  if ZMIJ_USE_SSE4_1
  bcd = _mm_shuffle_epi8(bcd, _mm_load_si128(m128ptr(&d.bswap)));  // SSSE3
#  endif
  return {_mm_or_si128(bcd, zeros), len};
#endif  // ZMIJ_USE_SSE
}

template <>
ZMIJ_INLINE auto to_digits<32>(uint64_t value,
                               [[ZMIJ_MAYBE_UNUSED]] const data& d) noexcept
    -> dec_digits<32> {
#if ZMIJ_USE_SSE4_1
  // Inline to_bcd8's SSE4.1 body so we can return the unshuffled xmm too;
  // the exponential-notation path uses it to skip the bswap-via-gpr.
  uint64_t abcd_efgh = value + neg10k * ((value * div10k_sig) >> div10k_exp);
  __m128i bcd_xmm = to_bcd_4x4(_mm_set_epi64x(0, abcd_efgh), d);
  uint64_t unshuffled_bcd = _mm_cvtsi128_si64(bcd_xmm);
  int len = unshuffled_bcd ? 8 - ctz(unshuffled_bcd) / 8 : 0;
  return {bcd_xmm, bswap64(unshuffled_bcd) + zeros, len};
#elif ZMIJ_USE_NEON
  // Inline to_bcd8's NEON body so we can return the unshuffled vector too;
  // the exponential-notation path uses it to skip the simd->gpr->bswap->simd
  // roundtrip needed to materialize `digits`.
  uint64_t abcd_efgh = value + neg10k * ((value * div10k_sig) >> div10k_exp);
  int32x4_t input =
      vcombine_s32(vreinterpret_s32_u64(vcreate_u64(abcd_efgh)), vdup_n_s32(0));
  uint8x16_t unshuffled = to_bcd_4x4(input, d);
  uint64_t unshuffled_bcd =
      vget_lane_u64(vreinterpret_u64_u8(vget_low_u8(unshuffled)), 0);
  int len = unshuffled_bcd ? 8 - ctz(unshuffled_bcd) / 8 : 0;
  return {unshuffled, bswap64(unshuffled_bcd) + zeros, len};
#else
  auto result = to_bcd8(value);
  return {result.bcd + zeros, result.len};
#endif
}

// Writes `digits` to `buffer`, dropping the leading '0' when drop_leading_zero
// is set. On SIMD, folds the shift into the digit shuffle to avoid a
// dependent 16-byte memmove.
ZMIJ_INLINE void write_digits(char* buffer, dec_digits<64>::digits_type digits,
                              bool drop_leading_zero, const data& d) noexcept {
  if (!ZMIJ_USE_NEON && !ZMIJ_USE_SSE4_1) {
    memcpy(buffer, &digits, sizeof(digits));
    memmove(buffer, buffer + drop_leading_zero, sizeof(digits));
    return;
  }
#if ZMIJ_USE_NEON
  uint8x16_t shuffle = vld1q_u8(d.shift_shuffle + drop_leading_zero);
  uint8x16_t shifted = vqtbl1q_u8(vreinterpretq_u8_u16(digits), shuffle);
  vst1q_u8(reinterpret_cast<uint8_t*>(buffer), shifted);
#elif ZMIJ_USE_SSE4_1
  __m128i shuffle = _mm_loadu_si128(
      reinterpret_cast<const __m128i*>(d.shift_shuffle + drop_leading_zero));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(buffer),
                   _mm_shuffle_epi8(digits, shuffle));
#endif
}

ZMIJ_INLINE void write_digits(char* buffer, uint64_t digits,
                              bool drop_leading_zero, const data&) noexcept {
  unsigned shift = unsigned(drop_leading_zero) * 8;
  digits = is_big_endian ? digits << shift : digits >> shift;
  memcpy(buffer, &digits, sizeof(digits));
}

ZMIJ_INLINE auto write_float_simd(char* buffer, const dec_digits<32>& dig,
                                  int last_digit, bool has_last_digit,
                                  bool has_extra_digit, int dec_exp,
                                  const data& d) noexcept -> char* {
  // Packed for insertion into lane 1: byte 0 of `tail` lands at register
  // byte exp_pos (8), so the exp string fills exp_pos..exp_pos+3; the prefix
  // shifts place '0'+last_digit at last_digit_pos (12), '.' at point_pos (13)
  // and '0' at zero_pos (14). Only exp_data's four exponent chars are used; the
  // length it also carries comes from the shuffle entry instead.
  uint64_t exp_data = d.exp_strings.data[dec_exp + exp_string_table::offset];
  uint32_t prefix = (uint32_t('.') << 8) + uint32_t('0') + last_digit;
  uint64_t tail =
      uint32_t(exp_data) | (uint64_t(prefix) << 32) | (uint64_t('0') << 48);
  auto entry = d.float_shuffles.get_entry(dec_exp, dig.num_digits,
                                          has_last_digit, has_extra_digit);
#if ZMIJ_USE_SSE4_1
  __m128i ascii =
      _mm_or_si128(dig.unshuffled, _mm_load_si128(m128ptr(&d.zeros)));
  __m128i src = _mm_insert_epi64(ascii, int64_t(tail), 1);
  __m128i shuffle = _mm_load_si128(m128ptr(entry.shuffle));
  __m128i out = _mm_shuffle_epi8(src, shuffle);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(buffer), out);
#elif ZMIJ_USE_NEON
  uint8x16_t ascii = vorrq_u8(dig.unshuffled, vdupq_n_u8('0'));
  uint8x16_t src = vreinterpretq_u8_u64(
      vsetq_lane_u64(tail, vreinterpretq_u64_u8(ascii), 1));
  uint8x16_t shuffle = vld1q_u8(entry.shuffle);
  uint8x16_t out = vqtbl1q_u8(src, shuffle);
  vst1q_u8(reinterpret_cast<uint8_t*>(buffer), out);
#endif
  return buffer + entry.length;
}

ZMIJ_INLINE auto write_float_simd(char*, const dec_digits<64>&, int, bool, bool,
                                  int, const data&) noexcept -> char* {
  return nullptr;
}

// Writes "inf"/"nan" with one 4-byte store, so the buffer must hold 4 bytes.
auto write_inf_nan(char* buffer, bool is_nan) noexcept -> char* {
  return memcpy(buffer, is_nan ? "nan" : "inf", 4), buffer + 3;
}

// An output sink that appends to `out`, discarding anything past `end`.
struct writer {
  char* out;
  char* end;
  size_t count;  // total chars, including any dropped past `end`

  // The write functions return `count`, the running total of requested chars.
  ZMIJ_INLINE auto write(char c) noexcept -> size_t {
    if (out < end) *out++ = c;
    return ++count;
  }
  // Copies at most `end - out` chars of `s[0..n)`; `n` must be non-negative.
  ZMIJ_INLINE auto write(const char* s, int n) noexcept -> size_t {
    assert(n >= 0);
    size_t take = n < end - out ? size_t(n) : size_t(end - out);
    memcpy(out, s, take);
    out += take;
    return count += size_t(n);
  }
  // Writes at most `end - out` copies of '0'; `n` must be non-negative.
  ZMIJ_INLINE auto write_zeros(int n) noexcept -> size_t {
    assert(n >= 0);
    size_t take = n < end - out ? size_t(n) : size_t(end - out);
    memset(out, '0', take);
    out += take;
    return count += size_t(n);
  }
};

// Returns true if any character in [first, last) is not '0'.
auto any_nonzero(const char* first, const char* last) noexcept -> bool {
  for (; first < last; ++first)
    if (*first != '0') return true;
  return false;
}

// Writes zero in fixed notation, e.g. "0.000" (or "0" when precision is 0).
ZMIJ_INLINE auto write_zero(char* buffer, int precision) noexcept -> char* {
  *buffer++ = '0';
  if (precision == 0) return buffer;
  *buffer = '.';
  memset(buffer + 1, '0', precision);
  return buffer + 1 + precision;
}

// Writes the exponent as 'e', a sign and at least two digits (e.g. e+05).
template <typename Float>
ZMIJ_INLINE auto write_exp(char* buffer, int dec_exp) noexcept -> char* {
  static_assert(float_traits<Float>::max_exponent10 < 1000, "");
  buffer = write2(buffer, 'e', dec_exp >= 0 ? '+' : '-');
  uint32_t abs_exp = dec_exp >= 0 ? uint32_t(dec_exp) : uint32_t(-dec_exp);
  if (float_traits<Float>::max_exponent10 >= 100) {
    uint32_t hi = use_umul128_hi64 ? umul128_hi64(abs_exp, 0x290000000000000)
                                   : (abs_exp * div100_sig) >> div100_exp;
    *buffer = char('0' + hi);  // hundreds (0-3)
    buffer += abs_exp >= 100;
    abs_exp -= hi * 100;
  }
  memcpy(buffer, digits2(abs_exp), 2);
  return buffer + 2;
}

// Writes the binary exponent as 'p', a sign and at least one digit (e.g. p+3).
auto write_hex_exp(char* buffer, int bin_exp) noexcept -> char* {
  buffer = write2(buffer, 'p', bin_exp < 0 ? '-' : '+');
  unsigned abs_exp = unsigned(bin_exp < 0 ? -bin_exp : bin_exp);
  char digits[8];
  char* p = digits + sizeof(digits);
  do {
    *--p = char('0' + abs_exp % 10);
    abs_exp /= 10;
  } while (abs_exp != 0);
  size_t n = size_t(digits + sizeof(digits) - p);
  memcpy(buffer, p, n);
  return buffer + n;
}

template <typename Float, typename UInt>
ZMIJ_INLINE void normalize(UInt& bin_sig, int64_t& bin_exp) noexcept {
  // clz counts from the top of its operand, so measure from that width.
  int lz, width;
  if (sizeof(UInt) > 8) {
    uint128_t v = bin_sig;
    uint64_t hi = uint64_t(v >> 64);
    lz = hi != 0 ? clz(hi) : 64 + clz(uint64_t(v));
    width = 128;
  } else {
    lz = clz(uint64_t(bin_sig));
    width = 64;
  }
  int shift = lz - (width - 1 - float_traits<Float>::num_sig_bits);
  bin_sig = bin_sig << shift;
  bin_exp = 1 - shift;
}

// Writes num_digits significant digits in scientific form, d[.ddd]e±EE
// (trailing zeros kept), from the top 16 BCD digits `digits` and low two digits
// lo. dec_exp is the leading digit's exponent.
template <typename Float>
ZMIJ_INLINE auto write_scientific_digits(char* buffer,
                                         dec_digits<64>::digits_type digits,
                                         unsigned lo, int num_digits,
                                         int dec_exp) noexcept -> char* {
  memcpy(buffer + 1, &digits, 16);
  memcpy(buffer + 17, digits2(lo), 2);
  buffer[0] = buffer[1];
  buffer[1] = '.';  // Overwritten by the exponent when num_digits is 1.
  return write_exp<Float>(buffer + num_digits + (num_digits > 1), dec_exp);
}

struct shortest_decimal {
  uint64_t sig;
  int exp;
  int last_digit ZMIJ_DEFAULT(0);
  bool has_last_digit ZMIJ_DEFAULT(false);
};

// Here be 🐉s.
// Converts a binary FP number bin_sig * 2**bin_exp to the shortest decimal
// representation, where bin_exp = raw_exp - exp_offset. The smallest normal may
// be passed as irregular without affecting the result.
template <typename Float, typename UInt>
ZMIJ_INLINE auto to_decimal(UInt bin_sig, int64_t raw_exp, bool regular,
                            const data& d) noexcept -> shortest_decimal {
  using traits = float_traits<Float>;
  int64_t bin_exp = raw_exp - traits::exp_offset;
  constexpr int num_bits = std::numeric_limits<UInt>::digits;
  constexpr int extra_shift = exp_shift_table::extra_shift;

  if (!regular) [[ZMIJ_UNLIKELY]] {
    int dec_exp = compute_dec_exp(bin_exp, false);
    unsigned char shift = compute_exp_shift(bin_exp, dec_exp + 1) + extra_shift;
    uint128 pow10 = d.pow10_significands[-dec_exp - 1];
    // Cast to 64 bits: for float, bin_sig << shift can exceed 32 bits.
    uint128 p = umul192_hi128(pow10.hi, pow10.lo, uint64_t(bin_sig) << shift);

    uint64_t integral = p.hi >> extra_shift;
    uint64_t fractional = p.hi << (64 - extra_shift) | p.lo >> extra_shift;

    uint64_t half_ulp = pow10.hi >> (extra_shift + 1 - shift);
    bool round_up = half_ulp > ~uint64_t(0) - fractional;
    bool round_down = (half_ulp >> 1) > fractional;
    integral += round_up;

    int digit = int(umul128_add_hi64(fractional, 10, (uint64_t(1) << 63) - 1));
    int lo =
        int(umul128_add_hi64(fractional - (half_ulp >> 1), 10, ~uint64_t(0)));
    if (digit < lo) digit = lo;
    return {integral, dec_exp, digit, (round_up + round_down) == 0};
  }

  constexpr uint64_t log10_2_sig = 78913;
  constexpr int log10_2_exp = 18;
  int dec_exp = use_umul128_hi64
                    ? umul128_hi64(bin_exp, log10_2_sig << (64 - log10_2_exp))
                    : compute_dec_exp(bin_exp);
  ZMIJ_ASM(("" : "+r"(dec_exp)));  // Force 32-bit reg for sxtw addressing.
  unsigned char shift =
      exp_shift_table::enable
          ? d.exp_shifts.data[bin_exp + float_traits<double>::exp_offset]
          : compute_exp_shift(bin_exp, dec_exp + 1) + extra_shift;
  uint64_t even = 1 - (bin_sig & 1);

  if (num_bits == 32) {
    constexpr int extra_shift = 34;
    shift += extra_shift - exp_shift_table::extra_shift;
    uint64_t pow10_hi = d.pow10_significands[-dec_exp - 1].hi;
    uint64_t p = umul128_hi64(pow10_hi + 1, uint64_t(bin_sig) << shift);

    uint64_t integral = p >> extra_shift;
    uint64_t fractional = p & ((1ull << extra_shift) - 1);

    uint64_t half_ulp = (pow10_hi >> (65 - shift)) + even;
    bool round_up = (fractional + half_ulp) >> extra_shift;
    bool round_down = half_ulp > fractional;
    integral += round_up;

    int digit = int((fractional * 10 + (uint64_t(1) << (extra_shift - 1))) >>
                    extra_shift);
    if (fractional == (uint64_t(1) << (extra_shift - 2))) [[ZMIJ_UNLIKELY]]
      digit = 2;  // Round 2.5 to 2.
    return {integral, dec_exp, digit, (round_up + round_down) == 0};
  }

  // An optimization by Xiang JunBo:
  // Scale by 10**(-dec_exp-1) to directly produce the shorter candidate
  // (15-16 digits), deriving the extra digit from the fractional part.
  // This eliminates division by 10 from the critical path.
  //
  // value = 5.0507837461e-27
  // next  = 5.0507837461000010e-27
  //
  // c = integral.fractional' = 5050783746100000.3153987... (value)
  //                            5050783746100001.0328635... (next)
  //                 half_ulp =                0.3587324...
  //
  // fractional = fractional' * 2**64 = 5818079786399166407
  //
  //    5050783746100000.0       c                        5050783746100001.0
  //            d0             d1|  u1                            u0
  // ──┬────┬────┼────┬────┬────┼*───┼────┬────┬────┬────┬────┬────┼─*──┬───
  //  .8   .9   .0   .1   .2   .3   .4   .5   .6   .7   .8   .9   .0 | .1
  //           └─────────────────┬─────────────────┘                next
  //                            1ulp
  //
  // d0 - shorter underestimate, u0 - shorter overestimate
  // d1 - longer underestimate,  u1 - longer overestimate
  uint128 pow10 = d.pow10_significands[-dec_exp - 1];
  uint128 p = umul192_hi128(pow10.hi, pow10.lo, bin_sig << shift);

  uint64_t integral = p.hi >> extra_shift;
  uint64_t fractional = p.hi << (64 - extra_shift) | p.lo >> extra_shift;

  uint64_t half_ulp = (pow10.hi >> (extra_shift + 1 - shift)) + even;
  bool round_up = fractional + half_ulp < fractional;
  bool round_down = half_ulp > fractional;
  integral += round_up;  // Compute integral before digit.

  // Derive the extra digit from the fractional part (parallel with rounding).
  int digit = int(umul128_add_hi64(fractional, 10, d.biased_half));
  if (fractional == (1ull << 62)) [[ZMIJ_UNLIKELY]]
    digit = 2;  // Round 2.5 to 2.
  return {integral, dec_exp, digit, (round_up + round_down) == 0};
}

struct scaled_value {
  uint64_t integral;
  uint64_t fraction;
  uint64_t fraction_tail;
};

// Scales bin_sig * 2**bin_exp by 10**-dec_exp, retaining the fractional part.
template <typename Float>
ZMIJ_INLINE auto scale(uint64_t bin_sig, int bin_exp, int dec_exp) noexcept
    -> scaled_value {
  constexpr int shift = 64 - float_traits<Float>::digits;
  int point_shift = shift - compute_exp_shift(bin_exp, dec_exp);
  assert(point_shift >= 1 && point_shift < 64);
  uint128 pow10 = static_data.pow10_significands[-dec_exp];
  uint128 p = umul192_hi128(pow10.hi, pow10.lo + 1, uint64_t(bin_sig) << shift);
  return {p.hi >> point_shift, p.hi << (64 - point_shift), p.lo};
}

// Rounds a retained fixed-point value to nearest, ties to even.
ZMIJ_INLINE auto round_even(scaled_value value) noexcept -> uint64_t {
  constexpr uint64_t half = uint64_t(1) << 63;
  bool round_up = half < value.fraction ||
                  (value.fraction == half &&
                   (value.fraction_tail != 0 || (value.integral & 1) != 0));
  return value.integral + round_up;
}

// A value rounded to `precision` significant digits.
struct precision_decimal {
  uint64_t sig;  // precision significant digits, zero-padded right to 18.
  int lead_exp;  // Leading digit's decimal exponent.
};

// Converts a binary FP number bin_sig * 2**bin_exp to a correctly rounded
// decimal with `precision` significant digits.
template <typename Float>
ZMIJ_INLINE auto to_decimal(uint64_t bin_sig, int bin_exp,
                            int precision) noexcept -> precision_decimal {
  constexpr int num_sig_bits = float_traits<Float>::num_sig_bits;
  int dec_exp = compute_dec_exp(bin_exp + num_sig_bits) - (precision - 1);
  scaled_value scaled = scale<Float>(bin_sig, bin_exp, dec_exp);
  uint64_t dec_sig = round_even(scaled);

  if (dec_sig >= pow10s[precision]) {
    // Round one decimal place coarser; the fraction disambiguates a trailing 5.
    dec_sig = scaled.integral / 10;
    uint64_t last_digit = scaled.integral - dec_sig * 10;
    bool has_fraction = (scaled.fraction | scaled.fraction_tail) != 0;
    bool round_up =
        5 < last_digit ||
        (last_digit == 5 && (has_fraction || (dec_sig & 1) != 0));
    dec_sig += round_up;
    ++dec_exp;
  }
  return {dec_sig * pow10s[18 - precision], dec_exp + precision - 1};
}

// A minimal binary big integer (little-endian base-2**32 limbs) over caller-
// provided storage.
struct bigint {
  uint32_t* limbs;
  int num_limbs;  // Significant limbs; 0 represents zero.
  int max_limbs;  // Available limbs in `limbs`.

  // Callers must size `max_limbs` for the largest value the number will reach.
  bigint(uint128_t value, uint32_t* buffer, int max_limbs) noexcept
      : limbs(buffer), max_limbs(max_limbs) {
    uint64_t lo = uint64_t(value), hi = uint64_t(value >> 64);
    limbs[0] = uint32_t(lo);
    limbs[1] = uint32_t(lo >> 32);
    limbs[2] = uint32_t(hi);
    limbs[3] = uint32_t(hi >> 32);
    num_limbs = 4;
    trim();
  }

  void trim() noexcept {
    while (num_limbs > 0 && limbs[num_limbs - 1] == 0) --num_limbs;
  }

  // Shifts left by `n`.
  void shl(int n) noexcept {
    assert(n >= 0);
    if (num_limbs == 0) return;
    int limb_shift = n >> 5, bit_shift = n & 31;
    assert(num_limbs + limb_shift + (bit_shift != 0) <= max_limbs);
    if (bit_shift == 0) {
      for (int i = num_limbs - 1; i >= 0; --i) limbs[i + limb_shift] = limbs[i];
      num_limbs += limb_shift;
    } else {
      limbs[num_limbs + limb_shift] = limbs[num_limbs - 1] >> (32 - bit_shift);
      for (int i = num_limbs - 1; i > 0; --i) {
        limbs[i + limb_shift] =
            limbs[i] << bit_shift | limbs[i - 1] >> (32 - bit_shift);
      }
      limbs[limb_shift] = limbs[0] << bit_shift;
      num_limbs += limb_shift + 1;
    }
    for (int i = 0; i < limb_shift; ++i) limbs[i] = 0;
    trim();
  }

  // Divides by 10**9 in place and returns the remainder.
  auto divmod_1e9() noexcept -> uint32_t {
    uint64_t rem = 0;
    for (int i = num_limbs - 1; i >= 0; --i) {
      uint64_t div = rem << 32 | limbs[i];
      limbs[i] = uint32_t(div / 1000000000u);
      rem = div % 1000000000u;
    }
    trim();
    return uint32_t(rem);
  }

  // Multiplies in place by a nonzero `factor`.
  void mul(uint32_t factor) noexcept {
    assert(factor != 0);
    uint64_t carry = 0;
    for (int i = 0; i < num_limbs; ++i) {
      uint64_t product = uint64_t(limbs[i]) * factor + carry;
      limbs[i] = uint32_t(product);
      carry = product >> 32;
    }
    if (carry == 0) return;
    assert(num_limbs < max_limbs);
    limbs[num_limbs++] = uint32_t(carry);
  }

  // Multiplies in place by 5**n.
  void mul_pow5(int n) noexcept {
    assert(n >= 0);
    static constexpr uint32_t pow5[] = {
        1,     5,      25,      125,     625,      3125,     15625,
        78125, 390625, 1953125, 9765625, 48828125, 244140625};
    while (n >= 13) {  // 5**13 is the largest power of five below 2**32.
      mul(1220703125u);
      n -= 13;
    }
    if (n != 0) mul(pow5[n]);
  }
};

// Emits n's decimal digits (most significant first) ending at `end`, consuming
// n, and returns a pointer to the first (most significant) digit.
auto write_digits(bigint n, char* end) noexcept -> char* {
  char* p = end;
  uint32_t group = n.divmod_1e9();
  while (n.num_limbs != 0) {  // Lower groups keep all 9 digits.
    for (int k = 0; k < 9; ++k, group /= 10) *--p = char('0' + group % 10);
    group = n.divmod_1e9();
  }
  do {  // The most significant group drops its leading zeros.
    *--p = char('0' + group % 10);
  } while ((group /= 10) != 0);
  return p;
}

// Writes bin_sig * 2**bin_exp in fixed notation with `precision` fractional
// digits, correctly rounded (ties to even) via exact big-integer arithmetic.
auto write_fixed_big(char* buffer, uint64_t bin_sig, int bin_exp,
                     int precision) noexcept -> char* {
  uint128_t product = umul128(bin_sig, pow10s[precision]);
  uint32_t limbs[float_traits<double>::big_limbs];
  bigint n(bin_exp < 0 ? shr_round_even(product, -bin_exp) : product, limbs,
           float_traits<double>::big_limbs);
  if (bin_exp >= 0) n.shl(bin_exp);

  // n <= round(DBL_MAX * 10**18): DBL_MAX has 309 integer digits and precision
  // adds at most 18 more.
  char digits[309 + 18];
  char* p = write_digits(n, digits + sizeof(digits));
  int num_digits = int(digits + sizeof(digits) - p);

  // Place the decimal point `precision` digits from the right.
  if (num_digits > precision) {
    int num_int_digits = num_digits - precision;
    memcpy(buffer, p, num_int_digits);
    if (precision == 0) return buffer + num_int_digits;
    buffer[num_int_digits] = '.';
    memcpy(buffer + num_int_digits + 1, p + num_int_digits, precision);
    return buffer + num_int_digits + 1 + precision;
  }
  write2(buffer, '0', '.');  // |value| < 1: "0." + leading zeros + digits.
  int lead_zeros = precision - num_digits;
  memset(buffer + 2, '0', lead_zeros);
  memcpy(buffer + 2 + lead_zeros, p, num_digits);
  return buffer + 2 + lead_zeros + num_digits;
}

// Emits the digit run digits[0..num_digits) with the decimal point placed per
// lead_exp, in fixed or scientific notation. Fixed pads to decimal_places
// fractional digits; scientific appends num_tail_zeros to the fraction.
auto write_number(writer& w, const char* digits, int num_digits, int lead_exp,
                  bool fixed, int decimal_places, int num_tail_zeros) noexcept
    -> size_t {
  if (!fixed) {
    // Emit scientific notation d.ddde±XX.
    w.write(digits[0]);
    // Emit a point only when fractional digits follow it.
    if (num_digits - 1 + num_tail_zeros > 0) {
      w.write('.');
      w.write(digits + 1, num_digits - 1);
      w.write_zeros(num_tail_zeros);  // pad the fraction to precision
    }
    char exp[8];
    char* end = zmij::detail::write_big_exp(exp, lead_exp);
    return w.write(exp, int(end - exp));
  }

  // Emit fixed notation.
  int point_pos = lead_exp + 1;
  int num_int_digits = 0;  // significant digits before the point
  if (point_pos <= 0) {    // |value| < 1, e.g. 0.00123
    w.write('0');
  } else {
    num_int_digits = point_pos < num_digits ? point_pos : num_digits;
    w.write(digits, num_int_digits);
    w.write_zeros(point_pos - num_int_digits);  // integer zeros, e.g. 12300
  }
  if (decimal_places <= 0) return w.count;
  w.write('.');
  int num_lead_zeros = point_pos < 0 ? -point_pos : 0;
  w.write_zeros(num_lead_zeros);  // 0.00...
  int num_frac_digits = num_digits - num_int_digits;
  w.write(digits + num_int_digits, num_frac_digits);
  return w.write_zeros(decimal_places - num_lead_zeros - num_frac_digits);
}

auto write_big(writer& w, bigint num, int bin_exp, int precision, char* digits,
               int digits_size, zmij::format fmt) noexcept -> size_t {
  bool general = fmt == zmij::format::general;
  bool fixed = fmt == zmij::format::fixed;
  assert(precision >= general);

  digits[0] = '0';
  char* p = digits;
  int num_digits = 1;
  int lead_exp = 0;  // exponent of the leading digit

  if (num.num_limbs != 0) {
    // Represent the value exactly as an integer num times a power of ten:
    // value = sig * 2**bin_exp = num * 10**base_exp, so its decimal digits are
    // num's. For bin_exp < 0 the identity 2**bin_exp = 5**-bin_exp *
    // 10**bin_exp keeps num integral via a power-of-five multiply.
    int base_exp = 0;
    if (bin_exp >= 0) {
      num.shl(bin_exp);
    } else {
      num.mul_pow5(-bin_exp);
      base_exp = bin_exp;
    }

    p = write_digits(num, digits + digits_size);
    num_digits = int(digits + digits_size - p);
    lead_exp = num_digits - 1 + base_exp;

    // Significant digits to keep: precision for %g. For %e/%f it counts
    // fractional digits, so add one leading digit; %f adds lead_exp more.
    int max_digits = precision + !general + (fixed ? lead_exp : 0);
    // Round to max_digits significant digits, ties to even.
    if (max_digits < 1) {
      // |value| < 10**-precision: rounds to 0, or up to 10**-precision when the
      // discarded part exceeds half a unit (a tie rounds to even, i.e. 0).
      bool round_up = false;
      if (max_digits == 0) {
        round_up =
            p[0] > '5' || (p[0] == '5' && any_nonzero(p + 1, p + num_digits));
      }
      digits[0] = char('0' + round_up);
      p = digits;
      num_digits = 1;
      lead_exp = round_up ? -precision : 0;
    } else if (num_digits > max_digits) {
      char dropped = p[max_digits];
      bool round_up = dropped > '5';
      // A dropped 5 ties to even unless a lower nonzero digit rounds up.
      if (dropped == '5') {
        round_up = ((p[max_digits - 1] - '0') & 1) ||
                   any_nonzero(p + max_digits + 1, p + num_digits);
      }
      num_digits = max_digits;
      if (round_up) {
        char* q = p + max_digits - 1;
        // Propagate the carry over trailing nines.
        while (*q == '9') *q-- = '0';
        // 999.. rolling over to 1000.. adds a significant digit.
        if (q < p) {
          *--p = '1';
          ++lead_exp;
        } else {
          ++*q;
        }
      }
    }
  }

  int decimal_places = precision;  // fractional digit positions to emit
  if (general) {
    // Drop trailing zeros and pick fixed or scientific.
    while (num_digits > 1 && p[num_digits - 1] == '0') --num_digits;
    fixed = lead_exp >= -4 && lead_exp < precision;
    decimal_places = num_digits - lead_exp - 1;
  }

  // %e pads the fraction to `precision` digits; %g and shortest emit none.
  int num_tail_zeros = general ? 0 : precision - num_digits + 1;
  return write_number(w, p, num_digits, lead_exp, fixed, decimal_places,
                      num_tail_zeros);
}

}  // namespace

namespace zmij {

namespace detail {

template <typename Float>
auto to_decimal(Float value) noexcept -> dec_fp<> {
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto bin_exp = traits::get_exp(bits);  // binary exponent
  auto bin_sig = traits::get_sig(bits);  // binary significand
  auto negative = traits::is_negative(bits);
  if (bin_exp == 0 || bin_exp == traits::exp_mask) [[ZMIJ_UNLIKELY]] {
    if (bin_exp != 0) return {bin_sig, nonfinite_exp, negative};
    if (bin_sig == 0) return {0, 0, negative};
    bin_exp = 1;
    bin_sig |= traits::implicit_bit;
  }
  auto dec = ::to_decimal<Float>(bin_sig ^ traits::implicit_bit, bin_exp,
                                 bin_sig != 0, static_data);
  auto last_digit = -dec.has_last_digit & dec.last_digit;
  return {dec.sig * 10 + last_digit, dec.exp, negative};
}

auto write_big_exp(char* buffer, int dec_exp) noexcept -> char* {
  buffer = write2(buffer, 'e', dec_exp >= 0 ? '+' : '-');
  uint32_t abs_exp = dec_exp >= 0 ? uint32_t(dec_exp) : uint32_t(-dec_exp);
  uint32_t hi = (abs_exp * div100_sig) >> div100_exp;  // abs_exp / 100
  *buffer = char('0' + hi / 10);
  buffer += hi >= 10;
  *buffer = char('0' + hi % 10);
  buffer += abs_exp >= 100;
  memcpy(buffer, digits2(abs_exp - hi * 100), 2);
  return buffer + 2;
}

// It is slightly faster to return a pointer to the end than the size.
template <typename Float>
auto write(char* buffer, Float value) noexcept -> char* {
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto bin_exp = traits::get_exp(bits);  // binary exponent
  auto bin_sig = traits::get_sig(bits);  // binary significand

  *buffer = '-';
  buffer += traits::is_negative(bits);

  const auto* d = &static_data;
  ZMIJ_ASM(("" : "+r"(d)));  // Load constants from memory.
  uint64_t threshold = traits::num_bits == 64 ? d->threshold : uint64_t(1e7);

  shortest_decimal dec;
  if (!traits::is_normal(bin_exp)) [[ZMIJ_UNLIKELY]] {
    if (bin_exp != 0) return write_inf_nan(buffer, bin_sig != 0);
    if (bin_sig == 0) {
      *buffer = '0';
      return buffer + 1;
    }
    dec = ::to_decimal<Float>(bin_sig, 1, true, *d);
    // to_decimal's power of ten comes from the exponent alone, so a subnormal
    // significand comes out short; clz estimates its length within a digit, and
    // dec.sig is 0 for the smallest subnormals, which are all last digit.
    int num_digits = compute_dec_exp(63 - clz(dec.sig | 1)) + (dec.sig != 0);
    num_digits += dec.sig >= pow10s[num_digits];
    int num_zeros = traits::max_digits10 - 3 - num_digits;
    if (num_zeros >= 0) {
      // Padding dec_sig, a digit longer than dec.sig, absorbs the last digit.
      uint64_t dec_sig = dec.sig * 10 + (-dec.has_last_digit & dec.last_digit);
      dec = {dec_sig * pow10s[num_zeros], dec.exp - num_zeros - 1, 0, false};
    }
  } else {
    dec = ::to_decimal<Float>(bin_sig | traits::implicit_bit, bin_exp,
                              bin_sig != 0, *d);
  }
  bool has_last_digit = dec.has_last_digit;
  bool has_extra_digit = dec.sig >= threshold;
  int dec_exp = dec.exp + traits::max_digits10 - 2 + has_extra_digit;
  if (traits::num_bits == 32 && dec.sig < uint32_t(1e6)) [[ZMIJ_UNLIKELY]] {
    dec.sig = 10 * dec.sig + (-has_last_digit & dec.last_digit);
    has_last_digit = false;
    --dec_exp;
  }

  auto dig = to_digits<traits::num_bits>(dec.sig, *d);

  if (traits::num_bits == 32 && float_shuffle_table::enable) {
    return write_float_simd(buffer, dig, dec.last_digit, has_last_digit,
                            has_extra_digit, dec_exp, *d);
  }

  // Write significand/fixed.
  char* start = buffer;
  constexpr int bcd_size = traits::num_bits == 64 ? 16 : 8;
  if (dec_exp >= traits::min_fixed_dec_exp &&
      dec_exp <= traits::max_fixed_dec_exp) {
    memcpy(start, &zeros, 8);  // For dec_exp < 0.
    char last_digit = '0' + (-has_last_digit & dec.last_digit);
    int num_digits = select(has_last_digit, bcd_size, dig.num_digits - 1);

    // Materialize the base early so the entry address is `base + idx*32`;
    // otherwise Clang folds the offset in and adds a cycle to the idx chain.
    const auto* fixed_layouts = &d->fixed_layouts;
    if (ZMIJ_AARCH64) ZMIJ_ASM(("" : "+r"(fixed_layouts)));

    fixed_layout_table::entry scratch;
    const auto& layout = fixed_layouts->get(dec_exp, scratch);
    buffer += layout.start_pos;
#if ZMIJ_USE_SSE4_1
    if (bcd_size == 16) {
      auto& digits = reinterpret_cast<const __m128i&>(dig.digits);
      __m128i tbl = _mm_load_si128(m128ptr(&layout.shuffle[has_extra_digit]));
      __m128i out = _mm_shuffle_epi8(digits, tbl);
      memcpy(buffer, &out, bcd_size);  // Store the assembled digits in one go.
      // The point can push BCD[15] outside the vector to buffer[16], so write
      // it unconditionally (otherwise it's in-vector or overwritten below).
      buffer[bcd_size] = char(_mm_extract_epi8(digits, 15));
      start[layout.point_pos] = '.';
      buffer[layout.last_digit_pos[has_extra_digit]] = last_digit;
      return buffer + layout.end_pos[num_digits + has_extra_digit - 1];
    }
#endif  // ZMIJ_USE_SSE4_1
    write_digits(buffer, dig.digits, !has_extra_digit, *d);
    buffer[bcd_size + has_extra_digit - 1] = last_digit;
    unsigned point_pos = layout.point_pos;
    memmove(start + layout.shift_pos, start + point_pos, bcd_size);
    start[point_pos] = '.';
    return buffer + layout.end_pos[num_digits + has_extra_digit - 1];
  }

  buffer += has_extra_digit;
  memcpy(buffer, &dig.digits, bcd_size);
  buffer[bcd_size] = '0' + dec.last_digit;
  buffer += select(has_last_digit, bcd_size + 1, dig.num_digits);
  start[0] = start[1];
  start[1] = '.';
  buffer -= (buffer - 1 == start + 1);  // Remove trailing point.

  // Write exponent.
  if (exp_string_table::enable) {
    uint64_t exp_data = d->exp_strings.data[dec_exp + exp_string_table::offset];
    int len = int(exp_data >> 48);
    if (is_big_endian) exp_data = bswap64(exp_data);
    memcpy(buffer, &exp_data, traits::max_exponent10 >= 100 ? 8 : 4);
    return buffer + len;
  }
  return write_exp<Float>(buffer, dec_exp);
}

template <typename Float>
auto to_decimal_big(Float value) noexcept -> dec_fp<uint128_t> {
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto raw_exp = traits::get_exp(bits);
  auto bin_sig = traits::get_sig(bits);
  bool negative = traits::is_negative(bits);

  if (!traits::is_normal(raw_exp)) [[ZMIJ_UNLIKELY]] {
    if (raw_exp != 0) return {bin_sig, nonfinite_exp, negative};
    if (bin_sig == 0) return {0, 0, negative};
    raw_exp = 1;
    bin_sig = bin_sig | traits::implicit_bit;
  }

  // The smallest normal is treated as irregular, but the result is unaffected.
  bool regular = bin_sig != 0;
  bin_sig = bin_sig ^ traits::implicit_bit;
  int bin_exp = raw_exp - traits::exp_offset;

  // dec_exp = floor(bin_exp * log10(2) [+ log10(3/4) at a power of two]);
  // scaling by 10**-dec_exp puts the ulp in [1, 10) so the shortest form is the
  // integral part or a neighbor.
  constexpr int64_t log10_2_sig = 20201781;   // round(log10(2) * 2**26)
  constexpr int64_t log10_3_4_sig = 8384497;  // round(-log10(3/4) * 2**26)
  constexpr int64_t log10_2_exp = 26;
  int dec_exp = int((bin_exp * log10_2_sig - (regular ? 0 : log10_3_4_sig)) >>
                    log10_2_exp);

  uint64_t p10_sig[pow10::result_limbs];
  int p10_exp = pow10::compute(-dec_exp, p10_sig);
  // value * 10**-dec_exp = bin_sig * p10_sig / 2**shift
  int shift = -(bin_exp + p10_exp);
  assert(shift >= 128 && shift <= 256);

  // Scale by a power of 10 and split into integral and fractional parts.
  uint64_t sig64[2] = {uint64_t(bin_sig), uint64_t(uint128_t(bin_sig) >> 64)};
  uint64_t product[2 + pow10::result_limbs], scaled[4];
  pow10::mul(sig64, 2, p10_sig, pow10::result_limbs, product);
  pow10::extract(product, 2 + pow10::result_limbs, shift - 128, scaled, 4);
  uint128_t integral = uint128_t(scaled[3]) << 64 | scaled[2];
  uint128_t fractional = uint128_t(scaled[1]) << 64 | scaled[0];
  uint64_t last_digit = uint64_t(integral % 10);

  // half_ulp = floor(p10_sig / 2**(shift-123)): half a binary ulp in c units.
  uint64_t half_ulp64[2];
  pow10::extract(p10_sig, pow10::result_limbs, shift - 123, half_ulp64, 2);
  uint128_t half_ulp = uint128_t(half_ulp64[1]) << 64 | half_ulp64[0];

  // Based on Yaoyuan Guo's (yy) method, adapted to long double.
  uint128_t c = uint128_t(last_digit) << 124 | fractional >> 4;
  uint128_t half = uint128_t(1) << 127;  // fixed point 1/2
  bool even = (uint64_t(bin_sig) & 1) == 0;

  // round_up keeps all digits and rounds the significand to nearest; trim_down
  // drops the last digit; trim_up drops it and carries to the next ten. Exact
  // boundaries are detected by equality and broken toward an even significand.
  bool round_up, trim_down;
  if (regular) {
    round_up = fractional >= half;
    if (fractional == half) round_up = (uint64_t(integral) & 1) != 0;
    trim_down = c <= half_ulp;
    if (c == half_ulp) {
      // 124-bit tie: low 64 bits break it, to even on an exact match.
      uint64_t frac_lo = uint64_t(fractional), ulp_lo;
      pow10::extract(p10_sig, pow10::result_limbs, shift - 127, &ulp_lo, 1);
      trim_down = frac_lo == ulp_lo ? even : frac_lo < ulp_lo;
    }
  } else {
    round_up = fractional > half;
    uint128_t quarter_ulp = half_ulp >> 1;
    if ((fractional >> 4) > quarter_ulp) round_up = true;
    trim_down = c <= quarter_ulp;
  }

  // trim_up iff value + half_ulp reaches the next ten, i.e. c + half_ulp
  // reaches ten; compared as c >= ten - half_ulp so the sum can't overflow
  // 128 bits. A boundary (gap in {0, 1}) breaks to even, guarded by
  // dec_exp == 0 for the exact gap == 0 case.
  uint128_t ten = uint128_t(10) << 124;  // the next ten in c units
  bool trim_up = c >= ten - half_ulp;    // c + half_ulp >= ten
  uint128_t gap = ten - half_ulp - c;    // wraps large if c + half_ulp > ten
  if (gap <= 1 && (dec_exp == 0 || gap == 1)) trim_up = even;

  uint128_t dec_sig = trim_down || trim_up
                          ? integral - last_digit + trim_up * 10
                          : integral + round_up;
  return {dec_sig, dec_exp, negative};
}

template <typename Float>
auto write_big(char* out, size_t n, Float value) noexcept -> size_t {
  using traits = float_traits<Float>;
  dec_fp<uint128_t> dec = to_decimal_big(value);

  writer w = {out, out + n, 0};
  if (dec.negative) w.write('-');
  if (dec.exp == nonfinite_exp)
    return w.write(dec.sig != uint128_t(0) ? "nan" : "inf", 3);
  if (dec.sig == uint128_t(0)) return w.write('0');

  // Convert the significand to digits, msb first, and drop trailing zeros.
  char digits[40];
  char* start = digits + sizeof(digits);
  for (uint128_t x = dec.sig; x != uint128_t(0);)
    *--start = char('0' + divmod10(x));
  int num_digits = int(digits + sizeof(digits) - start);
  int lead_exp = dec.exp + num_digits - 1;
  while (num_digits > 1 && start[num_digits - 1] == '0') --num_digits;

  bool fixed = lead_exp >= traits::min_fixed_dec_exp &&
               lead_exp <= traits::max_fixed_dec_exp;
  return write_number(w, start, num_digits, lead_exp, fixed,
                      num_digits - lead_exp - 1, 0);
}

template <typename Float>
auto write_big(char* out, size_t n, Float value, int precision,
               format fmt) noexcept -> size_t {
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto bin_exp = traits::get_exp(bits);
  auto bin_sig = traits::get_sig(bits);

  writer w = {out, out + n, 0};
  if (traits::is_negative(bits)) w.write('-');

  if (!traits::is_normal(bin_exp)) [[ZMIJ_UNLIKELY]] {
    if (bin_exp != 0)  // inf or nan
      return w.write(bin_sig != 0 ? "nan" : "inf", 3);
    if (bin_sig != 0) normalize<Float>(bin_sig, bin_exp);
    bin_sig = bin_sig ^ traits::implicit_bit;
  }
  bin_sig = bin_sig ^ traits::implicit_bit;

  // One scratch block holds the big integer's limbs followed by the decimal
  // digits. It stays on the stack except for the extended long double case,
  // which is too large (~11.6 KB).
  constexpr int digit_words = (traits::big_digits + 3) / 4;
  constexpr int scratch_words = traits::big_limbs + digit_words;
  constexpr bool use_heap =
      traits::big_digits > float_traits<double>::big_digits;
  uint32_t stack_scratch[use_heap ? 1 : scratch_words];
  uint32_t* scratch =
      use_heap ? static_cast<uint32_t*>(malloc(size_t(scratch_words) * 4))
               : stack_scratch;
  if (!scratch) return 0;
  bigint num(bin_sig, scratch, traits::big_limbs);
  char* digits = reinterpret_cast<char*>(scratch + traits::big_limbs);
  size_t size = ::write_big(w, num, int(bin_exp - traits::exp_offset),
                            precision, digits, traits::big_digits, fmt);
  if (use_heap) free(scratch);
  return size;
}

template <typename Float>
auto write_scientific(char* buffer, Float value, int precision) noexcept
    -> char* {
  assert(precision >= 1 && precision <= 18);
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto bin_exp = traits::get_exp(bits);
  auto bin_sig = traits::get_sig(bits);

  *buffer = '-';
  buffer += traits::is_negative(bits);

  if (!traits::is_normal(bin_exp)) [[ZMIJ_UNLIKELY]] {
    if (bin_exp != 0) return write_inf_nan(buffer, bin_sig != 0);
    if (bin_sig == 0) {  // zero, e.g. 0.000e+00
      memset(buffer, '0', precision + 1);
      buffer[1] = '.';  // Overwritten by the exponent when precision is 1.
      return write_exp<Float>(buffer + precision + (precision > 1), 0);
    }
    normalize<Float>(bin_sig, bin_exp);
  }

  auto dec = ::to_decimal<Float>(bin_sig | traits::implicit_bit,
                                 bin_exp - traits::exp_offset, precision);
  auto digits = to_digits<64>(dec.sig / 100, static_data).digits;
  return write_scientific_digits<Float>(buffer, digits, unsigned(dec.sig % 100),
                                        precision, dec.lead_exp);
}

template <typename Float>
auto write_general(char* buffer, Float value, int precision) noexcept -> char* {
  assert(precision >= 1 && precision <= 18);
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto bin_exp = traits::get_exp(bits);
  auto bin_sig = traits::get_sig(bits);

  *buffer = '-';
  buffer += traits::is_negative(bits);

  if (!traits::is_normal(bin_exp)) [[ZMIJ_UNLIKELY]] {
    if (bin_exp != 0) return write_inf_nan(buffer, bin_sig != 0);
    if (bin_sig == 0) {  // zero
      *buffer = '0';
      return buffer + 1;
    }
    normalize<Float>(bin_sig, bin_exp);
  }

  auto dec = ::to_decimal<Float>(bin_sig | traits::implicit_bit,
                                 bin_exp - traits::exp_offset, precision);

  auto lo = unsigned(dec.sig % 100);
  auto hi = to_digits<64>(dec.sig / 100, static_data);
  int num_digits = lo ? 18 - (lo % 10 == 0) : hi.num_digits;

  int lead_exp = dec.lead_exp;
  if (lead_exp < -4 || lead_exp >= precision) {
    return write_scientific_digits<Float>(buffer, hi.digits, lo, num_digits,
                                          lead_exp);
  }

  if (lead_exp < 0) {  // fixed with a leading 0.00...: lead_exp in [-4, -1]
    memcpy(buffer, "0.0000", 1 - lead_exp);  // "0.", then -lead_exp-1 zeros.
    memcpy(buffer + 1 - lead_exp, &hi.digits, 16);
    memcpy(buffer + 17 - lead_exp, digits2(lo), 2);
    return buffer + (1 - lead_exp) + num_digits;
  }

  memcpy(buffer, &hi.digits, 16);
  memcpy(buffer + 16, digits2(lo), 2);
  int point_pos = lead_exp + 1;
  if (point_pos >= num_digits) {  // All significant digits are integral.
    memset(buffer + num_digits, '0', point_pos - num_digits);
    return buffer + point_pos;
  }
  memmove(buffer + point_pos + 1, buffer + point_pos, num_digits - point_pos);
  buffer[point_pos] = '.';
  return buffer + num_digits + 1;
}

template <typename Float>
auto write_fixed(char* buffer, Float value, int precision) noexcept -> char* {
  assert(precision >= 0 && precision <= 18);
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto bin_exp = traits::get_exp(bits);
  auto bin_sig = traits::get_sig(bits);

  *buffer = '-';
  buffer += traits::is_negative(bits);

  if (!traits::is_normal(bin_exp)) [[ZMIJ_UNLIKELY]] {
    if (bin_exp != 0) return write_inf_nan(buffer, bin_sig != 0);
    if (bin_sig == 0) return write_zero(buffer, precision);  // e.g. 0.000
    normalize<Float>(bin_sig, bin_exp);
  }

  bin_exp -= traits::exp_offset;
  bin_sig |= traits::implicit_bit;

  // Scale to 18 significant digits, retaining the fraction for later rounding.
  int dec_exp = compute_dec_exp(bin_exp + traits::num_sig_bits) - 17;
  scaled_value scaled = scale<Float>(bin_sig, bin_exp, dec_exp);
  // A one-too-small dec_exp estimate leaves 19 significant digits, so derive
  // the count (and the exact leading exponent) from the truncated integral.
  uint64_t integral = scaled.integral;
  bool has_fraction = (scaled.fraction | scaled.fraction_tail) != 0;
  int num_scaled_digits = 18 + (integral >= pow10s[18]);
  int lead_exp = dec_exp + num_scaled_digits - 1;
  int num_digits = lead_exp + 1 + precision;  // significant digits to emit

  if (num_digits <= 0) [[ZMIJ_UNLIKELY]] {
    // |value| < 10**-precision, so it rounds to 0, or up to 10**-precision iff
    // |value| > 0.5 * 10**-precision (a tie rounds to 0). A 19-digit scale has
    // leading digit 1 (< 5), so only the 18-digit case rounds up.
    char* end = write_zero(buffer, precision);
    uint64_t half = 5 * pow10s[17];
    if (num_scaled_digits == 18 && lead_exp == -precision - 1 &&
        (half < integral || (half == integral && has_fraction))) {
      end[-1] = '1';
    }
    return end;
  }

  if (num_digits > 18) [[ZMIJ_UNLIKELY]]
    return write_fixed_big(buffer, bin_sig, int(bin_exp), precision);

  uint64_t dec_sig = 0;
  if (num_digits < num_scaled_digits) {
    // Round the dropped decimal digits and retained fraction in one pass.
    uint64_t pow = pow10s[num_scaled_digits - num_digits];
    dec_sig = integral / pow;
    uint64_t remainder = integral - dec_sig * pow;
    uint64_t half = pow / 2;
    bool round_up =
        half < remainder ||
        (half == remainder && (has_fraction || (dec_sig & 1) != 0));
    dec_sig += round_up;
  } else {
    dec_sig = round_even(scaled);
  }
  if (dec_sig >= pow10s[num_digits]) {  // carry to next power of ten
    ++lead_exp;
    dec_sig /= 10;
  }
  dec_sig *= pow10s[18 - num_digits];
  auto lo = unsigned(dec_sig % 100);
  auto hi = to_digits<64>(dec_sig / 100, static_data);

  int num_int_digits = lead_exp + 1;
  int total = num_int_digits + precision;  // significant digits + zero padding

  if (num_int_digits <= 0) {  // |value| < 1: "0." + leading zeros + digits
    write2(buffer, '0', '.');
    memset(buffer + 2, '0', -num_int_digits);
    buffer += 2 - num_int_digits;
    memcpy(buffer, &hi.digits, 16);
    memcpy(buffer + 16, digits2(lo), 2);
    return buffer + total;
  }

  memcpy(buffer, &hi.digits, 16);
  memcpy(buffer + 16, digits2(lo), 2);
  buffer[18] = '0';  // at most one carry digit: total <= 18, or 19 on carry
  if (precision == 0) return buffer + total;
  memmove(buffer + num_int_digits + 1, buffer + num_int_digits, precision);
  buffer[num_int_digits] = '.';
  return buffer + total + 1;
}

template <typename Float>
auto write_hex(char* buffer, Float value, bool prefix) noexcept -> char* {
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto bin_exp = traits::get_exp(bits);
  auto bin_sig = traits::get_sig(bits);

  *buffer = '-';
  buffer += traits::is_negative(bits);

  bool zero = false;
  if (!traits::is_normal(bin_exp)) [[ZMIJ_UNLIKELY]] {
    if (bin_exp != 0) return write_inf_nan(buffer, bin_sig != 0);
    zero = bin_sig == 0;
    if (zero) bin_exp = traits::exp_bias;  // This cancels to 0 below.
    else normalize<Float>(bin_sig, bin_exp);
  }
  bin_exp -= traits::exp_bias;

  if (prefix) buffer = write2(buffer, '0', 'x');
  *buffer++ = char('0' + !zero);

  constexpr int width = int(sizeof(bin_sig)) * 8;
  bin_sig = bin_sig << (width - traits::num_sig_bits);
  if (bin_sig != 0) {
    *buffer++ = '.';
    do {
      *buffer++ = "0123456789abcdef"[uint64_t(bin_sig >> (width - 4))];
      bin_sig = bin_sig << 4;
    } while (bin_sig != 0);
  }

  return write_hex_exp(buffer, bin_exp);
}

template <typename Float>
auto write_hex(char* out, size_t n, Float value, int precision,
               bool prefix) noexcept -> size_t {
  using traits = float_traits<Float>;
  auto bits = traits::to_bits(value);
  auto bin_exp = traits::get_exp(bits);
  auto bin_sig = traits::get_sig(bits);

  writer w = {out, out + n, 0};
  if (traits::is_negative(bits)) w.write('-');

  bool zero = false;
  if (!traits::is_normal(bin_exp)) [[ZMIJ_UNLIKELY]] {
    if (bin_exp != 0) return w.write(bin_sig != 0 ? "nan" : "inf", 3);
    zero = bin_sig == 0;
    if (zero) bin_exp = traits::exp_bias;  // This cancels to 0 below.
    else normalize<Float>(bin_sig, bin_exp);
  }
  bin_exp -= traits::exp_bias;

  // Left-align the fraction so hex digits can be read off the top.
  constexpr int width = int(sizeof(bin_sig)) * 8;
  bin_sig = bin_sig << (width - traits::num_sig_bits);

  // Round to `precision` hex digits (ties to even). A carry out of all kept
  // bits propagates into the leading digit, which for a normalized value just
  // increments the binary exponent.
  int keep = precision * 4;
  if (keep < traits::num_sig_bits) {
    int drop = width - keep;
    auto half = decltype(bin_sig)(1) << (drop - 1);
    // Round to nearest, ties to even: bias by (half - 1 + lsb) then truncate,
    // where lsb is the retained part's low bit. A tie rounds up only when lsb
    // is set (odd); for precision 0 that bit is the implicit, odd leading 1.
    auto lsb = keep == 0 ? 1 : (bin_sig >> drop) & 1;
    auto before = bin_sig;
    bin_sig = bin_sig + (half - 1 + lsb);
    if (bin_sig < before) ++bin_exp;
    bin_sig = keep == 0 ? 0 : (bin_sig >> drop) << drop;
  }

  if (prefix) w.write("0x", 2);
  w.write(char('0' + !zero));

  if (precision > 0) {
    w.write('.');
    int i = 0;
    do {
      w.write("0123456789abcdef"[uint64_t(bin_sig >> (width - 4))]);
      bin_sig = bin_sig << 4;
      ++i;
    } while (bin_sig != 0);
    w.write_zeros(precision - i);
  }

  char exp[8];
  return w.write(exp, int(write_hex_exp(exp, bin_exp) - exp));
}

template auto to_decimal(float value) noexcept -> dec_fp<>;
template auto to_decimal(double value) noexcept -> dec_fp<>;

template auto write(char* buffer, float value) noexcept -> char*;
template auto write(char* buffer, double value) noexcept -> char*;

template auto write_big(char* out, size_t n, double value, int precision,
                        format fmt) noexcept -> size_t;

template auto write_scientific(char* buffer, float value,
                               int precision) noexcept -> char*;
template auto write_scientific(char* buffer, double value,
                               int precision) noexcept -> char*;

template auto write_general(char* buffer, float value, int precision) noexcept
    -> char*;
template auto write_general(char* buffer, double value, int precision) noexcept
    -> char*;

template auto write_fixed(char* buffer, float value, int precision) noexcept
    -> char*;
template auto write_fixed(char* buffer, double value, int precision) noexcept
    -> char*;

template auto write_hex(char* buffer, double value, bool prefix) noexcept
    -> char*;
template auto write_hex(char* out, size_t n, double value, int precision,
                        bool prefix) noexcept -> size_t;

// long double instantiations, only needed when it differs from double; else
// the public wrappers forward long double to the double path.
#if LDBL_MANT_DIG != DBL_MANT_DIG
template auto to_decimal_big(long double value) noexcept -> dec_fp<uint128_t>;
template auto write_big(char* out, size_t n, long double value) noexcept
    -> size_t;
template auto write_big(char* out, size_t n, long double value, int precision,
                        format fmt) noexcept -> size_t;
template auto write_hex(char* buffer, long double value, bool prefix) noexcept
    -> char*;
template auto write_hex(char* out, size_t n, long double value, int precision,
                        bool prefix) noexcept -> size_t;
#endif

}  // namespace detail
}  // namespace zmij
