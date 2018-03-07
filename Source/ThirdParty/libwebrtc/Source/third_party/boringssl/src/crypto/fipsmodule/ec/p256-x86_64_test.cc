/* Copyright (c) 2016, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include <openssl/base.h>

#include <stdio.h>
#include <string.h>

#include <gtest/gtest.h>

#include <openssl/bn.h>
#include <openssl/mem.h>

#include "../bn/internal.h"
#include "../../internal.h"
#include "../../test/file_test.h"
#include "../../test/test_util.h"
#include "p256-x86_64.h"


// Disable tests if BORINGSSL_SHARED_LIBRARY is defined. These tests need access
// to internal functions.
#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && \
    !defined(OPENSSL_SMALL) && !defined(BORINGSSL_SHARED_LIBRARY)

TEST(P256_X86_64Test, SelectW5) {
  // Fill a table with some garbage input.
  alignas(64) P256_POINT table[16];
  for (size_t i = 0; i < 16; i++) {
    OPENSSL_memset(table[i].X, 3 * i, sizeof(table[i].X));
    OPENSSL_memset(table[i].Y, 3 * i + 1, sizeof(table[i].Y));
    OPENSSL_memset(table[i].Z, 3 * i + 2, sizeof(table[i].Z));
  }

  for (int i = 0; i <= 16; i++) {
    P256_POINT val;
    ecp_nistz256_select_w5(&val, table, i);

    P256_POINT expected;
    if (i == 0) {
      OPENSSL_memset(&expected, 0, sizeof(expected));
    } else {
      expected = table[i-1];
    }

    EXPECT_EQ(Bytes(reinterpret_cast<const char *>(&expected), sizeof(expected)),
              Bytes(reinterpret_cast<const char *>(&val), sizeof(val)));
  }
}

TEST(P256_X86_64Test, SelectW7) {
  // Fill a table with some garbage input.
  alignas(64) P256_POINT_AFFINE table[64];
  for (size_t i = 0; i < 64; i++) {
    OPENSSL_memset(table[i].X, 2 * i, sizeof(table[i].X));
    OPENSSL_memset(table[i].Y, 2 * i + 1, sizeof(table[i].Y));
  }

  for (int i = 0; i <= 64; i++) {
    P256_POINT_AFFINE val;
    ecp_nistz256_select_w7(&val, table, i);

    P256_POINT_AFFINE expected;
    if (i == 0) {
      OPENSSL_memset(&expected, 0, sizeof(expected));
    } else {
      expected = table[i-1];
    }

    EXPECT_EQ(Bytes(reinterpret_cast<const char *>(&expected), sizeof(expected)),
              Bytes(reinterpret_cast<const char *>(&val), sizeof(val)));
  }
}

static bool GetFieldElement(FileTest *t, BN_ULONG out[P256_LIMBS],
                            const char *name) {
  std::vector<uint8_t> bytes;
  if (!t->GetBytes(&bytes, name)) {
    return false;
  }

  if (bytes.size() != BN_BYTES * P256_LIMBS) {
    ADD_FAILURE() << "Invalid length: " << name;
    return false;
  }

  // |byte| contains bytes in big-endian while |out| should contain |BN_ULONG|s
  // in little-endian.
  OPENSSL_memset(out, 0, P256_LIMBS * sizeof(BN_ULONG));
  for (size_t i = 0; i < bytes.size(); i++) {
    out[P256_LIMBS - 1 - (i / BN_BYTES)] <<= 8;
    out[P256_LIMBS - 1 - (i / BN_BYTES)] |= bytes[i];
  }

  return true;
}

static std::string FieldElementToString(const BN_ULONG a[P256_LIMBS]) {
  std::string ret;
  for (size_t i = P256_LIMBS-1; i < P256_LIMBS; i--) {
    char buf[2 * BN_BYTES + 1];
    BIO_snprintf(buf, sizeof(buf), BN_HEX_FMT2, a[i]);
    ret += buf;
  }
  return ret;
}

static testing::AssertionResult ExpectFieldElementsEqual(
    const char *expected_expr, const char *actual_expr,
    const BN_ULONG expected[P256_LIMBS], const BN_ULONG actual[P256_LIMBS]) {
  if (OPENSSL_memcmp(expected, actual, sizeof(BN_ULONG) * P256_LIMBS) == 0) {
    return testing::AssertionSuccess();
  }

  return testing::AssertionFailure()
         << "Expected: " << FieldElementToString(expected) << " ("
         << expected_expr << ")\n"
         << "Actual:   " << FieldElementToString(actual) << " (" << actual_expr
         << ")";
}

#define EXPECT_FIELD_ELEMENTS_EQUAL(a, b) \
  EXPECT_PRED_FORMAT2(ExpectFieldElementsEqual, a, b)

static bool PointToAffine(P256_POINT_AFFINE *out, const P256_POINT *in) {
  static const uint8_t kP[] = {
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };

  bssl::UniquePtr<BIGNUM> x(BN_new()), y(BN_new()), z(BN_new());
  bssl::UniquePtr<BIGNUM> p(BN_bin2bn(kP, sizeof(kP), nullptr));
  if (!x || !y || !z || !p ||
      !bn_set_words(x.get(), in->X, P256_LIMBS) ||
      !bn_set_words(y.get(), in->Y, P256_LIMBS) ||
      !bn_set_words(z.get(), in->Z, P256_LIMBS)) {
    return false;
  }

  // Coordinates must be fully-reduced.
  if (BN_cmp(x.get(), p.get()) >= 0 ||
      BN_cmp(y.get(), p.get()) >= 0 ||
      BN_cmp(z.get(), p.get()) >= 0) {
    return false;
  }

  OPENSSL_memset(out, 0, sizeof(P256_POINT_AFFINE));

  if (BN_is_zero(z.get())) {
    // The point at infinity is represented as (0, 0).
    return true;
  }

  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  bssl::UniquePtr<BN_MONT_CTX> mont(BN_MONT_CTX_new());
  if (!ctx || !mont ||
      !BN_MONT_CTX_set(mont.get(), p.get(), ctx.get()) ||
      // Invert Z.
      !BN_from_montgomery(z.get(), z.get(), mont.get(), ctx.get()) ||
      !BN_mod_inverse(z.get(), z.get(), p.get(), ctx.get()) ||
      !BN_to_montgomery(z.get(), z.get(), mont.get(), ctx.get()) ||
      // Convert (X, Y, Z) to (X/Z^2, Y/Z^3).
      !BN_mod_mul_montgomery(x.get(), x.get(), z.get(), mont.get(),
                             ctx.get()) ||
      !BN_mod_mul_montgomery(x.get(), x.get(), z.get(), mont.get(),
                             ctx.get()) ||
      !BN_mod_mul_montgomery(y.get(), y.get(), z.get(), mont.get(),
                             ctx.get()) ||
      !BN_mod_mul_montgomery(y.get(), y.get(), z.get(), mont.get(),
                             ctx.get()) ||
      !BN_mod_mul_montgomery(y.get(), y.get(), z.get(), mont.get(),
                             ctx.get())) {
    return false;
  }

  OPENSSL_memcpy(out->X, x->d, sizeof(BN_ULONG) * x->top);
  OPENSSL_memcpy(out->Y, y->d, sizeof(BN_ULONG) * y->top);
  return true;
}

static testing::AssertionResult ExpectPointsEqual(
    const char *expected_expr, const char *actual_expr,
    const P256_POINT_AFFINE *expected, const P256_POINT *actual) {
  // There are multiple representations of the same |P256_POINT|, so convert to
  // |P256_POINT_AFFINE| and compare.
  P256_POINT_AFFINE affine;
  if (!PointToAffine(&affine, actual)) {
    return testing::AssertionFailure()
           << "Could not convert " << actual_expr << " to affine: ("
           << FieldElementToString(actual->X) << ", "
           << FieldElementToString(actual->Y) << ", "
           << FieldElementToString(actual->Z) << ")";
  }

  if (OPENSSL_memcmp(expected, &affine, sizeof(P256_POINT_AFFINE)) != 0) {
    return testing::AssertionFailure()
           << "Expected: (" << FieldElementToString(expected->X) << ", "
           << FieldElementToString(expected->Y) << ") (" << expected_expr
           << "; affine)\n"
           << "Actual:   (" << FieldElementToString(affine.X) << ", "
           << FieldElementToString(affine.Y) << ") (" << actual_expr << ")";
  }

  return testing::AssertionSuccess();
}

#define EXPECT_POINTS_EQUAL(a, b) EXPECT_PRED_FORMAT2(ExpectPointsEqual, a, b)

static void TestNegate(FileTest *t) {
  BN_ULONG a[P256_LIMBS], b[P256_LIMBS];
  ASSERT_TRUE(GetFieldElement(t, a, "A"));
  ASSERT_TRUE(GetFieldElement(t, b, "B"));

  // Test that -A = B.
  BN_ULONG ret[P256_LIMBS];
  ecp_nistz256_neg(ret, a);
  EXPECT_FIELD_ELEMENTS_EQUAL(b, ret);

  OPENSSL_memcpy(ret, a, sizeof(ret));
  ecp_nistz256_neg(ret, ret /* a */);
  EXPECT_FIELD_ELEMENTS_EQUAL(b, ret);

  // Test that -B = A.
  ecp_nistz256_neg(ret, b);
  EXPECT_FIELD_ELEMENTS_EQUAL(a, ret);

  OPENSSL_memcpy(ret, b, sizeof(ret));
  ecp_nistz256_neg(ret, ret /* b */);
  EXPECT_FIELD_ELEMENTS_EQUAL(a, ret);
}

