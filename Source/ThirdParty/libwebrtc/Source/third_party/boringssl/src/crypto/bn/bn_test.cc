/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the Eric Young open source
 * license provided above.
 *
 * The binary polynomial arithmetic software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems
 * Laboratories. */

/* Per C99, various stdint.h and inttypes.h macros (the latter used by bn.h) are
 * unavailable in C++ unless some macros are defined. C++11 overruled this
 * decision, but older Android NDKs still require it. */
#if !defined(__STDC_CONSTANT_MACROS)
#define __STDC_CONSTANT_MACROS
#endif
#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <utility>

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../internal.h"
#include "../test/file_test.h"
#include "../test/test_util.h"


static int HexToBIGNUM(bssl::UniquePtr<BIGNUM> *out, const char *in) {
  BIGNUM *raw = NULL;
  int ret = BN_hex2bn(&raw, in);
  out->reset(raw);
  return ret;
}

static bssl::UniquePtr<BIGNUM> GetBIGNUM(FileTest *t, const char *attribute) {
  std::string hex;
  if (!t->GetAttribute(&hex, attribute)) {
    return nullptr;
  }

  bssl::UniquePtr<BIGNUM> ret;
  if (HexToBIGNUM(&ret, hex.c_str()) != static_cast<int>(hex.size())) {
    t->PrintLine("Could not decode '%s'.", hex.c_str());
    return nullptr;
  }
  return ret;
}

static bool GetInt(FileTest *t, int *out, const char *attribute) {
  bssl::UniquePtr<BIGNUM> ret = GetBIGNUM(t, attribute);
  if (!ret) {
    return false;
  }

  BN_ULONG word = BN_get_word(ret.get());
  if (word > INT_MAX) {
    return false;
  }

  *out = static_cast<int>(word);
  return true;
}

static bool ExpectBIGNUMsEqual(FileTest *t, const char *operation,
                               const BIGNUM *expected, const BIGNUM *actual) {
  if (BN_cmp(expected, actual) == 0) {
    return true;
  }

  bssl::UniquePtr<char> expected_str(BN_bn2hex(expected));
  bssl::UniquePtr<char> actual_str(BN_bn2hex(actual));
  if (!expected_str || !actual_str) {
    return false;
  }

  t->PrintLine("Got %s =", operation);
  t->PrintLine("\t%s", actual_str.get());
  t->PrintLine("wanted:");
  t->PrintLine("\t%s", expected_str.get());
  return false;
}

static bool TestSum(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> b = GetBIGNUM(t, "B");
  bssl::UniquePtr<BIGNUM> sum = GetBIGNUM(t, "Sum");
  if (!a || !b || !sum) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> ret(BN_new());
  if (!ret ||
      !BN_add(ret.get(), a.get(), b.get()) ||
      !ExpectBIGNUMsEqual(t, "A + B", sum.get(), ret.get()) ||
      !BN_sub(ret.get(), sum.get(), a.get()) ||
      !ExpectBIGNUMsEqual(t, "Sum - A", b.get(), ret.get()) ||
      !BN_sub(ret.get(), sum.get(), b.get()) ||
      !ExpectBIGNUMsEqual(t, "Sum - B", a.get(), ret.get())) {
    return false;
  }

  // Test that the functions work when |r| and |a| point to the same |BIGNUM|,
  // or when |r| and |b| point to the same |BIGNUM|. TODO: Test the case where
  // all of |r|, |a|, and |b| point to the same |BIGNUM|.
  if (!BN_copy(ret.get(), a.get()) ||
      !BN_add(ret.get(), ret.get(), b.get()) ||
      !ExpectBIGNUMsEqual(t, "A + B (r is a)", sum.get(), ret.get()) ||
      !BN_copy(ret.get(), b.get()) ||
      !BN_add(ret.get(), a.get(), ret.get()) ||
      !ExpectBIGNUMsEqual(t, "A + B (r is b)", sum.get(), ret.get()) ||
      !BN_copy(ret.get(), sum.get()) ||
      !BN_sub(ret.get(), ret.get(), a.get()) ||
      !ExpectBIGNUMsEqual(t, "Sum - A (r is a)", b.get(), ret.get()) ||
      !BN_copy(ret.get(), a.get()) ||
      !BN_sub(ret.get(), sum.get(), ret.get()) ||
      !ExpectBIGNUMsEqual(t, "Sum - A (r is b)", b.get(), ret.get()) ||
      !BN_copy(ret.get(), sum.get()) ||
      !BN_sub(ret.get(), ret.get(), b.get()) ||
      !ExpectBIGNUMsEqual(t, "Sum - B (r is a)", a.get(), ret.get()) ||
      !BN_copy(ret.get(), b.get()) ||
      !BN_sub(ret.get(), sum.get(), ret.get()) ||
      !ExpectBIGNUMsEqual(t, "Sum - B (r is b)", a.get(), ret.get())) {
    return false;
  }

  // Test |BN_uadd| and |BN_usub| with the prerequisites they are documented as
  // having. Note that these functions are frequently used when the
  // prerequisites don't hold. In those cases, they are supposed to work as if
  // the prerequisite hold, but we don't test that yet. TODO: test that.
  if (!BN_is_negative(a.get()) &&
      !BN_is_negative(b.get()) && BN_cmp(a.get(), b.get()) >= 0) {
    if (!BN_uadd(ret.get(), a.get(), b.get()) ||
        !ExpectBIGNUMsEqual(t, "A +u B", sum.get(), ret.get()) ||
        !BN_usub(ret.get(), sum.get(), a.get()) ||
        !ExpectBIGNUMsEqual(t, "Sum -u A", b.get(), ret.get()) ||
        !BN_usub(ret.get(), sum.get(), b.get()) ||
        !ExpectBIGNUMsEqual(t, "Sum -u B", a.get(), ret.get())) {
      return false;
    }

    // Test that the functions work when |r| and |a| point to the same |BIGNUM|,
    // or when |r| and |b| point to the same |BIGNUM|. TODO: Test the case where
    // all of |r|, |a|, and |b| point to the same |BIGNUM|.
    if (!BN_copy(ret.get(), a.get()) ||
        !BN_uadd(ret.get(), ret.get(), b.get()) ||
        !ExpectBIGNUMsEqual(t, "A +u B (r is a)", sum.get(), ret.get()) ||
        !BN_copy(ret.get(), b.get()) ||
        !BN_uadd(ret.get(), a.get(), ret.get()) ||
        !ExpectBIGNUMsEqual(t, "A +u B (r is b)", sum.get(), ret.get()) ||
        !BN_copy(ret.get(), sum.get()) ||
        !BN_usub(ret.get(), ret.get(), a.get()) ||
        !ExpectBIGNUMsEqual(t, "Sum -u A (r is a)", b.get(), ret.get()) ||
        !BN_copy(ret.get(), a.get()) ||
        !BN_usub(ret.get(), sum.get(), ret.get()) ||
        !ExpectBIGNUMsEqual(t, "Sum -u A (r is b)", b.get(), ret.get()) ||
        !BN_copy(ret.get(), sum.get()) ||
        !BN_usub(ret.get(), ret.get(), b.get()) ||
        !ExpectBIGNUMsEqual(t, "Sum -u B (r is a)", a.get(), ret.get()) ||
        !BN_copy(ret.get(), b.get()) ||
        !BN_usub(ret.get(), sum.get(), ret.get()) ||
        !ExpectBIGNUMsEqual(t, "Sum -u B (r is b)", a.get(), ret.get())) {
      return false;
    }
  }

  // Test with |BN_add_word| and |BN_sub_word| if |b| is small enough.
  BN_ULONG b_word = BN_get_word(b.get());
  if (!BN_is_negative(b.get()) && b_word != (BN_ULONG)-1) {
    if (!BN_copy(ret.get(), a.get()) ||
        !BN_add_word(ret.get(), b_word) ||
        !ExpectBIGNUMsEqual(t, "A + B (word)", sum.get(), ret.get()) ||
        !BN_copy(ret.get(), sum.get()) ||
        !BN_sub_word(ret.get(), b_word) ||
        !ExpectBIGNUMsEqual(t, "Sum - B (word)", a.get(), ret.get())) {
      return false;
    }
  }

  return true;
}

