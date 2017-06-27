/* Copyright (c) 2014, Google Inc.
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

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/base64.h>
#include <openssl/bio.h>
#include <openssl/cipher.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include "internal.h"
#include "../crypto/internal.h"
#include "../crypto/test/test_util.h"

#if defined(OPENSSL_WINDOWS)
/* Windows defines struct timeval in winsock2.h. */
OPENSSL_MSVC_PRAGMA(warning(push, 3))
#include <winsock2.h>
OPENSSL_MSVC_PRAGMA(warning(pop))
#else
#include <sys/time.h>
#endif


struct ExpectedCipher {
  unsigned long id;
  int in_group_flag;
};

struct CipherTest {
  // The rule string to apply.
  const char *rule;
  // The list of expected ciphers, in order.
  std::vector<ExpectedCipher> expected;
  // True if this cipher list should fail in strict mode.
  bool strict_fail;
};

struct CurveTest {
  // The rule string to apply.
  const char *rule;
  // The list of expected curves, in order.
  std::vector<uint16_t> expected;
};

static const CipherTest kCipherTests[] = {
    // Selecting individual ciphers should work.
    {
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
        false,
    },
    // + reorders selected ciphers to the end, keeping their relative order.
    {
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "+aRSA",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
        false,
    },
    // ! banishes ciphers from future selections.
    {
        "!aRSA:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
        },
        false,
    },
    // Multiple masks can be ANDed in a single rule.
    {
        "kRSA+AESGCM+AES128",
        {
            {TLS1_CK_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
        false,
    },
    // - removes selected ciphers, but preserves their order for future
    // selections. Select AES_128_GCM, but order the key exchanges RSA,
    // ECDHE_RSA.
    {
        "ALL:-kECDHE:"
        "-kRSA:-ALL:"
        "AESGCM+AES128+aRSA",
        {
            {TLS1_CK_RSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
        false,
    },
    // Unknown selectors are no-ops, except in strict mode.
    {
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "BOGUS1",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
        true,
    },
    // Unknown selectors are no-ops, except in strict mode.
    {
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "-BOGUS2:+BOGUS3:!BOGUS4",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
        true,
    },
    // Square brackets specify equi-preference groups.
    {
        "[ECDHE-ECDSA-CHACHA20-POLY1305|ECDHE-ECDSA-AES128-GCM-SHA256]:"
        "[ECDHE-RSA-CHACHA20-POLY1305]:"
        "ECDHE-RSA-AES128-GCM-SHA256",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 1},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
        false,
    },
    // @STRENGTH performs a stable strength-sort of the selected ciphers and
    // only the selected ciphers.
    {
        // To simplify things, banish all but {ECDHE_RSA,RSA} x
        // {CHACHA20,AES_256_CBC,AES_128_CBC} x SHA1.
        "!AESGCM:!3DES:!SHA256:!SHA384:"
        // Order some ciphers backwards by strength.
        "ALL:-CHACHA20:-AES256:-AES128:-ALL:"
        // Select ECDHE ones and sort them by strength. Ties should resolve
        // based on the order above.
        "kECDHE:@STRENGTH:-ALL:"
        // Now bring back everything uses RSA. ECDHE_RSA should be first, sorted
        // by strength. Then RSA, backwards by strength.
        "aRSA",
        {
            {TLS1_CK_ECDHE_RSA_WITH_AES_256_CBC_SHA, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA, 0},
            {TLS1_CK_RSA_WITH_AES_128_SHA, 0},
            {TLS1_CK_RSA_WITH_AES_256_SHA, 0},
        },
        false,
    },
    // Exact ciphers may not be used in multi-part rules; they are treated
    // as unknown aliases.
    {
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "!ECDHE-RSA-AES128-GCM-SHA256+RSA:"
        "!ECDSA+ECDHE-ECDSA-AES128-GCM-SHA256",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
        true,
    },
    // SSLv3 matches everything that existed before TLS 1.2.
    {
        "AES128-SHA:AES128-SHA256:!SSLv3",
        {
            {TLS1_CK_RSA_WITH_AES_128_SHA256, 0},
        },
        false,
    },
    // TLSv1.2 matches everything added in TLS 1.2.
    {
        "AES128-SHA:AES128-SHA256:!TLSv1.2",
        {
            {TLS1_CK_RSA_WITH_AES_128_SHA, 0},
        },
        false,
    },
    // The two directives have no intersection.  But each component is valid, so
    // even in strict mode it is accepted.
    {
        "AES128-SHA:AES128-SHA256:!TLSv1.2+SSLv3",
        {
            {TLS1_CK_RSA_WITH_AES_128_SHA, 0},
            {TLS1_CK_RSA_WITH_AES_128_SHA256, 0},
        },
        false,
    },
};

static const char *kBadRules[] = {
  // Invalid brackets.
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256",
  "RSA]",
  "[[RSA]]",
  // Operators inside brackets.
  "[+RSA]",
  // Unknown directive.
  "@BOGUS",
  // Empty cipher lists error at SSL_CTX_set_cipher_list.
  "",
  "BOGUS",
  // COMPLEMENTOFDEFAULT is empty.
  "COMPLEMENTOFDEFAULT",
  // Invalid command.
  "?BAR",
  // Special operators are not allowed if groups are used.
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256]:+FOO",
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256]:!FOO",
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256]:-FOO",
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256]:@STRENGTH",
  // Opcode supplied, but missing selector.
  "+",
};

static const char *kMustNotIncludeNull[] = {
  "ALL",
  "DEFAULT",
  "HIGH",
  "FIPS",
  "SHA",
  "SHA1",
  "RSA",
  "SSLv3",
  "TLSv1",
  "TLSv1.2",
};

static const CurveTest kCurveTests[] = {
  {
    "P-256",
    { SSL_CURVE_SECP256R1 },
  },
  {
    "P-256:P-384:P-521:X25519",
    {
      SSL_CURVE_SECP256R1,
      SSL_CURVE_SECP384R1,
      SSL_CURVE_SECP521R1,
      SSL_CURVE_X25519,
    },
  },
};

static const char *kBadCurvesLists[] = {
  "",
  ":",
  "::",
  "P-256::X25519",
  "RSA:P-256",
  "P-256:RSA",
  "X25519:P-256:",
  ":X25519:P-256",
};

static std::string CipherListToString(ssl_cipher_preference_list_st *list) {
  bool in_group = false;
  std::string ret;
  for (size_t i = 0; i < sk_SSL_CIPHER_num(list->ciphers); i++) {
    const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(list->ciphers, i);
    if (!in_group && list->in_group_flags[i]) {
      ret += "\t[\n";
      in_group = true;
    }
    ret += "\t";
    if (in_group) {
      ret += "  ";
    }
    ret += SSL_CIPHER_get_name(cipher);
    ret += "\n";
    if (in_group && !list->in_group_flags[i]) {
      ret += "\t]\n";
      in_group = false;
    }
  }
  return ret;
}

static bool CipherListsEqual(ssl_cipher_preference_list_st *list,
                             const std::vector<ExpectedCipher> &expected) {
  if (sk_SSL_CIPHER_num(list->ciphers) != expected.size()) {
    return false;
  }

  for (size_t i = 0; i < expected.size(); i++) {
    const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(list->ciphers, i);
    if (expected[i].id != SSL_CIPHER_get_id(cipher) ||
        expected[i].in_group_flag != list->in_group_flags[i]) {
      return false;
    }
  }

  return true;
}

TEST(SSLTest, CipherRules) {
  for (const CipherTest &t : kCipherTests) {
    SCOPED_TRACE(t.rule);
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
    ASSERT_TRUE(ctx);

    // Test lax mode.
    ASSERT_TRUE(SSL_CTX_set_cipher_list(ctx.get(), t.rule));
    EXPECT_TRUE(CipherListsEqual(ctx->cipher_list, t.expected))
        << "Cipher rule evaluated to:\n"
        << CipherListToString(ctx->cipher_list);

    // Test strict mode.
    if (t.strict_fail) {
      EXPECT_FALSE(SSL_CTX_set_strict_cipher_list(ctx.get(), t.rule));
    } else {
      ASSERT_TRUE(SSL_CTX_set_strict_cipher_list(ctx.get(), t.rule));
      EXPECT_TRUE(CipherListsEqual(ctx->cipher_list, t.expected))
          << "Cipher rule evaluated to:\n"
          << CipherListToString(ctx->cipher_list);
    }
  }

  for (const char *rule : kBadRules) {
    SCOPED_TRACE(rule);
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
    ASSERT_TRUE(ctx);

    EXPECT_FALSE(SSL_CTX_set_cipher_list(ctx.get(), rule));
    ERR_clear_error();
  }

  for (const char *rule : kMustNotIncludeNull) {
    SCOPED_TRACE(rule);
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
    ASSERT_TRUE(ctx);

    ASSERT_TRUE(SSL_CTX_set_strict_cipher_list(ctx.get(), rule));
    for (size_t i = 0; i < sk_SSL_CIPHER_num(ctx->cipher_list->ciphers); i++) {
      EXPECT_FALSE(SSL_CIPHER_is_NULL(
          sk_SSL_CIPHER_value(ctx->cipher_list->ciphers, i)));
    }
  }
}

TEST(SSLTest, CurveRules) {
  for (const CurveTest &t : kCurveTests) {
    SCOPED_TRACE(t.rule);
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
    ASSERT_TRUE(ctx);

    ASSERT_TRUE(SSL_CTX_set1_curves_list(ctx.get(), t.rule));
    ASSERT_EQ(t.expected.size(), ctx->supported_group_list_len);
    for (size_t i = 0; i < t.expected.size(); i++) {
      EXPECT_EQ(t.expected[i], ctx->supported_group_list[i]);
    }
  }

  for (const char *rule : kBadCurvesLists) {
    SCOPED_TRACE(rule);
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
    ASSERT_TRUE(ctx);

    EXPECT_FALSE(SSL_CTX_set1_curves_list(ctx.get(), rule));
    ERR_clear_error();
  }
}

// kOpenSSLSession is a serialized SSL_SESSION.
static const char kOpenSSLSession[] =
    "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"
    "i4gv7Y5oliyntgMBAQA=";

// kCustomSession is a custom serialized SSL_SESSION generated by
// filling in missing fields from |kOpenSSLSession|. This includes
// providing |peer_sha256|, so |peer| is not serialized.
static const char kCustomSession[] =
    "MIIBdgIBAQICAwMEAsAvBCAG5Q1ndq4Yfmbeo1zwLkNRKmCXGdNgWvGT3cskV0yQ"
    "kAQwJlrlzkAWBOWiLj/jJ76D7l+UXoizP2KI2C7I2FccqMmIfFmmkUy32nIJ0mZH"
    "IWoJoQYCBFRDO46iBAICASykAwQBAqUDAgEUphAEDnd3dy5nb29nbGUuY29tqAcE"
    "BXdvcmxkqQUCAwGJwKqBpwSBpBwUQvoeOk0Kg36SYTcLEkXqKwOBfF9vE4KX0Nxe"
    "LwjcDTpsuh3qXEaZ992r1N38VDcyS6P7I6HBYN9BsNHM362zZnY27GpTw+Kwd751"
    "CLoXFPoaMOe57dbBpXoro6Pd3BTbf/Tzr88K06yEOTDKPNj3+inbMaVigtK4PLyP"
    "q+Topyzvx9USFgRvyuoxn0Hgb+R0A3j6SLRuyOdAi4gv7Y5oliynrSIEIAYGBgYG"
    "BgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGrgMEAQevAwQBBLADBAEF";

// kBoringSSLSession is a serialized SSL_SESSION generated from bssl client.
static const char kBoringSSLSession[] =
    "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"
    "lQI5kQsO/jgu0R0FmvZNPm8PBx2vLB6PYDni+jZTEznUXiYr2z2oFL0y6xgDKFIE"
    "ceWrMz3hOLsHNoRinHnqFjD0X8Ar6HFr5PkCAwEAAaOB8DCB7TAfBgNVHSMEGDAW"
    "gBRI5mj5K9KylddH2CMgEE8zmJCf1DAdBgNVHQ4EFgQUwHqYaI2J+6sFZAwRfap9"
    "ZbjKzE4wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwOgYDVR0fBDMw"
    "MTAvoC2gK4YpaHR0cDovL2NybC5nZW90cnVzdC5jb20vY3Jscy9zZWN1cmVjYS5j"
    "cmwwTgYDVR0gBEcwRTBDBgRVHSAAMDswOQYIKwYBBQUHAgEWLWh0dHBzOi8vd3d3"
    "Lmdlb3RydXN0LmNvbS9yZXNvdXJjZXMvcmVwb3NpdG9yeTANBgkqhkiG9w0BAQUF"
    "AAOBgQB24RJuTksWEoYwBrKBCM/wCMfHcX5m7sLt1Dsf//DwyE7WQziwuTB9GNBV"
    "g6JqyzYRnOhIZqNtf7gT1Ef+i1pcc/yu2RsyGTirlzQUqpbS66McFAhJtrvlke+D"
    "NusdVm/K2rxzY5Dkf3s+Iss9B+1fOHSc4wNQTqGvmO5h8oQ/Eg==";

// kBadSessionExtraField is a custom serialized SSL_SESSION generated by replacing
// the final (optional) element of |kCustomSession| with tag number 30.
static const char kBadSessionExtraField[] =
    "MIIBdgIBAQICAwMEAsAvBCAG5Q1ndq4Yfmbeo1zwLkNRKmCXGdNgWvGT3cskV0yQ"
    "kAQwJlrlzkAWBOWiLj/jJ76D7l+UXoizP2KI2C7I2FccqMmIfFmmkUy32nIJ0mZH"
    "IWoJoQYCBFRDO46iBAICASykAwQBAqUDAgEUphAEDnd3dy5nb29nbGUuY29tqAcE"
    "BXdvcmxkqQUCAwGJwKqBpwSBpBwUQvoeOk0Kg36SYTcLEkXqKwOBfF9vE4KX0Nxe"
    "LwjcDTpsuh3qXEaZ992r1N38VDcyS6P7I6HBYN9BsNHM362zZnY27GpTw+Kwd751"
    "CLoXFPoaMOe57dbBpXoro6Pd3BTbf/Tzr88K06yEOTDKPNj3+inbMaVigtK4PLyP"
    "q+Topyzvx9USFgRvyuoxn0Hgb+R0A3j6SLRuyOdAi4gv7Y5oliynrSIEIAYGBgYG"
    "BgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGrgMEAQevAwQBBL4DBAEF";

// kBadSessionVersion is a custom serialized SSL_SESSION generated by replacing
// the version of |kCustomSession| with 2.
static const char kBadSessionVersion[] =
    "MIIBdgIBAgICAwMEAsAvBCAG5Q1ndq4Yfmbeo1zwLkNRKmCXGdNgWvGT3cskV0yQ"
    "kAQwJlrlzkAWBOWiLj/jJ76D7l+UXoizP2KI2C7I2FccqMmIfFmmkUy32nIJ0mZH"
    "IWoJoQYCBFRDO46iBAICASykAwQBAqUDAgEUphAEDnd3dy5nb29nbGUuY29tqAcE"
    "BXdvcmxkqQUCAwGJwKqBpwSBpBwUQvoeOk0Kg36SYTcLEkXqKwOBfF9vE4KX0Nxe"
    "LwjcDTpsuh3qXEaZ992r1N38VDcyS6P7I6HBYN9BsNHM362zZnY27GpTw+Kwd751"
    "CLoXFPoaMOe57dbBpXoro6Pd3BTbf/Tzr88K06yEOTDKPNj3+inbMaVigtK4PLyP"
    "q+Topyzvx9USFgRvyuoxn0Hgb+R0A3j6SLRuyOdAi4gv7Y5oliynrSIEIAYGBgYG"
    "BgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGrgMEAQevAwQBBLADBAEF";

// kBadSessionTrailingData is a custom serialized SSL_SESSION with trailing data
// appended.
static const char kBadSessionTrailingData[] =
    "MIIBdgIBAQICAwMEAsAvBCAG5Q1ndq4Yfmbeo1zwLkNRKmCXGdNgWvGT3cskV0yQ"
    "kAQwJlrlzkAWBOWiLj/jJ76D7l+UXoizP2KI2C7I2FccqMmIfFmmkUy32nIJ0mZH"
    "IWoJoQYCBFRDO46iBAICASykAwQBAqUDAgEUphAEDnd3dy5nb29nbGUuY29tqAcE"
    "BXdvcmxkqQUCAwGJwKqBpwSBpBwUQvoeOk0Kg36SYTcLEkXqKwOBfF9vE4KX0Nxe"
    "LwjcDTpsuh3qXEaZ992r1N38VDcyS6P7I6HBYN9BsNHM362zZnY27GpTw+Kwd751"
    "CLoXFPoaMOe57dbBpXoro6Pd3BTbf/Tzr88K06yEOTDKPNj3+inbMaVigtK4PLyP"
    "q+Topyzvx9USFgRvyuoxn0Hgb+R0A3j6SLRuyOdAi4gv7Y5oliynrSIEIAYGBgYG"
    "BgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGrgMEAQevAwQBBLADBAEFAAAA";

