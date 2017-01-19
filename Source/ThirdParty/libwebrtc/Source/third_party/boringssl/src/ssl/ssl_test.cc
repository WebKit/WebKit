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

#include <openssl/base64.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
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
            {TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
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
            {TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
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
            {TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
        },
    },
    // Multiple masks can be ANDed in a single rule.
    {
        "kRSA+AESGCM+AES128",
        {
            {TLS1_CK_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
    },
    // - removes selected ciphers, but preserves their order for future
    // selections. Select AES_128_GCM, but order the key exchanges RSA, DHE_RSA,
    // ECDHE_RSA.
    {
        "ALL:-kECDHE:-kDHE:-kRSA:-ALL:"
        "AESGCM+AES128+aRSA",
        {
            {TLS1_CK_RSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_DHE_RSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
    },
    // Unknown selectors are no-ops.
    {
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "BOGUS1:-BOGUS2:+BOGUS3:!BOGUS4",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
    },
    // Square brackets specify equi-preference groups.
    {
        "[ECDHE-ECDSA-CHACHA20-POLY1305|ECDHE-ECDSA-AES128-GCM-SHA256]:"
        "[ECDHE-RSA-CHACHA20-POLY1305]:"
        "ECDHE-RSA-AES128-GCM-SHA256",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 1},
            {TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305_OLD, 1},
            {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 1},
            {TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0},
        },
    },
    // @STRENGTH performs a stable strength-sort of the selected ciphers and
    // only the selected ciphers.
    {
        // To simplify things, banish all but {ECDHE_RSA,RSA} x
        // {CHACHA20,AES_256_CBC,AES_128_CBC} x SHA1.
        "!kEDH:!AESGCM:!3DES:!SHA256:!MD5:!SHA384:"
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
            {TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA, 0},
            {TLS1_CK_RSA_WITH_AES_128_SHA, 0},
            {TLS1_CK_RSA_WITH_AES_256_SHA, 0},
        },
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
    },
    // SSLv3 matches everything that existed before TLS 1.2.
    {
        "AES128-SHA:AES128-SHA256:!SSLv3",
        {
            {TLS1_CK_RSA_WITH_AES_128_SHA256, 0},
        },
    },
    // TLSv1.2 matches everything added in TLS 1.2.
    {
        "AES128-SHA:AES128-SHA256:!TLSv1.2",
        {
            {TLS1_CK_RSA_WITH_AES_128_SHA, 0},
        },
    },
    // The two directives have no intersection.
    {
        "AES128-SHA:AES128-SHA256:!TLSv1.2+SSLv3",
        {
            {TLS1_CK_RSA_WITH_AES_128_SHA, 0},
            {TLS1_CK_RSA_WITH_AES_128_SHA256, 0},
        },
    },
    // The shared name of the CHACHA20_POLY1305 variants behaves like a cipher
    // name and not an alias. It may not be used in a multipart rule. (That the
    // shared name works is covered by the standard tests.)
    {
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "!ECDHE-RSA-CHACHA20-POLY1305+RSA:"
        "!ECDSA+ECDHE-ECDSA-CHACHA20-POLY1305",
        {
            {TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305_OLD, 0},
            {TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0},
            {TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305_OLD, 0},
        },
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
  "ALL:!eNULL",
  "ALL:!NULL",
  "HIGH",
  "FIPS",
  "SHA",
  "SHA1",
  "RSA",
  "SSLv3",
  "TLSv1",
  "TLSv1.2",
  "GENERIC",
};

static const char *kMustNotIncludeCECPQ1[] = {
  "ALL",
  "DEFAULT",
  "HIGH",
  "FIPS",
  "SHA",
  "SHA1",
  "SHA256",
  "SHA384",
  "RSA",
  "SSLv3",
  "TLSv1",
  "TLSv1.2",
  "aRSA",
  "RSA",
  "aECDSA",
  "ECDSA",
  "AES",
  "AES128",
  "AES256",
  "AESGCM",
  "CHACHA20",
  "GENERIC",
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

static void PrintCipherPreferenceList(ssl_cipher_preference_list_st *list) {
  bool in_group = false;
  for (size_t i = 0; i < sk_SSL_CIPHER_num(list->ciphers); i++) {
    const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(list->ciphers, i);
    if (!in_group && list->in_group_flags[i]) {
      fprintf(stderr, "\t[\n");
      in_group = true;
    }
    fprintf(stderr, "\t");
    if (in_group) {
      fprintf(stderr, "  ");
    }
    fprintf(stderr, "%s\n", SSL_CIPHER_get_name(cipher));
    if (in_group && !list->in_group_flags[i]) {
      fprintf(stderr, "\t]\n");
      in_group = false;
    }
  }
}

static bool TestCipherRule(const CipherTest &t) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  if (!ctx) {
    return false;
  }

  if (!SSL_CTX_set_cipher_list(ctx.get(), t.rule)) {
    fprintf(stderr, "Error testing cipher rule '%s'\n", t.rule);
    return false;
  }

  // Compare the two lists.
  if (sk_SSL_CIPHER_num(ctx->cipher_list->ciphers) != t.expected.size()) {
    fprintf(stderr, "Error: cipher rule '%s' evaluated to:\n", t.rule);
    PrintCipherPreferenceList(ctx->cipher_list);
    return false;
  }

  for (size_t i = 0; i < t.expected.size(); i++) {
    const SSL_CIPHER *cipher =
        sk_SSL_CIPHER_value(ctx->cipher_list->ciphers, i);
    if (t.expected[i].id != SSL_CIPHER_get_id(cipher) ||
        t.expected[i].in_group_flag != ctx->cipher_list->in_group_flags[i]) {
      fprintf(stderr, "Error: cipher rule '%s' evaluated to:\n", t.rule);
      PrintCipherPreferenceList(ctx->cipher_list);
      return false;
    }
  }

  return true;
}

static bool TestRuleDoesNotIncludeNull(const char *rule) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(SSLv23_server_method()));
  if (!ctx) {
    return false;
  }
  if (!SSL_CTX_set_cipher_list(ctx.get(), rule)) {
    fprintf(stderr, "Error: cipher rule '%s' failed\n", rule);
    return false;
  }
  for (size_t i = 0; i < sk_SSL_CIPHER_num(ctx->cipher_list->ciphers); i++) {
    if (SSL_CIPHER_is_NULL(sk_SSL_CIPHER_value(ctx->cipher_list->ciphers, i))) {
      fprintf(stderr, "Error: cipher rule '%s' includes NULL\n",rule);
      return false;
    }
  }
  return true;
}

