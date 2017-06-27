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

#if !defined(__STDC_CONSTANT_MACROS)
#define __STDC_CONSTANT_MACROS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include <gtest/gtest.h>

#include <openssl/bytestring.h>
#include <openssl/crypto.h>

#include "internal.h"
#include "../internal.h"
#include "../test/test_util.h"


TEST(CBSTest, Skip) {
  static const uint8_t kData[] = {1, 2, 3};
  CBS data;

  CBS_init(&data, kData, sizeof(kData));
  EXPECT_EQ(3u, CBS_len(&data));
  EXPECT_TRUE(CBS_skip(&data, 1));
  EXPECT_EQ(2u, CBS_len(&data));
  EXPECT_TRUE(CBS_skip(&data, 2));
  EXPECT_EQ(0u, CBS_len(&data));
  EXPECT_FALSE(CBS_skip(&data, 1));
}

TEST(CBSTest, GetUint) {
  static const uint8_t kData[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  CBS data;

  CBS_init(&data, kData, sizeof(kData));
  ASSERT_TRUE(CBS_get_u8(&data, &u8));
  EXPECT_EQ(1u, u8);
  ASSERT_TRUE(CBS_get_u16(&data, &u16));
  EXPECT_EQ(0x203u, u16);
  ASSERT_TRUE(CBS_get_u24(&data, &u32));
  EXPECT_EQ(0x40506u, u32);
  ASSERT_TRUE(CBS_get_u32(&data, &u32));
  EXPECT_EQ(0x708090au, u32);
  ASSERT_TRUE(CBS_get_last_u8(&data, &u8));
  EXPECT_EQ(0xcu, u8);
  ASSERT_TRUE(CBS_get_last_u8(&data, &u8));
  EXPECT_EQ(0xbu, u8);
  EXPECT_FALSE(CBS_get_u8(&data, &u8));
  EXPECT_FALSE(CBS_get_last_u8(&data, &u8));
}

TEST(CBSTest, GetPrefixed) {
  static const uint8_t kData[] = {1, 2, 0, 2, 3, 4, 0, 0, 3, 3, 2, 1};
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  CBS data, prefixed;

  CBS_init(&data, kData, sizeof(kData));
  ASSERT_TRUE(CBS_get_u8_length_prefixed(&data, &prefixed));
  EXPECT_EQ(1u, CBS_len(&prefixed));
  ASSERT_TRUE(CBS_get_u8(&prefixed, &u8));
  EXPECT_EQ(2u, u8);
  ASSERT_TRUE(CBS_get_u16_length_prefixed(&data, &prefixed));
  EXPECT_EQ(2u, CBS_len(&prefixed));
  ASSERT_TRUE(CBS_get_u16(&prefixed, &u16));
  EXPECT_EQ(0x304u, u16);
  ASSERT_TRUE(CBS_get_u24_length_prefixed(&data, &prefixed));
  EXPECT_EQ(3u, CBS_len(&prefixed));
  ASSERT_TRUE(CBS_get_u24(&prefixed, &u32));
  EXPECT_EQ(0x30201u, u32);
}

TEST(CBSTest, GetPrefixedBad) {
  static const uint8_t kData1[] = {2, 1};
  static const uint8_t kData2[] = {0, 2, 1};
  static const uint8_t kData3[] = {0, 0, 2, 1};
  CBS data, prefixed;

  CBS_init(&data, kData1, sizeof(kData1));
  EXPECT_FALSE(CBS_get_u8_length_prefixed(&data, &prefixed));

  CBS_init(&data, kData2, sizeof(kData2));
  EXPECT_FALSE(CBS_get_u16_length_prefixed(&data, &prefixed));

  CBS_init(&data, kData3, sizeof(kData3));
  EXPECT_FALSE(CBS_get_u24_length_prefixed(&data, &prefixed));
}

TEST(CBSTest, GetASN1) {
  static const uint8_t kData1[] = {0x30, 2, 1, 2};
  static const uint8_t kData2[] = {0x30, 3, 1, 2};
  static const uint8_t kData3[] = {0x30, 0x80};
  static const uint8_t kData4[] = {0x30, 0x81, 1, 1};
  static const uint8_t kData5[4 + 0x80] = {0x30, 0x82, 0, 0x80};
  static const uint8_t kData6[] = {0xa1, 3, 0x4, 1, 1};
  static const uint8_t kData7[] = {0xa1, 3, 0x4, 2, 1};
  static const uint8_t kData8[] = {0xa1, 3, 0x2, 1, 1};
  static const uint8_t kData9[] = {0xa1, 3, 0x2, 1, 0xff};

  CBS data, contents;
  int present;
  uint64_t value;

  CBS_init(&data, kData1, sizeof(kData1));
  EXPECT_FALSE(CBS_peek_asn1_tag(&data, 0x1));
  EXPECT_TRUE(CBS_peek_asn1_tag(&data, 0x30));

  ASSERT_TRUE(CBS_get_asn1(&data, &contents, 0x30));
  EXPECT_EQ(Bytes("\x01\x02"), Bytes(CBS_data(&contents), CBS_len(&contents)));

  CBS_init(&data, kData2, sizeof(kData2));
  // data is truncated
  EXPECT_FALSE(CBS_get_asn1(&data, &contents, 0x30));

  CBS_init(&data, kData3, sizeof(kData3));
  // zero byte length of length
  EXPECT_FALSE(CBS_get_asn1(&data, &contents, 0x30));

  CBS_init(&data, kData4, sizeof(kData4));
  // long form mistakenly used.
  EXPECT_FALSE(CBS_get_asn1(&data, &contents, 0x30));

  CBS_init(&data, kData5, sizeof(kData5));
  // length takes too many bytes.
  EXPECT_FALSE(CBS_get_asn1(&data, &contents, 0x30));

  CBS_init(&data, kData1, sizeof(kData1));
  // wrong tag.
  EXPECT_FALSE(CBS_get_asn1(&data, &contents, 0x31));

  CBS_init(&data, NULL, 0);
  // peek at empty data.
  EXPECT_FALSE(CBS_peek_asn1_tag(&data, 0x30));

  CBS_init(&data, NULL, 0);
  // optional elements at empty data.
  ASSERT_TRUE(CBS_get_optional_asn1(&data, &contents, &present, 0xa0));
  EXPECT_FALSE(present);
  ASSERT_TRUE(
      CBS_get_optional_asn1_octet_string(&data, &contents, &present, 0xa0));
  EXPECT_FALSE(present);
  EXPECT_EQ(0u, CBS_len(&contents));
  ASSERT_TRUE(CBS_get_optional_asn1_octet_string(&data, &contents, NULL, 0xa0));
  EXPECT_EQ(0u, CBS_len(&contents));
  ASSERT_TRUE(CBS_get_optional_asn1_uint64(&data, &value, 0xa0, 42));
  EXPECT_EQ(42u, value);

  CBS_init(&data, kData6, sizeof(kData6));
  // optional element.
  ASSERT_TRUE(CBS_get_optional_asn1(&data, &contents, &present, 0xa0));
  EXPECT_FALSE(present);
  ASSERT_TRUE(CBS_get_optional_asn1(&data, &contents, &present, 0xa1));
  EXPECT_TRUE(present);
  EXPECT_EQ(Bytes("\x04\x01\x01"),
            Bytes(CBS_data(&contents), CBS_len(&contents)));

  CBS_init(&data, kData6, sizeof(kData6));
  // optional octet string.
  ASSERT_TRUE(
      CBS_get_optional_asn1_octet_string(&data, &contents, &present, 0xa0));
  EXPECT_FALSE(present);
  EXPECT_EQ(0u, CBS_len(&contents));
  ASSERT_TRUE(
      CBS_get_optional_asn1_octet_string(&data, &contents, &present, 0xa1));
  EXPECT_TRUE(present);
  EXPECT_EQ(Bytes("\x01"), Bytes(CBS_data(&contents), CBS_len(&contents)));

  CBS_init(&data, kData7, sizeof(kData7));
  // invalid optional octet string.
  EXPECT_FALSE(
      CBS_get_optional_asn1_octet_string(&data, &contents, &present, 0xa1));

  CBS_init(&data, kData8, sizeof(kData8));
  // optional integer.
  ASSERT_TRUE(CBS_get_optional_asn1_uint64(&data, &value, 0xa0, 42));
  EXPECT_EQ(42u, value);
  ASSERT_TRUE(CBS_get_optional_asn1_uint64(&data, &value, 0xa1, 42));
  EXPECT_EQ(1u, value);

  CBS_init(&data, kData9, sizeof(kData9));
  // invalid optional integer.
  EXPECT_FALSE(CBS_get_optional_asn1_uint64(&data, &value, 0xa1, 42));

  unsigned tag;
  CBS_init(&data, kData1, sizeof(kData1));
  ASSERT_TRUE(CBS_get_any_asn1(&data, &contents, &tag));
  EXPECT_EQ(CBS_ASN1_SEQUENCE, tag);
  EXPECT_EQ(Bytes("\x01\x02"), Bytes(CBS_data(&contents), CBS_len(&contents)));

  size_t header_len;
  CBS_init(&data, kData1, sizeof(kData1));
  ASSERT_TRUE(CBS_get_any_asn1_element(&data, &contents, &tag, &header_len));
  EXPECT_EQ(CBS_ASN1_SEQUENCE, tag);
  EXPECT_EQ(2u, header_len);
  EXPECT_EQ(Bytes("\x30\x02\x01\x02"),
            Bytes(CBS_data(&contents), CBS_len(&contents)));
}

TEST(CBSTest, GetOptionalASN1Bool) {
  static const uint8_t kTrue[] = {0x0a, 3, CBS_ASN1_BOOLEAN, 1, 0xff};
  static const uint8_t kFalse[] = {0x0a, 3, CBS_ASN1_BOOLEAN, 1, 0x00};
  static const uint8_t kInvalid[] = {0x0a, 3, CBS_ASN1_BOOLEAN, 1, 0x01};

  CBS data;
  CBS_init(&data, NULL, 0);
  int val = 2;
  ASSERT_TRUE(CBS_get_optional_asn1_bool(&data, &val, 0x0a, 0));
  EXPECT_EQ(0, val);

  CBS_init(&data, kTrue, sizeof(kTrue));
  val = 2;
  ASSERT_TRUE(CBS_get_optional_asn1_bool(&data, &val, 0x0a, 0));
  EXPECT_EQ(1, val);

  CBS_init(&data, kFalse, sizeof(kFalse));
  val = 2;
  ASSERT_TRUE(CBS_get_optional_asn1_bool(&data, &val, 0x0a, 1));
  EXPECT_EQ(0, val);

  CBS_init(&data, kInvalid, sizeof(kInvalid));
  EXPECT_FALSE(CBS_get_optional_asn1_bool(&data, &val, 0x0a, 1));
}

// Test that CBB_init may be used on an uninitialized input.
TEST(CBBTest, InitUninitialized) {
  CBB cbb;
  ASSERT_TRUE(CBB_init(&cbb, 100));
  CBB_cleanup(&cbb);
}

TEST(CBBTest, Basic) {
  static const uint8_t kExpected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc};
  uint8_t *buf;
  size_t buf_len;

  bssl::ScopedCBB cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), 100));
  cbb.Reset();

  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(CBB_add_u8(cbb.get(), 1));
  ASSERT_TRUE(CBB_add_u16(cbb.get(), 0x203));
  ASSERT_TRUE(CBB_add_u24(cbb.get(), 0x40506));
  ASSERT_TRUE(CBB_add_u32(cbb.get(), 0x708090a));
  ASSERT_TRUE(CBB_add_bytes(cbb.get(), (const uint8_t *)"\x0b\x0c", 2));
  ASSERT_TRUE(CBB_finish(cbb.get(), &buf, &buf_len));

  bssl::UniquePtr<uint8_t> scoper(buf);
  EXPECT_EQ(Bytes(kExpected), Bytes(buf, buf_len));
}