static bool TestLShift1(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> lshift1 = GetBIGNUM(t, "LShift1");
  bssl::UniquePtr<BIGNUM> zero(BN_new());
  if (!a || !lshift1 || !zero) {
    return false;
  }

  BN_zero(zero.get());

  bssl::UniquePtr<BIGNUM> ret(BN_new()), two(BN_new()), remainder(BN_new());
  if (!ret || !two || !remainder ||
      !BN_set_word(two.get(), 2) ||
      !BN_add(ret.get(), a.get(), a.get()) ||
      !ExpectBIGNUMsEqual(t, "A + A", lshift1.get(), ret.get()) ||
      !BN_mul(ret.get(), a.get(), two.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "A * 2", lshift1.get(), ret.get()) ||
      !BN_div(ret.get(), remainder.get(), lshift1.get(), two.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "LShift1 / 2", a.get(), ret.get()) ||
      !ExpectBIGNUMsEqual(t, "LShift1 % 2", zero.get(), remainder.get()) ||
      !BN_lshift1(ret.get(), a.get()) ||
      !ExpectBIGNUMsEqual(t, "A << 1", lshift1.get(), ret.get()) ||
      !BN_rshift1(ret.get(), lshift1.get()) ||
      !ExpectBIGNUMsEqual(t, "LShift >> 1", a.get(), ret.get()) ||
      !BN_rshift1(ret.get(), lshift1.get()) ||
      !ExpectBIGNUMsEqual(t, "LShift >> 1", a.get(), ret.get())) {
    return false;
  }

  // Set the LSB to 1 and test rshift1 again.
  if (!BN_set_bit(lshift1.get(), 0) ||
      !BN_div(ret.get(), nullptr /* rem */, lshift1.get(), two.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "(LShift1 | 1) / 2", a.get(), ret.get()) ||
      !BN_rshift1(ret.get(), lshift1.get()) ||
      !ExpectBIGNUMsEqual(t, "(LShift | 1) >> 1", a.get(), ret.get())) {
    return false;
  }

  return true;
}

static bool TestLShift(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> lshift = GetBIGNUM(t, "LShift");
  int n = 0;
  if (!a || !lshift || !GetInt(t, &n, "N")) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> ret(BN_new());
  if (!ret ||
      !BN_lshift(ret.get(), a.get(), n) ||
      !ExpectBIGNUMsEqual(t, "A << N", lshift.get(), ret.get()) ||
      !BN_rshift(ret.get(), lshift.get(), n) ||
      !ExpectBIGNUMsEqual(t, "A >> N", a.get(), ret.get())) {
    return false;
  }

  return true;
}

static bool TestRShift(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> rshift = GetBIGNUM(t, "RShift");
  int n = 0;
  if (!a || !rshift || !GetInt(t, &n, "N")) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> ret(BN_new());
  if (!ret ||
      !BN_rshift(ret.get(), a.get(), n) ||
      !ExpectBIGNUMsEqual(t, "A >> N", rshift.get(), ret.get())) {
    return false;
  }

  return true;
}

static bool TestSquare(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> square = GetBIGNUM(t, "Square");
  bssl::UniquePtr<BIGNUM> zero(BN_new());
  if (!a || !square || !zero) {
    return false;
  }

  BN_zero(zero.get());

  bssl::UniquePtr<BIGNUM> ret(BN_new()), remainder(BN_new());
  if (!ret || !remainder ||
      !BN_sqr(ret.get(), a.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "A^2", square.get(), ret.get()) ||
      !BN_mul(ret.get(), a.get(), a.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "A * A", square.get(), ret.get()) ||
      !BN_div(ret.get(), remainder.get(), square.get(), a.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "Square / A", a.get(), ret.get()) ||
      !ExpectBIGNUMsEqual(t, "Square % A", zero.get(), remainder.get())) {
    return false;
  }

  BN_set_negative(a.get(), 0);
  if (!BN_sqrt(ret.get(), square.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "sqrt(Square)", a.get(), ret.get())) {
    return false;
  }

  // BN_sqrt should fail on non-squares and negative numbers.
  if (!BN_is_zero(square.get())) {
    bssl::UniquePtr<BIGNUM> tmp(BN_new());
    if (!tmp || !BN_copy(tmp.get(), square.get())) {
      return false;
    }
    BN_set_negative(tmp.get(), 1);

    if (BN_sqrt(ret.get(), tmp.get(), ctx)) {
      t->PrintLine("BN_sqrt succeeded on a negative number");
      return false;
    }
    ERR_clear_error();

    BN_set_negative(tmp.get(), 0);
    if (!BN_add(tmp.get(), tmp.get(), BN_value_one())) {
      return false;
    }
    if (BN_sqrt(ret.get(), tmp.get(), ctx)) {
      t->PrintLine("BN_sqrt succeeded on a non-square");
      return false;
    }
    ERR_clear_error();
  }

  return true;
}

