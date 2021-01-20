/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/copy_on_write_buffer.h"

#include <cstdint>

#include "test/gtest.h"

namespace rtc {

namespace {

// clang-format off
const uint8_t kTestData[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                             0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
// clang-format on

}  // namespace

void EnsureBuffersShareData(const CopyOnWriteBuffer& buf1,
                            const CopyOnWriteBuffer& buf2) {
  // Data is shared between buffers.
  EXPECT_EQ(buf1.size(), buf2.size());
  EXPECT_EQ(buf1.capacity(), buf2.capacity());
  const uint8_t* data1 = buf1.data();
  const uint8_t* data2 = buf2.data();
  EXPECT_EQ(data1, data2);
  EXPECT_EQ(buf1, buf2);
}

void EnsureBuffersDontShareData(const CopyOnWriteBuffer& buf1,
                                const CopyOnWriteBuffer& buf2) {
  // Data is not shared between buffers.
  const uint8_t* data1 = buf1.cdata();
  const uint8_t* data2 = buf2.cdata();
  EXPECT_NE(data1, data2);
}

TEST(CopyOnWriteBufferTest, TestCreateEmptyData) {
  CopyOnWriteBuffer buf(static_cast<const uint8_t*>(nullptr), 0);
  EXPECT_EQ(buf.size(), 0u);
  EXPECT_EQ(buf.capacity(), 0u);
  EXPECT_EQ(buf.data(), nullptr);
}

TEST(CopyOnWriteBufferTest, TestMoveConstruct) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  size_t buf1_size = buf1.size();
  size_t buf1_capacity = buf1.capacity();
  const uint8_t* buf1_data = buf1.cdata();

  CopyOnWriteBuffer buf2(std::move(buf1));
  EXPECT_EQ(buf1.size(), 0u);
  EXPECT_EQ(buf1.capacity(), 0u);
  EXPECT_EQ(buf1.data(), nullptr);
  EXPECT_EQ(buf2.size(), buf1_size);
  EXPECT_EQ(buf2.capacity(), buf1_capacity);
  EXPECT_EQ(buf2.data(), buf1_data);
}

TEST(CopyOnWriteBufferTest, TestMoveAssign) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  size_t buf1_size = buf1.size();
  size_t buf1_capacity = buf1.capacity();
  const uint8_t* buf1_data = buf1.cdata();

  CopyOnWriteBuffer buf2;
  buf2 = std::move(buf1);
  EXPECT_EQ(buf1.size(), 0u);
  EXPECT_EQ(buf1.capacity(), 0u);
  EXPECT_EQ(buf1.data(), nullptr);
  EXPECT_EQ(buf2.size(), buf1_size);
  EXPECT_EQ(buf2.capacity(), buf1_capacity);
  EXPECT_EQ(buf2.data(), buf1_data);
}

TEST(CopyOnWriteBufferTest, TestSwap) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  size_t buf1_size = buf1.size();
  size_t buf1_capacity = buf1.capacity();
  const uint8_t* buf1_data = buf1.cdata();

  CopyOnWriteBuffer buf2(kTestData, 6, 20);
  size_t buf2_size = buf2.size();
  size_t buf2_capacity = buf2.capacity();
  const uint8_t* buf2_data = buf2.cdata();

  std::swap(buf1, buf2);
  EXPECT_EQ(buf1.size(), buf2_size);
  EXPECT_EQ(buf1.capacity(), buf2_capacity);
  EXPECT_EQ(buf1.data(), buf2_data);
  EXPECT_EQ(buf2.size(), buf1_size);
  EXPECT_EQ(buf2.capacity(), buf1_capacity);
  EXPECT_EQ(buf2.data(), buf1_data);
}

TEST(CopyOnWriteBufferTest, TestAppendData) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2(buf1);

  EnsureBuffersShareData(buf1, buf2);

  // AppendData copies the underlying buffer.
  buf2.AppendData("foo");
  EXPECT_EQ(buf2.size(), buf1.size() + 4);  // "foo" + trailing 0x00
  EXPECT_EQ(buf2.capacity(), buf1.capacity());
  EXPECT_NE(buf2.data(), buf1.data());

  EXPECT_EQ(buf1, CopyOnWriteBuffer(kTestData, 3));
  const int8_t exp[] = {0x0, 0x1, 0x2, 'f', 'o', 'o', 0x0};
  EXPECT_EQ(buf2, CopyOnWriteBuffer(exp));
}

TEST(CopyOnWriteBufferTest, SetEmptyData) {
  CopyOnWriteBuffer buf(10);

  buf.SetData<uint8_t>(nullptr, 0);

  EXPECT_EQ(0u, buf.size());
}