TEST(CBBTest, Fixed) {
  bssl::ScopedCBB cbb;
  uint8_t buf[1];
  uint8_t *out_buf;
  size_t out_size;

  ASSERT_TRUE(CBB_init_fixed(cbb.get(), NULL, 0));
  ASSERT_TRUE(CBB_finish(cbb.get(), &out_buf, &out_size));
  EXPECT_EQ(NULL, out_buf);
  EXPECT_EQ(0u, out_size);

  cbb.Reset();
  ASSERT_TRUE(CBB_init_fixed(cbb.get(), buf, 1));
  ASSERT_TRUE(CBB_add_u8(cbb.get(), 1));
  ASSERT_TRUE(CBB_finish(cbb.get(), &out_buf, &out_size));
  EXPECT_EQ(buf, out_buf);
  EXPECT_EQ(1u, out_size);
  EXPECT_EQ(1u, buf[0]);

  cbb.Reset();
  ASSERT_TRUE(CBB_init_fixed(cbb.get(), buf, 1));
  ASSERT_TRUE(CBB_add_u8(cbb.get(), 1));
  EXPECT_FALSE(CBB_add_u8(cbb.get(), 2));
}

// Test that calling CBB_finish on a child does nothing.
TEST(CBBTest, FinishChild) {
  CBB child;
  uint8_t *out_buf;
  size_t out_size;

  bssl::ScopedCBB cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), 16));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(cbb.get(), &child));

  EXPECT_FALSE(CBB_finish(&child, &out_buf, &out_size));

  ASSERT_TRUE(CBB_finish(cbb.get(), &out_buf, &out_size));
  bssl::UniquePtr<uint8_t> scoper(out_buf);
  ASSERT_EQ(1u, out_size);
  EXPECT_EQ(0u, out_buf[0]);
}