static bool TestProduct(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> b = GetBIGNUM(t, "B");
  bssl::UniquePtr<BIGNUM> product = GetBIGNUM(t, "Product");
  bssl::UniquePtr<BIGNUM> zero(BN_new());
  if (!a || !b || !product || !zero) {
    return false;
  }

  BN_zero(zero.get());

  bssl::UniquePtr<BIGNUM> ret(BN_new()), remainder(BN_new());
  if (!ret || !remainder ||
      !BN_mul(ret.get(), a.get(), b.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "A * B", product.get(), ret.get()) ||
      !BN_div(ret.get(), remainder.get(), product.get(), a.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "Product / A", b.get(), ret.get()) ||
      !ExpectBIGNUMsEqual(t, "Product % A", zero.get(), remainder.get()) ||
      !BN_div(ret.get(), remainder.get(), product.get(), b.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "Product / B", a.get(), ret.get()) ||
      !ExpectBIGNUMsEqual(t, "Product % B", zero.get(), remainder.get())) {
    return false;
  }

  return true;
}

static bool TestQuotient(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> b = GetBIGNUM(t, "B");
  bssl::UniquePtr<BIGNUM> quotient = GetBIGNUM(t, "Quotient");
  bssl::UniquePtr<BIGNUM> remainder = GetBIGNUM(t, "Remainder");
  if (!a || !b || !quotient || !remainder) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> ret(BN_new()), ret2(BN_new());
  if (!ret || !ret2 ||
      !BN_div(ret.get(), ret2.get(), a.get(), b.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "A / B", quotient.get(), ret.get()) ||
      !ExpectBIGNUMsEqual(t, "A % B", remainder.get(), ret2.get()) ||
      !BN_mul(ret.get(), quotient.get(), b.get(), ctx) ||
      !BN_add(ret.get(), ret.get(), remainder.get()) ||
      !ExpectBIGNUMsEqual(t, "Quotient * B + Remainder", a.get(), ret.get())) {
    return false;
  }

  // Test with |BN_mod_word| and |BN_div_word| if the divisor is small enough.
  BN_ULONG b_word = BN_get_word(b.get());
  if (!BN_is_negative(b.get()) && b_word != (BN_ULONG)-1) {
    BN_ULONG remainder_word = BN_get_word(remainder.get());
    assert(remainder_word != (BN_ULONG)-1);
    if (!BN_copy(ret.get(), a.get())) {
      return false;
    }
    BN_ULONG ret_word = BN_div_word(ret.get(), b_word);
    if (ret_word != remainder_word) {
      t->PrintLine("Got A %% B (word) = " BN_HEX_FMT1 ", wanted " BN_HEX_FMT1
                   "\n",
                   ret_word, remainder_word);
      return false;
    }
    if (!ExpectBIGNUMsEqual(t, "A / B (word)", quotient.get(), ret.get())) {
      return false;
    }

    ret_word = BN_mod_word(a.get(), b_word);
    if (ret_word != remainder_word) {
      t->PrintLine("Got A %% B (word) = " BN_HEX_FMT1 ", wanted " BN_HEX_FMT1
                   "\n",
                   ret_word, remainder_word);
      return false;
    }
  }

  // Test BN_nnmod.
  if (!BN_is_negative(b.get())) {
    bssl::UniquePtr<BIGNUM> nnmod(BN_new());
    if (!nnmod ||
        !BN_copy(nnmod.get(), remainder.get()) ||
        (BN_is_negative(nnmod.get()) &&
         !BN_add(nnmod.get(), nnmod.get(), b.get())) ||
        !BN_nnmod(ret.get(), a.get(), b.get(), ctx) ||
        !ExpectBIGNUMsEqual(t, "A % B (non-negative)", nnmod.get(),
                            ret.get())) {
      return false;
    }
  }

  return true;
}

static bool TestModMul(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> b = GetBIGNUM(t, "B");
  bssl::UniquePtr<BIGNUM> m = GetBIGNUM(t, "M");
  bssl::UniquePtr<BIGNUM> mod_mul = GetBIGNUM(t, "ModMul");
  if (!a || !b || !m || !mod_mul) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> ret(BN_new());
  if (!ret ||
      !BN_mod_mul(ret.get(), a.get(), b.get(), m.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "A * B (mod M)", mod_mul.get(), ret.get())) {
    return false;
  }

  if (BN_is_odd(m.get())) {
    // Reduce |a| and |b| and test the Montgomery version.
    bssl::UniquePtr<BN_MONT_CTX> mont(BN_MONT_CTX_new());
    bssl::UniquePtr<BIGNUM> a_tmp(BN_new()), b_tmp(BN_new());
    if (!mont || !a_tmp || !b_tmp ||
        !BN_MONT_CTX_set(mont.get(), m.get(), ctx) ||
        !BN_nnmod(a_tmp.get(), a.get(), m.get(), ctx) ||
        !BN_nnmod(b_tmp.get(), b.get(), m.get(), ctx) ||
        !BN_to_montgomery(a_tmp.get(), a_tmp.get(), mont.get(), ctx) ||
        !BN_to_montgomery(b_tmp.get(), b_tmp.get(), mont.get(), ctx) ||
        !BN_mod_mul_montgomery(ret.get(), a_tmp.get(), b_tmp.get(), mont.get(),
                               ctx) ||
        !BN_from_montgomery(ret.get(), ret.get(), mont.get(), ctx) ||
        !ExpectBIGNUMsEqual(t, "A * B (mod M) (Montgomery)",
                            mod_mul.get(), ret.get())) {
      return false;
    }
  }

  return true;
}

static bool TestModExp(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> e = GetBIGNUM(t, "E");
  bssl::UniquePtr<BIGNUM> m = GetBIGNUM(t, "M");
  bssl::UniquePtr<BIGNUM> mod_exp = GetBIGNUM(t, "ModExp");
  if (!a || !e || !m || !mod_exp) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> ret(BN_new());
  if (!ret ||
      !BN_mod_exp(ret.get(), a.get(), e.get(), m.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "A ^ E (mod M)", mod_exp.get(), ret.get())) {
    return false;
  }

  if (BN_is_odd(m.get())) {
    if (!BN_mod_exp_mont(ret.get(), a.get(), e.get(), m.get(), ctx, NULL) ||
        !ExpectBIGNUMsEqual(t, "A ^ E (mod M) (Montgomery)", mod_exp.get(),
                            ret.get()) ||
        !BN_mod_exp_mont_consttime(ret.get(), a.get(), e.get(), m.get(), ctx,
                                   NULL) ||
        !ExpectBIGNUMsEqual(t, "A ^ E (mod M) (constant-time)", mod_exp.get(),
                            ret.get())) {
      return false;
    }
  }

  return true;
}