static bool DecodeBase64(std::vector<uint8_t> *out, const char *in) {
  size_t len;
  if (!EVP_DecodedLength(&len, strlen(in))) {
    fprintf(stderr, "EVP_DecodedLength failed\n");
    return false;
  }

  out->resize(len);
  if (!EVP_DecodeBase64(out->data(), &len, len, (const uint8_t *)in,
                        strlen(in))) {
    fprintf(stderr, "EVP_DecodeBase64 failed\n");
    return false;
  }
  out->resize(len);
  return true;
}

static bool TestSSL_SESSIONEncoding(const char *input_b64) {
  const uint8_t *cptr;
  uint8_t *ptr;

  // Decode the input.
  std::vector<uint8_t> input;
  if (!DecodeBase64(&input, input_b64)) {
    return false;
  }

  // Verify the SSL_SESSION decodes.
  bssl::UniquePtr<SSL_CTX> ssl_ctx(SSL_CTX_new(TLS_method()));
  if (!ssl_ctx) {
    return false;
  }
  bssl::UniquePtr<SSL_SESSION> session(
      SSL_SESSION_from_bytes(input.data(), input.size(), ssl_ctx.get()));
  if (!session) {
    fprintf(stderr, "SSL_SESSION_from_bytes failed\n");
    return false;
  }

  // Verify the SSL_SESSION encoding round-trips.
  size_t encoded_len;
  bssl::UniquePtr<uint8_t> encoded;
  uint8_t *encoded_raw;
  if (!SSL_SESSION_to_bytes(session.get(), &encoded_raw, &encoded_len)) {
    fprintf(stderr, "SSL_SESSION_to_bytes failed\n");
    return false;
  }
  encoded.reset(encoded_raw);
  if (encoded_len != input.size() ||
      OPENSSL_memcmp(input.data(), encoded.get(), input.size()) != 0) {
    fprintf(stderr, "SSL_SESSION_to_bytes did not round-trip\n");
    hexdump(stderr, "Before: ", input.data(), input.size());
    hexdump(stderr, "After:  ", encoded_raw, encoded_len);
    return false;
  }

  // Verify the SSL_SESSION also decodes with the legacy API.
  cptr = input.data();
  session.reset(d2i_SSL_SESSION(NULL, &cptr, input.size()));
  if (!session || cptr != input.data() + input.size()) {
    fprintf(stderr, "d2i_SSL_SESSION failed\n");
    return false;
  }

  // Verify the SSL_SESSION encoding round-trips via the legacy API.
  int len = i2d_SSL_SESSION(session.get(), NULL);
  if (len < 0 || (size_t)len != input.size()) {
    fprintf(stderr, "i2d_SSL_SESSION(NULL) returned invalid length\n");
    return false;
  }

  encoded.reset((uint8_t *)OPENSSL_malloc(input.size()));
  if (!encoded) {
    fprintf(stderr, "malloc failed\n");
    return false;
  }

  ptr = encoded.get();
  len = i2d_SSL_SESSION(session.get(), &ptr);
  if (len < 0 || (size_t)len != input.size()) {
    fprintf(stderr, "i2d_SSL_SESSION returned invalid length\n");
    return false;
  }
  if (ptr != encoded.get() + input.size()) {
    fprintf(stderr, "i2d_SSL_SESSION did not advance ptr correctly\n");
    return false;
  }
  if (OPENSSL_memcmp(input.data(), encoded.get(), input.size()) != 0) {
    fprintf(stderr, "i2d_SSL_SESSION did not round-trip\n");
    return false;
  }

  return true;
}

static bool TestBadSSL_SESSIONEncoding(const char *input_b64) {
  std::vector<uint8_t> input;
  if (!DecodeBase64(&input, input_b64)) {
    return false;
  }

  // Verify that the SSL_SESSION fails to decode.
  bssl::UniquePtr<SSL_CTX> ssl_ctx(SSL_CTX_new(TLS_method()));
  if (!ssl_ctx) {
    return false;
  }
  bssl::UniquePtr<SSL_SESSION> session(
      SSL_SESSION_from_bytes(input.data(), input.size(), ssl_ctx.get()));
  if (session) {
    fprintf(stderr, "SSL_SESSION_from_bytes unexpectedly succeeded\n");
    return false;
  }
  ERR_clear_error();
  return true;
}

static void ExpectDefaultVersion(uint16_t min_version, uint16_t max_version,
                                 const SSL_METHOD *(*method)(void)) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(method()));
  ASSERT_TRUE(ctx);
  EXPECT_EQ(min_version, ctx->min_version);
  EXPECT_EQ(max_version, ctx->max_version);
}

TEST(SSLTest, DefaultVersion) {
  // TODO(svaldez): Update this when TLS 1.3 is enabled by default.
  ExpectDefaultVersion(TLS1_VERSION, TLS1_2_VERSION, &TLS_method);
  ExpectDefaultVersion(TLS1_VERSION, TLS1_VERSION, &TLSv1_method);
  ExpectDefaultVersion(TLS1_1_VERSION, TLS1_1_VERSION, &TLSv1_1_method);
  ExpectDefaultVersion(TLS1_2_VERSION, TLS1_2_VERSION, &TLSv1_2_method);
  ExpectDefaultVersion(TLS1_1_VERSION, TLS1_2_VERSION, &DTLS_method);
  ExpectDefaultVersion(TLS1_1_VERSION, TLS1_1_VERSION, &DTLSv1_method);
  ExpectDefaultVersion(TLS1_2_VERSION, TLS1_2_VERSION, &DTLSv1_2_method);
}

typedef struct {
  int id;
  const char *rfc_name;
} CIPHER_RFC_NAME_TEST;

static const CIPHER_RFC_NAME_TEST kCipherRFCNameTests[] = {
    {SSL3_CK_RSA_DES_192_CBC3_SHA, "TLS_RSA_WITH_3DES_EDE_CBC_SHA"},
    {TLS1_CK_RSA_WITH_AES_128_SHA, "TLS_RSA_WITH_AES_128_CBC_SHA"},
    {TLS1_CK_ECDHE_RSA_WITH_AES_128_SHA256,
     "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256"},
    {TLS1_CK_ECDHE_RSA_WITH_AES_256_SHA384,
     "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384"},
    {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
     "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256"},
    {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
     "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256"},
    {TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
     "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384"},
    {TLS1_CK_ECDHE_PSK_WITH_AES_128_CBC_SHA,
     "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA"},
    {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
     "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256"},
    {TLS1_CK_AES_256_GCM_SHA384, "TLS_AES_256_GCM_SHA384"},
    {TLS1_CK_AES_128_GCM_SHA256, "TLS_AES_128_GCM_SHA256"},
    {TLS1_CK_CHACHA20_POLY1305_SHA256, "TLS_CHACHA20_POLY1305_SHA256"},
};

TEST(SSLTest, CipherGetRFCName) {
  for (const CIPHER_RFC_NAME_TEST &t : kCipherRFCNameTests) {
    SCOPED_TRACE(t.rfc_name);

    const SSL_CIPHER *cipher = SSL_get_cipher_by_value(t.id & 0xffff);
    ASSERT_TRUE(cipher);
    bssl::UniquePtr<char> rfc_name(SSL_CIPHER_get_rfc_name(cipher));
    ASSERT_TRUE(rfc_name);

    EXPECT_STREQ(t.rfc_name, rfc_name.get());
  }
}

// CreateSessionWithTicket returns a sample |SSL_SESSION| with the specified
// version and ticket length or nullptr on failure.
static bssl::UniquePtr<SSL_SESSION> CreateSessionWithTicket(uint16_t version,
                                                            size_t ticket_len) {
  std::vector<uint8_t> der;
  if (!DecodeBase64(&der, kOpenSSLSession)) {
    return nullptr;
  }

  bssl::UniquePtr<SSL_CTX> ssl_ctx(SSL_CTX_new(TLS_method()));
  if (!ssl_ctx) {
    return nullptr;
  }
  bssl::UniquePtr<SSL_SESSION> session(
      SSL_SESSION_from_bytes(der.data(), der.size(), ssl_ctx.get()));
  if (!session) {
    return nullptr;
  }

  session->ssl_version = version;

  // Swap out the ticket for a garbage one.
  OPENSSL_free(session->tlsext_tick);
  session->tlsext_tick = reinterpret_cast<uint8_t*>(OPENSSL_malloc(ticket_len));
  if (session->tlsext_tick == nullptr) {
    return nullptr;
  }
  OPENSSL_memset(session->tlsext_tick, 'a', ticket_len);
  session->tlsext_ticklen = ticket_len;

  // Fix up the timeout.
#if defined(BORINGSSL_UNSAFE_DETERMINISTIC_MODE)
  session->time = 1234;
#else
  session->time = time(NULL);
#endif
  return session;
}

static bool GetClientHello(SSL *ssl, std::vector<uint8_t> *out) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
  if (!bio) {
    return false;
  }
  // Do not configure a reading BIO, but record what's written to a memory BIO.
  BIO_up_ref(bio.get());
  SSL_set_bio(ssl, nullptr /* rbio */, bio.get());
  int ret = SSL_connect(ssl);
  if (ret > 0) {
    // SSL_connect should fail without a BIO to write to.
    return false;
  }
  ERR_clear_error();

  const uint8_t *client_hello;
  size_t client_hello_len;
  if (!BIO_mem_contents(bio.get(), &client_hello, &client_hello_len)) {
    return false;
  }
  *out = std::vector<uint8_t>(client_hello, client_hello + client_hello_len);
  return true;
}

// GetClientHelloLen creates a client SSL connection with the specified version
// and ticket length. It returns the length of the ClientHello, not including
// the record header, on success and zero on error.
static size_t GetClientHelloLen(uint16_t max_version, uint16_t session_version,
                                size_t ticket_len) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  bssl::UniquePtr<SSL_SESSION> session =
      CreateSessionWithTicket(session_version, ticket_len);
  if (!ctx || !session) {
    return 0;
  }

  // Set a one-element cipher list so the baseline ClientHello is unpadded.
  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  if (!ssl || !SSL_set_session(ssl.get(), session.get()) ||
      !SSL_set_strict_cipher_list(ssl.get(), "ECDHE-RSA-AES128-GCM-SHA256") ||
      !SSL_set_max_proto_version(ssl.get(), max_version)) {
    return 0;
  }

  std::vector<uint8_t> client_hello;
  if (!GetClientHello(ssl.get(), &client_hello) ||
      client_hello.size() <= SSL3_RT_HEADER_LENGTH) {
    return 0;
  }

  return client_hello.size() - SSL3_RT_HEADER_LENGTH;
}

struct PaddingTest {
  size_t input_len, padded_len;
};

static const PaddingTest kPaddingTests[] = {
    // ClientHellos of length below 0x100 do not require padding.
    {0xfe, 0xfe},
    {0xff, 0xff},
    // ClientHellos of length 0x100 through 0x1fb are padded up to 0x200.
    {0x100, 0x200},
    {0x123, 0x200},
    {0x1fb, 0x200},
    // ClientHellos of length 0x1fc through 0x1ff get padded beyond 0x200. The
    // padding extension takes a minimum of four bytes plus one required content
    // byte. (To work around yet more server bugs, we avoid empty final
    // extensions.)
    {0x1fc, 0x201},
    {0x1fd, 0x202},
    {0x1fe, 0x203},
    {0x1ff, 0x204},
    // Finally, larger ClientHellos need no padding.
    {0x200, 0x200},
    {0x201, 0x201},
};

static bool TestPaddingExtension(uint16_t max_version,
                                 uint16_t session_version) {
  // Sample a baseline length.
  size_t base_len = GetClientHelloLen(max_version, session_version, 1);
  if (base_len == 0) {
    return false;
  }

  for (const PaddingTest &test : kPaddingTests) {
    if (base_len > test.input_len) {
      fprintf(stderr,
              "Baseline ClientHello too long (max_version = %04x, "
              "session_version = %04x).\n",
              max_version, session_version);
      return false;
    }

    size_t padded_len = GetClientHelloLen(max_version, session_version,
                                          1 + test.input_len - base_len);
    if (padded_len != test.padded_len) {
      fprintf(stderr,
              "%u-byte ClientHello padded to %u bytes, not %u (max_version = "
              "%04x, session_version = %04x).\n",
              static_cast<unsigned>(test.input_len),
              static_cast<unsigned>(padded_len),
              static_cast<unsigned>(test.padded_len), max_version,
              session_version);
      return false;
    }
  }

  return true;
}

// Test that |SSL_get_client_CA_list| echoes back the configured parameter even
// before configuring as a server.
TEST(SSLTest, ClientCAList) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(ctx);
  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);

  bssl::UniquePtr<X509_NAME> name(X509_NAME_new());
  ASSERT_TRUE(name);

  bssl::UniquePtr<X509_NAME> name_dup(X509_NAME_dup(name.get()));
  ASSERT_TRUE(name_dup);

  bssl::UniquePtr<STACK_OF(X509_NAME)> stack(sk_X509_NAME_new_null());
  ASSERT_TRUE(stack);

  ASSERT_TRUE(sk_X509_NAME_push(stack.get(), name_dup.get()));
  name_dup.release();

  // |SSL_set_client_CA_list| takes ownership.
  SSL_set_client_CA_list(ssl.get(), stack.release());

  STACK_OF(X509_NAME) *result = SSL_get_client_CA_list(ssl.get());
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, sk_X509_NAME_num(result));
  EXPECT_EQ(0, X509_NAME_cmp(sk_X509_NAME_value(result, 0), name.get()));
}

static void AppendSession(SSL_SESSION *session, void *arg) {
  std::vector<SSL_SESSION*> *out =
      reinterpret_cast<std::vector<SSL_SESSION*>*>(arg);
  out->push_back(session);
}

// CacheEquals returns true if |ctx|'s session cache consists of |expected|, in
// order.
static bool CacheEquals(SSL_CTX *ctx,
                        const std::vector<SSL_SESSION*> &expected) {
  // Check the linked list.
  SSL_SESSION *ptr = ctx->session_cache_head;
  for (SSL_SESSION *session : expected) {
    if (ptr != session) {
      return false;
    }
    // TODO(davidben): This is an absurd way to denote the end of the list.
    if (ptr->next ==
        reinterpret_cast<SSL_SESSION *>(&ctx->session_cache_tail)) {
      ptr = nullptr;
    } else {
      ptr = ptr->next;
    }
  }
  if (ptr != nullptr) {
    return false;
  }

  // Check the hash table.
  std::vector<SSL_SESSION*> actual, expected_copy;
  lh_SSL_SESSION_doall_arg(SSL_CTX_sessions(ctx), AppendSession, &actual);
  expected_copy = expected;

  std::sort(actual.begin(), actual.end());
  std::sort(expected_copy.begin(), expected_copy.end());

  return actual == expected_copy;
}

static bssl::UniquePtr<SSL_SESSION> CreateTestSession(uint32_t number) {
  bssl::UniquePtr<SSL_CTX> ssl_ctx(SSL_CTX_new(TLS_method()));
  if (!ssl_ctx) {
    return nullptr;
  }
  bssl::UniquePtr<SSL_SESSION> ret(SSL_SESSION_new(ssl_ctx.get()));
  if (!ret) {
    return nullptr;
  }

  ret->session_id_length = SSL3_SSL_SESSION_ID_LENGTH;
  OPENSSL_memset(ret->session_id, 0, ret->session_id_length);
  OPENSSL_memcpy(ret->session_id, &number, sizeof(number));
  return ret;
}