static void TestMulMont(FileTest *t) {
  BN_ULONG a[P256_LIMBS], b[P256_LIMBS], result[P256_LIMBS];
  ASSERT_TRUE(GetFieldElement(t, a, "A"));
  ASSERT_TRUE(GetFieldElement(t, b, "B"));
  ASSERT_TRUE(GetFieldElement(t, result, "Result"));

  BN_ULONG ret[P256_LIMBS];
  ecp_nistz256_mul_mont(ret, a, b);
  EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);

  ecp_nistz256_mul_mont(ret, b, a);
  EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);

  OPENSSL_memcpy(ret, a, sizeof(ret));
  ecp_nistz256_mul_mont(ret, ret /* a */, b);
  EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);

  OPENSSL_memcpy(ret, a, sizeof(ret));
  ecp_nistz256_mul_mont(ret, b, ret);
  EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);

  OPENSSL_memcpy(ret, b, sizeof(ret));
  ecp_nistz256_mul_mont(ret, a, ret /* b */);
  EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);

  OPENSSL_memcpy(ret, b, sizeof(ret));
  ecp_nistz256_mul_mont(ret, ret /* b */, a);
  EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);

  if (OPENSSL_memcmp(a, b, sizeof(a)) == 0) {
    ecp_nistz256_sqr_mont(ret, a);
    EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);

    OPENSSL_memcpy(ret, a, sizeof(ret));
    ecp_nistz256_sqr_mont(ret, ret /* a */);
    EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);
  }
}