static bool TestRuleDoesNotIncludeCECPQ1(const char *rule) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  if (!ctx) {
    return false;
  }
  if (!SSL_CTX_set_cipher_list(ctx.get(), rule)) {
    fprintf(stderr, "Error: cipher rule '%s' failed\n", rule);
    return false;
  }
  for (size_t i = 0; i < sk_SSL_CIPHER_num(ctx->cipher_list->ciphers); i++) {
    if (SSL_CIPHER_is_CECPQ1(sk_SSL_CIPHER_value(ctx->cipher_list->ciphers, i))) {
      fprintf(stderr, "Error: cipher rule '%s' includes CECPQ1\n",rule);
      return false;
    }
  }
  return true;
}

static bool TestCipherRules() {
  for (const CipherTest &test : kCipherTests) {
    if (!TestCipherRule(test)) {
      return false;
    }
  }

  for (const char *rule : kBadRules) {
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(SSLv23_server_method()));
    if (!ctx) {
      return false;
    }
    if (SSL_CTX_set_cipher_list(ctx.get(), rule)) {
      fprintf(stderr, "Cipher rule '%s' unexpectedly succeeded\n", rule);
      return false;
    }
    ERR_clear_error();
  }

  for (const char *rule : kMustNotIncludeNull) {
    if (!TestRuleDoesNotIncludeNull(rule)) {
      return false;
    }
  }

  for (const char *rule : kMustNotIncludeCECPQ1) {
    if (!TestRuleDoesNotIncludeCECPQ1(rule)) {
      return false;
    }
  }

  return true;
}

static bool TestCurveRule(const CurveTest &t) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  if (!ctx) {
    return false;
  }

  if (!SSL_CTX_set1_curves_list(ctx.get(), t.rule)) {
    fprintf(stderr, "Error testing curves list '%s'\n", t.rule);
    return false;
  }

  // Compare the two lists.
  if (ctx->supported_group_list_len != t.expected.size()) {
    fprintf(stderr, "Error testing curves list '%s': length\n", t.rule);
    return false;
  }

  for (size_t i = 0; i < t.expected.size(); i++) {
    if (t.expected[i] != ctx->supported_group_list[i]) {
      fprintf(stderr, "Error testing curves list '%s': mismatch\n", t.rule);
      return false;
    }
  }

  return true;
}

static bool TestCurveRules() {
  for (const CurveTest &test : kCurveTests) {
    if (!TestCurveRule(test)) {
      return false;
    }
  }

  for (const char *rule : kBadCurvesLists) {
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(SSLv23_server_method()));
    if (!ctx) {
      return false;
    }
    if (SSL_CTX_set1_curves_list(ctx.get(), rule)) {
      fprintf(stderr, "Curves list '%s' unexpectedly succeeded\n", rule);
      return false;
    }
    ERR_clear_error();
  }

  return true;
}

// kOpenSSLSession is a serialized SSL_SESSION generated from openssl
// s_client -sess_out.
static const char kOpenSSLSession[] =
    "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"
    "i4gv7Y5oliyn";

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
  bssl::UniquePtr<SSL_SESSION> session(SSL_SESSION_from_bytes(input.data(), input.size()));
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
      memcmp(input.data(), encoded.get(), input.size()) != 0) {
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
  if (memcmp(input.data(), encoded.get(), input.size()) != 0) {
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
  bssl::UniquePtr<SSL_SESSION> session(SSL_SESSION_from_bytes(input.data(), input.size()));
  if (session) {
    fprintf(stderr, "SSL_SESSION_from_bytes unexpectedly succeeded\n");
    return false;
  }
  ERR_clear_error();
  return true;
}

static bool TestDefaultVersion(uint16_t min_version, uint16_t max_version,
                               const SSL_METHOD *(*method)(void)) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(method()));
  if (!ctx) {
    return false;
  }
  if (ctx->min_version != min_version || ctx->max_version != max_version) {
    fprintf(stderr, "Got min %04x, max %04x; wanted min %04x, max %04x\n",
            ctx->min_version, ctx->max_version, min_version, max_version);
    return false;
  }
  return true;
}

static bool CipherGetRFCName(std::string *out, uint16_t value) {
  const SSL_CIPHER *cipher = SSL_get_cipher_by_value(value);
  if (cipher == NULL) {
    return false;
  }
  bssl::UniquePtr<char> rfc_name(SSL_CIPHER_get_rfc_name(cipher));
  if (!rfc_name) {
    return false;
  }
  out->assign(rfc_name.get());
  return true;
}