// Test that the internal session cache behaves as expected.
TEST(SSLTest, InternalSessionCache) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(ctx);

  // Prepare 10 test sessions.
  std::vector<bssl::UniquePtr<SSL_SESSION>> sessions;
  for (int i = 0; i < 10; i++) {
    bssl::UniquePtr<SSL_SESSION> session = CreateTestSession(i);
    ASSERT_TRUE(session);
    sessions.push_back(std::move(session));
  }

  SSL_CTX_sess_set_cache_size(ctx.get(), 5);

  // Insert all the test sessions.
  for (const auto &session : sessions) {
    ASSERT_TRUE(SSL_CTX_add_session(ctx.get(), session.get()));
  }

  // Only the last five should be in the list.
  ASSERT_TRUE(CacheEquals(
      ctx.get(), {sessions[9].get(), sessions[8].get(), sessions[7].get(),
                  sessions[6].get(), sessions[5].get()}));

  // Inserting an element already in the cache should fail and leave the cache
  // unchanged.
  ASSERT_FALSE(SSL_CTX_add_session(ctx.get(), sessions[7].get()));
  ASSERT_TRUE(CacheEquals(
      ctx.get(), {sessions[9].get(), sessions[8].get(), sessions[7].get(),
                  sessions[6].get(), sessions[5].get()}));

  // Although collisions should be impossible (256-bit session IDs), the cache
  // must handle them gracefully.
  bssl::UniquePtr<SSL_SESSION> collision(CreateTestSession(7));
  ASSERT_TRUE(collision);
  ASSERT_TRUE(SSL_CTX_add_session(ctx.get(), collision.get()));
  ASSERT_TRUE(CacheEquals(
      ctx.get(), {collision.get(), sessions[9].get(), sessions[8].get(),
                  sessions[6].get(), sessions[5].get()}));

  // Removing sessions behaves correctly.
  ASSERT_TRUE(SSL_CTX_remove_session(ctx.get(), sessions[6].get()));
  ASSERT_TRUE(CacheEquals(ctx.get(), {collision.get(), sessions[9].get(),
                                      sessions[8].get(), sessions[5].get()}));

  // Removing sessions requires an exact match.
  ASSERT_FALSE(SSL_CTX_remove_session(ctx.get(), sessions[0].get()));
  ASSERT_FALSE(SSL_CTX_remove_session(ctx.get(), sessions[7].get()));

  // The cache remains unchanged.
  ASSERT_TRUE(CacheEquals(ctx.get(), {collision.get(), sessions[9].get(),
                                      sessions[8].get(), sessions[5].get()}));
}

static uint16_t EpochFromSequence(uint64_t seq) {
  return static_cast<uint16_t>(seq >> 48);
}

static bssl::UniquePtr<X509> GetTestCertificate() {
  static const char kCertPEM[] =
      "-----BEGIN CERTIFICATE-----\n"
      "MIICWDCCAcGgAwIBAgIJAPuwTC6rEJsMMA0GCSqGSIb3DQEBBQUAMEUxCzAJBgNV\n"
      "BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n"
      "aWRnaXRzIFB0eSBMdGQwHhcNMTQwNDIzMjA1MDQwWhcNMTcwNDIyMjA1MDQwWjBF\n"
      "MQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50\n"
      "ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKB\n"
      "gQDYK8imMuRi/03z0K1Zi0WnvfFHvwlYeyK9Na6XJYaUoIDAtB92kWdGMdAQhLci\n"
      "HnAjkXLI6W15OoV3gA/ElRZ1xUpxTMhjP6PyY5wqT5r6y8FxbiiFKKAnHmUcrgfV\n"
      "W28tQ+0rkLGMryRtrukXOgXBv7gcrmU7G1jC2a7WqmeI8QIDAQABo1AwTjAdBgNV\n"
      "HQ4EFgQUi3XVrMsIvg4fZbf6Vr5sp3Xaha8wHwYDVR0jBBgwFoAUi3XVrMsIvg4f\n"
      "Zbf6Vr5sp3Xaha8wDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOBgQA76Hht\n"
      "ldY9avcTGSwbwoiuIqv0jTL1fHFnzy3RHMLDh+Lpvolc5DSrSJHCP5WuK0eeJXhr\n"
      "T5oQpHL9z/cCDLAKCKRa4uV0fhEdOWBqyR9p8y5jJtye72t6CuFUV5iqcpF4BH4f\n"
      "j2VNHwsSrJwkD4QUGlUtH7vwnQmyCFxZMmWAJg==\n"
      "-----END CERTIFICATE-----\n";
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(kCertPEM, strlen(kCertPEM)));
  return bssl::UniquePtr<X509>(
      PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
}

static bssl::UniquePtr<EVP_PKEY> GetTestKey() {
  static const char kKeyPEM[] =
      "-----BEGIN RSA PRIVATE KEY-----\n"
      "MIICXgIBAAKBgQDYK8imMuRi/03z0K1Zi0WnvfFHvwlYeyK9Na6XJYaUoIDAtB92\n"
      "kWdGMdAQhLciHnAjkXLI6W15OoV3gA/ElRZ1xUpxTMhjP6PyY5wqT5r6y8FxbiiF\n"
      "KKAnHmUcrgfVW28tQ+0rkLGMryRtrukXOgXBv7gcrmU7G1jC2a7WqmeI8QIDAQAB\n"
      "AoGBAIBy09Fd4DOq/Ijp8HeKuCMKTHqTW1xGHshLQ6jwVV2vWZIn9aIgmDsvkjCe\n"
      "i6ssZvnbjVcwzSoByhjN8ZCf/i15HECWDFFh6gt0P5z0MnChwzZmvatV/FXCT0j+\n"
      "WmGNB/gkehKjGXLLcjTb6dRYVJSCZhVuOLLcbWIV10gggJQBAkEA8S8sGe4ezyyZ\n"
      "m4e9r95g6s43kPqtj5rewTsUxt+2n4eVodD+ZUlCULWVNAFLkYRTBCASlSrm9Xhj\n"
      "QpmWAHJUkQJBAOVzQdFUaewLtdOJoPCtpYoY1zd22eae8TQEmpGOR11L6kbxLQsk\n"
      "aMly/DOnOaa82tqAGTdqDEZgSNmCeKKknmECQAvpnY8GUOVAubGR6c+W90iBuQLj\n"
      "LtFp/9ihd2w/PoDwrHZaoUYVcT4VSfJQog/k7kjE4MYXYWL8eEKg3WTWQNECQQDk\n"
      "104Wi91Umd1PzF0ijd2jXOERJU1wEKe6XLkYYNHWQAe5l4J4MWj9OdxFXAxIuuR/\n"
      "tfDwbqkta4xcux67//khAkEAvvRXLHTaa6VFzTaiiO8SaFsHV3lQyXOtMrBpB5jd\n"
      "moZWgjHvB2W9Ckn7sDqsPB+U2tyX0joDdQEyuiMECDY8oQ==\n"
      "-----END RSA PRIVATE KEY-----\n";
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(kKeyPEM, strlen(kKeyPEM)));
  return bssl::UniquePtr<EVP_PKEY>(
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
}

static bssl::UniquePtr<X509> GetECDSATestCertificate() {
  static const char kCertPEM[] =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIBzzCCAXagAwIBAgIJANlMBNpJfb/rMAkGByqGSM49BAEwRTELMAkGA1UEBhMC\n"
      "QVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdp\n"
      "dHMgUHR5IEx0ZDAeFw0xNDA0MjMyMzIxNTdaFw0xNDA1MjMyMzIxNTdaMEUxCzAJ\n"
      "BgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5l\n"
      "dCBXaWRnaXRzIFB0eSBMdGQwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATmK2ni\n"
      "v2Wfl74vHg2UikzVl2u3qR4NRvvdqakendy6WgHn1peoChj5w8SjHlbifINI2xYa\n"
      "HPUdfvGULUvPciLBo1AwTjAdBgNVHQ4EFgQUq4TSrKuV8IJOFngHVVdf5CaNgtEw\n"
      "HwYDVR0jBBgwFoAUq4TSrKuV8IJOFngHVVdf5CaNgtEwDAYDVR0TBAUwAwEB/zAJ\n"
      "BgcqhkjOPQQBA0gAMEUCIQDyoDVeUTo2w4J5m+4nUIWOcAZ0lVfSKXQA9L4Vh13E\n"
      "BwIgfB55FGohg/B6dGh5XxSZmmi08cueFV7mHzJSYV51yRQ=\n"
      "-----END CERTIFICATE-----\n";
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(kCertPEM, strlen(kCertPEM)));
  return bssl::UniquePtr<X509>(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
}

static bssl::UniquePtr<EVP_PKEY> GetECDSATestKey() {
  static const char kKeyPEM[] =
      "-----BEGIN PRIVATE KEY-----\n"
      "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgBw8IcnrUoEqc3VnJ\n"
      "TYlodwi1b8ldMHcO6NHJzgqLtGqhRANCAATmK2niv2Wfl74vHg2UikzVl2u3qR4N\n"
      "Rvvdqakendy6WgHn1peoChj5w8SjHlbifINI2xYaHPUdfvGULUvPciLB\n"
      "-----END PRIVATE KEY-----\n";
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(kKeyPEM, strlen(kKeyPEM)));
  return bssl::UniquePtr<EVP_PKEY>(
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
}

static bssl::UniquePtr<CRYPTO_BUFFER> BufferFromPEM(const char *pem) {
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(pem, strlen(pem)));
  char *name, *header;
  uint8_t *data;
  long data_len;
  if (!PEM_read_bio(bio.get(), &name, &header, &data,
                    &data_len)) {
    return nullptr;
  }
  OPENSSL_free(name);
  OPENSSL_free(header);

  auto ret = bssl::UniquePtr<CRYPTO_BUFFER>(
      CRYPTO_BUFFER_new(data, data_len, nullptr));
  OPENSSL_free(data);
  return ret;
}

static bssl::UniquePtr<CRYPTO_BUFFER> GetChainTestCertificateBuffer() {
  static const char kCertPEM[] =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIC0jCCAbqgAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwDzENMAsGA1UEAwwEQiBD\n"
      "QTAeFw0xNjAyMjgyMDI3MDNaFw0yNjAyMjUyMDI3MDNaMBgxFjAUBgNVBAMMDUNs\n"
      "aWVudCBDZXJ0IEEwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDRvaz8\n"
      "CC/cshpCafJo4jLkHEoBqDLhdgFelJoAiQUyIqyWl2O7YHPnpJH+TgR7oelzNzt/\n"
      "kLRcH89M/TszB6zqyLTC4aqmvzKL0peD/jL2LWBucR0WXIvjA3zoRuF/x86+rYH3\n"
      "tHb+xs2PSs8EGL/Ev+ss+qTzTGEn26fuGNHkNw6tOwPpc+o8+wUtzf/kAthamo+c\n"
      "IDs2rQ+lP7+aLZTLeU/q4gcLutlzcK5imex5xy2jPkweq48kijK0kIzl1cPlA5d1\n"
      "z7C8jU50Pj9X9sQDJTN32j7UYRisJeeYQF8GaaN8SbrDI6zHgKzrRLyxDt/KQa9V\n"
      "iLeXANgZi+Xx9KgfAgMBAAGjLzAtMAwGA1UdEwEB/wQCMAAwHQYDVR0lBBYwFAYI\n"
      "KwYBBQUHAwEGCCsGAQUFBwMCMA0GCSqGSIb3DQEBCwUAA4IBAQBFEVbmYl+2RtNw\n"
      "rDftRDF1v2QUbcN2ouSnQDHxeDQdSgasLzT3ui8iYu0Rw2WWcZ0DV5e0ztGPhWq7\n"
      "AO0B120aFRMOY+4+bzu9Q2FFkQqc7/fKTvTDzIJI5wrMnFvUfzzvxh3OHWMYSs/w\n"
      "giq33hTKeHEq6Jyk3btCny0Ycecyc3yGXH10sizUfiHlhviCkDuESk8mFDwDDzqW\n"
      "ZF0IipzFbEDHoIxLlm3GQxpiLoEV4k8KYJp3R5KBLFyxM6UGPz8h72mIPCJp2RuK\n"
      "MYgF91UDvVzvnYm6TfseM2+ewKirC00GOrZ7rEcFvtxnKSqYf4ckqfNdSU1Y+RRC\n"
      "1ngWZ7Ih\n"
      "-----END CERTIFICATE-----\n";
  return BufferFromPEM(kCertPEM);
}

static bssl::UniquePtr<X509> X509FromBuffer(
    bssl::UniquePtr<CRYPTO_BUFFER> buffer) {
  if (!buffer) {
    return nullptr;
  }
  const uint8_t *derp = CRYPTO_BUFFER_data(buffer.get());
  return bssl::UniquePtr<X509>(
      d2i_X509(NULL, &derp, CRYPTO_BUFFER_len(buffer.get())));
}

static bssl::UniquePtr<X509> GetChainTestCertificate() {
  return X509FromBuffer(GetChainTestCertificateBuffer());
}

static bssl::UniquePtr<CRYPTO_BUFFER> GetChainTestIntermediateBuffer() {
  static const char kCertPEM[] =
      "-----BEGIN CERTIFICATE-----\n"
      "MIICwjCCAaqgAwIBAgICEAEwDQYJKoZIhvcNAQELBQAwFDESMBAGA1UEAwwJQyBS\n"
      "b290IENBMB4XDTE2MDIyODIwMjcwM1oXDTI2MDIyNTIwMjcwM1owDzENMAsGA1UE\n"
      "AwwEQiBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALsSCYmDip2D\n"
      "GkjFxw7ykz26JSjELkl6ArlYjFJ3aT/SCh8qbS4gln7RH8CPBd78oFdfhIKQrwtZ\n"
      "3/q21ykD9BAS3qHe2YdcJfm8/kWAy5DvXk6NXU4qX334KofBAEpgdA/igEFq1P1l\n"
      "HAuIfZCpMRfT+i5WohVsGi8f/NgpRvVaMONLNfgw57mz1lbtFeBEISmX0kbsuJxF\n"
      "Qj/Bwhi5/0HAEXG8e7zN4cEx0yPRvmOATRdVb/8dW2pwOHRJq9R5M0NUkIsTSnL7\n"
      "6N/z8hRAHMsV3IudC5Yd7GXW1AGu9a+iKU+Q4xcZCoj0DC99tL4VKujrV1kAeqsM\n"
      "cz5/dKzi6+cCAwEAAaMjMCEwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
      "AQYwDQYJKoZIhvcNAQELBQADggEBAIIeZiEeNhWWQ8Y4D+AGDwqUUeG8NjCbKrXQ\n"
      "BlHg5wZ8xftFaiP1Dp/UAezmx2LNazdmuwrYB8lm3FVTyaPDTKEGIPS4wJKHgqH1\n"
      "QPDhqNm85ey7TEtI9oYjsNim/Rb+iGkIAMXaxt58SzxbjvP0kMr1JfJIZbic9vye\n"
      "NwIspMFIpP3FB8ywyu0T0hWtCQgL4J47nigCHpOu58deP88fS/Nyz/fyGVWOZ76b\n"
      "WhWwgM3P3X95fQ3d7oFPR/bVh0YV+Cf861INwplokXgXQ3/TCQ+HNXeAMWn3JLWv\n"
      "XFwk8owk9dq/kQGdndGgy3KTEW4ctPX5GNhf3LJ9Q7dLji4ReQ4=\n"
      "-----END CERTIFICATE-----\n";
  return BufferFromPEM(kCertPEM);
}

static bssl::UniquePtr<X509> GetChainTestIntermediate() {
  return X509FromBuffer(GetChainTestIntermediateBuffer());
}