TEST(CBBTest, Prefixed) {
  static const uint8_t kExpected[] = {0, 1, 1, 0, 2, 2, 3, 0, 0, 3,
                                      4, 5, 6, 5, 4, 1, 0, 1, 2};
  uint8_t *buf;
  size_t buf_len;
  bssl::ScopedCBB cbb;
  CBB contents, inner_contents, inner_inner_contents;
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  EXPECT_EQ(0u, CBB_len(cbb.get()));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u8(&contents, 1));
  EXPECT_EQ(1u, CBB_len(&contents));
  ASSERT_TRUE(CBB_flush(cbb.get()));
  EXPECT_EQ(3u, CBB_len(cbb.get()));
  ASSERT_TRUE(CBB_add_u16_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u16(&contents, 0x203));
  ASSERT_TRUE(CBB_add_u24_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u24(&contents, 0x40506));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(&contents, &inner_contents));
  ASSERT_TRUE(CBB_add_u8(&inner_contents, 1));
  ASSERT_TRUE(
      CBB_add_u16_length_prefixed(&inner_contents, &inner_inner_contents));
  ASSERT_TRUE(CBB_add_u8(&inner_inner_contents, 2));
  ASSERT_TRUE(CBB_finish(cbb.get(), &buf, &buf_len));

  bssl::UniquePtr<uint8_t> scoper(buf);
  EXPECT_EQ(Bytes(kExpected), Bytes(buf, buf_len));
}