typedef struct {
  int id;
  const char *rfc_name;
} CIPHER_RFC_NAME_TEST;

static const CIPHER_RFC_NAME_TEST kCipherRFCNameTests[] = {
    {SSL3_CK_RSA_DES_192_CBC3_SHA, "TLS_RSA_WITH_3DES_EDE_CBC_SHA"},
    {TLS1_CK_RSA_WITH_AES_128_SHA, "TLS_RSA_WITH_AES_128_CBC_SHA"},
    {TLS1_CK_DHE_RSA_WITH_AES_256_SHA, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA"},
    {TLS1_CK_DHE_RSA_WITH_AES_256_SHA256,
     "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256"},
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

    // These names are non-standard:
    {TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305_OLD,
     "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256"},
    {TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305_OLD,
     "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256"},
};

static bool TestCipherGetRFCName(void) {
  for (size_t i = 0;
       i < OPENSSL_ARRAY_SIZE(kCipherRFCNameTests); i++) {
    const CIPHER_RFC_NAME_TEST *test = &kCipherRFCNameTests[i];
    std::string rfc_name;
    if (!CipherGetRFCName(&rfc_name, test->id & 0xffff)) {
      fprintf(stderr, "SSL_CIPHER_get_rfc_name failed\n");
      return false;
    }
    if (rfc_name != test->rfc_name) {
      fprintf(stderr, "SSL_CIPHER_get_rfc_name: got '%s', wanted '%s'\n",
              rfc_name.c_str(), test->rfc_name);
      return false;
    }
  }
  return true;
}

// CreateSessionWithTicket returns a sample |SSL_SESSION| with the ticket
// replaced for one of length |ticket_len| or nullptr on failure.
static bssl::UniquePtr<SSL_SESSION> CreateSessionWithTicket(size_t ticket_len) {
  std::vector<uint8_t> der;
  if (!DecodeBase64(&der, kOpenSSLSession)) {
    return nullptr;
  }
  bssl::UniquePtr<SSL_SESSION> session(SSL_SESSION_from_bytes(der.data(), der.size()));
  if (!session) {
    return nullptr;
  }

  // Swap out the ticket for a garbage one.
  OPENSSL_free(session->tlsext_tick);
  session->tlsext_tick = reinterpret_cast<uint8_t*>(OPENSSL_malloc(ticket_len));
  if (session->tlsext_tick == nullptr) {
    return nullptr;
  }
  memset(session->tlsext_tick, 'a', ticket_len);
  session->tlsext_ticklen = ticket_len;

  // Fix up the timeout.
  session->time = time(NULL);
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

// GetClientHelloLen creates a client SSL connection with a ticket of length
// |ticket_len| and records the ClientHello. It returns the length of the
// ClientHello, not including the record header, on success and zero on error.
static size_t GetClientHelloLen(size_t ticket_len) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  bssl::UniquePtr<SSL_SESSION> session = CreateSessionWithTicket(ticket_len);
  if (!ctx || !session) {
    return 0;
  }
  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  // Test at TLS 1.2. TLS 1.3 adds enough extensions that the ClientHello is
  // longer than our test vectors.
  if (!ssl || !SSL_set_session(ssl.get(), session.get()) ||
      !SSL_set_max_proto_version(ssl.get(), TLS1_2_VERSION)) {
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

static bool TestPaddingExtension() {
  // Sample a baseline length.
  size_t base_len = GetClientHelloLen(1);
  if (base_len == 0) {
    return false;
  }

  for (const PaddingTest &test : kPaddingTests) {
    if (base_len > test.input_len) {
      fprintf(stderr, "Baseline ClientHello too long.\n");
      return false;
    }

    size_t padded_len = GetClientHelloLen(1 + test.input_len - base_len);
    if (padded_len != test.padded_len) {
      fprintf(stderr, "%u-byte ClientHello padded to %u bytes, not %u.\n",
              static_cast<unsigned>(test.input_len),
              static_cast<unsigned>(padded_len),
              static_cast<unsigned>(test.padded_len));
      return false;
    }
  }
  return true;
}

// Test that |SSL_get_client_CA_list| echoes back the configured parameter even
// before configuring as a server.
static bool TestClientCAList() {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  if (!ctx) {
    return false;
  }
  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  if (!ssl) {
    return false;
  }

  STACK_OF(X509_NAME) *stack = sk_X509_NAME_new_null();
  if (stack == nullptr) {
    return false;
  }
  // |SSL_set_client_CA_list| takes ownership.
  SSL_set_client_CA_list(ssl.get(), stack);

  return SSL_get_client_CA_list(ssl.get()) == stack;
}

static void AppendSession(SSL_SESSION *session, void *arg) {
  std::vector<SSL_SESSION*> *out =
      reinterpret_cast<std::vector<SSL_SESSION*>*>(arg);
  out->push_back(session);
}

// ExpectCache returns true if |ctx|'s session cache consists of |expected|, in
// order.
static bool ExpectCache(SSL_CTX *ctx,
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
  bssl::UniquePtr<SSL_SESSION> ret(SSL_SESSION_new());
  if (!ret) {
    return nullptr;
  }

  ret->session_id_length = SSL3_SSL_SESSION_ID_LENGTH;
  memset(ret->session_id, 0, ret->session_id_length);
  memcpy(ret->session_id, &number, sizeof(number));
  return ret;
}

// Test that the internal session cache behaves as expected.
static bool TestInternalSessionCache() {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  if (!ctx) {
    return false;
  }

  // Prepare 10 test sessions.
  std::vector<bssl::UniquePtr<SSL_SESSION>> sessions;
  for (int i = 0; i < 10; i++) {
    bssl::UniquePtr<SSL_SESSION> session = CreateTestSession(i);
    if (!session) {
      return false;
    }
    sessions.push_back(std::move(session));
  }

  SSL_CTX_sess_set_cache_size(ctx.get(), 5);

  // Insert all the test sessions.
  for (const auto &session : sessions) {
    if (!SSL_CTX_add_session(ctx.get(), session.get())) {
      return false;
    }
  }

  // Only the last five should be in the list.
  std::vector<SSL_SESSION*> expected = {
      sessions[9].get(),
      sessions[8].get(),
      sessions[7].get(),
      sessions[6].get(),
      sessions[5].get(),
  };
  if (!ExpectCache(ctx.get(), expected)) {
    return false;
  }

  // Inserting an element already in the cache should fail.
  if (SSL_CTX_add_session(ctx.get(), sessions[7].get()) ||
      !ExpectCache(ctx.get(), expected)) {
    return false;
  }

  // Although collisions should be impossible (256-bit session IDs), the cache
  // must handle them gracefully.
  bssl::UniquePtr<SSL_SESSION> collision(CreateTestSession(7));
  if (!collision || !SSL_CTX_add_session(ctx.get(), collision.get())) {
    return false;
  }
  expected = {
      collision.get(),
      sessions[9].get(),
      sessions[8].get(),
      sessions[6].get(),
      sessions[5].get(),
  };
  if (!ExpectCache(ctx.get(), expected)) {
    return false;
  }

  // Removing sessions behaves correctly.
  if (!SSL_CTX_remove_session(ctx.get(), sessions[6].get())) {
    return false;
  }
  expected = {
      collision.get(),
      sessions[9].get(),
      sessions[8].get(),
      sessions[5].get(),
  };
  if (!ExpectCache(ctx.get(), expected)) {
    return false;
  }

  // Removing sessions requires an exact match.
  if (SSL_CTX_remove_session(ctx.get(), sessions[0].get()) ||
      SSL_CTX_remove_session(ctx.get(), sessions[7].get()) ||
      !ExpectCache(ctx.get(), expected)) {
    return false;
  }

  return true;
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
  return bssl::UniquePtr<X509>(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
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

static bool ConnectClientAndServer(bssl::UniquePtr<SSL> *out_client, bssl::UniquePtr<SSL> *out_server,
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

  // Drive both their handshakes to completion.
  for (;;) {
    int client_ret = SSL_do_handshake(client.get());
    int client_err = SSL_get_error(client.get(), client_ret);
    if (client_err != SSL_ERROR_NONE &&
        client_err != SSL_ERROR_WANT_READ &&
        client_err != SSL_ERROR_WANT_WRITE) {
      fprintf(stderr, "Client error: %d\n", client_err);
      return false;
    }

    int server_ret = SSL_do_handshake(server.get());
    int server_err = SSL_get_error(server.get(), server_ret);
    if (server_err != SSL_ERROR_NONE &&
        server_err != SSL_ERROR_WANT_READ &&
        server_err != SSL_ERROR_WANT_WRITE) {
      fprintf(stderr, "Server error: %d\n", server_err);
      return false;
    }

    if (client_ret == 1 && server_ret == 1) {
      break;
    }
  }

  *out_client = std::move(client);
  *out_server = std::move(server);
  return true;
}

static uint16_t kTLSVersions[] = {
    SSL3_VERSION, TLS1_VERSION, TLS1_1_VERSION, TLS1_2_VERSION, TLS1_3_VERSION,
};

static uint16_t kDTLSVersions[] = {
    DTLS1_VERSION, DTLS1_2_VERSION,
};

static bool TestSequenceNumber() {
  for (bool is_dtls : std::vector<bool>{false, true}) {
    const SSL_METHOD *method = is_dtls ? DTLS_method() : TLS_method();
    const uint16_t *versions = is_dtls ? kDTLSVersions : kTLSVersions;
    size_t num_versions = is_dtls ? OPENSSL_ARRAY_SIZE(kDTLSVersions)
                                  : OPENSSL_ARRAY_SIZE(kTLSVersions);
    for (size_t i = 0; i < num_versions; i++) {
      uint16_t version = versions[i];
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
      if (!cert || !key ||
          !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
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
    }
  }

  return true;
}

static bool TestOneSidedShutdown() {
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_method()));
  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_method()));
  if (!client_ctx || !server_ctx) {
    return false;
  }

  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key ||
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
static bool TestSessionDuplication() {
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_method()));
  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_method()));
  if (!client_ctx || !server_ctx) {
    return false;
  }

  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key ||
      !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get())) {
    return false;
  }

  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, client_ctx.get(),
                              server_ctx.get(), nullptr /* no session */)) {
    return false;
  }

  SSL_SESSION *session0 = SSL_get_session(client.get());
  bssl::UniquePtr<SSL_SESSION> session1(SSL_SESSION_dup(session0, SSL_SESSION_DUP_ALL));
  if (!session1) {
    return false;
  }

  session1->not_resumable = 0;

  uint8_t *s0_bytes, *s1_bytes;
  size_t s0_len, s1_len;

  if (!SSL_SESSION_to_bytes(session0, &s0_bytes, &s0_len)) {
    return false;
  }
  bssl::UniquePtr<uint8_t> free_s0(s0_bytes);

  if (!SSL_SESSION_to_bytes(session1.get(), &s1_bytes, &s1_len)) {
    return false;
  }
  bssl::UniquePtr<uint8_t> free_s1(s1_bytes);

  return s0_len == s1_len && memcmp(s0_bytes, s1_bytes, s0_len) == 0;
}