TEST(CopyOnWriteBufferTest, SetDataNoMoreThanCapacityDoesntCauseReallocation) {
  CopyOnWriteBuffer buf1(3, 10);
  const uint8_t* const original_allocation = buf1.cdata();

  buf1.SetData(kTestData, 10);

  EXPECT_EQ(original_allocation, buf1.cdata());
  EXPECT_EQ(buf1, CopyOnWriteBuffer(kTestData, 10));
}

TEST(CopyOnWriteBufferTest, SetDataMakeReferenceCopy) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2;

  buf2.SetData(buf1);

  EnsureBuffersShareData(buf1, buf2);
}

TEST(CopyOnWriteBufferTest, SetDataOnSharedKeepsOriginal) {
  const uint8_t data[] = "foo";
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  const uint8_t* const original_allocation = buf1.cdata();
  CopyOnWriteBuffer buf2(buf1);

  buf2.SetData(data);

  EnsureBuffersDontShareData(buf1, buf2);
  EXPECT_EQ(original_allocation, buf1.cdata());
  EXPECT_EQ(buf1, CopyOnWriteBuffer(kTestData, 3));
  EXPECT_EQ(buf2, CopyOnWriteBuffer(data));
}

TEST(CopyOnWriteBufferTest, SetDataOnSharedKeepsCapacity) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2(buf1);
  EnsureBuffersShareData(buf1, buf2);

  buf2.SetData(kTestData, 2);

  EnsureBuffersDontShareData(buf1, buf2);
  EXPECT_EQ(2u, buf2.size());
  EXPECT_EQ(10u, buf2.capacity());
}

TEST(CopyOnWriteBufferTest, TestEnsureCapacity) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2(buf1);

  // Smaller than existing capacity -> no change and still same contents.
  buf2.EnsureCapacity(8);
  EnsureBuffersShareData(buf1, buf2);
  EXPECT_EQ(buf1.size(), 3u);
  EXPECT_EQ(buf1.capacity(), 10u);
  EXPECT_EQ(buf2.size(), 3u);
  EXPECT_EQ(buf2.capacity(), 10u);

  // Lager than existing capacity -> data is cloned.
  buf2.EnsureCapacity(16);
  EnsureBuffersDontShareData(buf1, buf2);
  EXPECT_EQ(buf1.size(), 3u);
  EXPECT_EQ(buf1.capacity(), 10u);
  EXPECT_EQ(buf2.size(), 3u);
  EXPECT_EQ(buf2.capacity(), 16u);
  // The size and contents are still the same.
  EXPECT_EQ(buf1, buf2);
}

TEST(CopyOnWriteBufferTest, SetSizeDoesntChangeOriginal) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  const uint8_t* const original_allocation = buf1.cdata();
  CopyOnWriteBuffer buf2(buf1);

  buf2.SetSize(16);

  EnsureBuffersDontShareData(buf1, buf2);
  EXPECT_EQ(original_allocation, buf1.cdata());
  EXPECT_EQ(3u, buf1.size());
  EXPECT_EQ(10u, buf1.capacity());
}

TEST(CopyOnWriteBufferTest, SetSizeCloneContent) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2(buf1);

  buf2.SetSize(16);

  EXPECT_EQ(buf2.size(), 16u);
  EXPECT_EQ(0, memcmp(buf2.data(), kTestData, 3));
}

TEST(CopyOnWriteBufferTest, SetSizeMayIncreaseCapacity) {
  CopyOnWriteBuffer buf(kTestData, 3, 10);

  buf.SetSize(16);

  EXPECT_EQ(16u, buf.size());
  EXPECT_EQ(16u, buf.capacity());
}

TEST(CopyOnWriteBufferTest, SetSizeDoesntDecreaseCapacity) {
  CopyOnWriteBuffer buf1(kTestData, 5, 10);
  CopyOnWriteBuffer buf2(buf1);

  buf2.SetSize(2);

  EXPECT_EQ(2u, buf2.size());
  EXPECT_EQ(10u, buf2.capacity());
}

TEST(CopyOnWriteBufferTest, ClearDoesntChangeOriginal) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  const uint8_t* const original_allocation = buf1.cdata();
  CopyOnWriteBuffer buf2(buf1);

  buf2.Clear();

  EnsureBuffersDontShareData(buf1, buf2);
  EXPECT_EQ(3u, buf1.size());
  EXPECT_EQ(10u, buf1.capacity());
  EXPECT_EQ(original_allocation, buf1.cdata());
  EXPECT_EQ(0u, buf2.size());
}

TEST(CopyOnWriteBufferTest, ClearDoesntChangeCapacity) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2(buf1);

  buf2.Clear();

  EXPECT_EQ(0u, buf2.size());
  EXPECT_EQ(10u, buf2.capacity());
}

