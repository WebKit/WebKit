/* Copyright (c) 2019, Google Inc.
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

#include <string>
#include <vector>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstdarg>

#include <openssl/aes.h>
#include <openssl/bn.h>
#include <openssl/digest.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdsa.h>
#include <openssl/hmac.h>
#include <openssl/obj.h>
#include <openssl/sha.h>
#include <openssl/span.h>

#include "../../../../crypto/fipsmodule/rand/internal.h"

static constexpr size_t kMaxArgs = 8;
static constexpr size_t kMaxArgLength = (1 << 20);
static constexpr size_t kMaxNameLength = 30;

static_assert((kMaxArgs - 1 * kMaxArgLength) + kMaxNameLength > (1 << 30),
              "Argument limits permit excessive messages");

using namespace bssl;

static bool ReadAll(int fd, void *in_data, size_t data_len) {
  uint8_t *data = reinterpret_cast<uint8_t *>(in_data);
  size_t done = 0;

  while (done < data_len) {
    ssize_t r;
    do {
      r = read(fd, &data[done], data_len - done);
    } while (r == -1 && errno == EINTR);

    if (r <= 0) {
      return false;
    }

    done += r;
  }

  return true;
}

template <typename... Args>
static bool WriteReply(int fd, Args... args) {
  std::vector<Span<const uint8_t>> spans = {args...};
  if (spans.empty() || spans.size() > kMaxArgs) {
    abort();
  }

  uint32_t nums[1 + kMaxArgs];
  iovec iovs[kMaxArgs + 1];
  nums[0] = spans.size();
  iovs[0].iov_base = nums;
  iovs[0].iov_len = sizeof(uint32_t) * (1 + spans.size());

  for (size_t i = 0; i < spans.size(); i++) {
    const auto &span = spans[i];
    nums[i + 1] = span.size();
    iovs[i + 1].iov_base = const_cast<uint8_t *>(span.data());
    iovs[i + 1].iov_len = span.size();
  }

  const size_t num_iov = spans.size() + 1;
  size_t iov_done = 0;
  while (iov_done < num_iov) {
    ssize_t r;
    do {
      r = writev(fd, &iovs[iov_done], num_iov - iov_done);
    } while (r == -1 && errno == EINTR);

    if (r <= 0) {
      return false;
    }

    size_t written = r;
    for (size_t i = iov_done; written > 0 && i < num_iov; i++) {
      iovec &iov = iovs[i];

      size_t done = written;
      if (done > iov.iov_len) {
        done = iov.iov_len;
      }

      iov.iov_base = reinterpret_cast<uint8_t *>(iov.iov_base) + done;
      iov.iov_len -= done;
      written -= done;

      if (iov.iov_len == 0) {
        iov_done++;
      }
    }

    assert(written == 0);
  }

  return true;
}

static bool GetConfig(const Span<const uint8_t> args[]) {
  static constexpr char kConfig[] =
      R"([
      {
        "algorithm": "SHA2-224",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "SHA2-256",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "SHA2-384",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "SHA2-512",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "SHA-1",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "ACVP-AES-ECB",
        "revision": "1.0",
        "direction": ["encrypt", "decrypt"],
        "keyLen": [128, 192, 256]
      },
      {
        "algorithm": "ACVP-AES-CBC",
        "revision": "1.0",
        "direction": ["encrypt", "decrypt"],
        "keyLen": [128, 192, 256]
      },
      {
        "algorithm": "HMAC-SHA-1",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 160, "increment": 8
        }]
      },
      {
        "algorithm": "HMAC-SHA2-224",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 224, "increment": 8
        }]
      },
      {
        "algorithm": "HMAC-SHA2-256",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 256, "increment": 8
        }]
      },
      {
        "algorithm": "HMAC-SHA2-384",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 384, "increment": 8
        }]
      },
      {
        "algorithm": "HMAC-SHA2-512",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 512, "increment": 8
        }]
      },
      {
        "algorithm": "ctrDRBG",
        "revision": "1.0",
        "predResistanceEnabled": [false],
        "reseedImplemented": false,
        "capabilities": [{
          "mode": "AES-256",
          "derFuncEnabled": false,
          "entropyInputLen": [384],
          "nonceLen": [0],
          "persoStringLen": [{"min": 0, "max": 384, "increment": 16}],
          "additionalInputLen": [
            {"min": 0, "max": 384, "increment": 16}
          ],
          "returnedBitsLen": 2048
        }]
      },
      {
        "algorithm": "ECDSA",
        "mode": "keyGen",
        "revision": "1.0",
        "curve": [
          "P-224",
          "P-256",
          "P-384",
          "P-521"
        ],
        "secretGenerationMode": [
          "testing candidates"
        ]
      },
      {
        "algorithm": "ECDSA",
        "mode": "keyVer",
        "revision": "1.0",
        "curve": [
          "P-224",
          "P-256",
          "P-384",
          "P-521"
        ]
      },
      {
        "algorithm": "ECDSA",
        "mode": "sigGen",
        "revision": "1.0",
        "capabilities": [{
          "curve": [
            "P-224",
            "P-256",
            "P-384",
            "P-521"
          ],
          "hashAlg": [
            "SHA2-224",
            "SHA2-256",
            "SHA2-384",
            "SHA2-512"
          ]
        }]
      },
      {
        "algorithm": "ECDSA",
        "mode": "sigVer",
        "revision": "1.0",
        "capabilities": [{
          "curve": [
            "P-224",
            "P-256",
            "P-384",
            "P-521"
          ],
          "hashAlg": [
            "SHA2-224",
            "SHA2-256",
            "SHA2-384",
            "SHA2-512"
          ]
        }]
      }
    ])";
  return WriteReply(
      STDOUT_FILENO,
      Span<const uint8_t>(reinterpret_cast<const uint8_t *>(kConfig),
                          sizeof(kConfig) - 1));
}

template <uint8_t *(*OneShotHash)(const uint8_t *, size_t, uint8_t *),
          size_t DigestLength>
static bool Hash(const Span<const uint8_t> args[]) {
  uint8_t digest[DigestLength];
  OneShotHash(args[0].data(), args[0].size(), digest);
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(digest));
}

template <int (*SetKey)(const uint8_t *key, unsigned bits, AES_KEY *out),
          void (*Block)(const uint8_t *in, uint8_t *out, const AES_KEY *key)>
static bool AES(const Span<const uint8_t> args[]) {
  AES_KEY key;
  if (SetKey(args[0].data(), args[0].size() * 8, &key) != 0) {
    return false;
  }
  if (args[1].size() % AES_BLOCK_SIZE != 0) {
    return false;
  }

  std::vector<uint8_t> out;
  out.resize(args[1].size());
  for (size_t i = 0; i < args[1].size(); i += AES_BLOCK_SIZE) {
    Block(args[1].data() + i, &out[i], &key);
  }
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(out));
}

template <int (*SetKey)(const uint8_t *key, unsigned bits, AES_KEY *out),
          int Direction>
static bool AES_CBC(const Span<const uint8_t> args[]) {
  AES_KEY key;
  if (SetKey(args[0].data(), args[0].size() * 8, &key) != 0) {
    return false;
  }
  if (args[1].size() % AES_BLOCK_SIZE != 0 ||
      args[2].size() != AES_BLOCK_SIZE) {
    return false;
  }
  uint8_t iv[AES_BLOCK_SIZE];
  memcpy(iv, args[2].data(), AES_BLOCK_SIZE);

  std::vector<uint8_t> out;
  out.resize(args[1].size());
  AES_cbc_encrypt(args[1].data(), out.data(), args[1].size(), &key, iv,
                  Direction);
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(out));
}

template <const EVP_MD *HashFunc()>
static bool HMAC(const Span<const uint8_t> args[]) {
  const EVP_MD *const md = HashFunc();
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (::HMAC(md, args[1].data(), args[1].size(), args[0].data(), args[0].size(),
             digest, &digest_len) == nullptr) {
    return false;
  }
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(digest, digest_len));
}

static bool DRBG(const Span<const uint8_t> args[]) {
  const auto out_len_bytes = args[0];
  const auto entropy = args[1];
  const auto personalisation = args[2];
  const auto additional_data1 = args[3];
  const auto additional_data2 = args[4];
  const auto nonce = args[5];

  uint32_t out_len;
  if (out_len_bytes.size() != sizeof(out_len) ||
      entropy.size() != CTR_DRBG_ENTROPY_LEN ||
      // nonces are not supported
      nonce.size() != 0) {
    return false;
  }
  memcpy(&out_len, out_len_bytes.data(), sizeof(out_len));
  if (out_len > (1 << 24)) {
    return false;
  }
  std::vector<uint8_t> out(out_len);

  CTR_DRBG_STATE drbg;
  if (!CTR_DRBG_init(&drbg, entropy.data(), personalisation.data(),
                     personalisation.size()) ||
      !CTR_DRBG_generate(&drbg, out.data(), out_len, additional_data1.data(),
                         additional_data1.size()) ||
      !CTR_DRBG_generate(&drbg, out.data(), out_len, additional_data2.data(),
                         additional_data2.size())) {
    return false;
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(out));
}

static bool StringEq(Span<const uint8_t> a, const char *b) {
  const size_t len = strlen(b);
  return a.size() == len && memcmp(a.data(), b, len) == 0;
}

static bssl::UniquePtr<EC_KEY> ECKeyFromName(Span<const uint8_t> name) {
  int nid;
  if (StringEq(name, "P-224")) {
    nid = NID_secp224r1;
  } else if (StringEq(name, "P-256")) {
    nid = NID_X9_62_prime256v1;
  } else if (StringEq(name, "P-384")) {
    nid = NID_secp384r1;
  } else if (StringEq(name, "P-521")) {
    nid = NID_secp521r1;
  } else {
    return nullptr;
  }

  return bssl::UniquePtr<EC_KEY>(EC_KEY_new_by_curve_name(nid));
}

static std::vector<uint8_t> BIGNUMBytes(const BIGNUM *bn) {
  const size_t len = BN_num_bytes(bn);
  std::vector<uint8_t> ret(len);
  BN_bn2bin(bn, ret.data());
  return ret;
}

static std::pair<std::vector<uint8_t>, std::vector<uint8_t>> GetPublicKeyBytes(
    const EC_KEY *key) {
  bssl::UniquePtr<BIGNUM> x(BN_new());
  bssl::UniquePtr<BIGNUM> y(BN_new());
  if (!EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(key),
                                           EC_KEY_get0_public_key(key), x.get(),
                                           y.get(), /*ctx=*/nullptr)) {
    abort();
  }

  std::vector<uint8_t> x_bytes = BIGNUMBytes(x.get());
  std::vector<uint8_t> y_bytes = BIGNUMBytes(y.get());

  return std::make_pair(std::move(x_bytes), std::move(y_bytes));
}