static bool ExpectFDs(const SSL *ssl, int rfd, int wfd) {
  if (SSL_get_rfd(ssl) != rfd || SSL_get_wfd(ssl) != wfd) {
    fprintf(stderr, "Got fds %d and %d, wanted %d and %d.\n", SSL_get_rfd(ssl),
            SSL_get_wfd(ssl), rfd, wfd);
    return false;
  }

  // The wrapper BIOs are always equal when fds are equal, even if set
  // individually.
  if (rfd == wfd && SSL_get_rbio(ssl) != SSL_get_wbio(ssl)) {
    fprintf(stderr, "rbio and wbio did not match.\n");
    return false;
  }

  return true;
}

static bool TestSetFD() {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  if (!ctx) {
    return false;
  }

  // Test setting different read and write FDs.
  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  if (!ssl ||
      !SSL_set_rfd(ssl.get(), 1) ||
      !SSL_set_wfd(ssl.get(), 2) ||
      !ExpectFDs(ssl.get(), 1, 2)) {
    return false;
  }

  // Test setting the same FD.
  ssl.reset(SSL_new(ctx.get()));
  if (!ssl ||
      !SSL_set_fd(ssl.get(), 1) ||
      !ExpectFDs(ssl.get(), 1, 1)) {
    return false;
  }

  // Test setting the same FD one side at a time.
  ssl.reset(SSL_new(ctx.get()));
  if (!ssl ||
      !SSL_set_rfd(ssl.get(), 1) ||
      !SSL_set_wfd(ssl.get(), 1) ||
      !ExpectFDs(ssl.get(), 1, 1)) {
    return false;
  }

  // Test setting the same FD in the other order.
  ssl.reset(SSL_new(ctx.get()));
  if (!ssl ||
      !SSL_set_wfd(ssl.get(), 1) ||
      !SSL_set_rfd(ssl.get(), 1) ||
      !ExpectFDs(ssl.get(), 1, 1)) {
    return false;
  }

  // Test changing the read FD partway through.
  ssl.reset(SSL_new(ctx.get()));
  if (!ssl ||
      !SSL_set_fd(ssl.get(), 1) ||
      !SSL_set_rfd(ssl.get(), 2) ||
      !ExpectFDs(ssl.get(), 2, 1)) {
    return false;
  }

  // Test changing the write FD partway through.
  ssl.reset(SSL_new(ctx.get()));
  if (!ssl ||
      !SSL_set_fd(ssl.get(), 1) ||
      !SSL_set_wfd(ssl.get(), 2) ||
      !ExpectFDs(ssl.get(), 1, 2)) {
    return false;
  }

  // Test a no-op change to the read FD partway through.
  ssl.reset(SSL_new(ctx.get()));
  if (!ssl ||
      !SSL_set_fd(ssl.get(), 1) ||
      !SSL_set_rfd(ssl.get(), 1) ||
      !ExpectFDs(ssl.get(), 1, 1)) {
    return false;
  }

  // Test a no-op change to the write FD partway through.
  ssl.reset(SSL_new(ctx.get()));
  if (!ssl ||
      !SSL_set_fd(ssl.get(), 1) ||
      !SSL_set_wfd(ssl.get(), 1) ||
      !ExpectFDs(ssl.get(), 1, 1)) {
    return false;
  }

  // ASan builds will implicitly test that the internal |BIO| reference-counting
  // is correct.

  return true;
}