static bssl::UniquePtr<EVP_PKEY> GetChainTestKey() {
  static const char kKeyPEM[] =
      "-----BEGIN PRIVATE KEY-----\n"
      "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDRvaz8CC/cshpC\n"
      "afJo4jLkHEoBqDLhdgFelJoAiQUyIqyWl2O7YHPnpJH+TgR7oelzNzt/kLRcH89M\n"
      "/TszB6zqyLTC4aqmvzKL0peD/jL2LWBucR0WXIvjA3zoRuF/x86+rYH3tHb+xs2P\n"
      "Ss8EGL/Ev+ss+qTzTGEn26fuGNHkNw6tOwPpc+o8+wUtzf/kAthamo+cIDs2rQ+l\n"
      "P7+aLZTLeU/q4gcLutlzcK5imex5xy2jPkweq48kijK0kIzl1cPlA5d1z7C8jU50\n"
      "Pj9X9sQDJTN32j7UYRisJeeYQF8GaaN8SbrDI6zHgKzrRLyxDt/KQa9ViLeXANgZ\n"
      "i+Xx9KgfAgMBAAECggEBAK0VjSJzkyPaamcyTVSWjo7GdaBGcK60lk657RjR+lK0\n"
      "YJ7pkej4oM2hdsVZFsP8Cs4E33nXLa/0pDsRov/qrp0WQm2skwqGMC1I/bZ0WRPk\n"
      "wHaDrBBfESWnJDX/AGpVtlyOjPmgmK6J2usMPihQUDkKdAYrVWJePrMIxt1q6BMe\n"
      "iczs3qriMmtY3bUc4UyUwJ5fhDLjshHvfuIpYQyI6EXZM6dZksn9LylXJnigY6QJ\n"
      "HxOYO0BDwOsZ8yQ8J8afLk88i0GizEkgE1z3REtQUwgWfxr1WV/ud+T6/ZhSAgH9\n"
      "042mQvSFZnIUSEsmCvjhWuAunfxHKCTcAoYISWfzWpkCgYEA7gpf3HHU5Tn+CgUn\n"
      "1X5uGpG3DmcMgfeGgs2r2f/IIg/5Ac1dfYILiybL1tN9zbyLCJfcbFpWBc9hJL6f\n"
      "CPc5hUiwWFJqBJewxQkC1Ae/HakHbip+IZ+Jr0842O4BAArvixk4Lb7/N2Ct9sTE\n"
      "NJO6RtK9lbEZ5uK61DglHy8CS2UCgYEA4ZC1o36kPAMQBggajgnucb2yuUEelk0f\n"
      "AEr+GI32MGE+93xMr7rAhBoqLg4AITyIfEnOSQ5HwagnIHonBbv1LV/Gf9ursx8Z\n"
      "YOGbvT8zzzC+SU1bkDzdjAYnFQVGIjMtKOBJ3K07++ypwX1fr4QsQ8uKL8WSOWwt\n"
      "Z3Bym6XiZzMCgYADnhy+2OwHX85AkLt+PyGlPbmuelpyTzS4IDAQbBa6jcuW/2wA\n"
      "UE2km75VUXmD+u2R/9zVuLm99NzhFhSMqlUxdV1YukfqMfP5yp1EY6m/5aW7QuIP\n"
      "2MDa7TVL9rIFMiVZ09RKvbBbQxjhuzPQKL6X/PPspnhiTefQ+dl2k9xREQKBgHDS\n"
      "fMfGNEeAEKezrfSVqxphE9/tXms3L+ZpnCaT+yu/uEr5dTIAawKoQ6i9f/sf1/Sy\n"
      "xedsqR+IB+oKrzIDDWMgoJybN4pkZ8E5lzhVQIjFjKgFdWLzzqyW9z1gYfABQPlN\n"
      "FiS20WX0vgP1vcKAjdNrHzc9zyHBpgQzDmAj3NZZAoGBAI8vKCKdH7w3aL5CNkZQ\n"
      "2buIeWNA2HZazVwAGG5F2TU/LmXfRKnG6dX5bkU+AkBZh56jNZy//hfFSewJB4Kk\n"
      "buB7ERSdaNbO21zXt9FEA3+z0RfMd/Zv2vlIWOSB5nzl/7UKti3sribK6s9ZVLfi\n"
      "SxpiPQ8d/hmSGwn4ksrWUsJD\n"
      "-----END PRIVATE KEY-----\n";
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(kKeyPEM, strlen(kKeyPEM)));
  return bssl::UniquePtr<EVP_PKEY>(
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
}

static bool CompleteHandshakes(SSL *client, SSL *server) {
  // Drive both their handshakes to completion.
  for (;;) {
    int client_ret = SSL_do_handshake(client);
    int client_err = SSL_get_error(client, client_ret);
    if (client_err != SSL_ERROR_NONE &&
        client_err != SSL_ERROR_WANT_READ &&
        client_err != SSL_ERROR_WANT_WRITE &&
        client_err != SSL_ERROR_PENDING_TICKET) {
      fprintf(stderr, "Client error: %d\n", client_err);
      return false;
    }

    int server_ret = SSL_do_handshake(server);
    int server_err = SSL_get_error(server, server_ret);
    if (server_err != SSL_ERROR_NONE &&
        server_err != SSL_ERROR_WANT_READ &&
        server_err != SSL_ERROR_WANT_WRITE &&
        server_err != SSL_ERROR_PENDING_TICKET) {
      fprintf(stderr, "Server error: %d\n", server_err);
      return false;
    }

    if (client_ret == 1 && server_ret == 1) {
      break;
    }
  }

  return true;
}

static bool ConnectClientAndServer(bssl::UniquePtr<SSL> *out_client,
                                   bssl::UniquePtr<SSL> *out_server,
                                   SSL_CTX *client_ctx, SSL_CTX *server_ctx,
                                   SSL_SESSION *session) {
  bssl::UniquePtr<SSL> client(SSL_new(client_ctx)), server(SSL_new(server_ctx));
  if (!client || !server) {
    return false;
  }
  SSL_set_connect_state(client.get());
  SSL_set_accept_state(server.get());

  SSL_set_session(client.get(), session);

  BIO *bio1, *bio2;
  if (!BIO_new_bio_pair(&bio1, 0, &bio2, 0)) {
    return false;
  }
  // SSL_set_bio takes ownership.
  SSL_set_bio(client.get(), bio1, bio1);
  SSL_set_bio(server.get(), bio2, bio2);

  if (!CompleteHandshakes(client.get(), server.get())) {
    return false;
  }

  *out_client = std::move(client);
  *out_server = std::move(server);
  return true;
}

static bool TestSequenceNumber(bool is_dtls, const SSL_METHOD *method,
                               uint16_t version) {
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(method));
  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(method));
  if (!server_ctx || !client_ctx ||
      !SSL_CTX_set_min_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_min_proto_version(server_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(server_ctx.get(), version)) {
    return false;
  }

  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key || !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get())) {
    return false;
  }

  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, client_ctx.get(),
                              server_ctx.get(), nullptr /* no session */)) {
    return false;
  }

  // Drain any post-handshake messages to ensure there are no unread records
  // on either end.
  uint8_t byte = 0;
  if (SSL_read(client.get(), &byte, 1) > 0 ||
      SSL_read(server.get(), &byte, 1) > 0) {
    fprintf(stderr, "Received unexpected data.\n");
    return false;
  }

  uint64_t client_read_seq = SSL_get_read_sequence(client.get());
  uint64_t client_write_seq = SSL_get_write_sequence(client.get());
  uint64_t server_read_seq = SSL_get_read_sequence(server.get());
  uint64_t server_write_seq = SSL_get_write_sequence(server.get());

  if (is_dtls) {
    // Both client and server must be at epoch 1.
    if (EpochFromSequence(client_read_seq) != 1 ||
        EpochFromSequence(client_write_seq) != 1 ||
        EpochFromSequence(server_read_seq) != 1 ||
        EpochFromSequence(server_write_seq) != 1) {
      fprintf(stderr, "Bad epochs.\n");
      return false;
    }

    // The next record to be written should exceed the largest received.
    if (client_write_seq <= server_read_seq ||
        server_write_seq <= client_read_seq) {
      fprintf(stderr, "Inconsistent sequence numbers.\n");
      return false;
    }
  } else {
    // The next record to be written should equal the next to be received.
    if (client_write_seq != server_read_seq ||
        server_write_seq != client_read_seq) {
      fprintf(stderr, "Inconsistent sequence numbers.\n");
      return false;
    }
  }

  // Send a record from client to server.
  if (SSL_write(client.get(), &byte, 1) != 1 ||
      SSL_read(server.get(), &byte, 1) != 1) {
    fprintf(stderr, "Could not send byte.\n");
    return false;
  }

  // The client write and server read sequence numbers should have
  // incremented.
  if (client_write_seq + 1 != SSL_get_write_sequence(client.get()) ||
      server_read_seq + 1 != SSL_get_read_sequence(server.get())) {
    fprintf(stderr, "Sequence numbers did not increment.\n");
    return false;
  }

  return true;
}

static bool TestOneSidedShutdown(bool is_dtls, const SSL_METHOD *method,
                                 uint16_t version) {
  // SSL_shutdown is a no-op in DTLS.
  if (is_dtls) {
    return true;
  }

  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(method));
  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(method));
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!client_ctx || !server_ctx || !cert || !key ||
      !SSL_CTX_set_min_proto_version(server_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(server_ctx.get(), version) ||
      !SSL_CTX_set_min_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get())) {
    return false;
  }

  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, client_ctx.get(),
                              server_ctx.get(), nullptr /* no session */)) {
    return false;
  }

  // Shut down half the connection. SSL_shutdown will return 0 to signal only
  // one side has shut down.
  if (SSL_shutdown(client.get()) != 0) {
    fprintf(stderr, "Could not shutdown.\n");
    return false;
  }

  // Reading from the server should consume the EOF.
  uint8_t byte;
  if (SSL_read(server.get(), &byte, 1) != 0 ||
      SSL_get_error(server.get(), 0) != SSL_ERROR_ZERO_RETURN) {
    fprintf(stderr, "Connection was not shut down cleanly.\n");
    return false;
  }

  // However, the server may continue to write data and then shut down the
  // connection.
  byte = 42;
  if (SSL_write(server.get(), &byte, 1) != 1 ||
      SSL_read(client.get(), &byte, 1) != 1 ||
      byte != 42) {
    fprintf(stderr, "Could not send byte.\n");
    return false;
  }

  // The server may then shutdown the connection.
  if (SSL_shutdown(server.get()) != 1 ||
      SSL_shutdown(client.get()) != 1) {
    fprintf(stderr, "Could not complete shutdown.\n");
    return false;
  }

  return true;
}

TEST(SSLTest, SessionDuplication) {
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_method()));
  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(client_ctx);
  ASSERT_TRUE(server_ctx);

  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  ASSERT_TRUE(cert);
  ASSERT_TRUE(key);
  ASSERT_TRUE(SSL_CTX_use_certificate(server_ctx.get(), cert.get()));
  ASSERT_TRUE(SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()));

  bssl::UniquePtr<SSL> client, server;
  ASSERT_TRUE(ConnectClientAndServer(&client, &server, client_ctx.get(),
                                     server_ctx.get(),
                                     nullptr /* no session */));

  SSL_SESSION *session0 = SSL_get_session(client.get());
  bssl::UniquePtr<SSL_SESSION> session1(
      SSL_SESSION_dup(session0, SSL_SESSION_DUP_ALL));
  ASSERT_TRUE(session1);

  session1->not_resumable = 0;

  uint8_t *s0_bytes, *s1_bytes;
  size_t s0_len, s1_len;

  ASSERT_TRUE(SSL_SESSION_to_bytes(session0, &s0_bytes, &s0_len));
  bssl::UniquePtr<uint8_t> free_s0(s0_bytes);

  ASSERT_TRUE(SSL_SESSION_to_bytes(session1.get(), &s1_bytes, &s1_len));
  bssl::UniquePtr<uint8_t> free_s1(s1_bytes);

  EXPECT_EQ(Bytes(s0_bytes, s0_len), Bytes(s1_bytes, s1_len));
}

static void ExpectFDs(const SSL *ssl, int rfd, int wfd) {
  EXPECT_EQ(rfd, SSL_get_rfd(ssl));
  EXPECT_EQ(wfd, SSL_get_wfd(ssl));

  // The wrapper BIOs are always equal when fds are equal, even if set
  // individually.
  if (rfd == wfd) {
    EXPECT_EQ(SSL_get_rbio(ssl), SSL_get_wbio(ssl));
  }
}

TEST(SSLTest, SetFD) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(ctx);

  // Test setting different read and write FDs.
  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);
  EXPECT_TRUE(SSL_set_rfd(ssl.get(), 1));
  EXPECT_TRUE(SSL_set_wfd(ssl.get(), 2));
  ExpectFDs(ssl.get(), 1, 2);

  // Test setting the same FD.
  ssl.reset(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);
  EXPECT_TRUE(SSL_set_fd(ssl.get(), 1));
  ExpectFDs(ssl.get(), 1, 1);

  // Test setting the same FD one side at a time.
  ssl.reset(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);
  EXPECT_TRUE(SSL_set_rfd(ssl.get(), 1));
  EXPECT_TRUE(SSL_set_wfd(ssl.get(), 1));
  ExpectFDs(ssl.get(), 1, 1);

  // Test setting the same FD in the other order.
  ssl.reset(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);
  EXPECT_TRUE(SSL_set_wfd(ssl.get(), 1));
  EXPECT_TRUE(SSL_set_rfd(ssl.get(), 1));
  ExpectFDs(ssl.get(), 1, 1);

  // Test changing the read FD partway through.
  ssl.reset(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);
  EXPECT_TRUE(SSL_set_fd(ssl.get(), 1));
  EXPECT_TRUE(SSL_set_rfd(ssl.get(), 2));
  ExpectFDs(ssl.get(), 2, 1);

  // Test changing the write FD partway through.
  ssl.reset(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);
  EXPECT_TRUE(SSL_set_fd(ssl.get(), 1));
  EXPECT_TRUE(SSL_set_wfd(ssl.get(), 2));
  ExpectFDs(ssl.get(), 1, 2);

  // Test a no-op change to the read FD partway through.
  ssl.reset(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);
  EXPECT_TRUE(SSL_set_fd(ssl.get(), 1));
  EXPECT_TRUE(SSL_set_rfd(ssl.get(), 1));
  ExpectFDs(ssl.get(), 1, 1);

  // Test a no-op change to the write FD partway through.
  ssl.reset(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);
  EXPECT_TRUE(SSL_set_fd(ssl.get(), 1));
  EXPECT_TRUE(SSL_set_wfd(ssl.get(), 1));
  ExpectFDs(ssl.get(), 1, 1);

  // ASan builds will implicitly test that the internal |BIO| reference-counting
  // is correct.
}

TEST(SSLTest, SetBIO) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(ctx);

  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  bssl::UniquePtr<BIO> bio1(BIO_new(BIO_s_mem())), bio2(BIO_new(BIO_s_mem())),
      bio3(BIO_new(BIO_s_mem()));
  ASSERT_TRUE(ssl);
  ASSERT_TRUE(bio1);
  ASSERT_TRUE(bio2);
  ASSERT_TRUE(bio3);

  // SSL_set_bio takes one reference when the parameters are the same.
  BIO_up_ref(bio1.get());
  SSL_set_bio(ssl.get(), bio1.get(), bio1.get());

  // Repeating the call does nothing.
  SSL_set_bio(ssl.get(), bio1.get(), bio1.get());

  // It takes one reference each when the parameters are different.
  BIO_up_ref(bio2.get());
  BIO_up_ref(bio3.get());
  SSL_set_bio(ssl.get(), bio2.get(), bio3.get());

  // Repeating the call does nothing.
  SSL_set_bio(ssl.get(), bio2.get(), bio3.get());

  // It takes one reference when changing only wbio.
  BIO_up_ref(bio1.get());
  SSL_set_bio(ssl.get(), bio2.get(), bio1.get());

  // It takes one reference when changing only rbio and the two are different.
  BIO_up_ref(bio3.get());
  SSL_set_bio(ssl.get(), bio3.get(), bio1.get());

  // If setting wbio to rbio, it takes no additional references.
  SSL_set_bio(ssl.get(), bio3.get(), bio3.get());

  // From there, wbio may be switched to something else.
  BIO_up_ref(bio1.get());
  SSL_set_bio(ssl.get(), bio3.get(), bio1.get());

  // If setting rbio to wbio, it takes no additional references.
  SSL_set_bio(ssl.get(), bio1.get(), bio1.get());

  // From there, rbio may be switched to something else, but, for historical
  // reasons, it takes a reference to both parameters.
  BIO_up_ref(bio1.get());
  BIO_up_ref(bio2.get());
  SSL_set_bio(ssl.get(), bio2.get(), bio1.get());

  // ASAN builds will implicitly test that the internal |BIO| reference-counting
  // is correct.
}

static int VerifySucceed(X509_STORE_CTX *store_ctx, void *arg) { return 1; }

static bool TestGetPeerCertificate(bool is_dtls, const SSL_METHOD *method,
                                   uint16_t version) {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  // Configure both client and server to accept any certificate.
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(method));
  if (!ctx ||
      !SSL_CTX_use_certificate(ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(ctx.get(), key.get()) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(ctx.get(), version)) {
    return false;
  }
  SSL_CTX_set_verify(
      ctx.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
  SSL_CTX_set_cert_verify_callback(ctx.get(), VerifySucceed, NULL);

  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, ctx.get(), ctx.get(),
                              nullptr /* no session */)) {
    return false;
  }

  // Client and server should both see the leaf certificate.
  bssl::UniquePtr<X509> peer(SSL_get_peer_certificate(server.get()));
  if (!peer || X509_cmp(cert.get(), peer.get()) != 0) {
    fprintf(stderr, "Server peer certificate did not match.\n");
    return false;
  }

  peer.reset(SSL_get_peer_certificate(client.get()));
  if (!peer || X509_cmp(cert.get(), peer.get()) != 0) {
    fprintf(stderr, "Client peer certificate did not match.\n");
    return false;
  }

  // However, for historical reasons, the chain includes the leaf on the
  // client, but does not on the server.
  if (sk_X509_num(SSL_get_peer_cert_chain(client.get())) != 1) {
    fprintf(stderr, "Client peer chain was incorrect.\n");
    return false;
  }

  if (sk_X509_num(SSL_get_peer_cert_chain(server.get())) != 0) {
    fprintf(stderr, "Server peer chain was incorrect.\n");
    return false;
  }

  return true;
}

