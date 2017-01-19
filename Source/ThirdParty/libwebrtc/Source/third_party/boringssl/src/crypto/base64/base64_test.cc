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

#include <string>
#include <vector>

#include <openssl/base64.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#include "../internal.h"


enum encoding_relation {
  // canonical indicates that the encoding is the expected encoding of the
  // input.
  canonical,
  // valid indicates that the encoding is /a/ valid encoding of the input, but
  // need not be the canonical one.
  valid,
  // invalid indicates that the encoded data is valid.
  invalid,
};

struct TestVector {
  enum encoding_relation relation;
  const char *decoded;
  const char *encoded;
};

// Test vectors from RFC 4648.
static const TestVector kTestVectors[] = {
    {canonical, "", ""},
    {canonical, "f", "Zg==\n"},
    {canonical, "fo", "Zm8=\n"},
    {canonical, "foo", "Zm9v\n"},
    {canonical, "foob", "Zm9vYg==\n"},
    {canonical, "fooba", "Zm9vYmE=\n"},
    {canonical, "foobar", "Zm9vYmFy\n"},
    {valid, "foobar", "Zm9vYmFy\n\n"},
    {valid, "foobar", " Zm9vYmFy\n\n"},
    {valid, "foobar", " Z m 9 v Y m F y\n\n"},
    {invalid, "", "Zm9vYmFy=\n"},
    {invalid, "", "Zm9vYmFy==\n"},
    {invalid, "", "Zm9vYmFy===\n"},
    {invalid, "", "Z"},
    {invalid, "", "Z\n"},
    {invalid, "", "ab!c"},
    {invalid, "", "ab=c"},
    {invalid, "", "abc"},

    {canonical, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
     "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eA==\n"},
    {valid, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
     "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eA\n==\n"},
    {valid, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
     "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eA=\n=\n"},
    {invalid, "",
     "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eA=\n==\n"},
    {canonical, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
     "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4\neHh4eHh"
     "4eHh4eHh4\n"},
    {canonical,
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
     "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4\neHh4eHh"
     "4eHh4eHh4eHh4eA==\n"},
    {valid, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
     "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh\n4eHh4eHh"
     "4eHh4eHh4eHh4eA==\n"},
    {valid, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
     "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4e"
     "Hh4eHh4eHh4eA==\n"},
    {invalid, "",
     "eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eA=="
     "\neHh4eHh4eHh4eHh4eHh4eHh4\n"},

    // A '-' has traditionally been treated as the end of the data by OpenSSL
    // and anything following would be ignored. BoringSSL does not accept this
    // non-standard extension.
    {invalid, "", "Zm9vYmFy-anythinggoes"},
    {invalid, "", "Zm9vYmFy\n-anythinggoes"},

    // CVE-2015-0292
    {invalid, "",
     "ZW5jb2RlIG1lCg==========================================================="
     "=======\n"},
};

static const size_t kNumTests = OPENSSL_ARRAY_SIZE(kTestVectors);

// RemoveNewlines returns a copy of |in| with all '\n' characters removed.
static std::string RemoveNewlines(const char *in) {
  std::string ret;
  const size_t in_len = strlen(in);

  for (size_t i = 0; i < in_len; i++) {
    if (in[i] != '\n') {
      ret.push_back(in[i]);
    }
  }

  return ret;
}

static bool TestEncodeBlock() {
  for (unsigned i = 0; i < kNumTests; i++) {
    const TestVector *t = &kTestVectors[i];
    if (t->relation != canonical) {
      continue;
    }

    const size_t decoded_len = strlen(t->decoded);
    size_t max_encoded_len;
    if (!EVP_EncodedLength(&max_encoded_len, decoded_len)) {
      fprintf(stderr, "#%u: EVP_EncodedLength failed\n", i);
      return false;
    }

    std::vector<uint8_t> out_vec(max_encoded_len);
    uint8_t *out = out_vec.data();
    size_t len = EVP_EncodeBlock(out, (const uint8_t *)t->decoded, decoded_len);

    std::string encoded(RemoveNewlines(t->encoded));
    if (len != encoded.size() ||
        memcmp(out, encoded.data(), len) != 0) {
      fprintf(stderr, "encode(\"%s\") = \"%.*s\", want \"%s\"\n",
              t->decoded, (int)len, (const char*)out, encoded.c_str());
      return false;
    }
  }

  return true;
}