static bool TestSetBIO() {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  if (!ctx) {
    return false;
  }

  bssl::UniquePtr<SSL> ssl(SSL_new(ctx.get()));
  bssl::UniquePtr<BIO> bio1(BIO_new(BIO_s_mem())), bio2(BIO_new(BIO_s_mem())),
      bio3(BIO_new(BIO_s_mem()));
  if (!ssl || !bio1 || !bio2 || !bio3) {
    return false;
  }

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
  return true;
}

static int VerifySucceed(X509_STORE_CTX *store_ctx, void *arg) { return 1; }

static bool TestGetPeerCertificate() {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  for (uint16_t version : kTLSVersions) {
    // Configure both client and server to accept any certificate.
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
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
      fprintf(stderr, "%x: Server peer certificate did not match.\n", version);
      return false;
    }

    peer.reset(SSL_get_peer_certificate(client.get()));
    if (!peer || X509_cmp(cert.get(), peer.get()) != 0) {
      fprintf(stderr, "%x: Client peer certificate did not match.\n", version);
      return false;
    }

    // However, for historical reasons, the chain includes the leaf on the
    // client, but does not on the server.
    if (sk_X509_num(SSL_get_peer_cert_chain(client.get())) != 1) {
      fprintf(stderr, "%x: Client peer chain was incorrect.\n", version);
      return false;
    }

    if (sk_X509_num(SSL_get_peer_cert_chain(server.get())) != 0) {
      fprintf(stderr, "%x: Server peer chain was incorrect.\n", version);
      return false;
    }
  }

  return true;
}

static bool TestRetainOnlySHA256OfCerts() {
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

  for (uint16_t version : kTLSVersions) {
    // Configure both client and server to accept any certificate, but the
    // server must retain only the SHA-256 of the peer.
    bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
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
      fprintf(stderr, "%x: Peer certificate was retained.\n", version);
      return false;
    }

    SSL_SESSION *session = SSL_get_session(server.get());
    if (!session->peer_sha256_valid) {
      fprintf(stderr, "%x: peer_sha256_valid was not set.\n", version);
      return false;
    }

    if (memcmp(cert_sha256, session->peer_sha256, SHA256_DIGEST_LENGTH) != 0) {
      fprintf(stderr, "%x: peer_sha256 did not match.\n", version);
      return false;
    }
  }

  return true;
}