static bool TestRetainOnlySHA256OfCerts(bool is_dtls, const SSL_METHOD *method,
                                        uint16_t version) {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  uint8_t *cert_der = NULL;
  int cert_der_len = i2d_X509(cert.get(), &cert_der);
  if (cert_der_len < 0) {
    return false;
  }
  bssl::UniquePtr<uint8_t> free_cert_der(cert_der);

  uint8_t cert_sha256[SHA256_DIGEST_LENGTH];
  SHA256(cert_der, cert_der_len, cert_sha256);

  // Configure both client and server to accept any certificate, but the
  // server must retain only the SHA-256 of the peer.
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(method));
  if (!ctx ||
      !SSL_CTX_use_certificate(ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(ctx.get(), key.get()) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(ctx.get(), version)) {
    return false;
  }
  SSL_CTX_set_verify(
      ctx.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
  SSL_CTX_set_cert_verify_callback(ctx.get(), VerifySucceed, NULL);
  SSL_CTX_set_retain_only_sha256_of_client_certs(ctx.get(), 1);

  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, ctx.get(), ctx.get(),
                              nullptr /* no session */)) {
    return false;
  }

  // The peer certificate has been dropped.
  bssl::UniquePtr<X509> peer(SSL_get_peer_certificate(server.get()));
  if (peer) {
    fprintf(stderr, "Peer certificate was retained.\n");
    return false;
  }

  SSL_SESSION *session = SSL_get_session(server.get());
  if (!session->peer_sha256_valid) {
    fprintf(stderr, "peer_sha256_valid was not set.\n");
    return false;
  }

  if (OPENSSL_memcmp(cert_sha256, session->peer_sha256, SHA256_DIGEST_LENGTH) !=
      0) {
    fprintf(stderr, "peer_sha256 did not match.\n");
    return false;
  }

  return true;
}

static bool ClientHelloMatches(uint16_t version, const uint8_t *expected,
                               size_t expected_len) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  // Our default cipher list varies by CPU capabilities, so manually place the
  // ChaCha20 ciphers in front.
  const char* cipher_list = "CHACHA20:ALL";
  if (!ctx ||
      // SSLv3 is off by default.
      !SSL_CTX_set_min_proto_version(ctx.get(), SSL3_VERSION) ||
      !SSL_CTX_set_max_proto_version(ctx.get(), version) ||
      !SSL_CTX_set_strict_cipher_list(ctx.get(), cipher_list)) {
    return false;
  }

  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  if (!ssl) {
    return false;
  }
  std::vector<uint8_t> client_hello;
  if (!GetClientHello(ssl.get(), &client_hello)) {
    return false;
  }

  // Zero the client_random.
  constexpr size_t kRandomOffset = 1 + 2 + 2 +  // record header
                                   1 + 3 +      // handshake message header
                                   2;           // client_version
  if (client_hello.size() < kRandomOffset + SSL3_RANDOM_SIZE) {
    fprintf(stderr, "ClientHello for version %04x too short.\n", version);
    return false;
  }
  OPENSSL_memset(client_hello.data() + kRandomOffset, 0, SSL3_RANDOM_SIZE);

  if (client_hello.size() != expected_len ||
      OPENSSL_memcmp(client_hello.data(), expected, expected_len) != 0) {
    fprintf(stderr, "ClientHello for version %04x did not match:\n", version);
    fprintf(stderr, "Got:\n\t");
    for (size_t i = 0; i < client_hello.size(); i++) {
      fprintf(stderr, "0x%02x, ", client_hello[i]);
    }
    fprintf(stderr, "\nWanted:\n\t");
    for (size_t i = 0; i < expected_len; i++) {
      fprintf(stderr, "0x%02x, ", expected[i]);
    }
    fprintf(stderr, "\n");
    return false;
  }

  return true;
}

// Tests that our ClientHellos do not change unexpectedly.
static bool TestClientHello() {
  static const uint8_t kSSL3ClientHello[] = {
    0x16,
    0x03, 0x00,
    0x00, 0x3b,
    0x01,
    0x00, 0x00, 0x37,
    0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
    0x00, 0x10,
    0xc0, 0x09,
    0xc0, 0x13,
    0xc0, 0x0a,
    0xc0, 0x14,
    0x00, 0x2f,
    0x00, 0x35,
    0x00, 0x0a,
    0x00, 0xff, 0x01, 0x00,
  };
  if (!ClientHelloMatches(SSL3_VERSION, kSSL3ClientHello,
                          sizeof(kSSL3ClientHello))) {
    return false;
  }

  static const uint8_t kTLS1ClientHello[] = {
      0x16,
      0x03, 0x01,
      0x00, 0x5a,
      0x01,
      0x00, 0x00, 0x56,
      0x03, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      0x00, 0x0e,
      0xc0, 0x09,
      0xc0, 0x13,
      0xc0, 0x0a,
      0xc0, 0x14,
      0x00, 0x2f,
      0x00, 0x35,
      0x00, 0x0a,
      0x01, 0x00, 0x00, 0x1f, 0xff, 0x01, 0x00, 0x01, 0x00, 0x00, 0x17, 0x00,
      0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x00,
      0x0a, 0x00, 0x08, 0x00, 0x06, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x18,
  };
  if (!ClientHelloMatches(TLS1_VERSION, kTLS1ClientHello,
                          sizeof(kTLS1ClientHello))) {
    return false;
  }

  static const uint8_t kTLS11ClientHello[] = {
      0x16,
      0x03, 0x01,
      0x00, 0x5a,
      0x01,
      0x00, 0x00, 0x56,
      0x03, 0x02,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      0x00, 0x0e,
      0xc0, 0x09,
      0xc0, 0x13,
      0xc0, 0x0a,
      0xc0, 0x14,
      0x00, 0x2f,
      0x00, 0x35,
      0x00, 0x0a,
      0x01, 0x00, 0x00, 0x1f, 0xff, 0x01, 0x00, 0x01, 0x00, 0x00, 0x17, 0x00,
      0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x00,
      0x0a, 0x00, 0x08, 0x00, 0x06, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x18,
  };
  if (!ClientHelloMatches(TLS1_1_VERSION, kTLS11ClientHello,
                          sizeof(kTLS11ClientHello))) {
    return false;
  }

  // kTLS12ClientHello assumes RSA-PSS, which is disabled for Android system
  // builds.
#if defined(BORINGSSL_ANDROID_SYSTEM)
  return true;
#endif

  static const uint8_t kTLS12ClientHello[] = {
      0x16,
      0x03, 0x01,
      0x00, 0x8e,
      0x01,
      0x00, 0x00, 0x8a,
      0x03, 0x03,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x2a,
      0xcc, 0xa9,
      0xcc, 0xa8,
      0xc0, 0x2b,
      0xc0, 0x2f,
      0xc0, 0x2c,
      0xc0, 0x30,
      0xc0, 0x09,
      0xc0, 0x23,
      0xc0, 0x13,
      0xc0, 0x27,
      0xc0, 0x0a,
      0xc0, 0x24,
      0xc0, 0x14,
      0xc0, 0x28,
      0x00, 0x9c,
      0x00, 0x9d,
      0x00, 0x2f,
      0x00, 0x3c,
      0x00, 0x35,
      0x00, 0x3d,
      0x00, 0x0a,
      0x01, 0x00, 0x00, 0x37, 0xff, 0x01, 0x00, 0x01, 0x00, 0x00, 0x17, 0x00,
      0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x14, 0x00, 0x12, 0x04,
      0x03, 0x08, 0x04, 0x04, 0x01, 0x05, 0x03, 0x08, 0x05, 0x05, 0x01, 0x08,
      0x06, 0x06, 0x01, 0x02, 0x01, 0x00, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x00,
      0x0a, 0x00, 0x08, 0x00, 0x06, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x18,
  };
  if (!ClientHelloMatches(TLS1_2_VERSION, kTLS12ClientHello,
                          sizeof(kTLS12ClientHello))) {
    return false;
  }

  // TODO(davidben): Add a change detector for TLS 1.3 once the spec and our
  // implementation has settled enough that it won't change.

  return true;
}

static bssl::UniquePtr<SSL_SESSION> g_last_session;

static int SaveLastSession(SSL *ssl, SSL_SESSION *session) {
  // Save the most recent session.
  g_last_session.reset(session);
  return 1;
}

static bssl::UniquePtr<SSL_SESSION> CreateClientSession(SSL_CTX *client_ctx,
                                             SSL_CTX *server_ctx) {
  g_last_session = nullptr;
  SSL_CTX_sess_set_new_cb(client_ctx, SaveLastSession);

  // Connect client and server to get a session.
  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, client_ctx, server_ctx,
                              nullptr /* no session */)) {
    fprintf(stderr, "Failed to connect client and server.\n");
    return nullptr;
  }

  // Run the read loop to account for post-handshake tickets in TLS 1.3.
  SSL_read(client.get(), nullptr, 0);

  SSL_CTX_sess_set_new_cb(client_ctx, nullptr);

  if (!g_last_session) {
    fprintf(stderr, "Client did not receive a session.\n");
    return nullptr;
  }
  return std::move(g_last_session);
}

static bool ExpectSessionReused(SSL_CTX *client_ctx, SSL_CTX *server_ctx,
                                SSL_SESSION *session,
                                bool reused) {
  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, client_ctx,
                              server_ctx, session)) {
    fprintf(stderr, "Failed to connect client and server.\n");
    return false;
  }

  if (SSL_session_reused(client.get()) != SSL_session_reused(server.get())) {
    fprintf(stderr, "Client and server were inconsistent.\n");
    return false;
  }

  bool was_reused = !!SSL_session_reused(client.get());
  if (was_reused != reused) {
    fprintf(stderr, "Session was%s reused, but we expected the opposite.\n",
            was_reused ? "" : " not");
    return false;
  }

  return true;
}

static bssl::UniquePtr<SSL_SESSION> ExpectSessionRenewed(SSL_CTX *client_ctx,
                                                         SSL_CTX *server_ctx,
                                                         SSL_SESSION *session) {
  g_last_session = nullptr;
  SSL_CTX_sess_set_new_cb(client_ctx, SaveLastSession);

  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, client_ctx,
                              server_ctx, session)) {
    fprintf(stderr, "Failed to connect client and server.\n");
    return nullptr;
  }

  if (SSL_session_reused(client.get()) != SSL_session_reused(server.get())) {
    fprintf(stderr, "Client and server were inconsistent.\n");
    return nullptr;
  }

  if (!SSL_session_reused(client.get())) {
    fprintf(stderr, "Session was not reused.\n");
    return nullptr;
  }

  // Run the read loop to account for post-handshake tickets in TLS 1.3.
  SSL_read(client.get(), nullptr, 0);

  SSL_CTX_sess_set_new_cb(client_ctx, nullptr);

  if (!g_last_session) {
    fprintf(stderr, "Client did not receive a renewed session.\n");
    return nullptr;
  }
  return std::move(g_last_session);
}

static int SwitchSessionIDContextSNI(SSL *ssl, int *out_alert, void *arg) {
  static const uint8_t kContext[] = {3};

  if (!SSL_set_session_id_context(ssl, kContext, sizeof(kContext))) {
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  return SSL_TLSEXT_ERR_OK;
}

static bool TestSessionIDContext(bool is_dtls, const SSL_METHOD *method,
                                 uint16_t version) {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  static const uint8_t kContext1[] = {1};
  static const uint8_t kContext2[] = {2};

  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(method));
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(method));
  if (!server_ctx || !client_ctx ||
      !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()) ||
      !SSL_CTX_set_session_id_context(server_ctx.get(), kContext1,
                                      sizeof(kContext1)) ||
      !SSL_CTX_set_min_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_min_proto_version(server_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(server_ctx.get(), version)) {
    return false;
  }

  SSL_CTX_set_session_cache_mode(client_ctx.get(), SSL_SESS_CACHE_BOTH);
  SSL_CTX_set_session_cache_mode(server_ctx.get(), SSL_SESS_CACHE_BOTH);

  bssl::UniquePtr<SSL_SESSION> session =
      CreateClientSession(client_ctx.get(), server_ctx.get());
  if (!session) {
    fprintf(stderr, "Error getting session.\n");
    return false;
  }

  if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                           true /* expect session reused */)) {
    fprintf(stderr, "Error resuming session.\n");
    return false;
  }

  // Change the session ID context.
  if (!SSL_CTX_set_session_id_context(server_ctx.get(), kContext2,
                                      sizeof(kContext2))) {
    return false;
  }

  if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                           false /* expect session not reused */)) {
    fprintf(stderr, "Error connecting with a different context.\n");
    return false;
  }

  // Change the session ID context back and install an SNI callback to switch
  // it.
  if (!SSL_CTX_set_session_id_context(server_ctx.get(), kContext1,
                                      sizeof(kContext1))) {
    return false;
  }

  SSL_CTX_set_tlsext_servername_callback(server_ctx.get(),
                                         SwitchSessionIDContextSNI);

  if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                           false /* expect session not reused */)) {
    fprintf(stderr, "Error connecting with a context switch on SNI callback.\n");
    return false;
  }

  // Switch the session ID context with the early callback instead.
  SSL_CTX_set_tlsext_servername_callback(server_ctx.get(), nullptr);
  SSL_CTX_set_select_certificate_cb(
      server_ctx.get(),
      [](const SSL_CLIENT_HELLO *client_hello) -> ssl_select_cert_result_t {
        static const uint8_t kContext[] = {3};

        if (!SSL_set_session_id_context(client_hello->ssl, kContext,
                                        sizeof(kContext))) {
          return ssl_select_cert_error;
        }

        return ssl_select_cert_success;
      });

  if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                           false /* expect session not reused */)) {
    fprintf(stderr,
            "Error connecting with a context switch on early callback.\n");
    return false;
  }

  return true;
}

static timeval g_current_time;

static void CurrentTimeCallback(const SSL *ssl, timeval *out_clock) {
  *out_clock = g_current_time;
}

static void FrozenTimeCallback(const SSL *ssl, timeval *out_clock) {
  out_clock->tv_sec = 1000;
  out_clock->tv_usec = 0;
}

static int RenewTicketCallback(SSL *ssl, uint8_t *key_name, uint8_t *iv,
                               EVP_CIPHER_CTX *ctx, HMAC_CTX *hmac_ctx,
                               int encrypt) {
  static const uint8_t kZeros[16] = {0};

  if (encrypt) {
    OPENSSL_memcpy(key_name, kZeros, sizeof(kZeros));
    RAND_bytes(iv, 16);
  } else if (OPENSSL_memcmp(key_name, kZeros, 16) != 0) {
    return 0;
  }

  if (!HMAC_Init_ex(hmac_ctx, kZeros, sizeof(kZeros), EVP_sha256(), NULL) ||
      !EVP_CipherInit_ex(ctx, EVP_aes_128_cbc(), NULL, kZeros, iv, encrypt)) {
    return -1;
  }

  // Returning two from the callback in decrypt mode renews the
  // session in TLS 1.2 and below.
  return encrypt ? 1 : 2;
}

static bool GetServerTicketTime(long *out, const SSL_SESSION *session) {
  if (session->tlsext_ticklen < 16 + 16 + SHA256_DIGEST_LENGTH) {
    return false;
  }

  const uint8_t *ciphertext = session->tlsext_tick + 16 + 16;
  size_t len = session->tlsext_ticklen - 16 - 16 - SHA256_DIGEST_LENGTH;
  std::unique_ptr<uint8_t[]> plaintext(new uint8_t[len]);

#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  // Fuzzer-mode tickets are unencrypted.
  OPENSSL_memcpy(plaintext.get(), ciphertext, len);
#else
  static const uint8_t kZeros[16] = {0};
  const uint8_t *iv = session->tlsext_tick + 16;
  bssl::ScopedEVP_CIPHER_CTX ctx;
  int len1, len2;
  if (!EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, kZeros, iv) ||
      !EVP_DecryptUpdate(ctx.get(), plaintext.get(), &len1, ciphertext, len) ||
      !EVP_DecryptFinal_ex(ctx.get(), plaintext.get() + len1, &len2)) {
    return false;
  }

  len = static_cast<size_t>(len1 + len2);
#endif

  bssl::UniquePtr<SSL_CTX> ssl_ctx(SSL_CTX_new(TLS_method()));
  if (!ssl_ctx) {
    return false;
  }
  bssl::UniquePtr<SSL_SESSION> server_session(
      SSL_SESSION_from_bytes(plaintext.get(), len, ssl_ctx.get()));
  if (!server_session) {
    return false;
  }

  *out = server_session->time;
  return true;
}