static bool ECDSAKeyGen(const Span<const uint8_t> args[]) {
  bssl::UniquePtr<EC_KEY> key = ECKeyFromName(args[0]);
  if (!key || !EC_KEY_generate_key_fips(key.get())) {
    return false;
  }

  const auto pub_key = GetPublicKeyBytes(key.get());
  std::vector<uint8_t> d_bytes =
      BIGNUMBytes(EC_KEY_get0_private_key(key.get()));

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(d_bytes),
                    Span<const uint8_t>(pub_key.first),
                    Span<const uint8_t>(pub_key.second));
}

static bssl::UniquePtr<BIGNUM> BytesToBIGNUM(Span<const uint8_t> bytes) {
  bssl::UniquePtr<BIGNUM> bn(BN_new());
  BN_bin2bn(bytes.data(), bytes.size(), bn.get());
  return bn;
}

static bool ECDSAKeyVer(const Span<const uint8_t> args[]) {
  bssl::UniquePtr<EC_KEY> key = ECKeyFromName(args[0]);
  if (!key) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> x(BytesToBIGNUM(args[1]));
  bssl::UniquePtr<BIGNUM> y(BytesToBIGNUM(args[2]));

  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(EC_KEY_get0_group(key.get())));
  uint8_t reply[1];
  if (!EC_POINT_set_affine_coordinates_GFp(EC_KEY_get0_group(key.get()),
                                           point.get(), x.get(), y.get(),
                                           /*ctx=*/nullptr) ||
      !EC_KEY_set_public_key(key.get(), point.get()) ||
      !EC_KEY_check_fips(key.get())) {
    reply[0] = 0;
  } else {
    reply[0] = 1;
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(reply));
}