static bool ClientHelloMatches(uint16_t version, const uint8_t *expected,
                               size_t expected_len) {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  if (!ctx ||
      !SSL_CTX_set_max_proto_version(ctx.get(), version) ||
      // Our default cipher list varies by CPU capabilities, so manually place
      // the ChaCha20 ciphers in front.
      !SSL_CTX_set_cipher_list(ctx.get(), "CHACHA20:ALL")) {
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
  memset(client_hello.data() + kRandomOffset, 0, SSL3_RANDOM_SIZE);

  if (client_hello.size() != expected_len ||
      memcmp(client_hello.data(), expected, expected_len) != 0) {
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
    0x00, 0x3f,
    0x01,
    0x00, 0x00, 0x3b,
    0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
    0x00, 0x14,
    0xc0, 0x09,
    0xc0, 0x13,
    0x00, 0x33,
    0xc0, 0x0a,
    0xc0, 0x14,
    0x00, 0x39,
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
      0x00, 0x5e,
      0x01,
      0x00, 0x00, 0x5a,
      0x03, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      0x00, 0x12,
      0xc0, 0x09,
      0xc0, 0x13,
      0x00, 0x33,
      0xc0, 0x0a,
      0xc0, 0x14,
      0x00, 0x39,
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
      0x00, 0x5e,
      0x01,
      0x00, 0x00, 0x5a,
      0x03, 0x02,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      0x00, 0x12,
      0xc0, 0x09,
      0xc0, 0x13,
      0x00, 0x33,
      0xc0, 0x0a,
      0xc0, 0x14,
      0x00, 0x39,
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

  static const uint8_t kTLS12ClientHello[] = {
      0x16, 0x03, 0x01, 0x00, 0x9e, 0x01, 0x00, 0x00, 0x9a, 0x03, 0x03, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3a, 0xcc, 0xa9,
      0xcc, 0xa8, 0xcc, 0x14, 0xcc, 0x13, 0xc0, 0x2b, 0xc0, 0x2f, 0x00, 0x9e,
      0xc0, 0x2c, 0xc0, 0x30, 0x00, 0x9f, 0xc0, 0x09, 0xc0, 0x23, 0xc0, 0x13,
      0xc0, 0x27, 0x00, 0x33, 0x00, 0x67, 0xc0, 0x0a, 0xc0, 0x24, 0xc0, 0x14,
      0xc0, 0x28, 0x00, 0x39, 0x00, 0x6b, 0x00, 0x9c, 0x00, 0x9d, 0x00, 0x2f,
      0x00, 0x3c, 0x00, 0x35, 0x00, 0x3d, 0x00, 0x0a, 0x01, 0x00, 0x00, 0x37,
      0xff, 0x01, 0x00, 0x01, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x23, 0x00,
      0x00, 0x00, 0x0d, 0x00, 0x14, 0x00, 0x12, 0x08, 0x06, 0x06, 0x01, 0x08,
      0x05, 0x05, 0x01, 0x05, 0x03, 0x08, 0x04, 0x04, 0x01, 0x04, 0x03, 0x02,
      0x01, 0x00, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x00, 0x0a, 0x00, 0x08, 0x00,
      0x06, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x18,
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

static bool TestSessionIDContext() {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  static const uint8_t kContext1[] = {1};
  static const uint8_t kContext2[] = {2};

  for (uint16_t version : kTLSVersions) {
    bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_method()));
    bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_method()));
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
      fprintf(stderr, "Error getting session (version = %04x).\n", version);
      return false;
    }

    if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                             true /* expect session reused */)) {
      fprintf(stderr, "Error resuming session (version = %04x).\n", version);
      return false;
    }

    // Change the session ID context.
    if (!SSL_CTX_set_session_id_context(server_ctx.get(), kContext2,
                                        sizeof(kContext2))) {
      return false;
    }

    if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                             false /* expect session not reused */)) {
      fprintf(stderr,
              "Error connection with different context (version = %04x).\n",
              version);
      return false;
    }
  }

  return true;
}

static timeval g_current_time;

static void CurrentTimeCallback(const SSL *ssl, timeval *out_clock) {
  *out_clock = g_current_time;
}

static bool TestSessionTimeout() {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  for (uint16_t version : kTLSVersions) {
    bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_method()));
    bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_method()));
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
    SSL_CTX_set_current_time_cb(server_ctx.get(), CurrentTimeCallback);

    bssl::UniquePtr<SSL_SESSION> session =
        CreateClientSession(client_ctx.get(), server_ctx.get());
    if (!session) {
      fprintf(stderr, "Error getting session (version = %04x).\n", version);
      return false;
    }

    // Advance the clock just behind the timeout.
    g_current_time.tv_sec += SSL_DEFAULT_SESSION_TIMEOUT;

    if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                             true /* expect session reused */)) {
      fprintf(stderr, "Error resuming session (version = %04x).\n", version);
      return false;
    }

    // Advance the clock one more second.
    g_current_time.tv_sec++;

    if (!ExpectSessionReused(client_ctx.get(), server_ctx.get(), session.get(),
                             false /* expect session not reused */)) {
      fprintf(stderr, "Error resuming session (version = %04x).\n", version);
      return false;
    }
  }

  return true;
}

