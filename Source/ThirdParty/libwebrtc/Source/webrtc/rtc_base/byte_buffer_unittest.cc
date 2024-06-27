/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/byte_buffer.h"

#include <string.h>

#include <cstdint>

#include "rtc_base/arraysize.h"
#include "rtc_base/byte_order.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace rtc {

using ::testing::ElementsAre;

TEST(ByteBufferTest, WriterAccessors) {
  // To be changed into ByteBufferWriter when base type is converted.
  ByteBufferWriterT<BufferT<uint8_t>> buffer;
  buffer.WriteString("abc");
  EXPECT_EQ(buffer.Length(), 3U);
  EXPECT_THAT(buffer.DataView(), ElementsAre('a', 'b', 'c'));
  EXPECT_EQ(absl::string_view("abc"), buffer.DataAsStringView());

  buffer.WriteUInt8(0);
  EXPECT_STREQ(buffer.DataAsCharPointer(), "abc");
  EXPECT_STREQ(reinterpret_cast<const char*>(buffer.Data()), "abc");
}

TEST(ByteBufferTest, TestByteOrder) {
  uint16_t n16 = 1;
  uint32_t n32 = 1;
  uint64_t n64 = 1;

  EXPECT_EQ(n16, NetworkToHost16(HostToNetwork16(n16)));
  EXPECT_EQ(n32, NetworkToHost32(HostToNetwork32(n32)));
  EXPECT_EQ(n64, NetworkToHost64(HostToNetwork64(n64)));

  if (IsHostBigEndian()) {
    // The host is the network (big) endian.
    EXPECT_EQ(n16, HostToNetwork16(n16));
    EXPECT_EQ(n32, HostToNetwork32(n32));
    EXPECT_EQ(n64, HostToNetwork64(n64));

    // GetBE converts big endian to little endian here.
    EXPECT_EQ(n16 >> 8, GetBE16(&n16));
    EXPECT_EQ(n32 >> 24, GetBE32(&n32));
    EXPECT_EQ(n64 >> 56, GetBE64(&n64));
  } else {
    // The host is little endian.
    EXPECT_NE(n16, HostToNetwork16(n16));
    EXPECT_NE(n32, HostToNetwork32(n32));
    EXPECT_NE(n64, HostToNetwork64(n64));

    // GetBE converts little endian to big endian here.
    EXPECT_EQ(GetBE16(&n16), HostToNetwork16(n16));
    EXPECT_EQ(GetBE32(&n32), HostToNetwork32(n32));
    EXPECT_EQ(GetBE64(&n64), HostToNetwork64(n64));

    // GetBE converts little endian to big endian here.
    EXPECT_EQ(n16 << 8, GetBE16(&n16));
    EXPECT_EQ(n32 << 24, GetBE32(&n32));
    EXPECT_EQ(n64 << 56, GetBE64(&n64));
  }
}

TEST(ByteBufferTest, TestBufferLength) {
  ByteBufferWriter buffer;
  size_t size = 0;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt8(1);
  ++size;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt16(1);
  size += 2;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt24(1);
  size += 3;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt32(1);
  size += 4;
  EXPECT_EQ(size, buffer.Length());

  buffer.WriteUInt64(1);
  size += 8;
  EXPECT_EQ(size, buffer.Length());
}