static const EVP_MD *HashFromName(Span<const uint8_t> name) {
  if (StringEq(name, "SHA2-224")) {
    return EVP_sha224();
  } else if (StringEq(name, "SHA2-256")) {
    return EVP_sha256();
  } else if (StringEq(name, "SHA2-384")) {
    return EVP_sha384();
  } else if (StringEq(name, "SHA2-512")) {
    return EVP_sha512();
  } else {
    return nullptr;
  }
}

static bool ECDSASigGen(const Span<const uint8_t> args[]) {
  bssl::UniquePtr<EC_KEY> key = ECKeyFromName(args[0]);
  bssl::UniquePtr<BIGNUM> d = BytesToBIGNUM(args[1]);
  const EVP_MD *hash = HashFromName(args[2]);
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (!key || !hash ||
      !EVP_Digest(args[3].data(), args[3].size(), digest, &digest_len, hash,
                  /*impl=*/nullptr) ||
      !EC_KEY_set_private_key(key.get(), d.get())) {
    return false;
  }

  bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_do_sign(digest, digest_len, key.get()));
  if (!sig) {
    return false;
  }

  std::vector<uint8_t> r_bytes(BIGNUMBytes(sig->r));
  std::vector<uint8_t> s_bytes(BIGNUMBytes(sig->s));

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(r_bytes),
                    Span<const uint8_t>(s_bytes));
}