static bool TestExp(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> e = GetBIGNUM(t, "E");
  bssl::UniquePtr<BIGNUM> exp = GetBIGNUM(t, "Exp");
  if (!a || !e || !exp) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> ret(BN_new());
  if (!ret ||
      !BN_exp(ret.get(), a.get(), e.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "A ^ E", exp.get(), ret.get())) {
    return false;
  }

  return true;
}

static bool TestModSqrt(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> p = GetBIGNUM(t, "P");
  bssl::UniquePtr<BIGNUM> mod_sqrt = GetBIGNUM(t, "ModSqrt");
  if (!a || !p || !mod_sqrt) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> ret(BN_new());
  bssl::UniquePtr<BIGNUM> ret2(BN_new());
  if (!ret ||
      !ret2 ||
      !BN_mod_sqrt(ret.get(), a.get(), p.get(), ctx) ||
      // There are two possible answers.
      !BN_sub(ret2.get(), p.get(), ret.get())) {
    return false;
  }

  if (BN_cmp(ret2.get(), mod_sqrt.get()) != 0 &&
      !ExpectBIGNUMsEqual(t, "sqrt(A) (mod P)", mod_sqrt.get(), ret.get())) {
    return false;
  }

  return true;
}

static bool TestModInv(FileTest *t, BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a = GetBIGNUM(t, "A");
  bssl::UniquePtr<BIGNUM> m = GetBIGNUM(t, "M");
  bssl::UniquePtr<BIGNUM> mod_inv = GetBIGNUM(t, "ModInv");
  if (!a || !m || !mod_inv) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> ret(BN_new());
  if (!ret ||
      !BN_mod_inverse(ret.get(), a.get(), m.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "inv(A) (mod M)", mod_inv.get(), ret.get())) {
    return false;
  }

  BN_set_flags(a.get(), BN_FLG_CONSTTIME);

  if (!ret ||
      !BN_mod_inverse(ret.get(), a.get(), m.get(), ctx) ||
      !ExpectBIGNUMsEqual(t, "inv(A) (mod M) (constant-time)", mod_inv.get(),
                          ret.get())) {
    return false;
  }

  return true;
}

struct Test {
  const char *name;
  bool (*func)(FileTest *t, BN_CTX *ctx);
};

static const Test kTests[] = {
    {"Sum", TestSum},
    {"LShift1", TestLShift1},
    {"LShift", TestLShift},
    {"RShift", TestRShift},
    {"Square", TestSquare},
    {"Product", TestProduct},
    {"Quotient", TestQuotient},
    {"ModMul", TestModMul},
    {"ModExp", TestModExp},
    {"Exp", TestExp},
    {"ModSqrt", TestModSqrt},
    {"ModInv", TestModInv},
};

static bool RunTest(FileTest *t, void *arg) {
  BN_CTX *ctx = reinterpret_cast<BN_CTX *>(arg);
  for (const Test &test : kTests) {
    if (t->GetType() != test.name) {
      continue;
    }
    return test.func(t, ctx);
  }
  t->PrintLine("Unknown test type: %s", t->GetType().c_str());
  return false;
}

static bool TestBN2BinPadded(BN_CTX *ctx) {
  uint8_t zeros[256], out[256], reference[128];

  memset(zeros, 0, sizeof(zeros));

  // Test edge case at 0.
  bssl::UniquePtr<BIGNUM> n(BN_new());
  if (!n || !BN_bn2bin_padded(NULL, 0, n.get())) {
    fprintf(stderr,
            "BN_bn2bin_padded failed to encode 0 in an empty buffer.\n");
    return false;
  }
  memset(out, -1, sizeof(out));
  if (!BN_bn2bin_padded(out, sizeof(out), n.get())) {
    fprintf(stderr,
            "BN_bn2bin_padded failed to encode 0 in a non-empty buffer.\n");
    return false;
  }
  if (memcmp(zeros, out, sizeof(out))) {
    fprintf(stderr, "BN_bn2bin_padded did not zero buffer.\n");
    return false;
  }

  // Test a random numbers at various byte lengths.
  for (size_t bytes = 128 - 7; bytes <= 128; bytes++) {
    if (!BN_rand(n.get(), bytes * 8, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY)) {
      ERR_print_errors_fp(stderr);
      return false;
    }
    if (BN_num_bytes(n.get()) != bytes ||
        BN_bn2bin(n.get(), reference) != bytes) {
      fprintf(stderr, "Bad result from BN_rand; bytes.\n");
      return false;
    }
    // Empty buffer should fail.
    if (BN_bn2bin_padded(NULL, 0, n.get())) {
      fprintf(stderr,
              "BN_bn2bin_padded incorrectly succeeded on empty buffer.\n");
      return false;
    }
    // One byte short should fail.
    if (BN_bn2bin_padded(out, bytes - 1, n.get())) {
      fprintf(stderr, "BN_bn2bin_padded incorrectly succeeded on short.\n");
      return false;
    }
    // Exactly right size should encode.
    if (!BN_bn2bin_padded(out, bytes, n.get()) ||
        memcmp(out, reference, bytes) != 0) {
      fprintf(stderr, "BN_bn2bin_padded gave a bad result.\n");
      return false;
    }
    // Pad up one byte extra.
    if (!BN_bn2bin_padded(out, bytes + 1, n.get()) ||
        memcmp(out + 1, reference, bytes) || memcmp(out, zeros, 1)) {
      fprintf(stderr, "BN_bn2bin_padded gave a bad result.\n");
      return false;
    }
    // Pad up to 256.
    if (!BN_bn2bin_padded(out, sizeof(out), n.get()) ||
        memcmp(out + sizeof(out) - bytes, reference, bytes) ||
        memcmp(out, zeros, sizeof(out) - bytes)) {
      fprintf(stderr, "BN_bn2bin_padded gave a bad result.\n");
      return false;
    }
  }

  return true;
}

static int DecimalToBIGNUM(bssl::UniquePtr<BIGNUM> *out, const char *in) {
  BIGNUM *raw = NULL;
  int ret = BN_dec2bn(&raw, in);
  out->reset(raw);
  return ret;
}

static bool TestDec2BN(BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> bn;
  int ret = DecimalToBIGNUM(&bn, "0");
  if (ret != 1 || !BN_is_zero(bn.get()) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_dec2bn gave a bad result.\n");
    return false;
  }

  ret = DecimalToBIGNUM(&bn, "256");
  if (ret != 3 || !BN_is_word(bn.get(), 256) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_dec2bn gave a bad result.\n");
    return false;
  }

  ret = DecimalToBIGNUM(&bn, "-42");
  if (ret != 3 || !BN_abs_is_word(bn.get(), 42) || !BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_dec2bn gave a bad result.\n");
    return false;
  }

  ret = DecimalToBIGNUM(&bn, "-0");
  if (ret != 2 || !BN_is_zero(bn.get()) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_dec2bn gave a bad result.\n");
    return false;
  }

  ret = DecimalToBIGNUM(&bn, "42trailing garbage is ignored");
  if (ret != 2 || !BN_abs_is_word(bn.get(), 42) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_dec2bn gave a bad result.\n");
    return false;
  }

  return true;
}