static bool TestSessionTimeout(bool is_dtls, const SSL_METHOD *method,
                               uint16_t version) {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  for (bool server_test : std::vector<bool>{false, true}) {
    static const time_t kStartTime = 1000;
    g_current_time.tv_sec = kStartTime;

    // We are willing to use a longer lifetime for TLS 1.3 sessions as
    // resumptions still perform ECDHE.
    const time_t timeout = version == TLS1_3_VERSION
                               ? SSL_DEFAULT_SESSION_PSK_DHE_TIMEOUT
                               : SSL_DEFAULT_SESSION_TIMEOUT;

    bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(method));
    bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(method));
    if (!server_ctx || !client_ctx ||
        !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
        !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()) ||
        !SSL_CTX_set_min_proto_version(client_ctx.get(), version) ||
        !SSL_CTX_set_max_proto_version(client_ctx.get(), version) ||
        !SSL_CTX_set_min_proto_version(server_ctx.get(), version) ||
        !SSL_CTX_set_max_proto_version(server_ctx.get(), version)) {
      return false;
    }

    SSL_CTX_set_session_cache_mode(client_ctx.get(), SSL_SESS_CACHE_BOTH);
    SSL_CTX_set_session_cache_mode(server_ctx.get(), SSL_SESS_CACHE_BOTH);

    // Both client and server must enforce session timeouts. We configure the
    // other side with a frozen clock so it never expires tickets.
    if (server_test) {
      SSL_CTX_set_current_time_cb(client_ctx.get(), FrozenTimeCallback);
      SSL_CTX_set_current_time_cb(server_ctx.get(), CurrentTimeCallback);
    } else {
      SSL_CTX_set_current_time_cb(client_ctx.get(), CurrentTimeCallback);
      SSL_CTX_set_current_time_cb(server_ctx.get(), FrozenTimeCallback);
    }

    // Configure a ticket callback which renews tickets.
    SSL_CTX_set_tlsext_ticket_key_cb(server_ctx.get(), RenewTicketCallback);

    bssl::UniquePtr<SSL_SESSION> session =
        CreateClientSession(client_ctx.get(), server_ctx.get());
    if (!session) {
      fprintf(stderr, "Error getting session.\n");
      return false;
    }

    // Advance the clock just behind the timeout.
    g_current_time.tv_sec += timeout - 1;

    if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                             true /* expect session reused */)) {
      fprintf(stderr, "Error resuming session.\n");
      return false;
    }

    // Advance the clock one more second.
    g_current_time.tv_sec++;

    if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                             false /* expect session not reused */)) {
      fprintf(stderr, "Error resuming session.\n");
      return false;
    }

    // Rewind the clock to before the session was minted.
    g_current_time.tv_sec = kStartTime - 1;

    if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                             false /* expect session not reused */)) {
      fprintf(stderr, "Error resuming session.\n");
      return false;
    }

    // SSL 3.0 cannot renew sessions.
    if (version == SSL3_VERSION) {
      continue;
    }

    // Renew the session 10 seconds before expiration.
    time_t new_start_time = kStartTime + timeout - 10;
    g_current_time.tv_sec = new_start_time;
    bssl::UniquePtr<SSL_SESSION> new_session =
        ExpectSessionRenewed(client_ctx.get(), server_ctx.get(), session.get());
    if (!new_session) {
      fprintf(stderr, "Error renewing session.\n");
      return false;
    }

    // This new session is not the same object as before.
    if (session.get() == new_session.get()) {
      fprintf(stderr, "New and old sessions alias.\n");
      return false;
    }

    // Check the sessions have timestamps measured from issuance.
    long session_time = 0;
    if (server_test) {
      if (!GetServerTicketTime(&session_time, new_session.get())) {
        fprintf(stderr, "Failed to decode session ticket.\n");
        return false;
      }
    } else {
      session_time = new_session->time;
    }

    if (session_time != g_current_time.tv_sec) {
      fprintf(stderr, "New session is not measured from issuance.\n");
      return false;
    }

    if (version == TLS1_3_VERSION) {
      // Renewal incorporates fresh key material in TLS 1.3, so we extend the
      // lifetime TLS 1.3.
      g_current_time.tv_sec = new_start_time + timeout - 1;
      if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(),
                               new_session.get(),
                               true /* expect session reused */)) {
        fprintf(stderr, "Error resuming renewed session.\n");
        return false;
      }

      // The new session expires after the new timeout.
      g_current_time.tv_sec = new_start_time + timeout + 1;
      if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(),
                               new_session.get(),
                               false /* expect session ot reused */)) {
        fprintf(stderr, "Renewed session's lifetime is too long.\n");
        return false;
      }

      // Renew the session until it begins just past the auth timeout.
      time_t auth_end_time = kStartTime + SSL_DEFAULT_SESSION_AUTH_TIMEOUT;
      while (new_start_time < auth_end_time - 1000) {
        // Get as close as possible to target start time.
        new_start_time =
            std::min(auth_end_time - 1000, new_start_time + timeout - 1);
        g_current_time.tv_sec = new_start_time;
        new_session = ExpectSessionRenewed(client_ctx.get(), server_ctx.get(),
                                           new_session.get());
        if (!new_session) {
          fprintf(stderr, "Error renewing session.\n");
          return false;
        }
      }

      // Now the session's lifetime is bound by the auth timeout.
      g_current_time.tv_sec = auth_end_time - 1;
      if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(),
                               new_session.get(),
                               true /* expect session reused */)) {
        fprintf(stderr, "Error resuming renewed session.\n");
        return false;
      }

      g_current_time.tv_sec = auth_end_time + 1;
      if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(),
                               new_session.get(),
                               false /* expect session ot reused */)) {
        fprintf(stderr, "Renewed session's lifetime is too long.\n");
        return false;
      }
    } else {
      // The new session is usable just before the old expiration.
      g_current_time.tv_sec = kStartTime + timeout - 1;
      if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(),
                               new_session.get(),
                               true /* expect session reused */)) {
        fprintf(stderr, "Error resuming renewed session.\n");
        return false;
      }

      // Renewal does not extend the lifetime, so it is not usable beyond the
      // old expiration.
      g_current_time.tv_sec = kStartTime + timeout + 1;
      if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(),
                               new_session.get(),
                               false /* expect session not reused */)) {
        fprintf(stderr, "Renewed session's lifetime is too long.\n");
        return false;
      }
    }
  }

  return true;
}

static int SwitchContext(SSL *ssl, int *out_alert, void *arg) {
  SSL_CTX *ctx = reinterpret_cast<SSL_CTX*>(arg);
  SSL_set_SSL_CTX(ssl, ctx);
  return SSL_TLSEXT_ERR_OK;
}

static bool TestSNICallback(bool is_dtls, const SSL_METHOD *method,
                            uint16_t version) {
  // SSL 3.0 lacks extensions.
  if (version == SSL3_VERSION) {
    return true;
  }

  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  bssl::UniquePtr<X509> cert2 = GetECDSATestCertificate();
  bssl::UniquePtr<EVP_PKEY> key2 = GetECDSATestKey();
  if (!cert || !key || !cert2 || !key2) {
    return false;
  }

  // Test that switching the |SSL_CTX| at the SNI callback behaves correctly.
  static const uint16_t kECDSAWithSHA256 = SSL_SIGN_ECDSA_SECP256R1_SHA256;

  static const uint8_t kSCTList[] = {0, 6, 0, 4, 5, 6, 7, 8};
  static const uint8_t kOCSPResponse[] = {1, 2, 3, 4};

  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(method));
  bssl::UniquePtr<SSL_CTX> server_ctx2(SSL_CTX_new(method));
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(method));
  if (!server_ctx || !server_ctx2 || !client_ctx ||
      !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()) ||
      !SSL_CTX_use_certificate(server_ctx2.get(), cert2.get()) ||
      !SSL_CTX_use_PrivateKey(server_ctx2.get(), key2.get()) ||
      !SSL_CTX_set_signed_cert_timestamp_list(server_ctx2.get(), kSCTList,
                                              sizeof(kSCTList)) ||
      !SSL_CTX_set_ocsp_response(server_ctx2.get(), kOCSPResponse,
                                 sizeof(kOCSPResponse)) ||
      // Historically signing preferences would be lost in some cases with the
      // SNI callback, which triggers the TLS 1.2 SHA-1 default. To ensure
      // this doesn't happen when |version| is TLS 1.2, configure the private
      // key to only sign SHA-256.
      !SSL_CTX_set_signing_algorithm_prefs(server_ctx2.get(), &kECDSAWithSHA256,
                                           1) ||
      !SSL_CTX_set_min_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_min_proto_version(server_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(server_ctx.get(), version) ||
      !SSL_CTX_set_min_proto_version(server_ctx2.get(), version) ||
      !SSL_CTX_set_max_proto_version(server_ctx2.get(), version)) {
    return false;
  }

  SSL_CTX_set_tlsext_servername_callback(server_ctx.get(), SwitchContext);
  SSL_CTX_set_tlsext_servername_arg(server_ctx.get(), server_ctx2.get());

  SSL_CTX_enable_signed_cert_timestamps(client_ctx.get());
  SSL_CTX_enable_ocsp_stapling(client_ctx.get());

  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, client_ctx.get(),
                              server_ctx.get(), nullptr)) {
    fprintf(stderr, "Handshake failed.\n");
    return false;
  }

  // The client should have received |cert2|.
  bssl::UniquePtr<X509> peer(SSL_get_peer_certificate(client.get()));
  if (!peer || X509_cmp(peer.get(), cert2.get()) != 0) {
    fprintf(stderr, "Incorrect certificate received.\n");
    return false;
  }

  // The client should have received |server_ctx2|'s SCT list.
  const uint8_t *data;
  size_t len;
  SSL_get0_signed_cert_timestamp_list(client.get(), &data, &len);
  if (Bytes(kSCTList) != Bytes(data, len)) {
    fprintf(stderr, "Incorrect SCT list received.\n");
    return false;
  }

  // The client should have received |server_ctx2|'s OCSP response.
  SSL_get0_ocsp_response(client.get(), &data, &len);
  if (Bytes(kOCSPResponse) != Bytes(data, len)) {
    fprintf(stderr, "Incorrect OCSP response received.\n");
    return false;
  }

  return true;
}

// Test that the early callback can swap the maximum version.
TEST(SSLTest, EarlyCallbackVersionSwitch) {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_method()));
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(cert);
  ASSERT_TRUE(key);
  ASSERT_TRUE(server_ctx);
  ASSERT_TRUE(client_ctx);
  ASSERT_TRUE(SSL_CTX_use_certificate(server_ctx.get(), cert.get()));
  ASSERT_TRUE(SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()));
  ASSERT_TRUE(SSL_CTX_set_max_proto_version(client_ctx.get(), TLS1_3_VERSION));
  ASSERT_TRUE(SSL_CTX_set_max_proto_version(server_ctx.get(), TLS1_3_VERSION));

  SSL_CTX_set_select_certificate_cb(
      server_ctx.get(),
      [](const SSL_CLIENT_HELLO *client_hello) -> ssl_select_cert_result_t {
        if (!SSL_set_max_proto_version(client_hello->ssl, TLS1_2_VERSION)) {
          return ssl_select_cert_error;
        }

        return ssl_select_cert_success;
      });

  bssl::UniquePtr<SSL> client, server;
  ASSERT_TRUE(ConnectClientAndServer(&client, &server, client_ctx.get(),
                                     server_ctx.get(), nullptr));
  EXPECT_EQ(TLS1_2_VERSION, SSL_version(client.get()));
}

TEST(SSLTest, SetVersion) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(ctx);

  // Set valid TLS versions.
  EXPECT_TRUE(SSL_CTX_set_max_proto_version(ctx.get(), TLS1_VERSION));
  EXPECT_TRUE(SSL_CTX_set_max_proto_version(ctx.get(), TLS1_1_VERSION));
  EXPECT_TRUE(SSL_CTX_set_min_proto_version(ctx.get(), TLS1_VERSION));
  EXPECT_TRUE(SSL_CTX_set_min_proto_version(ctx.get(), TLS1_1_VERSION));

  // Invalid TLS versions are rejected.
  EXPECT_FALSE(SSL_CTX_set_max_proto_version(ctx.get(), DTLS1_VERSION));
  EXPECT_FALSE(SSL_CTX_set_max_proto_version(ctx.get(), 0x0200));
  EXPECT_FALSE(SSL_CTX_set_max_proto_version(ctx.get(), 0x1234));
  EXPECT_FALSE(SSL_CTX_set_min_proto_version(ctx.get(), DTLS1_VERSION));
  EXPECT_FALSE(SSL_CTX_set_min_proto_version(ctx.get(), 0x0200));
  EXPECT_FALSE(SSL_CTX_set_min_proto_version(ctx.get(), 0x1234));

  // Zero is the default version.
  EXPECT_TRUE(SSL_CTX_set_max_proto_version(ctx.get(), 0));
  EXPECT_EQ(TLS1_2_VERSION, ctx->max_version);
  EXPECT_TRUE(SSL_CTX_set_min_proto_version(ctx.get(), 0));
  EXPECT_EQ(TLS1_VERSION, ctx->min_version);

  // SSL 3.0 and TLS 1.3 are available, but not by default.
  EXPECT_TRUE(SSL_CTX_set_min_proto_version(ctx.get(), SSL3_VERSION));
  EXPECT_EQ(SSL3_VERSION, ctx->min_version);
  EXPECT_TRUE(SSL_CTX_set_max_proto_version(ctx.get(), TLS1_3_VERSION));
  EXPECT_EQ(TLS1_3_VERSION, ctx->max_version);

  ctx.reset(SSL_CTX_new(DTLS_method()));
  ASSERT_TRUE(ctx);

  EXPECT_TRUE(SSL_CTX_set_max_proto_version(ctx.get(), DTLS1_VERSION));
  EXPECT_TRUE(SSL_CTX_set_max_proto_version(ctx.get(), DTLS1_2_VERSION));
  EXPECT_TRUE(SSL_CTX_set_min_proto_version(ctx.get(), DTLS1_VERSION));
  EXPECT_TRUE(SSL_CTX_set_min_proto_version(ctx.get(), DTLS1_2_VERSION));

  EXPECT_FALSE(SSL_CTX_set_max_proto_version(ctx.get(), TLS1_VERSION));
  EXPECT_FALSE(SSL_CTX_set_max_proto_version(ctx.get(), 0xfefe /* DTLS 1.1 */));
  EXPECT_FALSE(SSL_CTX_set_max_proto_version(ctx.get(), 0xfffe /* DTLS 0.1 */));
  EXPECT_FALSE(SSL_CTX_set_max_proto_version(ctx.get(), 0x1234));
  EXPECT_FALSE(SSL_CTX_set_min_proto_version(ctx.get(), TLS1_VERSION));
  EXPECT_FALSE(SSL_CTX_set_min_proto_version(ctx.get(), 0xfefe /* DTLS 1.1 */));
  EXPECT_FALSE(SSL_CTX_set_min_proto_version(ctx.get(), 0xfffe /* DTLS 0.1 */));
  EXPECT_FALSE(SSL_CTX_set_min_proto_version(ctx.get(), 0x1234));

  EXPECT_TRUE(SSL_CTX_set_max_proto_version(ctx.get(), 0));
  EXPECT_EQ(TLS1_2_VERSION, ctx->max_version);
  EXPECT_TRUE(SSL_CTX_set_min_proto_version(ctx.get(), 0));
  EXPECT_EQ(TLS1_1_VERSION, ctx->min_version);
}

static const char *GetVersionName(uint16_t version) {
  switch (version) {
    case SSL3_VERSION:
      return "SSLv3";
    case TLS1_VERSION:
      return "TLSv1";
    case TLS1_1_VERSION:
      return "TLSv1.1";
    case TLS1_2_VERSION:
      return "TLSv1.2";
    case TLS1_3_VERSION:
      return "TLSv1.3";
    case DTLS1_VERSION:
      return "DTLSv1";
    case DTLS1_2_VERSION:
      return "DTLSv1.2";
    default:
      return "???";
  }
}