static bool TestDecodeBase64() {
  size_t len;

  for (unsigned i = 0; i < kNumTests; i++) {
    const TestVector *t = &kTestVectors[i];

    if (t->relation == valid) {
      // The non-canonical encodings will generally have odd whitespace etc
      // that |EVP_DecodeBase64| will reject.
      continue;
    }

    const std::string encoded(RemoveNewlines(t->encoded));
    std::vector<uint8_t> out_vec(encoded.size());
    uint8_t *out = out_vec.data();

    int ok = EVP_DecodeBase64(out, &len, out_vec.size(),
                              (const uint8_t *)encoded.data(), encoded.size());

    if (t->relation == invalid) {
      if (ok) {
        fprintf(stderr, "decode(\"%s\") didn't fail but should have\n",
                encoded.c_str());
        return false;
      }
    } else if (t->relation == canonical) {
      if (!ok) {
        fprintf(stderr, "decode(\"%s\") failed\n", encoded.c_str());
        return false;
      }

      if (len != strlen(t->decoded) ||
          memcmp(out, t->decoded, len) != 0) {
        fprintf(stderr, "decode(\"%s\") = \"%.*s\", want \"%s\"\n",
                encoded.c_str(), (int)len, (const char*)out, t->decoded);
        return false;
      }
    }
  }

  return true;
}

static bool TestDecodeBlock() {
  for (unsigned i = 0; i < kNumTests; i++) {
    const TestVector *t = &kTestVectors[i];
    if (t->relation != canonical) {
      continue;
    }

    std::string encoded(RemoveNewlines(t->encoded));

    std::vector<uint8_t> out_vec(encoded.size());
    uint8_t *out = out_vec.data();

    // Test that the padding behavior of the deprecated API is preserved.
    int ret =
        EVP_DecodeBlock(out, (const uint8_t *)encoded.data(), encoded.size());
    if (ret < 0) {
      fprintf(stderr, "EVP_DecodeBlock(\"%s\") failed\n", t->encoded);
      return false;
    }
    if (ret % 3 != 0) {
      fprintf(stderr, "EVP_DecodeBlock did not ignore padding\n");
      return false;
    }
    size_t expected_len = strlen(t->decoded);
    if (expected_len % 3 != 0) {
      ret -= 3 - (expected_len % 3);
    }
    if (static_cast<size_t>(ret) != strlen(t->decoded) ||
        memcmp(out, t->decoded, ret) != 0) {
      fprintf(stderr, "decode(\"%s\") = \"%.*s\", want \"%s\"\n",
              t->encoded, ret, (const char*)out, t->decoded);
      return false;
    }
  }

  return true;
}