static void TestFromMont(FileTest *t) {
  BN_ULONG a[P256_LIMBS], result[P256_LIMBS];
  ASSERT_TRUE(GetFieldElement(t, a, "A"));
  ASSERT_TRUE(GetFieldElement(t, result, "Result"));

  BN_ULONG ret[P256_LIMBS];
  ecp_nistz256_from_mont(ret, a);
  EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);

  OPENSSL_memcpy(ret, a, sizeof(ret));
  ecp_nistz256_from_mont(ret, ret /* a */);
  EXPECT_FIELD_ELEMENTS_EQUAL(result, ret);
}

static void TestPointAdd(FileTest *t) {
  P256_POINT a, b;
  P256_POINT_AFFINE result;
  ASSERT_TRUE(GetFieldElement(t, a.X, "A.X"));
  ASSERT_TRUE(GetFieldElement(t, a.Y, "A.Y"));
  ASSERT_TRUE(GetFieldElement(t, a.Z, "A.Z"));
  ASSERT_TRUE(GetFieldElement(t, b.X, "B.X"));
  ASSERT_TRUE(GetFieldElement(t, b.Y, "B.Y"));
  ASSERT_TRUE(GetFieldElement(t, b.Z, "B.Z"));
  ASSERT_TRUE(GetFieldElement(t, result.X, "Result.X"));
  ASSERT_TRUE(GetFieldElement(t, result.Y, "Result.Y"));

  P256_POINT ret;
  ecp_nistz256_point_add(&ret, &a, &b);
  EXPECT_POINTS_EQUAL(&result, &ret);

  ecp_nistz256_point_add(&ret, &b, &a);
  EXPECT_POINTS_EQUAL(&result, &ret);

  OPENSSL_memcpy(&ret, &a, sizeof(ret));
  ecp_nistz256_point_add(&ret, &ret /* a */, &b);
  EXPECT_POINTS_EQUAL(&result, &ret);

  OPENSSL_memcpy(&ret, &a, sizeof(ret));
  ecp_nistz256_point_add(&ret, &b, &ret /* a */);
  EXPECT_POINTS_EQUAL(&result, &ret);

  OPENSSL_memcpy(&ret, &b, sizeof(ret));
  ecp_nistz256_point_add(&ret, &a, &ret /* b */);
  EXPECT_POINTS_EQUAL(&result, &ret);

  OPENSSL_memcpy(&ret, &b, sizeof(ret));
  ecp_nistz256_point_add(&ret, &ret /* b */, &a);
  EXPECT_POINTS_EQUAL(&result, &ret);

  P256_POINT_AFFINE a_affine, b_affine, infinity;
  OPENSSL_memset(&infinity, 0, sizeof(infinity));
  ASSERT_TRUE(PointToAffine(&a_affine, &a));
  ASSERT_TRUE(PointToAffine(&b_affine, &b));

  // ecp_nistz256_point_add_affine does not work when a == b unless doubling the
  // point at infinity.
  if (OPENSSL_memcmp(&a_affine, &b_affine, sizeof(a_affine)) != 0 ||
      OPENSSL_memcmp(&a_affine, &infinity, sizeof(a_affine)) == 0) {
    ecp_nistz256_point_add_affine(&ret, &a, &b_affine);
    EXPECT_POINTS_EQUAL(&result, &ret);

    OPENSSL_memcpy(&ret, &a, sizeof(ret));
    ecp_nistz256_point_add_affine(&ret, &ret /* a */, &b_affine);
    EXPECT_POINTS_EQUAL(&result, &ret);

    ecp_nistz256_point_add_affine(&ret, &b, &a_affine);
    EXPECT_POINTS_EQUAL(&result, &ret);

    OPENSSL_memcpy(&ret, &b, sizeof(ret));
    ecp_nistz256_point_add_affine(&ret, &ret /* b */, &a_affine);
    EXPECT_POINTS_EQUAL(&result, &ret);
  }

  if (OPENSSL_memcmp(&a, &b, sizeof(a)) == 0) {
    ecp_nistz256_point_double(&ret, &a);
    EXPECT_POINTS_EQUAL(&result, &ret);

    ret = a;
    ecp_nistz256_point_double(&ret, &ret /* a */);
    EXPECT_POINTS_EQUAL(&result, &ret);
  }
}

TEST(P256_X86_64Test, TestVectors) {
  return FileTestGTest("crypto/fipsmodule/ec/p256-x86_64_tests.txt",
                       [](FileTest *t) {
    if (t->GetParameter() == "Negate") {
      TestNegate(t);
    } else if (t->GetParameter() == "MulMont") {
      TestMulMont(t);
    } else if (t->GetParameter() == "FromMont") {
      TestFromMont(t);
    } else if (t->GetParameter() == "PointAdd") {
      TestPointAdd(t);
    } else {
      FAIL() << "Unknown test type:" << t->GetParameter();
    }
  });
}

#endif
