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

#include <stdio.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/bytestring.h>
#include <openssl/crypto.h>
#include <openssl/obj.h>


static bool TestBasic() {
  static const int kNID = NID_sha256WithRSAEncryption;
  static const char kShortName[] = "RSA-SHA256";
  static const char kLongName[] = "sha256WithRSAEncryption";
  static const char kText[] = "1.2.840.113549.1.1.11";
  static const uint8_t kDER[] = {
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
  };

  CBS cbs;
  CBS_init(&cbs, kDER, sizeof(kDER));
  if (OBJ_cbs2nid(&cbs) != kNID ||
      OBJ_sn2nid(kShortName) != kNID ||
      OBJ_ln2nid(kLongName) != kNID ||
      OBJ_txt2nid(kShortName) != kNID ||
      OBJ_txt2nid(kLongName) != kNID ||
      OBJ_txt2nid(kText) != kNID) {
    return false;
  }

  if (strcmp(kShortName, OBJ_nid2sn(kNID)) != 0 ||
      strcmp(kLongName, OBJ_nid2ln(kNID)) != 0) {
    return false;
  }

  if (OBJ_sn2nid("this is not an OID") != NID_undef ||
      OBJ_ln2nid("this is not an OID") != NID_undef ||
      OBJ_txt2nid("this is not an OID") != NID_undef) {
    return false;
  }

  CBS_init(&cbs, NULL, 0);
  if (OBJ_cbs2nid(&cbs) != NID_undef) {
    return false;
  }

  // 1.2.840.113554.4.1.72585.2 (https://davidben.net/oid).
  static const uint8_t kUnknownDER[] = {
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x02,
  };
  CBS_init(&cbs, kUnknownDER, sizeof(kUnknownDER));
  if (OBJ_cbs2nid(&cbs) != NID_undef) {
    return false;
  }

  return true;
}

static bool TestSignatureAlgorithms() {
  int digest_nid, pkey_nid;
  if (!OBJ_find_sigid_algs(NID_sha256WithRSAEncryption, &digest_nid,
                           &pkey_nid) ||
      digest_nid != NID_sha256 || pkey_nid != NID_rsaEncryption) {
    return false;
  }

  if (OBJ_find_sigid_algs(NID_sha256, &digest_nid, &pkey_nid)) {
    return false;
  }

  int sign_nid;
  if (!OBJ_find_sigid_by_algs(&sign_nid, NID_sha256, NID_rsaEncryption) ||
      sign_nid != NID_sha256WithRSAEncryption) {
    return false;
  }

  if (OBJ_find_sigid_by_algs(&sign_nid, NID_dsa, NID_rsaEncryption)) {
    return false;
  }

  return true;
}

static bool ExpectObj2Txt(const uint8_t *der, size_t der_len,
                          bool always_return_oid, const char *expected) {
  ASN1_OBJECT obj;
  memset(&obj, 0, sizeof(obj));
  obj.data = der;
  obj.length = static_cast<int>(der_len);

  int expected_len = static_cast<int>(strlen(expected));

  int len = OBJ_obj2txt(nullptr, 0, &obj, always_return_oid);
  if (len != expected_len) {
    fprintf(stderr,
            "OBJ_obj2txt of %s with out_len = 0 returned %d, wanted %d.\n",
            expected, len, expected_len);
    return false;
  }

  char short_buf[1];
  memset(short_buf, 0xff, sizeof(short_buf));
  len = OBJ_obj2txt(short_buf, sizeof(short_buf), &obj, always_return_oid);
  if (len != expected_len) {
    fprintf(stderr,
            "OBJ_obj2txt of %s with out_len = 1 returned %d, wanted %d.\n",
            expected, len, expected_len);
    return false;
  }

  if (memchr(short_buf, '\0', sizeof(short_buf)) == nullptr) {
    fprintf(stderr,
            "OBJ_obj2txt of %s with out_len = 1 did not NUL-terminate the "
            "output.\n",
            expected);
    return false;
  }

  char buf[256];
  len = OBJ_obj2txt(buf, sizeof(buf), &obj, always_return_oid);
  if (len != expected_len) {
    fprintf(stderr,
            "OBJ_obj2txt of %s with out_len = 256 returned %d, wanted %d.\n",
            expected, len, expected_len);
    return false;
  }

  if (strcmp(buf, expected) != 0) {
    fprintf(stderr, "OBJ_obj2txt returned \"%s\"; wanted \"%s\".\n", buf,
            expected);
    return false;
  }

  return true;
}

