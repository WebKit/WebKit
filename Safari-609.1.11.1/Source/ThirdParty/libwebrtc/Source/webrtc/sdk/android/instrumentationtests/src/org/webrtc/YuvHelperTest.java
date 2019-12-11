/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;
import java.nio.ByteBuffer;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(BaseJUnit4ClassRunner.class)
public class YuvHelperTest {
  private static final int TEST_WIDTH = 3;
  private static final int TEST_HEIGHT = 3;
  private static final int TEST_CHROMA_WIDTH = 2;
  private static final int TEST_CHROMA_HEIGHT = 2;

  private static final int TEST_I420_STRIDE_Y = 3;
  private static final int TEST_I420_STRIDE_V = 2;
  private static final int TEST_I420_STRIDE_U = 4;

  private static final ByteBuffer TEST_I420_Y = getTestY();
  private static final ByteBuffer TEST_I420_U = getTestU();
  private static final ByteBuffer TEST_I420_V = getTestV();

  private static ByteBuffer getTestY() {
    final ByteBuffer testY = ByteBuffer.allocateDirect(TEST_HEIGHT * TEST_I420_STRIDE_Y);
    testY.put(new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9});
    return testY;
  }

  private static ByteBuffer getTestU() {
    final ByteBuffer testU = ByteBuffer.allocateDirect(TEST_CHROMA_HEIGHT * TEST_I420_STRIDE_V);
    testU.put(new byte[] {51, 52, 53, 54});
    return testU;
  }

  private static ByteBuffer getTestV() {
    final ByteBuffer testV = ByteBuffer.allocateDirect(TEST_CHROMA_HEIGHT * TEST_I420_STRIDE_U);
    testV.put(new byte[] {101, 102, 103, 104, 105, 106, 107, 108});
    return testV;
  }

  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader(), TestConstants.NATIVE_LIBRARY);
  }

  @SmallTest
  @Test
  public void testCopyPlane() {
    final int dstStride = TEST_WIDTH;
    final ByteBuffer dst = ByteBuffer.allocateDirect(TEST_HEIGHT * dstStride);

    YuvHelper.copyPlane(TEST_I420_Y, TEST_I420_STRIDE_Y, dst, dstStride, TEST_WIDTH, TEST_HEIGHT);

    assertByteBufferContentEquals(new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9}, dst);
  }

  @SmallTest
  @Test
  public void testI420Copy() {
    final int dstStrideY = TEST_WIDTH;
    final int dstStrideU = TEST_CHROMA_WIDTH;
    final int dstStrideV = TEST_CHROMA_WIDTH;
    final ByteBuffer dstY = ByteBuffer.allocateDirect(TEST_HEIGHT * dstStrideY);
    final ByteBuffer dstU = ByteBuffer.allocateDirect(TEST_CHROMA_HEIGHT * dstStrideU);
    final ByteBuffer dstV = ByteBuffer.allocateDirect(TEST_CHROMA_HEIGHT * dstStrideV);

    YuvHelper.I420Copy(TEST_I420_Y, TEST_I420_STRIDE_Y, TEST_I420_U, TEST_I420_STRIDE_V,
        TEST_I420_V, TEST_I420_STRIDE_U, dstY, dstStrideY, dstU, dstStrideU, dstV, dstStrideV,
        TEST_WIDTH, TEST_HEIGHT);

    assertByteBufferContentEquals(new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9}, dstY);
    assertByteBufferContentEquals(new byte[] {51, 52, 53, 54}, dstU);
    assertByteBufferContentEquals(new byte[] {101, 102, 105, 106}, dstV);
  }

  @SmallTest
  @Test
  public void testI420CopyTight() {
    final ByteBuffer dst = ByteBuffer.allocateDirect(
        TEST_WIDTH * TEST_HEIGHT + TEST_CHROMA_WIDTH * TEST_CHROMA_HEIGHT * 2);

    YuvHelper.I420Copy(TEST_I420_Y, TEST_I420_STRIDE_Y, TEST_I420_U, TEST_I420_STRIDE_V,
        TEST_I420_V, TEST_I420_STRIDE_U, dst, TEST_WIDTH, TEST_HEIGHT);

    assertByteBufferContentEquals(
        new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 51, 52, 53, 54, 101, 102, 105, 106}, dst);
  }

  @SmallTest
  @Test
  public void testI420ToNV12() {
    final int dstStrideY = TEST_WIDTH;
    final int dstStrideUV = TEST_CHROMA_WIDTH * 2;
    final ByteBuffer dstY = ByteBuffer.allocateDirect(TEST_HEIGHT * dstStrideY);
    final ByteBuffer dstUV = ByteBuffer.allocateDirect(2 * TEST_CHROMA_HEIGHT * dstStrideUV);

    YuvHelper.I420ToNV12(TEST_I420_Y, TEST_I420_STRIDE_Y, TEST_I420_U, TEST_I420_STRIDE_V,
        TEST_I420_V, TEST_I420_STRIDE_U, dstY, dstStrideY, dstUV, dstStrideUV, TEST_WIDTH,
        TEST_HEIGHT);

    assertByteBufferContentEquals(new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9}, dstY);
    assertByteBufferContentEquals(new byte[] {51, 101, 52, 102, 53, 105, 54, 106}, dstUV);
  }

  @SmallTest
  @Test
  public void testI420ToNV12Tight() {
    final int dstStrideY = TEST_WIDTH;
    final int dstStrideUV = TEST_CHROMA_WIDTH * 2;
    final ByteBuffer dst = ByteBuffer.allocateDirect(
        TEST_WIDTH * TEST_HEIGHT + TEST_CHROMA_WIDTH * TEST_CHROMA_HEIGHT * 2);

    YuvHelper.I420ToNV12(TEST_I420_Y, TEST_I420_STRIDE_Y, TEST_I420_U, TEST_I420_STRIDE_V,
        TEST_I420_V, TEST_I420_STRIDE_U, dst, TEST_WIDTH, TEST_HEIGHT);

    assertByteBufferContentEquals(
        new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 51, 101, 52, 102, 53, 105, 54, 106}, dst);
  }

  private static void assertByteBufferContentEquals(byte[] expected, ByteBuffer test) {
    assertTrue(
        "ByteBuffer is too small. Expected " + expected.length + " but was " + test.capacity(),
        test.capacity() >= expected.length);
    for (int i = 0; i < expected.length; i++) {
      assertEquals("Unexpected ByteBuffer contents at index: " + i, expected[i], test.get(i));
    }
  }

  @SmallTest
  @Test
  public void testI420Rotate90() {
    final int dstStrideY = TEST_HEIGHT;
    final int dstStrideU = TEST_CHROMA_HEIGHT;
    final int dstStrideV = TEST_CHROMA_HEIGHT;
    final ByteBuffer dstY = ByteBuffer.allocateDirect(TEST_WIDTH * dstStrideY);
    final ByteBuffer dstU = ByteBuffer.allocateDirect(TEST_CHROMA_WIDTH * dstStrideU);
    final ByteBuffer dstV = ByteBuffer.allocateDirect(TEST_CHROMA_WIDTH * dstStrideV);

    YuvHelper.I420Rotate(TEST_I420_Y, TEST_I420_STRIDE_Y, TEST_I420_U, TEST_I420_STRIDE_V,
        TEST_I420_V, TEST_I420_STRIDE_U, dstY, dstStrideY, dstU, dstStrideU, dstV, dstStrideV,
        TEST_WIDTH, TEST_HEIGHT, 90);

    assertByteBufferContentEquals(new byte[] {7, 4, 1, 8, 5, 2, 9, 6, 3}, dstY);
    assertByteBufferContentEquals(new byte[] {53, 51, 54, 52}, dstU);
    assertByteBufferContentEquals(new byte[] {105, 101, 106, 102}, dstV);
  }

  @SmallTest
  @Test
  public void testI420Rotate90Tight() {
    final ByteBuffer dst = ByteBuffer.allocateDirect(
        TEST_WIDTH * TEST_HEIGHT + TEST_CHROMA_WIDTH * TEST_CHROMA_HEIGHT * 2);

    YuvHelper.I420Rotate(TEST_I420_Y, TEST_I420_STRIDE_Y, TEST_I420_U, TEST_I420_STRIDE_V,
        TEST_I420_V, TEST_I420_STRIDE_U, dst, TEST_WIDTH, TEST_HEIGHT, 90);

    assertByteBufferContentEquals(
        new byte[] {7, 4, 1, 8, 5, 2, 9, 6, 3, 53, 51, 54, 52, 105, 101, 106, 102}, dst);
  }
}
