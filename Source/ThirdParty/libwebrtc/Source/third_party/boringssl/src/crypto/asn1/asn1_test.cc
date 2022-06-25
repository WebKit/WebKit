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

#include <limits.h>
#include <stdio.h>

#include <vector>

#include <gtest/gtest.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/span.h>
#include <openssl/x509v3.h>

#include "../test/test_util.h"


// kTag128 is an ASN.1 structure with a universal tag with number 128.
static const uint8_t kTag128[] = {
    0x1f, 0x81, 0x00, 0x01, 0x00,
};

// kTag258 is an ASN.1 structure with a universal tag with number 258.
static const uint8_t kTag258[] = {
    0x1f, 0x82, 0x02, 0x01, 0x00,
};

static_assert(V_ASN1_NEG_INTEGER == 258,
              "V_ASN1_NEG_INTEGER changed. Update kTag258 to collide with it.");

// kTagOverflow is an ASN.1 structure with a universal tag with number 2^35-1,
// which will not fit in an int.
static const uint8_t kTagOverflow[] = {
    0x1f, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x01, 0x00,
};

TEST(ASN1Test, LargeTags) {
  const uint8_t *p = kTag258;
  bssl::UniquePtr<ASN1_TYPE> obj(d2i_ASN1_TYPE(NULL, &p, sizeof(kTag258)));
  EXPECT_FALSE(obj) << "Parsed value with illegal tag" << obj->type;
  ERR_clear_error();

  p = kTagOverflow;
  obj.reset(d2i_ASN1_TYPE(NULL, &p, sizeof(kTagOverflow)));
  EXPECT_FALSE(obj) << "Parsed value with tag overflow" << obj->type;
  ERR_clear_error();

  p = kTag128;
  obj.reset(d2i_ASN1_TYPE(NULL, &p, sizeof(kTag128)));
  ASSERT_TRUE(obj);
  EXPECT_EQ(128, obj->type);
  const uint8_t kZero = 0;
  EXPECT_EQ(Bytes(&kZero, 1), Bytes(obj->value.asn1_string->data,
                                    obj->value.asn1_string->length));
}

TEST(ASN1Test, IntegerSetting) {
  bssl::UniquePtr<ASN1_INTEGER> by_bn(ASN1_INTEGER_new());
  bssl::UniquePtr<ASN1_INTEGER> by_long(ASN1_INTEGER_new());
  bssl::UniquePtr<ASN1_INTEGER> by_uint64(ASN1_INTEGER_new());
  bssl::UniquePtr<BIGNUM> bn(BN_new());

  const std::vector<int64_t> kValues = {
      LONG_MIN, -2, -1, 0, 1, 2, 0xff, 0x100, 0xffff, 0x10000, LONG_MAX,
  };
  for (const auto &i : kValues) {
    SCOPED_TRACE(i);

    ASSERT_EQ(1, ASN1_INTEGER_set(by_long.get(), i));
    const uint64_t abs = i < 0 ? (0 - (uint64_t) i) : i;
    ASSERT_TRUE(BN_set_u64(bn.get(), abs));
    BN_set_negative(bn.get(), i < 0);
    ASSERT_TRUE(BN_to_ASN1_INTEGER(bn.get(), by_bn.get()));

    EXPECT_EQ(0, ASN1_INTEGER_cmp(by_bn.get(), by_long.get()));

    if (i >= 0) {
      ASSERT_EQ(1, ASN1_INTEGER_set_uint64(by_uint64.get(), i));
      EXPECT_EQ(0, ASN1_INTEGER_cmp(by_bn.get(), by_uint64.get()));
    }
  }
}