static int SwitchContext(SSL *ssl, int *out_alert, void *arg) {
  SSL_CTX *ctx = reinterpret_cast<SSL_CTX*>(arg);
  SSL_set_SSL_CTX(ssl, ctx);
  return SSL_TLSEXT_ERR_OK;
}

static bool TestSNICallback() {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  bssl::UniquePtr<X509> cert2 = GetECDSATestCertificate();
  bssl::UniquePtr<EVP_PKEY> key2 = GetECDSATestKey();
  if (!cert || !key || !cert2 || !key2) {
    return false;
  }

  // At each version, test that switching the |SSL_CTX| at the SNI callback
  // behaves correctly.
  for (uint16_t version : kTLSVersions) {
    if (version == SSL3_VERSION) {
      continue;
    }

    static const uint16_t kECDSAWithSHA256 = SSL_SIGN_ECDSA_SECP256R1_SHA256;

    bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_method()));
    bssl::UniquePtr<SSL_CTX> server_ctx2(SSL_CTX_new(TLS_method()));
    bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_method()));
    if (!server_ctx || !server_ctx2 || !client_ctx ||
        !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
        !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()) ||
        !SSL_CTX_use_certificate(server_ctx2.get(), cert2.get()) ||
        !SSL_CTX_use_PrivateKey(server_ctx2.get(), key2.get()) ||
        // Historically signing preferences would be lost in some cases with the
        // SNI callback, which triggers the TLS 1.2 SHA-1 default. To ensure
        // this doesn't happen when |version| is TLS 1.2, configure the private
        // key to only sign SHA-256.
        !SSL_CTX_set_signing_algorithm_prefs(server_ctx2.get(),
                                             &kECDSAWithSHA256, 1) ||
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

    bssl::UniquePtr<SSL> client, server;
    if (!ConnectClientAndServer(&client, &server, client_ctx.get(),
                                server_ctx.get(), nullptr)) {
      fprintf(stderr, "Handshake failed at version %04x.\n", version);
      return false;
    }

    // The client should have received |cert2|.
    bssl::UniquePtr<X509> peer(SSL_get_peer_certificate(client.get()));
    if (!peer ||
        X509_cmp(peer.get(), cert2.get()) != 0) {
      fprintf(stderr, "Incorrect certificate received at version %04x.\n",
              version);
      return false;
    }
  }

  return true;
}

static int SetMaxVersion(const struct ssl_early_callback_ctx *ctx) {
  if (!SSL_set_max_proto_version(ctx->ssl, TLS1_2_VERSION)) {
    return -1;
  }

  return 1;
}

// TestEarlyCallbackVersionSwitch tests that the early callback can swap the
// maximum version.
static bool TestEarlyCallbackVersionSwitch() {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  bssl::UniquePtr<SSL_CTX> server_ctx(SSL_CTX_new(TLS_method()));
  bssl::UniquePtr<SSL_CTX> client_ctx(SSL_CTX_new(TLS_method()));
  if (!cert || !key || !server_ctx || !client_ctx ||
      !SSL_CTX_use_certificate(server_ctx.get(), cert.get()) ||
      !SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()) ||
      !SSL_CTX_set_max_proto_version(client_ctx.get(), TLS1_3_VERSION) ||
      !SSL_CTX_set_max_proto_version(server_ctx.get(), TLS1_3_VERSION)) {
    return false;
  }

  SSL_CTX_set_select_certificate_cb(server_ctx.get(), SetMaxVersion);

  bssl::UniquePtr<SSL> client, server;
  if (!ConnectClientAndServer(&client, &server, client_ctx.get(),
                              server_ctx.get(), nullptr)) {
    return false;
  }

  if (SSL_version(client.get()) != TLS1_2_VERSION) {
    fprintf(stderr, "Early callback failed to switch the maximum version.\n");
    return false;
  }

  return true;
}