TEST(CBBTest, DiscardChild) {
  bssl::ScopedCBB cbb;
  CBB contents, inner_contents, inner_inner_contents;

  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(CBB_add_u8(cbb.get(), 0xaa));

  // Discarding |cbb|'s children preserves the byte written.
  CBB_discard_child(cbb.get());

  ASSERT_TRUE(CBB_add_u8_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u8(&contents, 0xbb));
  ASSERT_TRUE(CBB_add_u16_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u16(&contents, 0xcccc));
  ASSERT_TRUE(CBB_add_u24_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u24(&contents, 0xdddddd));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(cbb.get(), &contents));
  ASSERT_TRUE(CBB_add_u8(&contents, 0xff));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(&contents, &inner_contents));
  ASSERT_TRUE(CBB_add_u8(&inner_contents, 0x42));
  ASSERT_TRUE(
      CBB_add_u16_length_prefixed(&inner_contents, &inner_inner_contents));
  ASSERT_TRUE(CBB_add_u8(&inner_inner_contents, 0x99));

  // Discard everything from |inner_contents| down.
  CBB_discard_child(&contents);

  uint8_t *buf;
  size_t buf_len;
  ASSERT_TRUE(CBB_finish(cbb.get(), &buf, &buf_len));
  bssl::UniquePtr<uint8_t> scoper(buf);

  static const uint8_t kExpected[] = {
        0xaa,
        0,
        1, 0xbb,
        0, 2, 0xcc, 0xcc,
        0, 0, 3, 0xdd, 0xdd, 0xdd,
        1, 0xff,
  };
  EXPECT_EQ(Bytes(kExpected), Bytes(buf, buf_len));
}

TEST(CBBTest, Misuse) {
  bssl::ScopedCBB cbb;
  CBB child, contents;
  uint8_t *buf;
  size_t buf_len;

  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(cbb.get(), &child));
  ASSERT_TRUE(CBB_add_u8(&child, 1));
  ASSERT_TRUE(CBB_add_u8(cbb.get(), 2));

  // Since we wrote to |cbb|, |child| is now invalid and attempts to write to
  // it should fail.
  EXPECT_FALSE(CBB_add_u8(&child, 1));
  EXPECT_FALSE(CBB_add_u16(&child, 1));
  EXPECT_FALSE(CBB_add_u24(&child, 1));
  EXPECT_FALSE(CBB_add_u8_length_prefixed(&child, &contents));
  EXPECT_FALSE(CBB_add_u16_length_prefixed(&child, &contents));
  EXPECT_FALSE(CBB_add_asn1(&child, &contents, 1));
  EXPECT_FALSE(CBB_add_bytes(&child, (const uint8_t*) "a", 1));

  ASSERT_TRUE(CBB_finish(cbb.get(), &buf, &buf_len));
  bssl::UniquePtr<uint8_t> scoper(buf);

  EXPECT_EQ(Bytes("\x01\x01\x02"), Bytes(buf, buf_len));
}