TEST(ByteBufferTest, TestReadWriteBuffer) {
  ByteBufferWriter buffer;
  ByteBufferReader read_buf(rtc::ArrayView<const uint8_t>(nullptr, 0));
  uint8_t ru8;
  EXPECT_FALSE(read_buf.ReadUInt8(&ru8));

  // Write and read uint8_t.
  uint8_t wu8 = 1;
  buffer.WriteUInt8(wu8);
  ByteBufferReader read_buf1(buffer);
  EXPECT_TRUE(read_buf1.ReadUInt8(&ru8));
  EXPECT_EQ(wu8, ru8);
  EXPECT_EQ(0U, read_buf1.Length());
  buffer.Clear();

  // Write and read uint16_t.
  uint16_t wu16 = (1 << 8) + 1;
  buffer.WriteUInt16(wu16);
  ByteBufferReader read_buf2(buffer);
  uint16_t ru16;
  EXPECT_TRUE(read_buf2.ReadUInt16(&ru16));
  EXPECT_EQ(wu16, ru16);
  EXPECT_EQ(0U, read_buf2.Length());
  buffer.Clear();

  // Write and read uint24.
  uint32_t wu24 = (3 << 16) + (2 << 8) + 1;
  buffer.WriteUInt24(wu24);
  ByteBufferReader read_buf3(buffer);
  uint32_t ru24;
  EXPECT_TRUE(read_buf3.ReadUInt24(&ru24));
  EXPECT_EQ(wu24, ru24);
  EXPECT_EQ(0U, read_buf3.Length());
  buffer.Clear();

  // Write and read uint32_t.
  uint32_t wu32 = (4 << 24) + (3 << 16) + (2 << 8) + 1;
  buffer.WriteUInt32(wu32);
  ByteBufferReader read_buf4(buffer);
  uint32_t ru32;
  EXPECT_TRUE(read_buf4.ReadUInt32(&ru32));
  EXPECT_EQ(wu32, ru32);
  EXPECT_EQ(0U, read_buf3.Length());
  buffer.Clear();

  // Write and read uint64_t.
  uint32_t another32 = (8 << 24) + (7 << 16) + (6 << 8) + 5;
  uint64_t wu64 = (static_cast<uint64_t>(another32) << 32) + wu32;
  buffer.WriteUInt64(wu64);
  ByteBufferReader read_buf5(buffer);
  uint64_t ru64;
  EXPECT_TRUE(read_buf5.ReadUInt64(&ru64));
  EXPECT_EQ(wu64, ru64);
  EXPECT_EQ(0U, read_buf5.Length());
  buffer.Clear();

  // Write and read string.
  std::string write_string("hello");
  buffer.WriteString(write_string);
  ByteBufferReader read_buf6(buffer);
  std::string read_string;
  EXPECT_TRUE(read_buf6.ReadString(&read_string, write_string.size()));
  EXPECT_EQ(write_string, read_string);
  EXPECT_EQ(0U, read_buf6.Length());
  buffer.Clear();

  // Write and read bytes
  uint8_t write_bytes[] = "foo";
  buffer.WriteBytes(write_bytes, 3);
  ByteBufferReader read_buf7(buffer);
  uint8_t read_bytes[3];
  EXPECT_TRUE(read_buf7.ReadBytes(read_bytes));
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(write_bytes[i], read_bytes[i]);
  }
  EXPECT_EQ(0U, read_buf7.Length());
  buffer.Clear();

  // Write and read reserved buffer space
  uint8_t* write_dst = buffer.ReserveWriteBuffer(3);
  memcpy(write_dst, write_bytes, 3);
  ByteBufferReader read_buf8(buffer);
  memset(read_bytes, 0, 3);
  EXPECT_TRUE(read_buf8.ReadBytes(read_bytes));
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(write_bytes[i], read_bytes[i]);
  }
  EXPECT_EQ(0U, read_buf8.Length());
  buffer.Clear();

  // Write and read in order.
  buffer.WriteUInt8(wu8);
  buffer.WriteUInt16(wu16);
  buffer.WriteUInt24(wu24);
  buffer.WriteUInt32(wu32);
  buffer.WriteUInt64(wu64);
  ByteBufferReader read_buf9(buffer);
  EXPECT_TRUE(read_buf9.ReadUInt8(&ru8));
  EXPECT_EQ(wu8, ru8);
  EXPECT_TRUE(read_buf9.ReadUInt16(&ru16));
  EXPECT_EQ(wu16, ru16);
  EXPECT_TRUE(read_buf9.ReadUInt24(&ru24));
  EXPECT_EQ(wu24, ru24);
  EXPECT_TRUE(read_buf9.ReadUInt32(&ru32));
  EXPECT_EQ(wu32, ru32);
  EXPECT_TRUE(read_buf9.ReadUInt64(&ru64));
  EXPECT_EQ(wu64, ru64);
  EXPECT_EQ(0U, read_buf9.Length());
  buffer.Clear();
}