static bool TestObj2Txt() {
  // kSHA256WithRSAEncryption is the DER representation of
  // 1.2.840.113549.1.1.11, id-sha256WithRSAEncryption.
  static const uint8_t kSHA256WithRSAEncryption[] = {
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
  };

  // kBasicConstraints is the DER representation of 2.5.29.19,
  // id-basicConstraints.
  static const uint8_t kBasicConstraints[] = {
      0x55, 0x1d, 0x13,
  };

  // kTestOID is the DER representation of 1.2.840.113554.4.1.72585.0,
  // from https://davidben.net/oid.
  static const uint8_t kTestOID[] = {
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x00,
  };

  if (!ExpectObj2Txt(kSHA256WithRSAEncryption, sizeof(kSHA256WithRSAEncryption),
                     true /* don't return name */, "1.2.840.113549.1.1.11") ||
      !ExpectObj2Txt(kSHA256WithRSAEncryption, sizeof(kSHA256WithRSAEncryption),
                     false /* return name */, "sha256WithRSAEncryption") ||
      !ExpectObj2Txt(kBasicConstraints, sizeof(kBasicConstraints),
                     true /* don't return name */, "2.5.29.19") ||
      !ExpectObj2Txt(kBasicConstraints, sizeof(kBasicConstraints),
                     false /* return name */, "X509v3 Basic Constraints") ||
      !ExpectObj2Txt(kTestOID, sizeof(kTestOID), true /* don't return name */,
                     "1.2.840.113554.4.1.72585.0") ||
      !ExpectObj2Txt(kTestOID, sizeof(kTestOID), false /* return name */,
                     "1.2.840.113554.4.1.72585.0") ||
      // Python depends on the empty OID successfully encoding as the empty
      // string.
      !ExpectObj2Txt(nullptr, 0, false /* return name */, "") ||
      !ExpectObj2Txt(nullptr, 0, true /* don't return name */, "")) {
    return false;
  }

  ASN1_OBJECT obj;
  memset(&obj, 0, sizeof(obj));

  // kNonMinimalOID is kBasicConstraints with the final component non-minimally
  // encoded.
  static const uint8_t kNonMinimalOID[] = {
      0x55, 0x1d, 0x80, 0x13,
  };
  obj.data = kNonMinimalOID;
  obj.length = sizeof(kNonMinimalOID);
  if (OBJ_obj2txt(NULL, 0, &obj, 0) != -1) {
    fprintf(stderr, "OBJ_obj2txt accepted non-minimal OIDs.\n");
    return false;
  }

  // kOverflowOID is the DER representation of
  // 1.2.840.113554.4.1.72585.18446744073709551616. (The final value is 2^64.)
  static const uint8_t kOverflowOID[] = {
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x04, 0x01, 0x84, 0xb7, 0x09,
      0x82, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
  };
  obj.data = kOverflowOID;
  obj.length = sizeof(kOverflowOID);
  if (OBJ_obj2txt(NULL, 0, &obj, 0) != -1) {
    fprintf(stderr, "OBJ_obj2txt accepted an OID with a large component.\n");
    return false;
  }

  // kInvalidOID is a mis-encoded version of kBasicConstraints with the final
  // octet having the high bit set.
  static const uint8_t kInvalidOID[] = {
      0x55, 0x1d, 0x93,
  };
  obj.data = kInvalidOID;
  obj.length = sizeof(kInvalidOID);
  if (OBJ_obj2txt(NULL, 0, &obj, 0) != -1) {
    fprintf(stderr, "OBJ_obj2txt accepted a mis-encoded OID.\n");
    return false;
  }

  return true;
}

int main() {
  CRYPTO_library_init();

  if (!TestBasic() ||
      !TestSignatureAlgorithms() ||
      !TestObj2Txt()) {
    return 1;
  }

  printf("PASS\n");
  return 0;
}