TEST(CBBTest, ASN1) {
  static const uint8_t kExpected[] = {0x30, 3, 1, 2, 3};
  uint8_t *buf;
  size_t buf_len;
  bssl::ScopedCBB cbb;
  CBB contents, inner_contents;

  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &contents, 0x30));
  ASSERT_TRUE(CBB_add_bytes(&contents, (const uint8_t *)"\x01\x02\x03", 3));
  ASSERT_TRUE(CBB_finish(cbb.get(), &buf, &buf_len));
  bssl::UniquePtr<uint8_t> scoper(buf);

  EXPECT_EQ(Bytes(kExpected), Bytes(buf, buf_len));

  std::vector<uint8_t> test_data(100000, 0x42);
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &contents, 0x30));
  ASSERT_TRUE(CBB_add_bytes(&contents, test_data.data(), 130));
  ASSERT_TRUE(CBB_finish(cbb.get(), &buf, &buf_len));
  scoper.reset(buf);

  ASSERT_EQ(3u + 130u, buf_len);
  EXPECT_EQ(Bytes("\x30\x81\x82"), Bytes(buf, 3));
  EXPECT_EQ(Bytes(test_data.data(), 130), Bytes(buf + 3, 130));

  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &contents, 0x30));
  ASSERT_TRUE(CBB_add_bytes(&contents, test_data.data(), 1000));
  ASSERT_TRUE(CBB_finish(cbb.get(), &buf, &buf_len));
  scoper.reset(buf);

  ASSERT_EQ(4u + 1000u, buf_len);
  EXPECT_EQ(Bytes("\x30\x82\x03\xe8"), Bytes(buf, 4));
  EXPECT_EQ(Bytes(test_data.data(), 1000), Bytes(buf + 4, 1000));

  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(CBB_add_asn1(cbb.get(), &contents, 0x30));
  ASSERT_TRUE(CBB_add_asn1(&contents, &inner_contents, 0x30));
  ASSERT_TRUE(CBB_add_bytes(&inner_contents, test_data.data(), 100000));
  ASSERT_TRUE(CBB_finish(cbb.get(), &buf, &buf_len));
  scoper.reset(buf);

  ASSERT_EQ(5u + 5u + 100000u, buf_len);
  EXPECT_EQ(Bytes("\x30\x83\x01\x86\xa5\x30\x83\x01\x86\xa0"), Bytes(buf, 10));
  EXPECT_EQ(Bytes(test_data.data(), test_data.size()), Bytes(buf + 10, 100000));
}

static void ExpectBerConvert(const char *name, const uint8_t *der_expected,
                             size_t der_len, const uint8_t *ber,
                             size_t ber_len) {
  SCOPED_TRACE(name);
  CBS in;
  uint8_t *out;
  size_t out_len;

  CBS_init(&in, ber, ber_len);
  ASSERT_TRUE(CBS_asn1_ber_to_der(&in, &out, &out_len));
  bssl::UniquePtr<uint8_t> scoper(out);

  if (out == NULL) {
    EXPECT_EQ(Bytes(der_expected, der_len), Bytes(ber, ber_len));
  } else {
    EXPECT_NE(Bytes(der_expected, der_len), Bytes(ber, ber_len));
    EXPECT_EQ(Bytes(der_expected, der_len), Bytes(out, out_len));
  }
}

