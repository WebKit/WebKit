/* Copyright (c) 2017, Google Inc.
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

// cavp_keywrap_test processes a NIST CAVP AES test vector request file and
// emits the corresponding response.

#include <stdlib.h>

#include <openssl/aes.h>
#include <openssl/crypto.h>

#include "../crypto/test/file_test.h"
#include "cavp_test_util.h"


namespace {

struct TestCtx {
  bool encrypt;
};

}

static bool AESKeyWrap(std::vector<uint8_t> *out, bool encrypt,
                       const std::vector<uint8_t> &key,
                       const std::vector<uint8_t> &in) {
  size_t key_bits = key.size() * 8;
  if (key_bits != 128 && key_bits != 256) {
    return false;
  }
  AES_KEY aes_key;

  if (encrypt) {
    out->resize(in.size() + 8);
    if (AES_set_encrypt_key(key.data(), key_bits, &aes_key) ||
        AES_wrap_key(&aes_key, nullptr, out->data(), in.data(), in.size()) ==
            -1) {
      return false;
    }
  } else {
    out->resize(in.size() - 8);
    if (AES_set_decrypt_key(key.data(), key_bits, &aes_key) ||
        AES_unwrap_key(&aes_key, nullptr, out->data(), in.data(), in.size()) ==
            -1) {
      return false;
    }
  }

  return true;
}

static bool TestCipher(FileTest *t, void *arg) {
  TestCtx *ctx = reinterpret_cast<TestCtx *>(arg);

  std::string count, unused, in_label = ctx->encrypt ? "P" : "C",
                             result_label = ctx->encrypt ? "C" : "P";
  std::vector<uint8_t> key, in, result;
  // clang-format off
  if (!t->GetInstruction(&unused, "PLAINTEXT LENGTH") ||
      !t->GetAttribute(&count, "COUNT") ||
      !t->GetBytes(&key, "K") ||
      !t->GetBytes(&in, in_label)) {
    return false;
  }
  // clang-format on

  printf("%s", t->CurrentTestToString().c_str());
  if (!AESKeyWrap(&result, ctx->encrypt, key, in)) {
    if (ctx->encrypt) {
      return false;
    } else {
      printf("FAIL\r\n\r\n");
    }
  } else {
    printf("%s = %s\r\n\r\n", result_label.c_str(),
           EncodeHex(result.data(), result.size()).c_str());
  }

  return true;
}

static int usage(char *arg) {
  fprintf(
      stderr,
      "usage: %s (enc|dec) (128|256) <test file>\n",
      arg);
  return 1;
}

int cavp_keywrap_test_main(int argc, char **argv) {
  if (argc != 4) {
    return usage(argv[0]);
  }

  const std::string op(argv[1]);
  bool encrypt;
  if (op == "enc") {
    encrypt = true;
  } else if (op == "dec") {
    encrypt = false;
  } else {
    return usage(argv[0]);
  }

  TestCtx ctx = {encrypt};

  FileTest::Options opts;
  opts.path = argv[3];
  opts.callback = TestCipher;
  opts.arg = &ctx;
  opts.silent = true;
  opts.comment_callback = EchoComment;
  return FileTestMain(opts);
}