static bool TestSetVersion() {
  bssl::UniquePtr<SSL_CTX> ctx(SSL_CTX_new(TLS_method()));
  if (!ctx) {
    return false;
  }

  if (!SSL_CTX_set_max_proto_version(ctx.get(), TLS1_VERSION) ||
      !SSL_CTX_set_max_proto_version(ctx.get(), TLS1_1_VERSION) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), TLS1_VERSION) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), TLS1_1_VERSION)) {
    fprintf(stderr, "Could not set valid TLS version.\n");
    return false;
  }

  if (SSL_CTX_set_max_proto_version(ctx.get(), DTLS1_VERSION) ||
      SSL_CTX_set_max_proto_version(ctx.get(), 0x0200) ||
      SSL_CTX_set_max_proto_version(ctx.get(), 0x1234) ||
      SSL_CTX_set_min_proto_version(ctx.get(), DTLS1_VERSION) ||
      SSL_CTX_set_min_proto_version(ctx.get(), 0x0200) ||
      SSL_CTX_set_min_proto_version(ctx.get(), 0x1234)) {
    fprintf(stderr, "Unexpectedly set invalid TLS version.\n");
    return false;
  }

  if (!SSL_CTX_set_max_proto_version(ctx.get(), 0) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), 0)) {
    fprintf(stderr, "Could not set default TLS version.\n");
    return false;
  }

  if (ctx->min_version != SSL3_VERSION ||
      ctx->max_version != TLS1_2_VERSION) {
    fprintf(stderr, "Default TLS versions were incorrect (%04x and %04x).\n",
            ctx->min_version, ctx->max_version);
    return false;
  }

  ctx.reset(SSL_CTX_new(DTLS_method()));
  if (!ctx) {
    return false;
  }

  if (!SSL_CTX_set_max_proto_version(ctx.get(), DTLS1_VERSION) ||
      !SSL_CTX_set_max_proto_version(ctx.get(), DTLS1_2_VERSION) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), DTLS1_VERSION) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), DTLS1_2_VERSION)) {
    fprintf(stderr, "Could not set valid DTLS version.\n");
    return false;
  }

  if (SSL_CTX_set_max_proto_version(ctx.get(), TLS1_VERSION) ||
      SSL_CTX_set_max_proto_version(ctx.get(), 0xfefe /* DTLS 1.1 */) ||
      SSL_CTX_set_max_proto_version(ctx.get(), 0xfffe /* DTLS 0.1 */) ||
      SSL_CTX_set_max_proto_version(ctx.get(), 0x1234) ||
      SSL_CTX_set_min_proto_version(ctx.get(), TLS1_VERSION) ||
      SSL_CTX_set_min_proto_version(ctx.get(), 0xfefe /* DTLS 1.1 */) ||
      SSL_CTX_set_min_proto_version(ctx.get(), 0xfffe /* DTLS 0.1 */) ||
      SSL_CTX_set_min_proto_version(ctx.get(), 0x1234)) {
    fprintf(stderr, "Unexpectedly set invalid DTLS version.\n");
    return false;
  }

  if (!SSL_CTX_set_max_proto_version(ctx.get(), 0) ||
      !SSL_CTX_set_min_proto_version(ctx.get(), 0)) {
    fprintf(stderr, "Could not set default DTLS version.\n");
    return false;
  }

  if (ctx->min_version != TLS1_1_VERSION ||
      ctx->max_version != TLS1_2_VERSION) {
    fprintf(stderr, "Default DTLS versions were incorrect (%04x and %04x).\n",
            ctx->min_version, ctx->max_version);
    return false;
  }

  return true;
}

static bool TestVersions() {
  bssl::UniquePtr<X509> cert = GetTestCertificate();
  bssl::UniquePtr<EVP_PKEY> key = GetTestKey();
  if (!cert || !key) {
    return false;
  }

  for (bool is_dtls : std::vector<bool>{false, true}) {
    const SSL_METHOD *method = is_dtls ? DTLS_method() : TLS_method();
    const char *name = is_dtls ? "DTLS" : "TLS";
    const uint16_t *versions = is_dtls ? kDTLSVersions : kTLSVersions;
    size_t num_versions = is_dtls ? OPENSSL_ARRAY_SIZE(kDTLSVersions)
                                  : OPENSSL_ARRAY_SIZE(kTLSVersions);
    for (size_t i = 0; i < num_versions; i++) {
      uint16_t version = versions[i];
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
        fprintf(stderr, "Failed to connect %s at version %04x.\n", name,
                version);
        return false;
      }

      if (SSL_version(client.get()) != version ||
          SSL_version(server.get()) != version) {
        fprintf(stderr,
                "%s version mismatch. Got %04x and %04x, wanted %04x.\n", name,
                SSL_version(client.get()), SSL_version(server.get()), version);
        return false;
      }
    }
  }

  return true;
}

int main() {
  CRYPTO_library_init();

  if (!TestCipherRules() ||
      !TestCurveRules() ||
      !TestSSL_SESSIONEncoding(kOpenSSLSession) ||
      !TestSSL_SESSIONEncoding(kCustomSession) ||
      !TestSSL_SESSIONEncoding(kBoringSSLSession) ||
      !TestBadSSL_SESSIONEncoding(kBadSessionExtraField) ||
      !TestBadSSL_SESSIONEncoding(kBadSessionVersion) ||
      !TestBadSSL_SESSIONEncoding(kBadSessionTrailingData) ||
      // TODO(svaldez): Update this when TLS 1.3 is enabled by default.
      !TestDefaultVersion(SSL3_VERSION, TLS1_2_VERSION, &TLS_method) ||
      !TestDefaultVersion(SSL3_VERSION, SSL3_VERSION, &SSLv3_method) ||
      !TestDefaultVersion(TLS1_VERSION, TLS1_VERSION, &TLSv1_method) ||
      !TestDefaultVersion(TLS1_1_VERSION, TLS1_1_VERSION, &TLSv1_1_method) ||
      !TestDefaultVersion(TLS1_2_VERSION, TLS1_2_VERSION, &TLSv1_2_method) ||
      !TestDefaultVersion(TLS1_1_VERSION, TLS1_2_VERSION, &DTLS_method) ||
      !TestDefaultVersion(TLS1_1_VERSION, TLS1_1_VERSION, &DTLSv1_method) ||
      !TestDefaultVersion(TLS1_2_VERSION, TLS1_2_VERSION, &DTLSv1_2_method) ||
      !TestCipherGetRFCName() ||
      !TestPaddingExtension() ||
      !TestClientCAList() ||
      !TestInternalSessionCache() ||
      !TestSequenceNumber() ||
      !TestOneSidedShutdown() ||
      !TestSessionDuplication() ||
      !TestSetFD() ||
      !TestSetBIO() ||
      !TestGetPeerCertificate() ||
      !TestRetainOnlySHA256OfCerts() ||
      !TestClientHello() ||
      !TestSessionIDContext() ||
      !TestSessionTimeout() ||
      !TestSNICallback() ||
      !TestEarlyCallbackVersionSwitch() ||
      !TestSetVersion() ||
      !TestVersions()) {
    ERR_print_errors_fp(stderr);
    return 1;
  }

  printf("PASS\n");
  return 0;
}