template <typename T>
void TestSerialize(T obj, int (*i2d_func)(T a, uint8_t **pp),
                   bssl::Span<const uint8_t> expected) {
  int len = static_cast<int>(expected.size());
  ASSERT_EQ(i2d_func(obj, nullptr), len);

  std::vector<uint8_t> buf(expected.size());
  uint8_t *ptr = buf.data();
  ASSERT_EQ(i2d_func(obj, &ptr), len);
  EXPECT_EQ(ptr, buf.data() + buf.size());
  EXPECT_EQ(Bytes(expected), Bytes(buf));

  // Test the allocating version.
  ptr = nullptr;
  ASSERT_EQ(i2d_func(obj, &ptr), len);
  EXPECT_EQ(Bytes(expected), Bytes(ptr, expected.size()));
  OPENSSL_free(ptr);
}

TEST(ASN1Test, SerializeObject) {
  static const uint8_t kDER[] = {0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
                                 0xf7, 0x0d, 0x01, 0x01, 0x01};
  const ASN1_OBJECT *obj = OBJ_nid2obj(NID_rsaEncryption);
  TestSerialize(obj, i2d_ASN1_OBJECT, kDER);
}

TEST(ASN1Test, SerializeBoolean) {
  static const uint8_t kTrue[] = {0x01, 0x01, 0xff};
  TestSerialize(0xff, i2d_ASN1_BOOLEAN, kTrue);
  // Other constants are also correctly encoded as TRUE.
  TestSerialize(1, i2d_ASN1_BOOLEAN, kTrue);
  TestSerialize(0x100, i2d_ASN1_BOOLEAN, kTrue);

  static const uint8_t kFalse[] = {0x01, 0x01, 0x00};
  TestSerialize(0x00, i2d_ASN1_BOOLEAN, kFalse);
}

// The templates go through a different codepath, so test them separately.
TEST(ASN1Test, SerializeEmbeddedBoolean) {
  bssl::UniquePtr<BASIC_CONSTRAINTS> val(BASIC_CONSTRAINTS_new());
  ASSERT_TRUE(val);

  // BasicConstraints defaults to FALSE, so the encoding should be empty.
  static const uint8_t kLeaf[] = {0x30, 0x00};
  val->ca = 0;
  TestSerialize(val.get(), i2d_BASIC_CONSTRAINTS, kLeaf);

  // TRUE should always be encoded as 0xff, independent of what value the caller
  // placed in the |ASN1_BOOLEAN|.
  static const uint8_t kCA[] = {0x30, 0x03, 0x01, 0x01, 0xff};
  val->ca = 0xff;
  TestSerialize(val.get(), i2d_BASIC_CONSTRAINTS, kCA);
  val->ca = 1;
  TestSerialize(val.get(), i2d_BASIC_CONSTRAINTS, kCA);
  val->ca = 0x100;
  TestSerialize(val.get(), i2d_BASIC_CONSTRAINTS, kCA);
}

TEST(ASN1Test, ASN1Type) {
  const struct {
    int type;
    std::vector<uint8_t> der;
  } kTests[] = {
      // BOOLEAN { TRUE }
      {V_ASN1_BOOLEAN, {0x01, 0x01, 0xff}},
      // BOOLEAN { FALSE }
      {V_ASN1_BOOLEAN, {0x01, 0x01, 0x00}},
      // OCTET_STRING { "a" }
      {V_ASN1_OCTET_STRING, {0x04, 0x01, 0x61}},
      // BIT_STRING { `01` `00` }
      {V_ASN1_BIT_STRING, {0x03, 0x02, 0x01, 0x00}},
      // INTEGER { -1 }
      {V_ASN1_INTEGER, {0x02, 0x01, 0xff}},
      // OBJECT_IDENTIFIER { 1.2.840.113554.4.1.72585.2 }
      {V_ASN1_OBJECT,
       {0x06, 0x0c, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x04, 0x01, 0x84, 0xb7,
        0x09, 0x02}},
      // NULL {}
      {V_ASN1_NULL, {0x05, 0x00}},
      // SEQUENCE {}
      {V_ASN1_SEQUENCE, {0x30, 0x00}},
      // SET {}
      {V_ASN1_SET, {0x31, 0x00}},
      // [0] { UTF8String { "a" } }
      {V_ASN1_OTHER, {0xa0, 0x03, 0x0c, 0x01, 0x61}},
  };
  for (const auto &t : kTests) {
    SCOPED_TRACE(Bytes(t.der));

    // The input should successfully parse.
    const uint8_t *ptr = t.der.data();
    bssl::UniquePtr<ASN1_TYPE> val(d2i_ASN1_TYPE(nullptr, &ptr, t.der.size()));
    ASSERT_TRUE(val);

    EXPECT_EQ(ASN1_TYPE_get(val.get()), t.type);
    EXPECT_EQ(val->type, t.type);
    TestSerialize(val.get(), i2d_ASN1_TYPE, t.der);
  }
}