static bool TestHex2BN(BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> bn;
  int ret = HexToBIGNUM(&bn, "0");
  if (ret != 1 || !BN_is_zero(bn.get()) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_hex2bn gave a bad result.\n");
    return false;
  }

  ret = HexToBIGNUM(&bn, "256");
  if (ret != 3 || !BN_is_word(bn.get(), 0x256) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_hex2bn gave a bad result.\n");
    return false;
  }

  ret = HexToBIGNUM(&bn, "-42");
  if (ret != 3 || !BN_abs_is_word(bn.get(), 0x42) || !BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_hex2bn gave a bad result.\n");
    return false;
  }

  ret = HexToBIGNUM(&bn, "-0");
  if (ret != 2 || !BN_is_zero(bn.get()) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_hex2bn gave a bad result.\n");
    return false;
  }

  ret = HexToBIGNUM(&bn, "abctrailing garbage is ignored");
  if (ret != 3 || !BN_is_word(bn.get(), 0xabc) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_hex2bn gave a bad result.\n");
    return false;
  }

  return true;
}

static bssl::UniquePtr<BIGNUM> ASCIIToBIGNUM(const char *in) {
  BIGNUM *raw = NULL;
  if (!BN_asc2bn(&raw, in)) {
    return nullptr;
  }
  return bssl::UniquePtr<BIGNUM>(raw);
}

static bool TestASC2BN(BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> bn = ASCIIToBIGNUM("0");
  if (!bn || !BN_is_zero(bn.get()) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_asc2bn gave a bad result.\n");
    return false;
  }

  bn = ASCIIToBIGNUM("256");
  if (!bn || !BN_is_word(bn.get(), 256) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_asc2bn gave a bad result.\n");
    return false;
  }

  bn = ASCIIToBIGNUM("-42");
  if (!bn || !BN_abs_is_word(bn.get(), 42) || !BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_asc2bn gave a bad result.\n");
    return false;
  }

  bn = ASCIIToBIGNUM("0x1234");
  if (!bn || !BN_is_word(bn.get(), 0x1234) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_asc2bn gave a bad result.\n");
    return false;
  }

  bn = ASCIIToBIGNUM("0X1234");
  if (!bn || !BN_is_word(bn.get(), 0x1234) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_asc2bn gave a bad result.\n");
    return false;
  }

  bn = ASCIIToBIGNUM("-0xabcd");
  if (!bn || !BN_abs_is_word(bn.get(), 0xabcd) || !BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_asc2bn gave a bad result.\n");
    return false;
  }

  bn = ASCIIToBIGNUM("-0");
  if (!bn || !BN_is_zero(bn.get()) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_asc2bn gave a bad result.\n");
    return false;
  }

  bn = ASCIIToBIGNUM("123trailing garbage is ignored");
  if (!bn || !BN_is_word(bn.get(), 123) || BN_is_negative(bn.get())) {
    fprintf(stderr, "BN_asc2bn gave a bad result.\n");
    return false;
  }

  return true;
}

struct MPITest {
  const char *base10;
  const char *mpi;
  size_t mpi_len;
};

static const MPITest kMPITests[] = {
  { "0", "\x00\x00\x00\x00", 4 },
  { "1", "\x00\x00\x00\x01\x01", 5 },
  { "-1", "\x00\x00\x00\x01\x81", 5 },
  { "128", "\x00\x00\x00\x02\x00\x80", 6 },
  { "256", "\x00\x00\x00\x02\x01\x00", 6 },
  { "-256", "\x00\x00\x00\x02\x81\x00", 6 },
};

static bool TestMPI() {
  uint8_t scratch[8];

  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kMPITests); i++) {
    const MPITest &test = kMPITests[i];
    bssl::UniquePtr<BIGNUM> bn(ASCIIToBIGNUM(test.base10));
    if (!bn) {
      return false;
    }

    const size_t mpi_len = BN_bn2mpi(bn.get(), NULL);
    if (mpi_len > sizeof(scratch)) {
      fprintf(stderr, "MPI test #%u: MPI size is too large to test.\n",
              (unsigned)i);
      return false;
    }

    const size_t mpi_len2 = BN_bn2mpi(bn.get(), scratch);
    if (mpi_len != mpi_len2) {
      fprintf(stderr, "MPI test #%u: length changes.\n", (unsigned)i);
      return false;
    }

    if (mpi_len != test.mpi_len ||
        memcmp(test.mpi, scratch, mpi_len) != 0) {
      fprintf(stderr, "MPI test #%u failed:\n", (unsigned)i);
      hexdump(stderr, "Expected: ", test.mpi, test.mpi_len);
      hexdump(stderr, "Got:      ", scratch, mpi_len);
      return false;
    }

    bssl::UniquePtr<BIGNUM> bn2(BN_mpi2bn(scratch, mpi_len, NULL));
    if (bn2.get() == nullptr) {
      fprintf(stderr, "MPI test #%u: failed to parse\n", (unsigned)i);
      return false;
    }

    if (BN_cmp(bn.get(), bn2.get()) != 0) {
      fprintf(stderr, "MPI test #%u: wrong result\n", (unsigned)i);
      return false;
    }
  }

  return true;
}

static bool TestRand() {
  bssl::UniquePtr<BIGNUM> bn(BN_new());
  if (!bn) {
    return false;
  }

  // Test BN_rand accounts for degenerate cases with |top| and |bottom|
  // parameters.
  if (!BN_rand(bn.get(), 0, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) ||
      !BN_is_zero(bn.get())) {
    fprintf(stderr, "BN_rand gave a bad result.\n");
    return false;
  }
  if (!BN_rand(bn.get(), 0, BN_RAND_TOP_TWO, BN_RAND_BOTTOM_ODD) ||
      !BN_is_zero(bn.get())) {
    fprintf(stderr, "BN_rand gave a bad result.\n");
    return false;
  }

  if (!BN_rand(bn.get(), 1, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) ||
      !BN_is_word(bn.get(), 1)) {
    fprintf(stderr, "BN_rand gave a bad result.\n");
    return false;
  }
  if (!BN_rand(bn.get(), 1, BN_RAND_TOP_TWO, BN_RAND_BOTTOM_ANY) ||
      !BN_is_word(bn.get(), 1)) {
    fprintf(stderr, "BN_rand gave a bad result.\n");
    return false;
  }
  if (!BN_rand(bn.get(), 1, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ODD) ||
      !BN_is_word(bn.get(), 1)) {
    fprintf(stderr, "BN_rand gave a bad result.\n");
    return false;
  }

  if (!BN_rand(bn.get(), 2, BN_RAND_TOP_TWO, BN_RAND_BOTTOM_ANY) ||
      !BN_is_word(bn.get(), 3)) {
    fprintf(stderr, "BN_rand gave a bad result.\n");
    return false;
  }

  return true;
}