TEST(CBSTest, BerConvert) {
  static const uint8_t kSimpleBER[] = {0x01, 0x01, 0x00};

  // kIndefBER contains a SEQUENCE with an indefinite length.
  static const uint8_t kIndefBER[] = {0x30, 0x80, 0x01, 0x01, 0x02, 0x00, 0x00};
  static const uint8_t kIndefDER[] = {0x30, 0x03, 0x01, 0x01, 0x02};

  // kOctetStringBER contains an indefinite length OCTET STRING with two parts.
  // These parts need to be concatenated in DER form.
  static const uint8_t kOctetStringBER[] = {0x24, 0x80, 0x04, 0x02, 0,    1,
                                            0x04, 0x02, 2,    3,    0x00, 0x00};
  static const uint8_t kOctetStringDER[] = {0x04, 0x04, 0, 1, 2, 3};

  // kNSSBER is part of a PKCS#12 message generated by NSS that uses indefinite
  // length elements extensively.
  static const uint8_t kNSSBER[] = {
      0x30, 0x80, 0x02, 0x01, 0x03, 0x30, 0x80, 0x06, 0x09, 0x2a, 0x86, 0x48,
      0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x80, 0x24, 0x80, 0x04, 0x04,
      0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x39,
      0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05,
      0x00, 0x04, 0x14, 0x84, 0x98, 0xfc, 0x66, 0x33, 0xee, 0xba, 0xe7, 0x90,
      0xc1, 0xb6, 0xe8, 0x8f, 0xfe, 0x1d, 0xc5, 0xa5, 0x97, 0x93, 0x3e, 0x04,
      0x10, 0x38, 0x62, 0xc6, 0x44, 0x12, 0xd5, 0x30, 0x00, 0xf8, 0xf2, 0x1b,
      0xf0, 0x6e, 0x10, 0x9b, 0xb8, 0x02, 0x02, 0x07, 0xd0, 0x00, 0x00,
  };

  static const uint8_t kNSSDER[] = {
      0x30, 0x53, 0x02, 0x01, 0x03, 0x30, 0x13, 0x06, 0x09, 0x2a, 0x86,
      0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x06, 0x04, 0x04,
      0x01, 0x02, 0x03, 0x04, 0x30, 0x39, 0x30, 0x21, 0x30, 0x09, 0x06,
      0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14, 0x84,
      0x98, 0xfc, 0x66, 0x33, 0xee, 0xba, 0xe7, 0x90, 0xc1, 0xb6, 0xe8,
      0x8f, 0xfe, 0x1d, 0xc5, 0xa5, 0x97, 0x93, 0x3e, 0x04, 0x10, 0x38,
      0x62, 0xc6, 0x44, 0x12, 0xd5, 0x30, 0x00, 0xf8, 0xf2, 0x1b, 0xf0,
      0x6e, 0x10, 0x9b, 0xb8, 0x02, 0x02, 0x07, 0xd0,
  };

  // kConstructedStringBER contains a deeply-nested constructed OCTET STRING.
  // The BER conversion collapses this to one level deep, but not completely.
  static const uint8_t kConstructedStringBER[] = {
      0xa0, 0x10, 0x24, 0x06, 0x04, 0x01, 0x00, 0x04, 0x01,
      0x01, 0x24, 0x06, 0x04, 0x01, 0x02, 0x04, 0x01, 0x03,
  };
  static const uint8_t kConstructedStringDER[] = {
      0xa0, 0x08, 0x04, 0x02, 0x00, 0x01, 0x04, 0x02, 0x02, 0x03,
  };

  ExpectBerConvert("kSimpleBER", kSimpleBER, sizeof(kSimpleBER), kSimpleBER,
                   sizeof(kSimpleBER));
  ExpectBerConvert("kIndefBER", kIndefDER, sizeof(kIndefDER), kIndefBER,
                   sizeof(kIndefBER));
  ExpectBerConvert("kOctetStringBER", kOctetStringDER, sizeof(kOctetStringDER),
                   kOctetStringBER, sizeof(kOctetStringBER));
  ExpectBerConvert("kNSSBER", kNSSDER, sizeof(kNSSDER), kNSSBER,
                   sizeof(kNSSBER));
  ExpectBerConvert("kConstructedStringBER", kConstructedStringDER,
                   sizeof(kConstructedStringDER), kConstructedStringBER,
                   sizeof(kConstructedStringBER));
}

struct ImplicitStringTest {
  const char *in;
  size_t in_len;
  bool ok;
  const char *out;
  size_t out_len;
};

static const ImplicitStringTest kImplicitStringTests[] = {
    // A properly-encoded string.
    {"\x80\x03\x61\x61\x61", 5, true, "aaa", 3},
    // An implicit-tagged string.
    {"\xa0\x09\x04\x01\x61\x04\x01\x61\x04\x01\x61", 11, true, "aaa", 3},
    // |CBS_get_asn1_implicit_string| only accepts one level deep of nesting.
    {"\xa0\x0b\x24\x06\x04\x01\x61\x04\x01\x61\x04\x01\x61", 13, false, nullptr,
     0},
    // The outer tag must match.
    {"\x81\x03\x61\x61\x61", 5, false, nullptr, 0},
    {"\xa1\x09\x04\x01\x61\x04\x01\x61\x04\x01\x61", 11, false, nullptr, 0},
    // The inner tag must match.
    {"\xa1\x09\x0c\x01\x61\x0c\x01\x61\x0c\x01\x61", 11, false, nullptr, 0},
};