static bool ECDSASigVer(const Span<const uint8_t> args[]) {
  bssl::UniquePtr<EC_KEY> key = ECKeyFromName(args[0]);
  const EVP_MD *hash = HashFromName(args[1]);
  auto msg = args[2];
  bssl::UniquePtr<BIGNUM> x(BytesToBIGNUM(args[3]));
  bssl::UniquePtr<BIGNUM> y(BytesToBIGNUM(args[4]));
  bssl::UniquePtr<BIGNUM> r(BytesToBIGNUM(args[5]));
  bssl::UniquePtr<BIGNUM> s(BytesToBIGNUM(args[6]));
  ECDSA_SIG sig;
  sig.r = r.get();
  sig.s = s.get();

  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (!key || !hash ||
      !EVP_Digest(msg.data(), msg.size(), digest, &digest_len, hash,
                  /*impl=*/nullptr)) {
    return false;
  }

  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(EC_KEY_get0_group(key.get())));
  uint8_t reply[1];
  if (!EC_POINT_set_affine_coordinates_GFp(EC_KEY_get0_group(key.get()),
                                           point.get(), x.get(), y.get(),
                                           /*ctx=*/nullptr) ||
      !EC_KEY_set_public_key(key.get(), point.get()) ||
      !EC_KEY_check_fips(key.get()) ||
      !ECDSA_do_verify(digest, digest_len, &sig, key.get())) {
    reply[0] = 0;
  } else {
    reply[0] = 1;
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(reply));
}