TEST(ByteBufferTest, TestReadStringView) {
  const absl::string_view tests[] = {"hello", " ", "string_view"};
  std::string buffer;
  for (const auto& test : tests)
    buffer += test;

  rtc::ArrayView<const uint8_t> bytes(
      reinterpret_cast<const uint8_t*>(&buffer[0]), buffer.size());

  ByteBufferReader read_buf(bytes);
  size_t consumed = 0;
  for (const auto& test : tests) {
    absl::string_view sv;
    EXPECT_TRUE(read_buf.ReadStringView(&sv, test.length()));
    EXPECT_EQ(sv.compare(test), 0);
    // The returned string view should point directly into the original string.
    EXPECT_EQ(&sv[0], &buffer[0 + consumed]);
    consumed += sv.size();
  }
}

TEST(ByteBufferTest, TestReadWriteUVarint) {
  ByteBufferWriter write_buffer;
  size_t size = 0;
  EXPECT_EQ(size, write_buffer.Length());

  write_buffer.WriteUVarint(1u);
  ++size;
  EXPECT_EQ(size, write_buffer.Length());

  write_buffer.WriteUVarint(2u);
  ++size;
  EXPECT_EQ(size, write_buffer.Length());

  write_buffer.WriteUVarint(27u);
  ++size;
  EXPECT_EQ(size, write_buffer.Length());

  write_buffer.WriteUVarint(149u);
  size += 2;
  EXPECT_EQ(size, write_buffer.Length());

  write_buffer.WriteUVarint(68719476736u);
  size += 6;
  EXPECT_EQ(size, write_buffer.Length());

  ByteBufferReader read_buffer(write_buffer);
  EXPECT_EQ(size, read_buffer.Length());
  uint64_t val1, val2, val3, val4, val5;

  ASSERT_TRUE(read_buffer.ReadUVarint(&val1));
  EXPECT_EQ(1u, val1);
  --size;
  EXPECT_EQ(size, read_buffer.Length());

  ASSERT_TRUE(read_buffer.ReadUVarint(&val2));
  EXPECT_EQ(2u, val2);
  --size;
  EXPECT_EQ(size, read_buffer.Length());

  ASSERT_TRUE(read_buffer.ReadUVarint(&val3));
  EXPECT_EQ(27u, val3);
  --size;
  EXPECT_EQ(size, read_buffer.Length());

  ASSERT_TRUE(read_buffer.ReadUVarint(&val4));
  EXPECT_EQ(149u, val4);
  size -= 2;
  EXPECT_EQ(size, read_buffer.Length());

  ASSERT_TRUE(read_buffer.ReadUVarint(&val5));
  EXPECT_EQ(68719476736u, val5);
  size -= 6;
  EXPECT_EQ(size, read_buffer.Length());
}

TEST(ByteBufferTest, ReadFromArrayView) {
  const uint8_t buf[] = {'a', 'b', 'c'};
  ArrayView<const uint8_t> view(buf, 3);

  ByteBufferReader read_buffer(view);
  uint8_t val;
  EXPECT_TRUE(read_buffer.ReadUInt8(&val));
  EXPECT_EQ(val, 'a');
  EXPECT_TRUE(read_buffer.ReadUInt8(&val));
  EXPECT_EQ(val, 'b');
  EXPECT_TRUE(read_buffer.ReadUInt8(&val));
  EXPECT_EQ(val, 'c');
  EXPECT_FALSE(read_buffer.ReadUInt8(&val));
}

TEST(ByteBufferTest, ReadToArrayView) {
  const uint8_t buf[] = {'a', 'b', 'c'};
  ArrayView<const uint8_t> stored_view(buf, 3);
  ByteBufferReader read_buffer(stored_view);
  uint8_t result[] = {'1', '2', '3'};
  EXPECT_TRUE(read_buffer.ReadBytes(rtc::MakeArrayView(result, 2)));
  EXPECT_EQ(result[0], 'a');
  EXPECT_EQ(result[1], 'b');
  EXPECT_EQ(result[2], '3');
  EXPECT_TRUE(read_buffer.ReadBytes(rtc::MakeArrayView(&result[2], 1)));
  EXPECT_EQ(result[2], 'c');
  EXPECT_FALSE(read_buffer.ReadBytes(rtc::MakeArrayView(result, 1)));
}

}  // namespace rtc