struct ASN1Test {
  const char *value_ascii;
  const char *der;
  size_t der_len;
};

static const ASN1Test kASN1Tests[] = {
    {"0", "\x02\x01\x00", 3},
    {"1", "\x02\x01\x01", 3},
    {"127", "\x02\x01\x7f", 3},
    {"128", "\x02\x02\x00\x80", 4},
    {"0xdeadbeef", "\x02\x05\x00\xde\xad\xbe\xef", 7},
    {"0x0102030405060708",
     "\x02\x08\x01\x02\x03\x04\x05\x06\x07\x08", 10},
    {"0xffffffffffffffff",
      "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xff", 11},
};

struct ASN1InvalidTest {
  const char *der;
  size_t der_len;
};

static const ASN1InvalidTest kASN1InvalidTests[] = {
    // Bad tag.
    {"\x03\x01\x00", 3},
    // Empty contents.
    {"\x02\x00", 2},
};

// kASN1BuggyTests contains incorrect encodings and the corresponding, expected
// results of |BN_parse_asn1_unsigned_buggy| given that input.
static const ASN1Test kASN1BuggyTests[] = {
    // Negative numbers.
    {"128", "\x02\x01\x80", 3},
    {"255", "\x02\x01\xff", 3},
    // Unnecessary leading zeros.
    {"1", "\x02\x02\x00\x01", 4},
};

static bool TestASN1() {
  for (const ASN1Test &test : kASN1Tests) {
    bssl::UniquePtr<BIGNUM> bn = ASCIIToBIGNUM(test.value_ascii);
    if (!bn) {
      return false;
    }

    // Test that the input is correctly parsed.
    bssl::UniquePtr<BIGNUM> bn2(BN_new());
    if (!bn2) {
      return false;
    }
    CBS cbs;
    CBS_init(&cbs, reinterpret_cast<const uint8_t*>(test.der), test.der_len);
    if (!BN_parse_asn1_unsigned(&cbs, bn2.get()) || CBS_len(&cbs) != 0) {
      fprintf(stderr, "Parsing ASN.1 INTEGER failed.\n");
      return false;
    }
    if (BN_cmp(bn.get(), bn2.get()) != 0) {
      fprintf(stderr, "Bad parse.\n");
      return false;
    }

    // Test the value serializes correctly.
    CBB cbb;
    uint8_t *der;
    size_t der_len;
    CBB_zero(&cbb);
    if (!CBB_init(&cbb, 0) ||
        !BN_marshal_asn1(&cbb, bn.get()) ||
        !CBB_finish(&cbb, &der, &der_len)) {
      CBB_cleanup(&cbb);
      return false;
    }
    bssl::UniquePtr<uint8_t> delete_der(der);
    if (der_len != test.der_len ||
        memcmp(der, reinterpret_cast<const uint8_t*>(test.der), der_len) != 0) {
      fprintf(stderr, "Bad serialization.\n");
      return false;
    }

    // |BN_parse_asn1_unsigned_buggy| parses all valid input.
    CBS_init(&cbs, reinterpret_cast<const uint8_t*>(test.der), test.der_len);
    if (!BN_parse_asn1_unsigned_buggy(&cbs, bn2.get()) || CBS_len(&cbs) != 0) {
      fprintf(stderr, "Parsing ASN.1 INTEGER failed.\n");
      return false;
    }
    if (BN_cmp(bn.get(), bn2.get()) != 0) {
      fprintf(stderr, "Bad parse.\n");
      return false;
    }
  }

  for (const ASN1InvalidTest &test : kASN1InvalidTests) {
    bssl::UniquePtr<BIGNUM> bn(BN_new());
    if (!bn) {
      return false;
    }
    CBS cbs;
    CBS_init(&cbs, reinterpret_cast<const uint8_t*>(test.der), test.der_len);
    if (BN_parse_asn1_unsigned(&cbs, bn.get())) {
      fprintf(stderr, "Parsed invalid input.\n");
      return false;
    }
    ERR_clear_error();

    // All tests in kASN1InvalidTests are also rejected by
    // |BN_parse_asn1_unsigned_buggy|.
    CBS_init(&cbs, reinterpret_cast<const uint8_t*>(test.der), test.der_len);
    if (BN_parse_asn1_unsigned_buggy(&cbs, bn.get())) {
      fprintf(stderr, "Parsed invalid input.\n");
      return false;
    }
    ERR_clear_error();
  }

  for (const ASN1Test &test : kASN1BuggyTests) {
    // These broken encodings are rejected by |BN_parse_asn1_unsigned|.
    bssl::UniquePtr<BIGNUM> bn(BN_new());
    if (!bn) {
      return false;
    }

    CBS cbs;
    CBS_init(&cbs, reinterpret_cast<const uint8_t*>(test.der), test.der_len);
    if (BN_parse_asn1_unsigned(&cbs, bn.get())) {
      fprintf(stderr, "Parsed invalid input.\n");
      return false;
    }
    ERR_clear_error();

    // However |BN_parse_asn1_unsigned_buggy| accepts them.
    bssl::UniquePtr<BIGNUM> bn2 = ASCIIToBIGNUM(test.value_ascii);
    if (!bn2) {
      return false;
    }

    CBS_init(&cbs, reinterpret_cast<const uint8_t*>(test.der), test.der_len);
    if (!BN_parse_asn1_unsigned_buggy(&cbs, bn.get()) || CBS_len(&cbs) != 0) {
      fprintf(stderr, "Parsing (invalid) ASN.1 INTEGER failed.\n");
      return false;
    }

    if (BN_cmp(bn.get(), bn2.get()) != 0) {
      fprintf(stderr, "\"Bad\" parse.\n");
      return false;
    }
  }

  // Serializing negative numbers is not supported.
  bssl::UniquePtr<BIGNUM> bn = ASCIIToBIGNUM("-1");
  if (!bn) {
    return false;
  }
  CBB cbb;
  CBB_zero(&cbb);
  if (!CBB_init(&cbb, 0) ||
      BN_marshal_asn1(&cbb, bn.get())) {
    fprintf(stderr, "Serialized negative number.\n");
    CBB_cleanup(&cbb);
    return false;
  }
  ERR_clear_error();
  CBB_cleanup(&cbb);

  return true;
}