static constexpr struct {
  const char name[kMaxNameLength + 1];
  uint8_t expected_args;
  bool (*handler)(const Span<const uint8_t>[]);
} kFunctions[] = {
    {"getConfig", 0, GetConfig},
    {"SHA-1", 1, Hash<SHA1, SHA_DIGEST_LENGTH>},
    {"SHA2-224", 1, Hash<SHA224, SHA224_DIGEST_LENGTH>},
    {"SHA2-256", 1, Hash<SHA256, SHA256_DIGEST_LENGTH>},
    {"SHA2-384", 1, Hash<SHA384, SHA256_DIGEST_LENGTH>},
    {"SHA2-512", 1, Hash<SHA512, SHA512_DIGEST_LENGTH>},
    {"AES/encrypt", 2, AES<AES_set_encrypt_key, AES_encrypt>},
    {"AES/decrypt", 2, AES<AES_set_decrypt_key, AES_decrypt>},
    {"AES-CBC/encrypt", 3, AES_CBC<AES_set_encrypt_key, AES_ENCRYPT>},
    {"AES-CBC/decrypt", 3, AES_CBC<AES_set_decrypt_key, AES_DECRYPT>},
    {"HMAC-SHA-1", 2, HMAC<EVP_sha1>},
    {"HMAC-SHA2-224", 2, HMAC<EVP_sha224>},
    {"HMAC-SHA2-256", 2, HMAC<EVP_sha256>},
    {"HMAC-SHA2-384", 2, HMAC<EVP_sha384>},
    {"HMAC-SHA2-512", 2, HMAC<EVP_sha512>},
    {"ctrDRBG/AES-256", 6, DRBG},
    {"ECDSA/keyGen", 1, ECDSAKeyGen},
    {"ECDSA/keyVer", 3, ECDSAKeyVer},
    {"ECDSA/sigGen", 4, ECDSASigGen},
    {"ECDSA/sigVer", 7, ECDSASigVer},
};

int main() {
  uint32_t nums[1 + kMaxArgs];
  std::unique_ptr<uint8_t[]> buf;
  size_t buf_len = 0;
  Span<const uint8_t> args[kMaxArgs];

  for (;;) {
    if (!ReadAll(STDIN_FILENO, nums, sizeof(uint32_t) * 2)) {
      return 1;
    }

    const size_t num_args = nums[0];
    if (num_args == 0) {
      fprintf(stderr, "Invalid, zero-argument operation requested.\n");
      return 2;
    } else if (num_args > kMaxArgs) {
      fprintf(stderr,
              "Operation requested with %zu args, but %zu is the limit.\n",
              num_args, kMaxArgs);
      return 2;
    }

    if (num_args > 1 &&
        !ReadAll(STDIN_FILENO, &nums[2], sizeof(uint32_t) * (num_args - 1))) {
      return 1;
    }

    size_t need = 0;
    for (size_t i = 0; i < num_args; i++) {
      const size_t arg_length = nums[i + 1];
      if (i == 0 && arg_length > kMaxNameLength) {
        fprintf(stderr,
                "Operation with name of length %zu exceeded limit of %zu.\n",
                arg_length, kMaxNameLength);
        return 2;
      } else if (arg_length > kMaxArgLength) {
        fprintf(
            stderr,
            "Operation with argument of length %zu exceeded limit of %zu.\n",
            arg_length, kMaxArgLength);
        return 2;
      }

      // static_assert around kMaxArgs etc enforces that this doesn't overflow.
      need += arg_length;
    }

    if (need > buf_len) {
      size_t alloced = need + (need >> 1);
      if (alloced < need) {
        abort();
      }
      buf.reset(new uint8_t[alloced]);
      buf_len = alloced;
    }

    if (!ReadAll(STDIN_FILENO, buf.get(), need)) {
      return 1;
    }

    size_t offset = 0;
    for (size_t i = 0; i < num_args; i++) {
      args[i] = Span<const uint8_t>(&buf[offset], nums[i + 1]);
      offset += nums[i + 1];
    }

    bool found = true;
    for (const auto &func : kFunctions) {
      if (args[0].size() == strlen(func.name) &&
          memcmp(args[0].data(), func.name, args[0].size()) == 0) {
        if (num_args - 1 != func.expected_args) {
          fprintf(stderr,
                  "\'%s\' operation received %zu arguments but expected %u.\n",
                  func.name, num_args - 1, func.expected_args);
          return 2;
        }

        if (!func.handler(&args[1])) {
          return 4;
        }

        found = true;
        break;
      }
    }

    if (!found) {
      const std::string name(reinterpret_cast<const char *>(args[0].data()),
                             args[0].size());
      fprintf(stderr, "Unknown operation: %s\n", name.c_str());
      return 3;
    }
  }
}