TEST(CopyOnWriteBufferTest, TestConstDataAccessor) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2(buf1);

  // .cdata() doesn't clone data.
  const uint8_t* cdata1 = buf1.cdata();
  const uint8_t* cdata2 = buf2.cdata();
  EXPECT_EQ(cdata1, cdata2);

  // Non-const .data() clones data if shared.
  const uint8_t* data1 = buf1.data();
  const uint8_t* data2 = buf2.data();
  EXPECT_NE(data1, data2);
  // buf1 was cloned above.
  EXPECT_NE(data1, cdata1);
  // Therefore buf2 was no longer sharing data and was not cloned.
  EXPECT_EQ(data2, cdata1);
}

TEST(CopyOnWriteBufferTest, TestBacketRead) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2(buf1);

  EnsureBuffersShareData(buf1, buf2);
  // Non-const reads clone the data if shared.
  for (size_t i = 0; i != 3u; ++i) {
    EXPECT_EQ(buf1[i], kTestData[i]);
  }
  EnsureBuffersDontShareData(buf1, buf2);
}

TEST(CopyOnWriteBufferTest, TestBacketReadConst) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2(buf1);

  EnsureBuffersShareData(buf1, buf2);
  const CopyOnWriteBuffer& cbuf1 = buf1;
  for (size_t i = 0; i != 3u; ++i) {
    EXPECT_EQ(cbuf1[i], kTestData[i]);
  }
  EnsureBuffersShareData(buf1, buf2);
}

TEST(CopyOnWriteBufferTest, TestBacketWrite) {
  CopyOnWriteBuffer buf1(kTestData, 3, 10);
  CopyOnWriteBuffer buf2(buf1);

  EnsureBuffersShareData(buf1, buf2);
  for (size_t i = 0; i != 3u; ++i) {
    buf1[i] = kTestData[i] + 1;
  }
  EXPECT_EQ(buf1.size(), 3u);
  EXPECT_EQ(buf1.capacity(), 10u);
  EXPECT_EQ(buf2.size(), 3u);
  EXPECT_EQ(buf2.capacity(), 10u);
  EXPECT_EQ(0, memcmp(buf2.cdata(), kTestData, 3));
}

TEST(CopyOnWriteBufferTest, CreateSlice) {
  CopyOnWriteBuffer buf(kTestData, 10, 10);
  CopyOnWriteBuffer slice = buf.Slice(3, 4);
  EXPECT_EQ(slice.size(), 4u);
  EXPECT_EQ(0, memcmp(buf.cdata() + 3, slice.cdata(), 4));
}

TEST(CopyOnWriteBufferTest, NoCopyDataOnSlice) {
  CopyOnWriteBuffer buf(kTestData, 10, 10);
  CopyOnWriteBuffer slice = buf.Slice(3, 4);
  EXPECT_EQ(buf.cdata() + 3, slice.cdata());
}

TEST(CopyOnWriteBufferTest, WritingCopiesData) {
  CopyOnWriteBuffer buf(kTestData, 10, 10);
  CopyOnWriteBuffer slice = buf.Slice(3, 4);
  slice[0] = 0xaa;
  EXPECT_NE(buf.cdata() + 3, slice.cdata());
  EXPECT_EQ(0, memcmp(buf.cdata(), kTestData, 10));
}

TEST(CopyOnWriteBufferTest, WritingToBufferDoesntAffectsSlice) {
  CopyOnWriteBuffer buf(kTestData, 10, 10);
  CopyOnWriteBuffer slice = buf.Slice(3, 4);
  buf[0] = 0xaa;
  EXPECT_NE(buf.cdata() + 3, slice.cdata());
  EXPECT_EQ(0, memcmp(slice.cdata(), kTestData + 3, 4));
}

TEST(CopyOnWriteBufferTest, SliceOfASlice) {
  CopyOnWriteBuffer buf(kTestData, 10, 10);
  CopyOnWriteBuffer slice = buf.Slice(3, 7);
  CopyOnWriteBuffer slice2 = slice.Slice(2, 3);
  EXPECT_EQ(slice2.size(), 3u);
  EXPECT_EQ(slice.cdata() + 2, slice2.cdata());
  EXPECT_EQ(buf.cdata() + 5, slice2.cdata());
}

TEST(CopyOnWriteBufferTest, SlicesAreIndependent) {
  CopyOnWriteBuffer buf(kTestData, 10, 10);
  CopyOnWriteBuffer slice = buf.Slice(3, 7);
  CopyOnWriteBuffer slice2 = buf.Slice(3, 7);
  slice2[0] = 0xaa;
  EXPECT_EQ(buf.cdata() + 3, slice.cdata());
}

}  // namespace rtc