static bool TestVersion(bool is_dtls, const SSL_METHOD *method,
                        uint16_t version) {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(method));
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(method));
  bssl::UniquePtr<SSL> client, server;
  if (!server_ctx || !client_ctx ||
      !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()) ||
      !SSL_CTX_set_min_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_min_proto_version(server_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(server_ctx.get(), version) ||
      !ConnectClientAndServer(&client, &server, client_ctx.get(),
                              server_ctx.get(), nullptr /* no session */)) {
    fprintf(stderr, "Failed to connect.\n");
    return false;
  }

  if (SSL_version(client.get()) != version ||
      SSL_version(server.get()) != version) {
    fprintf(stderr, "Version mismatch. Got %04x and %04x, wanted %04x.\n",
            SSL_version(client.get()), SSL_version(server.get()), version);
    return false;
  }

  // Test the version name is reported as expected.
  const char *version_name = GetVersionName(version);
  if (strcmp(version_name, SSL_get_version(client.get())) != 0 ||
      strcmp(version_name, SSL_get_version(server.get())) != 0) {
    fprintf(stderr, "Version name mismatch. Got '%s' and '%s', wanted '%s'.\n",
            SSL_get_version(client.get()), SSL_get_version(server.get()),
            version_name);
    return false;
  }

  // Test SSL_SESSION reports the same name.
  const char *client_name =
      SSL_SESSION_get_version(SSL_get_session(client.get()));
  const char *server_name =
      SSL_SESSION_get_version(SSL_get_session(server.get()));
  if (strcmp(version_name, client_name) != 0 ||
      strcmp(version_name, server_name) != 0) {
    fprintf(stderr,
            "Session version name mismatch. Got '%s' and '%s', wanted '%s'.\n",
            client_name, server_name, version_name);
    return false;
  }

  return true;
}

// Tests that that |SSL_get_pending_cipher| is available during the ALPN
// selection callback.
static bool TestALPNCipherAvailable(bool is_dtls, const SSL_METHOD *method,
                                    uint16_t version) {
  // SSL 3.0 lacks extensions.
  if (version == SSL3_VERSION) {
    return true;
  }

  static const uint8_t kALPNProtos[] = {0x03, 'f', 'o', 'o'};

  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(method));
  if (!ctx || !SSL_CTX_use_certificate(ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(ctx.get(), key.get()) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(ctx.get(), version) ||
      SSL_CTX_set_alpn_protos(ctx.get(), kALPNProtos, sizeof(kALPNProtos)) !=
          0) {
    return false;
  }

  // The ALPN callback does not fail the handshake on error, so have the
  // callback write a boolean.
  std::pair<uint16_t, bool> callback_state(version, false);
  SSL_CTX_set_alpn_select_cb(
      ctx.get(),
      [](SSL *ssl, const uint8_t **out, uint8_t *out_len, const uint8_t *in,
         unsigned in_len, void *arg) -> int {
        auto state = reinterpret_cast<std::pair<uint16_t, bool> *>(arg);
        if (SSL_get_pending_cipher(ssl) != nullptr &&
            SSL_version(ssl) == state->first) {
          state->second = true;
        }
        return SSL_TLSEXT_ERR_NOACK;
      },
      &callback_state);

  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, ctx.get(), ctx.get(),
                              nullptr /* no session */)) {
    return false;
  }

  if (!callback_state.second) {
    fprintf(stderr, "The pending cipher was not known in the ALPN callback.\n");
    return false;
  }

  return true;
}

static bool TestSSLClearSessionResumption(bool is_dtls,
                                          const SSL_METHOD *method,
                                          uint16_t version) {
  // Skip this for TLS 1.3. TLS 1.3's ticket mechanism is incompatible with this
  // API pattern.
  if (version == TLS1_3_VERSION) {
    return true;
  }

  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(method));
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(method));
  if (!cert || !key || !server_ctx || !client_ctx ||
      !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()) ||
      !SSL_CTX_set_min_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(client_ctx.get(), version) ||
      !SSL_CTX_set_min_proto_version(server_ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(server_ctx.get(), version)) {
    return false;
  }

  // Connect a client and a server.
  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, client_ctx.get(),
                              server_ctx.get(), nullptr /* no session */)) {
    return false;
  }

  if (SSL_session_reused(client.get()) ||
      SSL_session_reused(server.get())) {
    fprintf(stderr, "Session unexpectedly reused.\n");
    return false;
  }

  // Reset everything.
  if (!SSL_clear(client.get()) ||
      !SSL_clear(server.get())) {
    fprintf(stderr, "SSL_clear failed.\n");
    return false;
  }

  // Attempt to connect a second time.
  if (!CompleteHandshakes(client.get(), server.get())) {
    fprintf(stderr, "Could not reuse SSL objects.\n");
    return false;
  }

  // |SSL_clear| should implicitly offer the previous session to the server.
  if (!SSL_session_reused(client.get()) ||
      !SSL_session_reused(server.get())) {
    fprintf(stderr, "Session was not reused in second try.\n");
    return false;
  }

  return true;
}

static bool ChainsEqual(STACK_OF(X509) *chain,
                         const std::vector<X509 *> &expected) {
  if (sk_X509_num(chain) != expected.size()) {
    return false;
  }

  for (size_t i = 0; i < expected.size(); i++) {
    if (X509_cmp(sk_X509_value(chain, i), expected[i]) != 0) {
      return false;
    }
  }

  return true;
}

static bool TestAutoChain(bool is_dtls, const SSL_METHOD *method,
                          uint16_t version) {
  bssl::UniquePtr<X509> cert = GetChainTestCertificate();
  bssl::UniquePtr<X509> intermediate = GetChainTestIntermediate();
  bssl::UniquePtr<EVP_PKEY> key = GetChainTestKey();
  if (!cert || !intermediate || !key) {
    return false;
  }

  // Configure both client and server to accept any certificate. Add
  // |intermediate| to the cert store.
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(method));
  if (!ctx ||
      !SSL_CTX_use_certificate(ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(ctx.get(), key.get()) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), version) ||
      !SSL_CTX_set_max_proto_version(ctx.get(), version) ||
      !X509_STORE_add_cert(SSL_CTX_get_cert_store(ctx.get()),
                           intermediate.get())) {
    return false;
  }
  SSL_CTX_set_verify(
      ctx.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
  SSL_CTX_set_cert_verify_callback(ctx.get(), VerifySucceed, NULL);

  // By default, the client and server should each only send the leaf.
  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, ctx.get(), ctx.get(),
                              nullptr /* no session */)) {
    return false;
  }

  if (!ChainsEqual(SSL_get_peer_full_cert_chain(client.get()), {cert.get()})) {
    fprintf(stderr, "Client-received chain did not match.\n");
    return false;
  }

  if (!ChainsEqual(SSL_get_peer_full_cert_chain(server.get()), {cert.get()})) {
    fprintf(stderr, "Server-received chain did not match.\n");
    return false;
  }

  // If auto-chaining is enabled, then the intermediate is sent.
  SSL_CTX_clear_mode(ctx.get(), SSL_MODE_NO_AUTO_CHAIN);
  if (!ConnectClientAndServer(&client, &server, ctx.get(), ctx.get(),
                              nullptr /* no session */)) {
    return false;
  }

  if (!ChainsEqual(SSL_get_peer_full_cert_chain(client.get()),
                   {cert.get(), intermediate.get()})) {
    fprintf(stderr, "Client-received chain did not match (auto-chaining).\n");
    return false;
  }

  if (!ChainsEqual(SSL_get_peer_full_cert_chain(server.get()),
                   {cert.get(), intermediate.get()})) {
    fprintf(stderr, "Server-received chain did not match (auto-chaining).\n");
    return false;
  }

  // Auto-chaining does not override explicitly-configured intermediates.
  if (!SSL_CTX_add1_chain_cert(ctx.get(), cert.get()) ||
      !ConnectClientAndServer(&client, &server, ctx.get(), ctx.get(),
                              nullptr /* no session */)) {
    return false;
  }

  if (!ChainsEqual(SSL_get_peer_full_cert_chain(client.get()),
                   {cert.get(), cert.get()})) {
    fprintf(stderr,
            "Client-received chain did not match (auto-chaining, explicit "
            "intermediate).\n");
    return false;
  }

  if (!ChainsEqual(SSL_get_peer_full_cert_chain(server.get()),
                   {cert.get(), cert.get()})) {
    fprintf(stderr,
            "Server-received chain did not match (auto-chaining, explicit "
            "intermediate).\n");
    return false;
  }

  return true;
}

static bool ExpectBadWriteRetry() {
  int err = ERR_get_error();
  if (ERR_GET_LIB(err) != ERR_LIB_SSL ||
      ERR_GET_REASON(err) != SSL_R_BAD_WRITE_RETRY) {
    char buf[ERR_ERROR_STRING_BUF_LEN];
    ERR_error_string_n(err, buf, sizeof(buf));
    fprintf(stderr, "Wanted SSL_R_BAD_WRITE_RETRY, got: %s.\n", buf);
    return false;
  }

  if (ERR_peek_error() != 0) {
    fprintf(stderr, "Unexpected error following SSL_R_BAD_WRITE_RETRY.\n");
    return false;
  }

  return true;
}

static bool TestSSLWriteRetry(bool is_dtls, const SSL_METHOD *method,
                              uint16_t version) {
  if (is_dtls) {
    return true;
  }

  for (bool enable_partial_write : std::vector<bool>{false, true}) {
    // Connect a client and server.
    bssl::UniquePtr<X509> cert = GetTestCertificate();
    bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(method));
    bssl::UniquePtr<SSL> client, server;
    if (!cert || !key || !ctx ||
        !SSL_CTX_use_certificate(ctx.get(), cert.get()) ||
        !SSL_CTX_use_PrivateKey(ctx.get(), key.get()) ||
        !SSL_CTX_set_min_proto_version(ctx.get(), version) ||
        !SSL_CTX_set_max_proto_version(ctx.get(), version) ||
        !ConnectClientAndServer(&client, &server, ctx.get(), ctx.get(),
                                nullptr /* no session */)) {
      return false;
    }

    if (enable_partial_write) {
      SSL_set_mode(client.get(), SSL_MODE_ENABLE_PARTIAL_WRITE);
    }

    // Write without reading until the buffer is full and we have an unfinished
    // write. Keep a count so we may reread it again later. "hello!" will be
    // written in two chunks, "hello" and "!".
    char data[] = "hello!";
    static const int kChunkLen = 5;  // The length of "hello".
    unsigned count = 0;
    for (;;) {
      int ret = SSL_write(client.get(), data, kChunkLen);
      if (ret <= 0) {
        int err = SSL_get_error(client.get(), ret);
        if (SSL_get_error(client.get(), ret) == SSL_ERROR_WANT_WRITE) {
          break;
        }
        fprintf(stderr, "SSL_write failed in unexpected way: %d\n", err);
        return false;
      }

      if (ret != 5) {
        fprintf(stderr, "SSL_write wrote %d bytes, expected 5.\n", ret);
        return false;
      }

      count++;
    }

    // Retrying with the same parameters is legal.
    if (SSL_get_error(client.get(), SSL_write(client.get(), data, kChunkLen)) !=
        SSL_ERROR_WANT_WRITE) {
      fprintf(stderr, "SSL_write retry unexpectedly failed.\n");
      return false;
    }

    // Retrying with the same buffer but shorter length is not legal.
    if (SSL_get_error(client.get(),
                      SSL_write(client.get(), data, kChunkLen - 1)) !=
            SSL_ERROR_SSL ||
        !ExpectBadWriteRetry()) {
      fprintf(stderr, "SSL_write retry did not fail as expected.\n");
      return false;
    }

    // Retrying with a different buffer pointer is not legal.
    char data2[] = "hello";
    if (SSL_get_error(client.get(), SSL_write(client.get(), data2,
                                              kChunkLen)) != SSL_ERROR_SSL ||
        !ExpectBadWriteRetry()) {
      fprintf(stderr, "SSL_write retry did not fail as expected.\n");
      return false;
    }

    // With |SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER|, the buffer may move.
    SSL_set_mode(client.get(), SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    if (SSL_get_error(client.get(),
                      SSL_write(client.get(), data2, kChunkLen)) !=
        SSL_ERROR_WANT_WRITE) {
      fprintf(stderr, "SSL_write retry unexpectedly failed.\n");
      return false;
    }

    // |SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER| does not disable length checks.
    if (SSL_get_error(client.get(),
                      SSL_write(client.get(), data2, kChunkLen - 1)) !=
            SSL_ERROR_SSL ||
        !ExpectBadWriteRetry()) {
      fprintf(stderr, "SSL_write retry did not fail as expected.\n");
      return false;
    }

    // Retrying with a larger buffer is legal.
    if (SSL_get_error(client.get(),
                      SSL_write(client.get(), data, kChunkLen + 1)) !=
        SSL_ERROR_WANT_WRITE) {
      fprintf(stderr, "SSL_write retry unexpectedly failed.\n");
      return false;
    }

    // Drain the buffer.
    char buf[20];
    for (unsigned i = 0; i < count; i++) {
      if (SSL_read(server.get(), buf, sizeof(buf)) != kChunkLen ||
          OPENSSL_memcmp(buf, "hello", kChunkLen) != 0) {
        fprintf(stderr, "Failed to read initial records.\n");
        return false;
      }
    }

    // Now that there is space, a retry with a larger buffer should flush the
    // pending record, skip over that many bytes of input (on assumption they
    // are the same), and write the remainder. If SSL_MODE_ENABLE_PARTIAL_WRITE
    // is set, this will complete in two steps.
    char data3[] = "_____!";
    if (enable_partial_write) {
      if (SSL_write(client.get(), data3, kChunkLen + 1) != kChunkLen ||
          SSL_write(client.get(), data3 + kChunkLen, 1) != 1) {
        fprintf(stderr, "SSL_write retry failed.\n");
        return false;
      }
    } else if (SSL_write(client.get(), data3, kChunkLen + 1) != kChunkLen + 1) {
      fprintf(stderr, "SSL_write retry failed.\n");
      return false;
    }

    // Check the last write was correct. The data will be spread over two
    // records, so SSL_read returns twice.
    if (SSL_read(server.get(), buf, sizeof(buf)) != kChunkLen ||
        OPENSSL_memcmp(buf, "hello", kChunkLen) != 0 ||
        SSL_read(server.get(), buf, sizeof(buf)) != 1 ||
        buf[0] != '!') {
      fprintf(stderr, "Failed to read write retry.\n");
      return false;
    }
  }

  return true;
}

static bool ForEachVersion(bool (*test_func)(bool is_dtls,
                                             const SSL_METHOD *method,
                                             uint16_t version)) {
  static uint16_t kTLSVersions[] = {
      SSL3_VERSION,
      TLS1_VERSION,
      TLS1_1_VERSION,
      TLS1_2_VERSION,
// TLS 1.3 requires RSA-PSS, which is disabled for Android system builds.
#if !defined(BORINGSSL_ANDROID_SYSTEM)
      TLS1_3_VERSION,
#endif
  };

  static uint16_t kDTLSVersions[] = {
      DTLS1_VERSION, DTLS1_2_VERSION,
  };

  for (uint16_t version : kTLSVersions) {
    if (!test_func(false, TLS_method(), version)) {
      fprintf(stderr, "Test failed at TLS version %04x.\n", version);
      return false;
    }
  }

  for (uint16_t version : kDTLSVersions) {
    if (!test_func(true, DTLS_method(), version)) {
      fprintf(stderr, "Test failed at DTLS version %04x.\n", version);
      return false;
    }
  }

  return true;
}

TEST(SSLTest, AddChainCertHack) {
  // Ensure that we don't accidently break the hack that we have in place to
  // keep curl and serf happy when they use an |X509| even after transfering
  // ownership.

  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(ctx);
  X509 *cert = GetTestCertificate().release();
  ASSERT_TRUE(cert);
  SSL_CTX_add0_chain_cert(ctx.get(), cert);

  // This should not trigger a use-after-free.
  X509_cmp(cert, cert);
}

TEST(SSLTest, GetCertificate) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(ctx);
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  ASSERT_TRUE(cert);
  ASSERT_TRUE(SSL_CTX_use_certificate(ctx.get(), cert.get()));
  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  ASSERT_TRUE(ssl);

  X509 *cert2 = SSL_CTX_get0_certificate(ctx.get());
  ASSERT_TRUE(cert2);
  X509 *cert3 = SSL_get_certificate(ssl.get());
  ASSERT_TRUE(cert3);

  // The old and new certificates must be identical.
  EXPECT_EQ(0, X509_cmp(cert.get(), cert2));
  EXPECT_EQ(0, X509_cmp(cert.get(), cert3));

  uint8_t *der = nullptr;
  long der_len = i2d_X509(cert.get(), &der);
  ASSERT_LT(0, der_len);
  bssl::UniquePtr<uint8_t> free_der(der);

  uint8_t *der2 = nullptr;
  long der2_len = i2d_X509(cert2, &der2);
  ASSERT_LT(0, der2_len);
  bssl::UniquePtr<uint8_t> free_der2(der2);

  uint8_t *der3 = nullptr;
  long der3_len = i2d_X509(cert3, &der3);
  ASSERT_LT(0, der3_len);
  bssl::UniquePtr<uint8_t> free_der3(der3);

  // They must also encode identically.
  EXPECT_EQ(Bytes(der, der_len), Bytes(der2, der2_len));
  EXPECT_EQ(Bytes(der, der_len), Bytes(der3, der3_len));
}