TEST(CBSTest, ImplicitString) {
  for (const auto &test : kImplicitStringTests) {
    SCOPED_TRACE(Bytes(test.in, test.in_len));
    uint8_t *storage = nullptr;
    CBS in, out;
    CBS_init(&in, reinterpret_cast<const uint8_t *>(test.in), test.in_len);
    int ok = CBS_get_asn1_implicit_string(&in, &out, &storage,
                                          CBS_ASN1_CONTEXT_SPECIFIC | 0,
                                          CBS_ASN1_OCTETSTRING);
    bssl::UniquePtr<uint8_t> scoper(storage);
    EXPECT_EQ(test.ok, static_cast<bool>(ok));

    if (ok) {
      EXPECT_EQ(Bytes(test.out, test.out_len),
                Bytes(CBS_data(&out), CBS_len(&out)));
    }
  }
}

struct ASN1Uint64Test {
  uint64_t value;
  const char *encoding;
  size_t encoding_len;
};

static const ASN1Uint64Test kASN1Uint64Tests[] = {
    {0, "\x02\x01\x00", 3},
    {1, "\x02\x01\x01", 3},
    {127, "\x02\x01\x7f", 3},
    {128, "\x02\x02\x00\x80", 4},
    {0xdeadbeef, "\x02\x05\x00\xde\xad\xbe\xef", 7},
    {UINT64_C(0x0102030405060708),
     "\x02\x08\x01\x02\x03\x04\x05\x06\x07\x08", 10},
    {UINT64_C(0xffffffffffffffff),
      "\x02\x09\x00\xff\xff\xff\xff\xff\xff\xff\xff", 11},
};

struct ASN1InvalidUint64Test {
  const char *encoding;
  size_t encoding_len;
};

static const ASN1InvalidUint64Test kASN1InvalidUint64Tests[] = {
    // Bad tag.
    {"\x03\x01\x00", 3},
    // Empty contents.
    {"\x02\x00", 2},
    // Negative number.
    {"\x02\x01\x80", 3},
    // Overflow.
    {"\x02\x09\x01\x00\x00\x00\x00\x00\x00\x00\x00", 11},
    // Leading zeros.
    {"\x02\x02\x00\x01", 4},
};

TEST(CBSTest, ASN1Uint64) {
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kASN1Uint64Tests); i++) {
    SCOPED_TRACE(i);
    const ASN1Uint64Test *test = &kASN1Uint64Tests[i];
    CBS cbs;
    uint64_t value;
    uint8_t *out;
    size_t len;

    CBS_init(&cbs, (const uint8_t *)test->encoding, test->encoding_len);
    ASSERT_TRUE(CBS_get_asn1_uint64(&cbs, &value));
    EXPECT_EQ(0u, CBS_len(&cbs));
    EXPECT_EQ(test->value, value);

    bssl::ScopedCBB cbb;
    ASSERT_TRUE(CBB_init(cbb.get(), 0));
    ASSERT_TRUE(CBB_add_asn1_uint64(cbb.get(), test->value));
    ASSERT_TRUE(CBB_finish(cbb.get(), &out, &len));
    bssl::UniquePtr<uint8_t> scoper(out);
    EXPECT_EQ(Bytes(test->encoding, test->encoding_len), Bytes(out, len));
  }

  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kASN1InvalidUint64Tests); i++) {
    const ASN1InvalidUint64Test *test = &kASN1InvalidUint64Tests[i];
    CBS cbs;
    uint64_t value;

    CBS_init(&cbs, (const uint8_t *)test->encoding, test->encoding_len);
    EXPECT_FALSE(CBS_get_asn1_uint64(&cbs, &value));
  }
}

TEST(CBBTest, Zero) {
  CBB cbb;
  CBB_zero(&cbb);
  // Calling |CBB_cleanup| on a zero-state |CBB| must not crash.
  CBB_cleanup(&cbb);
}

TEST(CBBTest, Reserve) {
  uint8_t buf[10];
  uint8_t *ptr;
  size_t len;
  bssl::ScopedCBB cbb;
  ASSERT_TRUE(CBB_init_fixed(cbb.get(), buf, sizeof(buf)));
  // Too large.
  EXPECT_FALSE(CBB_reserve(cbb.get(), &ptr, 11));

  cbb.Reset();
  ASSERT_TRUE(CBB_init_fixed(cbb.get(), buf, sizeof(buf)));
  // Successfully reserve the entire space.
  ASSERT_TRUE(CBB_reserve(cbb.get(), &ptr, 10));
  EXPECT_EQ(buf, ptr);
  // Advancing under the maximum bytes is legal.
  ASSERT_TRUE(CBB_did_write(cbb.get(), 5));
  ASSERT_TRUE(CBB_finish(cbb.get(), NULL, &len));
  EXPECT_EQ(5u, len);
}