// Test that reading |value.ptr| from a FALSE |ASN1_TYPE| behaves correctly. The
// type historically supported this, so maintain the invariant in case external
// code relies on it.
TEST(ASN1Test, UnusedBooleanBits) {
  // OCTET_STRING { "a" }
  static const uint8_t kDER[] = {0x04, 0x01, 0x61};
  const uint8_t *ptr = kDER;
  bssl::UniquePtr<ASN1_TYPE> val(d2i_ASN1_TYPE(nullptr, &ptr, sizeof(kDER)));
  ASSERT_TRUE(val);
  EXPECT_EQ(V_ASN1_OCTET_STRING, val->type);
  EXPECT_TRUE(val->value.ptr);

  // Set |val| to a BOOLEAN containing FALSE.
  ASN1_TYPE_set(val.get(), V_ASN1_BOOLEAN, NULL);
  EXPECT_EQ(V_ASN1_BOOLEAN, val->type);
  EXPECT_FALSE(val->value.ptr);
}

TEST(ASN1Test, ASN1ObjectReuse) {
  // 1.2.840.113554.4.1.72585.2, an arbitrary unknown OID.
  static const uint8_t kOID[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12,
                                 0x04, 0x01, 0x84, 0xb7, 0x09, 0x02};
  ASN1_OBJECT *obj = ASN1_OBJECT_create(NID_undef, kOID, sizeof(kOID),
                                        "short name", "long name");
  ASSERT_TRUE(obj);

  // OBJECT_IDENTIFIER { 1.3.101.112 }
  static const uint8_t kDER[] = {0x06, 0x03, 0x2b, 0x65, 0x70};
  const uint8_t *ptr = kDER;
  EXPECT_TRUE(d2i_ASN1_OBJECT(&obj, &ptr, sizeof(kDER)));
  EXPECT_EQ(NID_ED25519, OBJ_obj2nid(obj));
  ASN1_OBJECT_free(obj);

  // Repeat the test, this time overriding a static |ASN1_OBJECT|.
  obj = OBJ_nid2obj(NID_rsaEncryption);
  ptr = kDER;
  EXPECT_TRUE(d2i_ASN1_OBJECT(&obj, &ptr, sizeof(kDER)));
  EXPECT_EQ(NID_ED25519, OBJ_obj2nid(obj));
  ASN1_OBJECT_free(obj);
}

// The ASN.1 macros do not work on Windows shared library builds, where usage of
// |OPENSSL_EXPORT| is a bit stricter.
#if !defined(OPENSSL_WINDOWS) || !defined(BORINGSSL_SHARED_LIBRARY)

typedef struct asn1_linked_list_st {
  struct asn1_linked_list_st *next;
} ASN1_LINKED_LIST;

DECLARE_ASN1_ITEM(ASN1_LINKED_LIST)
DECLARE_ASN1_FUNCTIONS(ASN1_LINKED_LIST)

ASN1_SEQUENCE(ASN1_LINKED_LIST) = {
  ASN1_OPT(ASN1_LINKED_LIST, next, ASN1_LINKED_LIST),
} ASN1_SEQUENCE_END(ASN1_LINKED_LIST)

