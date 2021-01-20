/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/memory/fifo_buffer.h"

#include <string.h>

#include "test/gtest.h"

namespace rtc {

TEST(FifoBufferTest, TestAll) {
  const size_t kSize = 16;
  const char in[kSize * 2 + 1] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
  char out[kSize * 2];
  void* p;
  const void* q;
  size_t bytes;
  FifoBuffer buf(kSize);

  // Test assumptions about base state
  EXPECT_EQ(SS_OPEN, buf.GetState());
  EXPECT_EQ(SR_BLOCK, buf.Read(out, kSize, &bytes, nullptr));
  EXPECT_TRUE(nullptr != buf.GetWriteBuffer(&bytes));
  EXPECT_EQ(kSize, bytes);
  buf.ConsumeWriteBuffer(0);

  // Try a full write
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize, &bytes, nullptr));
  EXPECT_EQ(kSize, bytes);

  // Try a write that should block
  EXPECT_EQ(SR_BLOCK, buf.Write(in, kSize, &bytes, nullptr));

  // Try a full read
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize, &bytes, nullptr));
  EXPECT_EQ(kSize, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize));

  // Try a read that should block
  EXPECT_EQ(SR_BLOCK, buf.Read(out, kSize, &bytes, nullptr));

  // Try a too-big write
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize * 2, &bytes, nullptr));
  EXPECT_EQ(bytes, kSize);

  // Try a too-big read
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize * 2, &bytes, nullptr));
  EXPECT_EQ(kSize, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize));

  // Try some small writes and reads
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize / 2));
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize / 2));
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize / 2));

  // Try wraparound reads and writes in the following pattern
  // WWWWWWWWWWWW.... 0123456789AB....
  // RRRRRRRRXXXX.... ........89AB....
  // WWWW....XXXXWWWW 4567....89AB0123
  // XXXX....RRRRXXXX 4567........0123
  // XXXXWWWWWWWWXXXX 4567012345670123
  // RRRRXXXXXXXXRRRR ....01234567....
  // ....RRRRRRRR.... ................
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize * 3 / 4, &bytes, nullptr));
  EXPECT_EQ(kSize * 3 / 4, bytes);
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize / 2));
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize / 4, &bytes, nullptr));
  EXPECT_EQ(kSize / 4, bytes);
  EXPECT_EQ(0, memcmp(in + kSize / 2, out, kSize / 4));
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize / 2));
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize / 2));

  // Use GetWriteBuffer to reset the read_position for the next tests
  buf.GetWriteBuffer(&bytes);
  buf.ConsumeWriteBuffer(0);

  // Try using GetReadData to do a full read
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize, &bytes, nullptr));
  q = buf.GetReadData(&bytes);
  EXPECT_TRUE(nullptr != q);
  EXPECT_EQ(kSize, bytes);
  EXPECT_EQ(0, memcmp(q, in, kSize));
  buf.ConsumeReadData(kSize);
  EXPECT_EQ(SR_BLOCK, buf.Read(out, kSize, &bytes, nullptr));

  // Try using GetReadData to do some small reads
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize, &bytes, nullptr));
  q = buf.GetReadData(&bytes);
  EXPECT_TRUE(nullptr != q);
  EXPECT_EQ(kSize, bytes);
  EXPECT_EQ(0, memcmp(q, in, kSize / 2));
  buf.ConsumeReadData(kSize / 2);
  q = buf.GetReadData(&bytes);
  EXPECT_TRUE(nullptr != q);
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(0, memcmp(q, in + kSize / 2, kSize / 2));
  buf.ConsumeReadData(kSize / 2);
  EXPECT_EQ(SR_BLOCK, buf.Read(out, kSize, &bytes, nullptr));

  // Try using GetReadData in a wraparound case
  // WWWWWWWWWWWWWWWW 0123456789ABCDEF
  // RRRRRRRRRRRRXXXX ............CDEF
  // WWWWWWWW....XXXX 01234567....CDEF
  // ............RRRR 01234567........
  // RRRRRRRR........ ................
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize, &bytes, nullptr));
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize * 3 / 4, &bytes, nullptr));
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize / 2, &bytes, nullptr));
  q = buf.GetReadData(&bytes);
  EXPECT_TRUE(nullptr != q);
  EXPECT_EQ(kSize / 4, bytes);
  EXPECT_EQ(0, memcmp(q, in + kSize * 3 / 4, kSize / 4));
  buf.ConsumeReadData(kSize / 4);
  q = buf.GetReadData(&bytes);
  EXPECT_TRUE(nullptr != q);
  EXPECT_EQ(kSize / 2, bytes);
  EXPECT_EQ(0, memcmp(q, in, kSize / 2));
  buf.ConsumeReadData(kSize / 2);

  // Use GetWriteBuffer to reset the read_position for the next tests
  buf.GetWriteBuffer(&bytes);
  buf.ConsumeWriteBuffer(0);

  // Try using GetWriteBuffer to do a full write
  p = buf.GetWriteBuffer(&bytes);
  EXPECT_TRUE(nullptr != p);
  EXPECT_EQ(kSize, bytes);
  memcpy(p, in, kSize);
  buf.ConsumeWriteBuffer(kSize);
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize, &bytes, nullptr));
  EXPECT_EQ(kSize, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize));

  // Try using GetWriteBuffer to do some small writes
  p = buf.GetWriteBuffer(&bytes);
  EXPECT_TRUE(nullptr != p);
  EXPECT_EQ(kSize, bytes);
  memcpy(p, in, kSize / 2);
  buf.ConsumeWriteBuffer(kSize / 2);
  p = buf.GetWriteBuffer(&bytes);
  EXPECT_TRUE(nullptr != p);
  EXPECT_EQ(kSize / 2, bytes);
  memcpy(p, in + kSize / 2, kSize / 2);
  buf.ConsumeWriteBuffer(kSize / 2);
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize, &bytes, nullptr));
  EXPECT_EQ(kSize, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize));

  // Try using GetWriteBuffer in a wraparound case
  // WWWWWWWWWWWW.... 0123456789AB....
  // RRRRRRRRXXXX.... ........89AB....
  // ........XXXXWWWW ........89AB0123
  // WWWW....XXXXXXXX 4567....89AB0123
  // RRRR....RRRRRRRR ................
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize * 3 / 4, &bytes, nullptr));
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize / 2, &bytes, nullptr));
  p = buf.GetWriteBuffer(&bytes);
  EXPECT_TRUE(nullptr != p);
  EXPECT_EQ(kSize / 4, bytes);
  memcpy(p, in, kSize / 4);
  buf.ConsumeWriteBuffer(kSize / 4);
  p = buf.GetWriteBuffer(&bytes);
  EXPECT_TRUE(nullptr != p);
  EXPECT_EQ(kSize / 2, bytes);
  memcpy(p, in + kSize / 4, kSize / 4);
  buf.ConsumeWriteBuffer(kSize / 4);
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize * 3 / 4, &bytes, nullptr));
  EXPECT_EQ(kSize * 3 / 4, bytes);
  EXPECT_EQ(0, memcmp(in + kSize / 2, out, kSize / 4));
  EXPECT_EQ(0, memcmp(in, out + kSize / 4, kSize / 4));

  // Check that the stream is now empty
  EXPECT_EQ(SR_BLOCK, buf.Read(out, kSize, &bytes, nullptr));

  // Try growing the buffer
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize, &bytes, nullptr));
  EXPECT_EQ(kSize, bytes);
  EXPECT_TRUE(buf.SetCapacity(kSize * 2));
  EXPECT_EQ(SR_SUCCESS, buf.Write(in + kSize, kSize, &bytes, nullptr));
  EXPECT_EQ(kSize, bytes);
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize * 2, &bytes, nullptr));
  EXPECT_EQ(kSize * 2, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize * 2));

  // Try shrinking the buffer
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize, &bytes, nullptr));
  EXPECT_EQ(kSize, bytes);
  EXPECT_TRUE(buf.SetCapacity(kSize));
  EXPECT_EQ(SR_BLOCK, buf.Write(in, kSize, &bytes, nullptr));
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize, &bytes, nullptr));
  EXPECT_EQ(kSize, bytes);
  EXPECT_EQ(0, memcmp(in, out, kSize));

  // Write to the stream, close it, read the remaining bytes
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, kSize / 2, &bytes, nullptr));
  buf.Close();
  EXPECT_EQ(SS_CLOSED, buf.GetState());
  EXPECT_EQ(SR_EOS, buf.Write(in, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(SR_SUCCESS, buf.Read(out, kSize / 2, &bytes, nullptr));
  EXPECT_EQ(0, memcmp(in, out, kSize / 2));
  EXPECT_EQ(SR_EOS, buf.Read(out, kSize / 2, &bytes, nullptr));
}

TEST(FifoBufferTest, FullBufferCheck) {
  FifoBuffer buff(10);
  buff.ConsumeWriteBuffer(10);

  size_t free;
  EXPECT_TRUE(buff.GetWriteBuffer(&free) != nullptr);
  EXPECT_EQ(0U, free);
}

TEST(FifoBufferTest, WriteOffsetAndReadOffset) {
  const size_t kSize = 16;
  const char in[kSize * 2 + 1] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
  char out[kSize * 2];
  FifoBuffer buf(kSize);

  // Write 14 bytes.
  EXPECT_EQ(SR_SUCCESS, buf.Write(in, 14, nullptr, nullptr));

  // Make sure data is in |buf|.
  size_t buffered;
  EXPECT_TRUE(buf.GetBuffered(&buffered));
  EXPECT_EQ(14u, buffered);

  // Read 10 bytes.
  buf.ConsumeReadData(10);

  // There should be now 12 bytes of available space.
  size_t remaining;
  EXPECT_TRUE(buf.GetWriteRemaining(&remaining));
  EXPECT_EQ(12u, remaining);

  // Write at offset 12, this should fail.
  EXPECT_EQ(SR_BLOCK, buf.WriteOffset(in, 10, 12, nullptr));

  // Write 8 bytes at offset 4, this wraps around the buffer.
  EXPECT_EQ(SR_SUCCESS, buf.WriteOffset(in, 8, 4, nullptr));

  // Number of available space remains the same until we call
  // ConsumeWriteBuffer().
  EXPECT_TRUE(buf.GetWriteRemaining(&remaining));
  EXPECT_EQ(12u, remaining);
  buf.ConsumeWriteBuffer(12);

  // There's 4 bytes bypassed and 4 bytes no read so skip them and verify the
  // 8 bytes written.
  size_t read;
  EXPECT_EQ(SR_SUCCESS, buf.ReadOffset(out, 8, 8, &read));
  EXPECT_EQ(8u, read);
  EXPECT_EQ(0, memcmp(out, in, 8));

  // There should still be 16 bytes available for reading.
  EXPECT_TRUE(buf.GetBuffered(&buffered));
  EXPECT_EQ(16u, buffered);

  // Read at offset 16, this should fail since we don't have that much data.
  EXPECT_EQ(SR_BLOCK, buf.ReadOffset(out, 10, 16, nullptr));
}

}  // namespace rtc