// Test that CBB errors are sticky; once on operation on CBB fails, all
// subsequent ones do.
TEST(CBBTest, StickyError) {
  // Write an input that exceeds the limit for its length prefix.
  bssl::ScopedCBB cbb;
  CBB child;
  static const uint8_t kZeros[256] = {0};
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(CBB_add_u8_length_prefixed(cbb.get(), &child));
  ASSERT_TRUE(CBB_add_bytes(&child, kZeros, sizeof(kZeros)));
  ASSERT_FALSE(CBB_flush(cbb.get()));

  // All future operations should fail.
  uint8_t *ptr;
  size_t len;
  EXPECT_FALSE(CBB_add_u8(cbb.get(), 0));
  EXPECT_FALSE(CBB_finish(cbb.get(), &ptr, &len));

  // Write an input that cannot fit in a fixed CBB.
  cbb.Reset();
  uint8_t buf;
  ASSERT_TRUE(CBB_init_fixed(cbb.get(), &buf, 1));
  ASSERT_FALSE(CBB_add_bytes(cbb.get(), kZeros, sizeof(kZeros)));

  // All future operations should fail.
  EXPECT_FALSE(CBB_add_u8(cbb.get(), 0));
  EXPECT_FALSE(CBB_finish(cbb.get(), &ptr, &len));

  // Write a u32 that cannot fit in a u24.
  cbb.Reset();
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_FALSE(CBB_add_u24(cbb.get(), 1u << 24));

  // All future operations should fail.
  EXPECT_FALSE(CBB_add_u8(cbb.get(), 0));
  EXPECT_FALSE(CBB_finish(cbb.get(), &ptr, &len));
}

TEST(CBSTest, BitString) {
  static const std::vector<uint8_t> kValidBitStrings[] = {
      {0x00},                                      // 0 bits
      {0x07, 0x80},                                // 1 bit
      {0x04, 0xf0},                                // 4 bits
      {0x00, 0xff},                                // 8 bits
      {0x06, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0},  // 42 bits
  };
  for (const auto& test : kValidBitStrings) {
    SCOPED_TRACE(Bytes(test.data(), test.size()));
    CBS cbs;
    CBS_init(&cbs, test.data(), test.size());
    EXPECT_TRUE(CBS_is_valid_asn1_bitstring(&cbs));
  }

  static const std::vector<uint8_t> kInvalidBitStrings[] = {
      // BIT STRINGs always have a leading byte.
      std::vector<uint8_t>{},
      // It's not possible to take an unused bit off the empty string.
      {0x01},
      // There can be at most 7 unused bits.
      {0x08, 0xff},
      {0xff, 0xff},
      // All unused bits must be cleared.
      {0x06, 0xff, 0xc1},
  };
  for (const auto& test : kInvalidBitStrings) {
    SCOPED_TRACE(Bytes(test.data(), test.size()));
    CBS cbs;
    CBS_init(&cbs, test.data(), test.size());
    EXPECT_FALSE(CBS_is_valid_asn1_bitstring(&cbs));

    // CBS_asn1_bitstring_has_bit returns false on invalid inputs.
    EXPECT_FALSE(CBS_asn1_bitstring_has_bit(&cbs, 0));
  }

  static const struct {
    std::vector<uint8_t> in;
    unsigned bit;
    bool bit_set;
  } kBitTests[] = {
      // Basic tests.
      {{0x00}, 0, false},
      {{0x07, 0x80}, 0, true},
      {{0x06, 0x0f, 0x40}, 0, false},
      {{0x06, 0x0f, 0x40}, 1, false},
      {{0x06, 0x0f, 0x40}, 2, false},
      {{0x06, 0x0f, 0x40}, 3, false},
      {{0x06, 0x0f, 0x40}, 4, true},
      {{0x06, 0x0f, 0x40}, 5, true},
      {{0x06, 0x0f, 0x40}, 6, true},
      {{0x06, 0x0f, 0x40}, 7, true},
      {{0x06, 0x0f, 0x40}, 8, false},
      {{0x06, 0x0f, 0x40}, 9, true},
      // Out-of-bounds bits return 0.
      {{0x06, 0x0f, 0x40}, 10, false},
      {{0x06, 0x0f, 0x40}, 15, false},
      {{0x06, 0x0f, 0x40}, 16, false},
      {{0x06, 0x0f, 0x40}, 1000, false},
  };
  for (const auto& test : kBitTests) {
    SCOPED_TRACE(Bytes(test.in.data(), test.in.size()));
    SCOPED_TRACE(test.bit);
    CBS cbs;
    CBS_init(&cbs, test.in.data(), test.in.size());
    EXPECT_EQ(static_cast<int>(test.bit_set),
              CBS_asn1_bitstring_has_bit(&cbs, test.bit));
  }
}