IMPLEMENT_ASN1_FUNCTIONS(ASN1_LINKED_LIST)

static bool MakeLinkedList(bssl::UniquePtr<uint8_t> *out, size_t *out_len,
                           size_t count) {
  bssl::ScopedCBB cbb;
  std::vector<CBB> cbbs(count);
  if (!CBB_init(cbb.get(), 2 * count) ||
      !CBB_add_asn1(cbb.get(), &cbbs[0], CBS_ASN1_SEQUENCE)) {
    return false;
  }
  for (size_t i = 1; i < count; i++) {
    if (!CBB_add_asn1(&cbbs[i - 1], &cbbs[i], CBS_ASN1_SEQUENCE)) {
      return false;
    }
  }
  uint8_t *ptr;
  if (!CBB_finish(cbb.get(), &ptr, out_len)) {
    return false;
  }
  out->reset(ptr);
  return true;
}

TEST(ASN1Test, Recursive) {
  bssl::UniquePtr<uint8_t> data;
  size_t len;

  // Sanity-check that MakeLinkedList can be parsed.
  ASSERT_TRUE(MakeLinkedList(&data, &len, 5));
  const uint8_t *ptr = data.get();
  ASN1_LINKED_LIST *list = d2i_ASN1_LINKED_LIST(nullptr, &ptr, len);
  EXPECT_TRUE(list);
  ASN1_LINKED_LIST_free(list);

  // Excessively deep structures are rejected.
  ASSERT_TRUE(MakeLinkedList(&data, &len, 100));
  ptr = data.get();
  list = d2i_ASN1_LINKED_LIST(nullptr, &ptr, len);
  EXPECT_FALSE(list);
  // Note checking the error queue here does not work. The error "stack trace"
  // is too deep, so the |ASN1_R_NESTED_TOO_DEEP| entry drops off the queue.
  ASN1_LINKED_LIST_free(list);
}

struct IMPLICIT_CHOICE {
  ASN1_STRING *string;
};

// clang-format off
DECLARE_ASN1_FUNCTIONS(IMPLICIT_CHOICE)

ASN1_SEQUENCE(IMPLICIT_CHOICE) = {
  ASN1_IMP(IMPLICIT_CHOICE, string, DIRECTORYSTRING, 0)
} ASN1_SEQUENCE_END(IMPLICIT_CHOICE)

IMPLEMENT_ASN1_FUNCTIONS(IMPLICIT_CHOICE)
// clang-format on

// Test that the ASN.1 templates reject types with implicitly-tagged CHOICE
// types.
TEST(ASN1Test, ImplicitChoice) {
  // Serializing a type with an implicitly tagged CHOICE should fail.
  std::unique_ptr<IMPLICIT_CHOICE, decltype(&IMPLICIT_CHOICE_free)> obj(
      IMPLICIT_CHOICE_new(), IMPLICIT_CHOICE_free);
  EXPECT_EQ(-1, i2d_IMPLICIT_CHOICE(obj.get(), nullptr));

  // An implicitly-tagged CHOICE is an error. Depending on the implementation,
  // it may be misinterpreted as without the tag, or as clobbering the CHOICE
  // tag. Test both inputs and ensure they fail.

  // SEQUENCE { UTF8String {} }
  static const uint8_t kInput1[] = {0x30, 0x02, 0x0c, 0x00};
  const uint8_t *ptr = kInput1;
  EXPECT_EQ(nullptr, d2i_IMPLICIT_CHOICE(nullptr, &ptr, sizeof(kInput1)));

  // SEQUENCE { [0 PRIMITIVE] {} }
  static const uint8_t kInput2[] = {0x30, 0x02, 0x80, 0x00};
  ptr = kInput2;
  EXPECT_EQ(nullptr, d2i_IMPLICIT_CHOICE(nullptr, &ptr, sizeof(kInput2)));
}

#endif  // !WINDOWS || !SHARED_LIBRARY
