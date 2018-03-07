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

import java.nio.ByteBuffer;

/** Wraps libyuv methods to Java. All passed byte buffers must be direct byte buffers. */
public class YuvHelper {
  /** Helper method for copying I420 to tightly packed destination buffer. */
  public static void I420Copy(ByteBuffer srcY, int srcStrideY, ByteBuffer srcU, int srcStrideU,
      ByteBuffer srcV, int srcStrideV, ByteBuffer dst, int width, int height) {
    final int chromaHeight = (height + 1) / 2;
    final int chromaWidth = (width + 1) / 2;

    final int minSize = width * height + chromaWidth * chromaHeight * 2;
    if (dst.capacity() < minSize) {
      throw new IllegalArgumentException("Expected destination buffer capacity to be at least "
          + minSize + " was " + dst.capacity());
    }

    final int startY = 0;
    final int startU = height * width;
    final int startV = startU + chromaHeight * chromaWidth;

    dst.position(startY);
    final ByteBuffer dstY = dst.slice();
    dst.position(startU);
    final ByteBuffer dstU = dst.slice();
    dst.position(startV);
    final ByteBuffer dstV = dst.slice();

    I420Copy(srcY, srcStrideY, srcU, srcStrideU, srcV, srcStrideV, dstY, width, dstU, chromaWidth,
        dstV, chromaWidth, width, height);
  }

  /** Helper method for copying I420 to tightly packed NV12 destination buffer. */
  public static void I420ToNV12(ByteBuffer srcY, int srcStrideY, ByteBuffer srcU, int srcStrideU,
      ByteBuffer srcV, int srcStrideV, ByteBuffer dst, int width, int height) {
    final int chromaWidth = (width + 1) / 2;
    final int chromaHeight = (height + 1) / 2;

    final int minSize = width * height + chromaWidth * chromaHeight * 2;
    if (dst.capacity() < minSize) {
      throw new IllegalArgumentException("Expected destination buffer capacity to be at least "
          + minSize + " was " + dst.capacity());
    }

    final int startY = 0;
    final int startUV = height * width;

    dst.position(startY);
    final ByteBuffer dstY = dst.slice();
    dst.position(startUV);
    final ByteBuffer dstUV = dst.slice();

    I420ToNV12(srcY, srcStrideY, srcU, srcStrideU, srcV, srcStrideV, dstY, width, dstUV,
        chromaWidth * 2, width, height);
  }

  public static native void I420Copy(ByteBuffer srcY, int srcStrideY, ByteBuffer srcU,
      int srcStrideU, ByteBuffer srcV, int srcStrideV, ByteBuffer dstY, int dstStrideY,
      ByteBuffer dstU, int dstStrideU, ByteBuffer dstV, int dstStrideV, int width, int height);
  public static native void I420ToNV12(ByteBuffer srcY, int srcStrideY, ByteBuffer srcU,
      int srcStrideU, ByteBuffer srcV, int srcStrideV, ByteBuffer dstY, int dstStrideY,
      ByteBuffer dstUV, int dstStrideUV, int width, int height);
}