static bool TestNegativeZero(BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a(BN_new());
  bssl::UniquePtr<BIGNUM> b(BN_new());
  bssl::UniquePtr<BIGNUM> c(BN_new());
  if (!a || !b || !c) {
    return false;
  }

  // Test that BN_mul never gives negative zero.
  if (!BN_set_word(a.get(), 1)) {
    return false;
  }
  BN_set_negative(a.get(), 1);
  BN_zero(b.get());
  if (!BN_mul(c.get(), a.get(), b.get(), ctx)) {
    return false;
  }
  if (!BN_is_zero(c.get()) || BN_is_negative(c.get())) {
    fprintf(stderr, "Multiplication test failed.\n");
    return false;
  }

  for (int consttime = 0; consttime < 2; consttime++) {
    bssl::UniquePtr<BIGNUM> numerator(BN_new()), denominator(BN_new());
    if (!numerator || !denominator) {
      return false;
    }

    if (consttime) {
      BN_set_flags(numerator.get(), BN_FLG_CONSTTIME);
      BN_set_flags(denominator.get(), BN_FLG_CONSTTIME);
    }

    // Test that BN_div never gives negative zero in the quotient.
    if (!BN_set_word(numerator.get(), 1) ||
        !BN_set_word(denominator.get(), 2)) {
      return false;
    }
    BN_set_negative(numerator.get(), 1);
    if (!BN_div(a.get(), b.get(), numerator.get(), denominator.get(), ctx)) {
      return false;
    }
    if (!BN_is_zero(a.get()) || BN_is_negative(a.get())) {
      fprintf(stderr, "Incorrect quotient (consttime = %d).\n", consttime);
      return false;
    }

    // Test that BN_div never gives negative zero in the remainder.
    if (!BN_set_word(denominator.get(), 1)) {
      return false;
    }
    if (!BN_div(a.get(), b.get(), numerator.get(), denominator.get(), ctx)) {
      return false;
    }
    if (!BN_is_zero(b.get()) || BN_is_negative(b.get())) {
      fprintf(stderr, "Incorrect remainder (consttime = %d).\n", consttime);
      return false;
    }
  }

  // Test that BN_set_negative will not produce a negative zero.
  BN_zero(a.get());
  BN_set_negative(a.get(), 1);
  if (BN_is_negative(a.get())) {
    fprintf(stderr, "BN_set_negative produced a negative zero.\n");
    return false;
  }

  // Test that forcibly creating a negative zero does not break |BN_bn2hex| or
  // |BN_bn2dec|.
  a->neg = 1;
  bssl::UniquePtr<char> dec(BN_bn2dec(a.get()));
  bssl::UniquePtr<char> hex(BN_bn2hex(a.get()));
  if (!dec || !hex ||
      strcmp(dec.get(), "-0") != 0 ||
      strcmp(hex.get(), "-0") != 0) {
    fprintf(stderr, "BN_bn2dec or BN_bn2hex failed with negative zero.\n");
    return false;
  }

  return true;
}

static bool TestBadModulus(BN_CTX *ctx) {
  bssl::UniquePtr<BIGNUM> a(BN_new());
  bssl::UniquePtr<BIGNUM> b(BN_new());
  bssl::UniquePtr<BIGNUM> zero(BN_new());
  bssl::UniquePtr<BN_MONT_CTX> mont(BN_MONT_CTX_new());
  if (!a || !b || !zero || !mont) {
    return false;
  }

  BN_zero(zero.get());

  if (BN_div(a.get(), b.get(), BN_value_one(), zero.get(), ctx)) {
    fprintf(stderr, "Division by zero unexpectedly succeeded.\n");
    return false;
  }
  ERR_clear_error();

  if (BN_mod_mul(a.get(), BN_value_one(), BN_value_one(), zero.get(), ctx)) {
    fprintf(stderr, "BN_mod_mul with zero modulus unexpectedly succeeded.\n");
    return false;
  }
  ERR_clear_error();

  if (BN_mod_exp(a.get(), BN_value_one(), BN_value_one(), zero.get(), ctx)) {
    fprintf(stderr, "BN_mod_exp with zero modulus unexpectedly succeeded.\n");
    return 0;
  }
  ERR_clear_error();

  if (BN_mod_exp_mont(a.get(), BN_value_one(), BN_value_one(), zero.get(), ctx,
                      NULL)) {
    fprintf(stderr,
            "BN_mod_exp_mont with zero modulus unexpectedly succeeded.\n");
    return 0;
  }
  ERR_clear_error();

  if (BN_mod_exp_mont_consttime(a.get(), BN_value_one(), BN_value_one(),
                                zero.get(), ctx, nullptr)) {
    fprintf(stderr,
            "BN_mod_exp_mont_consttime with zero modulus unexpectedly "
            "succeeded.\n");
    return 0;
  }
  ERR_clear_error();

  if (BN_MONT_CTX_set(mont.get(), zero.get(), ctx)) {
    fprintf(stderr,
            "BN_MONT_CTX_set unexpectedly succeeded for zero modulus.\n");
    return false;
  }
  ERR_clear_error();

  // Some operations also may not be used with an even modulus.

  if (!BN_set_word(b.get(), 16)) {
    return false;
  }

  if (BN_MONT_CTX_set(mont.get(), b.get(), ctx)) {
    fprintf(stderr,
            "BN_MONT_CTX_set unexpectedly succeeded for even modulus.\n");
    return false;
  }
  ERR_clear_error();

  if (BN_mod_exp_mont(a.get(), BN_value_one(), BN_value_one(), b.get(), ctx,
                      NULL)) {
    fprintf(stderr,
            "BN_mod_exp_mont with even modulus unexpectedly succeeded.\n");
    return 0;
  }
  ERR_clear_error();

  if (BN_mod_exp_mont_consttime(a.get(), BN_value_one(), BN_value_one(),
                                b.get(), ctx, nullptr)) {
    fprintf(stderr,
            "BN_mod_exp_mont_consttime with even modulus unexpectedly "
            "succeeded.\n");
    return 0;
  }
  ERR_clear_error();

  return true;
}