TEST(SSLTest, SetChainAndKeyMismatch) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_with_buffers_method()));
  ASSERT_TRUE(ctx);

  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  ASSERT_TRUE(key);
  bssl::UniquePtr<CRYPTO_BUFFER> leaf = GetChainTestCertificateBuffer();
  ASSERT_TRUE(leaf);
  std::vector<CRYPTO_BUFFER*> chain = {
      leaf.get(),
  };

  // Should fail because |GetTestKey| doesn't match the chain-test certificate.
  ASSERT_FALSE(SSL_CTX_set_chain_and_key(ctx.get(), &chain[0], chain.size(),
                                         key.get(), nullptr));
  ERR_clear_error();
}

TEST(SSLTest, SetChainAndKey) {
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_with_buffers_method()));
  ASSERT_TRUE(client_ctx);
  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_with_buffers_method()));
  ASSERT_TRUE(server_ctx);

  bssl::UniquePtr<EVP_PKEY> key = GetChainTestKey();
  ASSERT_TRUE(key);
  bssl::UniquePtr<CRYPTO_BUFFER> leaf = GetChainTestCertificateBuffer();
  ASSERT_TRUE(leaf);
  bssl::UniquePtr<CRYPTO_BUFFER> intermediate =
      GetChainTestIntermediateBuffer();
  ASSERT_TRUE(intermediate);
  std::vector<CRYPTO_BUFFER*> chain = {
      leaf.get(), intermediate.get(),
  };
  ASSERT_TRUE(SSL_CTX_set_chain_and_key(server_ctx.get(), &chain[0],
                                        chain.size(), key.get(), nullptr));

  SSL_CTX_i_promise_to_verify_certs_after_the_handshake(client_ctx.get());

  bssl::UniquePtr<SSL> client, server;
  ASSERT_TRUE(ConnectClientAndServer(&client, &server, client_ctx.get(),
                                     server_ctx.get(),
                                     nullptr /* no session */));
}

// Configuring the empty cipher list, though an error, should still modify the
// configuration.
TEST(SSLTest, EmptyCipherList) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(ctx);

  // Initially, the cipher list is not empty.
  EXPECT_NE(0u, sk_SSL_CIPHER_num(SSL_CTX_get_ciphers(ctx.get())));

  // Configuring the empty cipher list fails.
  EXPECT_FALSE(SSL_CTX_set_cipher_list(ctx.get(), ""));
  ERR_clear_error();

  // But the cipher list is still updated to empty.
  EXPECT_EQ(0u, sk_SSL_CIPHER_num(SSL_CTX_get_ciphers(ctx.get())));
}

// ssl_test_ticket_aead_failure_mode enumerates the possible ways in which the
// test |SSL_TICKET_AEAD_METHOD| can fail.
enum ssl_test_ticket_aead_failure_mode {
  ssl_test_ticket_aead_ok = 0,
  ssl_test_ticket_aead_seal_fail,
  ssl_test_ticket_aead_open_soft_fail,
  ssl_test_ticket_aead_open_hard_fail,
};

struct ssl_test_ticket_aead_state {
  unsigned retry_count;
  ssl_test_ticket_aead_failure_mode failure_mode;
};

static int ssl_test_ticket_aead_ex_index_dup(CRYPTO_EX_DATA *to,
                                             const CRYPTO_EX_DATA *from,
                                             void **from_d, int index,
                                             long argl, void *argp) {
  abort();
}

static void ssl_test_ticket_aead_ex_index_free(void *parent, void *ptr,
                                               CRYPTO_EX_DATA *ad, int index,
                                               long argl, void *argp) {
  auto state = reinterpret_cast<ssl_test_ticket_aead_state*>(ptr);
  if (state == nullptr) {
    return;
  }

  OPENSSL_free(state);
}

static CRYPTO_once_t g_ssl_test_ticket_aead_ex_index_once = CRYPTO_ONCE_INIT;
static int g_ssl_test_ticket_aead_ex_index;

static int ssl_test_ticket_aead_get_ex_index() {
  CRYPTO_once(&g_ssl_test_ticket_aead_ex_index_once, [] {
    g_ssl_test_ticket_aead_ex_index = SSL_get_ex_new_index(
        0, nullptr, nullptr, ssl_test_ticket_aead_ex_index_dup,
        ssl_test_ticket_aead_ex_index_free);
  });
  return g_ssl_test_ticket_aead_ex_index;
}

static size_t ssl_test_ticket_aead_max_overhead(SSL *ssl) {
  return 1;
}

static int ssl_test_ticket_aead_seal(SSL *ssl, uint8_t *out, size_t *out_len,
                                     size_t max_out_len, const uint8_t *in,
                                     size_t in_len) {
  auto state = reinterpret_cast<ssl_test_ticket_aead_state *>(
      SSL_get_ex_data(ssl, ssl_test_ticket_aead_get_ex_index()));

  if (state->failure_mode == ssl_test_ticket_aead_seal_fail ||
      max_out_len < in_len + 1) {
    return 0;
  }

  OPENSSL_memmove(out, in, in_len);
  out[in_len] = 0xff;
  *out_len = in_len + 1;

  return 1;
}

static ssl_ticket_aead_result_t ssl_test_ticket_aead_open(
    SSL *ssl, uint8_t *out, size_t *out_len, size_t max_out_len,
    const uint8_t *in, size_t in_len) {
  auto state = reinterpret_cast<ssl_test_ticket_aead_state *>(
      SSL_get_ex_data(ssl, ssl_test_ticket_aead_get_ex_index()));

  if (state->retry_count > 0) {
    state->retry_count--;
    return ssl_ticket_aead_retry;
  }

  switch (state->failure_mode) {
    case ssl_test_ticket_aead_ok:
      break;
    case ssl_test_ticket_aead_seal_fail:
      // If |seal| failed then there shouldn't be any ticket to try and
      // decrypt.
      abort();
      break;
    case ssl_test_ticket_aead_open_soft_fail:
      return ssl_ticket_aead_ignore_ticket;
    case ssl_test_ticket_aead_open_hard_fail:
      return ssl_ticket_aead_error;
  }

  if (in_len == 0 || in[in_len - 1] != 0xff) {
    return ssl_ticket_aead_ignore_ticket;
  }

  if (max_out_len < in_len - 1) {
    return ssl_ticket_aead_error;
  }

  OPENSSL_memmove(out, in, in_len - 1);
  *out_len = in_len - 1;
  return ssl_ticket_aead_success;
}

static const SSL_TICKET_AEAD_METHOD kSSLTestTicketMethod = {
  ssl_test_ticket_aead_max_overhead,
  ssl_test_ticket_aead_seal,
  ssl_test_ticket_aead_open,
};

static void ConnectClientAndServerWithTicketMethod(
    bssl::UniquePtr<SSL> *out_client, bssl::UniquePtr<SSL> *out_server,
    SSL_CTX *client_ctx, SSL_CTX *server_ctx, unsigned retry_count,
    ssl_test_ticket_aead_failure_mode failure_mode, SSL_SESSION *session) {
  bssl::UniquePtr<SSL> client(SSL_new(client_ctx)), server(SSL_new(server_ctx));
  ASSERT_TRUE(client);
  ASSERT_TRUE(server);
  SSL_set_connect_state(client.get());
  SSL_set_accept_state(server.get());

  auto state = reinterpret_cast<ssl_test_ticket_aead_state *>(
      OPENSSL_malloc(sizeof(ssl_test_ticket_aead_state)));
  ASSERT_TRUE(state);
  OPENSSL_memset(state, 0, sizeof(ssl_test_ticket_aead_state));
  state->retry_count = retry_count;
  state->failure_mode = failure_mode;

  ASSERT_TRUE(SSL_set_ex_data(server.get(), ssl_test_ticket_aead_get_ex_index(),
                              state));

  SSL_set_session(client.get(), session);

  BIO *bio1, *bio2;
  ASSERT_TRUE(BIO_new_bio_pair(&bio1, 0, &bio2, 0));

  // SSL_set_bio takes ownership.
  SSL_set_bio(client.get(), bio1, bio1);
  SSL_set_bio(server.get(), bio2, bio2);

  if (CompleteHandshakes(client.get(), server.get())) {
    *out_client = std::move(client);
    *out_server = std::move(server);
  } else {
    out_client->reset();
    out_server->reset();
  }
}

class TicketAEADMethodTest
    : public ::testing::TestWithParam<testing::tuple<
          uint16_t, unsigned, ssl_test_ticket_aead_failure_mode>> {};

TEST_P(TicketAEADMethodTest, Resume) {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  ASSERT_TRUE(cert);
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  ASSERT_TRUE(key);

  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(server_ctx);
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(client_ctx);

  const uint16_t version = testing::get<0>(GetParam());
  const unsigned retry_count = testing::get<1>(GetParam());
  const ssl_test_ticket_aead_failure_mode failure_mode =
      testing::get<2>(GetParam());

  ASSERT_TRUE(SSL_CTX_use_certificate(server_ctx.get(), cert.get()));
  ASSERT_TRUE(SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()));
  ASSERT_TRUE(SSL_CTX_set_min_proto_version(client_ctx.get(), version));
  ASSERT_TRUE(SSL_CTX_set_max_proto_version(client_ctx.get(), version));
  ASSERT_TRUE(SSL_CTX_set_min_proto_version(server_ctx.get(), version));
  ASSERT_TRUE(SSL_CTX_set_max_proto_version(server_ctx.get(), version));

  SSL_CTX_set_session_cache_mode(client_ctx.get(), SSL_SESS_CACHE_BOTH);
  SSL_CTX_set_session_cache_mode(server_ctx.get(), SSL_SESS_CACHE_BOTH);
  SSL_CTX_set_current_time_cb(client_ctx.get(), FrozenTimeCallback);
  SSL_CTX_set_current_time_cb(server_ctx.get(), FrozenTimeCallback);
  SSL_CTX_sess_set_new_cb(client_ctx.get(), SaveLastSession);

  SSL_CTX_set_ticket_aead_method(server_ctx.get(), &kSSLTestTicketMethod);

  bssl::UniquePtr<SSL> client, server;
  ConnectClientAndServerWithTicketMethod(&client, &server, client_ctx.get(),
                                         server_ctx.get(), retry_count,
                                         failure_mode, nullptr);
  switch (failure_mode) {
    case ssl_test_ticket_aead_ok:
    case ssl_test_ticket_aead_open_hard_fail:
    case ssl_test_ticket_aead_open_soft_fail:
      ASSERT_TRUE(client);
      break;
    case ssl_test_ticket_aead_seal_fail:
      EXPECT_FALSE(client);
      return;
  }
  EXPECT_FALSE(SSL_session_reused(client.get()));
  EXPECT_FALSE(SSL_session_reused(server.get()));

  // Run the read loop to account for post-handshake tickets in TLS 1.3.
  SSL_read(client.get(), nullptr, 0);

  bssl::UniquePtr<SSL_SESSION> session = std::move(g_last_session);
  ConnectClientAndServerWithTicketMethod(&client, &server, client_ctx.get(),
                                         server_ctx.get(), retry_count,
                                         failure_mode, session.get());
  switch (failure_mode) {
    case ssl_test_ticket_aead_ok:
      ASSERT_TRUE(client);
      EXPECT_TRUE(SSL_session_reused(client.get()));
      EXPECT_TRUE(SSL_session_reused(server.get()));
      break;
    case ssl_test_ticket_aead_seal_fail:
      abort();
      break;
    case ssl_test_ticket_aead_open_hard_fail:
      EXPECT_FALSE(client);
      break;
    case ssl_test_ticket_aead_open_soft_fail:
      ASSERT_TRUE(client);
      EXPECT_FALSE(SSL_session_reused(client.get()));
      EXPECT_FALSE(SSL_session_reused(server.get()));
  }
}

INSTANTIATE_TEST_CASE_P(
    TicketAEADMethodTests, TicketAEADMethodTest,
    testing::Combine(
        testing::Values(TLS1_2_VERSION, TLS1_3_VERSION),
        testing::Values(0, 1, 2),
        testing::Values(ssl_test_ticket_aead_ok,
                        ssl_test_ticket_aead_seal_fail,
                        ssl_test_ticket_aead_open_soft_fail,
                        ssl_test_ticket_aead_open_hard_fail)));

TEST(SSLTest, SSL3Method) {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  ASSERT_TRUE(cert);

  // For compatibility, SSLv3_method should work up to SSL_CTX_new and SSL_new.
  bssl::UniquePtr<SSL_CTX> ssl3_ctx(SSL_CTX_new(SSLv3_method()));
  ASSERT_TRUE(ssl3_ctx);
  ASSERT_TRUE(SSL_CTX_use_certificate(ssl3_ctx.get(), cert.get()));
  bssl::UniquePtr<SSL> ssl(SSL_new(ssl3_ctx.get()));
  EXPECT_TRUE(ssl);

  // Create a normal TLS context to test against.
  bssl::UniquePtr<SSL_CTX> tls_ctx(SSL_CTX_new(TLS_method()));
  ASSERT_TRUE(tls_ctx);
  ASSERT_TRUE(SSL_CTX_use_certificate(tls_ctx.get(), cert.get()));

  // However, handshaking an SSLv3_method server should fail to resolve the
  // version range. Explicit calls to SSL_CTX_set_min_proto_version are the only
  // way to enable SSL 3.0.
  bssl::UniquePtr<SSL> client, server;
  EXPECT_FALSE(ConnectClientAndServer(&client, &server, tls_ctx.get(),
                                      ssl3_ctx.get(),
                                      nullptr /* no session */));
  uint32_t err = ERR_get_error();
  EXPECT_EQ(ERR_LIB_SSL, ERR_GET_LIB(err));
  EXPECT_EQ(SSL_R_NO_SUPPORTED_VERSIONS_ENABLED, ERR_GET_REASON(err));

  // Likewise for SSLv3_method clients.
  EXPECT_FALSE(ConnectClientAndServer(&client, &server, ssl3_ctx.get(),
                                      tls_ctx.get(),
                                      nullptr /* no session */));
  err = ERR_get_error();
  EXPECT_EQ(ERR_LIB_SSL, ERR_GET_LIB(err));
  EXPECT_EQ(SSL_R_NO_SUPPORTED_VERSIONS_ENABLED, ERR_GET_REASON(err));
}

// TODO(davidben): Convert this file to GTest properly.
TEST(SSLTest, AllTests) {
  if (!TestSSL_SESSIONEncoding(kOpenSSLSession) ||
      !TestSSL_SESSIONEncoding(kCustomSession) ||
      !TestSSL_SESSIONEncoding(kBoringSSLSession) ||
      !TestBadSSL_SESSIONEncoding(kBadSessionExtraField) ||
      !TestBadSSL_SESSIONEncoding(kBadSessionVersion) ||
      !TestBadSSL_SESSIONEncoding(kBadSessionTrailingData) ||
      // Test the padding extension at TLS 1.2.
      !TestPaddingExtension(TLS1_2_VERSION, TLS1_2_VERSION) ||
      // Test the padding extension at TLS 1.3 with a TLS 1.2 session, so there
      // will be no PSK binder after the padding extension.
      !TestPaddingExtension(TLS1_3_VERSION, TLS1_2_VERSION) ||
      // Test the padding extension at TLS 1.3 with a TLS 1.3 session, so there
      // will be a PSK binder after the padding extension.
      !TestPaddingExtension(TLS1_3_VERSION, TLS1_3_DRAFT_VERSION) ||
      !ForEachVersion(TestSequenceNumber) ||
      !ForEachVersion(TestOneSidedShutdown) ||
      !ForEachVersion(TestGetPeerCertificate) ||
      !ForEachVersion(TestRetainOnlySHA256OfCerts) ||
      !TestClientHello() ||
      !ForEachVersion(TestSessionIDContext) ||
      !ForEachVersion(TestSessionTimeout) ||
      !ForEachVersion(TestSNICallback) ||
      !ForEachVersion(TestVersion) ||
      !ForEachVersion(TestALPNCipherAvailable) ||
      !ForEachVersion(TestSSLClearSessionResumption) ||
      !ForEachVersion(TestAutoChain) ||
      !ForEachVersion(TestSSLWriteRetry)) {
    ADD_FAILURE() << "Tests failed";
  }
}