static bool TestEncodeDecode() {
  for (unsigned test_num = 0; test_num < kNumTests; test_num++) {
    const TestVector *t = &kTestVectors[test_num];

    EVP_ENCODE_CTX ctx;
    const size_t decoded_len = strlen(t->decoded);

    if (t->relation == canonical) {
      size_t max_encoded_len;
      if (!EVP_EncodedLength(&max_encoded_len, decoded_len)) {
        fprintf(stderr, "#%u: EVP_EncodedLength failed\n", test_num);
        return false;
      }

      // EVP_EncodeUpdate will output new lines every 64 bytes of output so we
      // need slightly more than |EVP_EncodedLength| returns. */
      max_encoded_len += (max_encoded_len + 63) >> 6;
      std::vector<uint8_t> out_vec(max_encoded_len);
      uint8_t *out = out_vec.data();

      EVP_EncodeInit(&ctx);

      int out_len;
      EVP_EncodeUpdate(&ctx, out, &out_len,
                       reinterpret_cast<const uint8_t *>(t->decoded),
                       decoded_len);
      size_t total = out_len;

      EVP_EncodeFinal(&ctx, out + total, &out_len);
      total += out_len;

      if (total != strlen(t->encoded) || memcmp(out, t->encoded, total) != 0) {
        fprintf(stderr, "#%u: EVP_EncodeUpdate produced different output: '%s' (%u)\n",
                test_num, out, static_cast<unsigned>(total));
        return false;
      }
    }

    std::vector<uint8_t> out_vec(strlen(t->encoded));
    uint8_t *out = out_vec.data();

    EVP_DecodeInit(&ctx);
    int out_len;
    size_t total = 0;
    int ret = EVP_DecodeUpdate(&ctx, out, &out_len,
                               reinterpret_cast<const uint8_t *>(t->encoded),
                               strlen(t->encoded));
    if (ret != -1) {
      total = out_len;
      ret = EVP_DecodeFinal(&ctx, out + total, &out_len);
      total += out_len;
    }

    switch (t->relation) {
      case canonical:
      case valid:
        if (ret == -1) {
          fprintf(stderr, "#%u: EVP_DecodeUpdate failed\n", test_num);
          return false;
        }
        if (total != decoded_len || memcmp(out, t->decoded, decoded_len)) {
          fprintf(stderr, "#%u: EVP_DecodeUpdate produced incorrect output\n",
                  test_num);
          return false;
        }
        break;

      case invalid:
        if (ret != -1) {
          fprintf(stderr, "#%u: EVP_DecodeUpdate was successful but shouldn't have been\n", test_num);
          return false;
        }
        break;
    }
  }

  return true;
}

static bool TestDecodeUpdateStreaming() {
  for (unsigned test_num = 0; test_num < kNumTests; test_num++) {
    const TestVector *t = &kTestVectors[test_num];
    if (t->relation == invalid) {
      continue;
    }

    const size_t encoded_len = strlen(t->encoded);

    std::vector<uint8_t> out(encoded_len);

    for (size_t chunk_size = 1; chunk_size <= encoded_len; chunk_size++) {
      size_t out_len = 0;
      EVP_ENCODE_CTX ctx;
      EVP_DecodeInit(&ctx);

      for (size_t i = 0; i < encoded_len;) {
        size_t todo = encoded_len - i;
        if (todo > chunk_size) {
          todo = chunk_size;
        }

        int bytes_written;
        int ret = EVP_DecodeUpdate(
            &ctx, out.data() + out_len, &bytes_written,
            reinterpret_cast<const uint8_t *>(t->encoded + i), todo);
        i += todo;

        switch (ret) {
          case -1:
            fprintf(stderr, "#%u: EVP_DecodeUpdate returned error\n", test_num);
            return 0;
          case 0:
            out_len += bytes_written;
            if (i == encoded_len ||
                (i + 1 == encoded_len && t->encoded[i] == '\n') ||
                /* If there was an '-' in the input (which means “EOF”) then
                 * this loop will continue to test that |EVP_DecodeUpdate| will
                 * ignore the remainder of the input. */
                strchr(t->encoded, '-') != nullptr) {
              break;
            }

            fprintf(stderr,
                    "#%u: EVP_DecodeUpdate returned zero before end of "
                    "encoded data\n",
                    test_num);
            return 0;
          default:
            out_len += bytes_written;
        }
      }

      int bytes_written;
      int ret = EVP_DecodeFinal(&ctx, out.data() + out_len, &bytes_written);
      if (ret == -1) {
        fprintf(stderr, "#%u: EVP_DecodeFinal returned error\n", test_num);
        return 0;
      }
      out_len += bytes_written;

      if (out_len != strlen(t->decoded) ||
          memcmp(out.data(), t->decoded, out_len) != 0) {
        fprintf(stderr, "#%u: incorrect output\n", test_num);
        return 0;
      }
    }
  }

  return true;
}

int main(void) {
  CRYPTO_library_init();

  if (!TestEncodeBlock() ||
      !TestDecodeBase64() ||
      !TestDecodeBlock() ||
      !TestDecodeUpdateStreaming() ||
      !TestEncodeDecode()) {
    return 1;
  }

  printf("PASS\n");
  return 0;
}