// TestExpModZero tests that 1**0 mod 1 == 0.
static bool TestExpModZero() {
  bssl::UniquePtr<BIGNUM> zero(BN_new()), a(BN_new()), r(BN_new());
  if (!zero || !a || !r ||
      !BN_rand(a.get(), 1024, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY)) {
    return false;
  }
  BN_zero(zero.get());

  if (!BN_mod_exp(r.get(), a.get(), zero.get(), BN_value_one(), nullptr) ||
      !BN_is_zero(r.get()) ||
      !BN_mod_exp_mont(r.get(), a.get(), zero.get(), BN_value_one(), nullptr,
                       nullptr) ||
      !BN_is_zero(r.get()) ||
      !BN_mod_exp_mont_consttime(r.get(), a.get(), zero.get(), BN_value_one(),
                                 nullptr, nullptr) ||
      !BN_is_zero(r.get()) ||
      !BN_mod_exp_mont_word(r.get(), 42, zero.get(), BN_value_one(), nullptr,
                            nullptr) ||
      !BN_is_zero(r.get())) {
    return false;
  }

  return true;
}

static bool TestSmallPrime(BN_CTX *ctx) {
  static const unsigned kBits = 10;

  bssl::UniquePtr<BIGNUM> r(BN_new());
  if (!r || !BN_generate_prime_ex(r.get(), static_cast<int>(kBits), 0, NULL,
                                  NULL, NULL)) {
    return false;
  }
  if (BN_num_bits(r.get()) != kBits) {
    fprintf(stderr, "Expected %u bit prime, got %u bit number\n", kBits,
            BN_num_bits(r.get()));
    return false;
  }

  return true;
}

static bool TestCmpWord() {
  static const BN_ULONG kMaxWord = (BN_ULONG)-1;

  bssl::UniquePtr<BIGNUM> r(BN_new());
  if (!r ||
      !BN_set_word(r.get(), 0)) {
    return false;
  }

  if (BN_cmp_word(r.get(), 0) != 0 ||
      BN_cmp_word(r.get(), 1) >= 0 ||
      BN_cmp_word(r.get(), kMaxWord) >= 0) {
    fprintf(stderr, "BN_cmp_word compared against 0 incorrectly.\n");
    return false;
  }

  if (!BN_set_word(r.get(), 100)) {
    return false;
  }

  if (BN_cmp_word(r.get(), 0) <= 0 ||
      BN_cmp_word(r.get(), 99) <= 0 ||
      BN_cmp_word(r.get(), 100) != 0 ||
      BN_cmp_word(r.get(), 101) >= 0 ||
      BN_cmp_word(r.get(), kMaxWord) >= 0) {
    fprintf(stderr, "BN_cmp_word compared against 100 incorrectly.\n");
    return false;
  }

  BN_set_negative(r.get(), 1);

  if (BN_cmp_word(r.get(), 0) >= 0 ||
      BN_cmp_word(r.get(), 100) >= 0 ||
      BN_cmp_word(r.get(), kMaxWord) >= 0) {
    fprintf(stderr, "BN_cmp_word compared against -100 incorrectly.\n");
    return false;
  }

  if (!BN_set_word(r.get(), kMaxWord)) {
    return false;
  }

  if (BN_cmp_word(r.get(), 0) <= 0 ||
      BN_cmp_word(r.get(), kMaxWord - 1) <= 0 ||
      BN_cmp_word(r.get(), kMaxWord) != 0) {
    fprintf(stderr, "BN_cmp_word compared against kMaxWord incorrectly.\n");
    return false;
  }

  if (!BN_add(r.get(), r.get(), BN_value_one())) {
    return false;
  }

  if (BN_cmp_word(r.get(), 0) <= 0 ||
      BN_cmp_word(r.get(), kMaxWord) <= 0) {
    fprintf(stderr, "BN_cmp_word compared against kMaxWord + 1 incorrectly.\n");
    return false;
  }

  BN_set_negative(r.get(), 1);

  if (BN_cmp_word(r.get(), 0) >= 0 ||
      BN_cmp_word(r.get(), kMaxWord) >= 0) {
    fprintf(stderr,
            "BN_cmp_word compared against -kMaxWord - 1 incorrectly.\n");
    return false;
  }

  return true;
}

static bool TestBN2Dec() {
  static const char *kBN2DecTests[] = {
      "0",
      "1",
      "-1",
      "100",
      "-100",
      "123456789012345678901234567890",
      "-123456789012345678901234567890",
      "123456789012345678901234567890123456789012345678901234567890",
      "-123456789012345678901234567890123456789012345678901234567890",
  };

  for (const char *test : kBN2DecTests) {
    bssl::UniquePtr<BIGNUM> bn;
    int ret = DecimalToBIGNUM(&bn, test);
    if (ret == 0) {
      return false;
    }

    bssl::UniquePtr<char> dec(BN_bn2dec(bn.get()));
    if (!dec) {
      fprintf(stderr, "BN_bn2dec failed on %s.\n", test);
      return false;
    }

    if (strcmp(dec.get(), test) != 0) {
      fprintf(stderr, "BN_bn2dec gave %s, wanted %s.\n", dec.get(), test);
      return false;
    }
  }

  return true;
}

static bool TestBNSetU64() {
  static const struct {
    const char *hex;
    uint64_t value;
  } kU64Tests[] = {
      {"0", UINT64_C(0x0)},
      {"1", UINT64_C(0x1)},
      {"ffffffff", UINT64_C(0xffffffff)},
      {"100000000", UINT64_C(0x100000000)},
      {"ffffffffffffffff", UINT64_C(0xffffffffffffffff)},
  };

  for (const auto& test : kU64Tests) {
    bssl::UniquePtr<BIGNUM> bn(BN_new()), expected;
    if (!bn ||
        !BN_set_u64(bn.get(), test.value) ||
        !HexToBIGNUM(&expected, test.hex) ||
        BN_cmp(bn.get(), expected.get()) != 0) {
      fprintf(stderr, "BN_set_u64 test failed for 0x%s.\n", test.hex);
      ERR_print_errors_fp(stderr);
      return false;
    }
  }

  return true;
}

int main(int argc, char *argv[]) {
  CRYPTO_library_init();

  if (argc != 2) {
    fprintf(stderr, "%s TEST_FILE\n", argv[0]);
    return 1;
  }

  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (!ctx) {
    return 1;
  }

  if (!TestBN2BinPadded(ctx.get()) ||
      !TestDec2BN(ctx.get()) ||
      !TestHex2BN(ctx.get()) ||
      !TestASC2BN(ctx.get()) ||
      !TestMPI() ||
      !TestRand() ||
      !TestASN1() ||
      !TestNegativeZero(ctx.get()) ||
      !TestBadModulus(ctx.get()) ||
      !TestExpModZero() ||
      !TestSmallPrime(ctx.get()) ||
      !TestCmpWord() ||
      !TestBN2Dec() ||
      !TestBNSetU64()) {
    return 1;
  }

  return FileTestMain(RunTest, ctx.get(), argv[1]);
}
