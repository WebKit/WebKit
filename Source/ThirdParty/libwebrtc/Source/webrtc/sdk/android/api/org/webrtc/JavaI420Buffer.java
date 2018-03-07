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
import org.webrtc.VideoFrame.I420Buffer;

/** Implementation of VideoFrame.I420Buffer backed by Java direct byte buffers. */
public class JavaI420Buffer implements VideoFrame.I420Buffer {
  private final int width;
  private final int height;
  private final ByteBuffer dataY;
  private final ByteBuffer dataU;
  private final ByteBuffer dataV;
  private final int strideY;
  private final int strideU;
  private final int strideV;
  private final Runnable releaseCallback;
  private final Object refCountLock = new Object();

  private int refCount;

  private JavaI420Buffer(int width, int height, ByteBuffer dataY, int strideY, ByteBuffer dataU,
      int strideU, ByteBuffer dataV, int strideV, Runnable releaseCallback) {
    this.width = width;
    this.height = height;
    this.dataY = dataY;
    this.dataU = dataU;
    this.dataV = dataV;
    this.strideY = strideY;
    this.strideU = strideU;
    this.strideV = strideV;
    this.releaseCallback = releaseCallback;

    this.refCount = 1;
  }

  /** Wraps existing ByteBuffers into JavaI420Buffer object without copying the contents. */
  public static JavaI420Buffer wrap(int width, int height, ByteBuffer dataY, int strideY,
      ByteBuffer dataU, int strideU, ByteBuffer dataV, int strideV, Runnable releaseCallback) {
    if (dataY == null || dataU == null || dataV == null) {
      throw new IllegalArgumentException("Data buffers cannot be null.");
    }
    if (!dataY.isDirect() || !dataU.isDirect() || !dataV.isDirect()) {
      throw new IllegalArgumentException("Data buffers must be direct byte buffers.");
    }

    // Slice the buffers to prevent external modifications to the position / limit of the buffer.
    // Note that this doesn't protect the contents of the buffers from modifications.
    dataY = dataY.slice();
    dataU = dataU.slice();
    dataV = dataV.slice();

    final int chromaHeight = (height + 1) / 2;
    final int minCapacityY = strideY * height;
    final int minCapacityU = strideU * chromaHeight;
    final int minCapacityV = strideV * chromaHeight;
    if (dataY.capacity() < minCapacityY) {
      throw new IllegalArgumentException("Y-buffer must be at least " + minCapacityY + " bytes.");
    }
    if (dataU.capacity() < minCapacityU) {
      throw new IllegalArgumentException("U-buffer must be at least " + minCapacityU + " bytes.");
    }
    if (dataV.capacity() < minCapacityV) {
      throw new IllegalArgumentException("V-buffer must be at least " + minCapacityV + " bytes.");
    }

    return new JavaI420Buffer(
        width, height, dataY, strideY, dataU, strideU, dataV, strideV, releaseCallback);
  }

  /** Allocates an empty I420Buffer suitable for an image of the given dimensions. */
  public static JavaI420Buffer allocate(int width, int height) {
    int chromaHeight = (height + 1) / 2;
    int strideUV = (width + 1) / 2;
    int yPos = 0;
    int uPos = yPos + width * height;
    int vPos = uPos + strideUV * chromaHeight;

    ByteBuffer buffer = ByteBuffer.allocateDirect(width * height + 2 * strideUV * chromaHeight);

    buffer.position(yPos);
    buffer.limit(uPos);
    ByteBuffer dataY = buffer.slice();

    buffer.position(uPos);
    buffer.limit(vPos);
    ByteBuffer dataU = buffer.slice();

    buffer.position(vPos);
    buffer.limit(vPos + strideUV * chromaHeight);
    ByteBuffer dataV = buffer.slice();

    return new JavaI420Buffer(
        width, height, dataY, width, dataU, strideUV, dataV, strideUV, null /* releaseCallback */);
  }

  @Override
  public int getWidth() {
    return width;
  }

  @Override
  public int getHeight() {
    return height;
  }

  @Override
  public ByteBuffer getDataY() {
    // Return a slice to prevent relative reads from changing the position.
    return dataY.slice();
  }

  @Override
  public ByteBuffer getDataU() {
    // Return a slice to prevent relative reads from changing the position.
    return dataU.slice();
  }

  @Override
  public ByteBuffer getDataV() {
    // Return a slice to prevent relative reads from changing the position.
    return dataV.slice();
  }

  @Override
  public int getStrideY() {
    return strideY;
  }

  @Override
  public int getStrideU() {
    return strideU;
  }

  @Override
  public int getStrideV() {
    return strideV;
  }

  @Override
  public I420Buffer toI420() {
    retain();
    return this;
  }

  @Override
  public void retain() {
    synchronized (refCountLock) {
      ++refCount;
    }
  }

  @Override
  public void release() {
    synchronized (refCountLock) {
      if (--refCount == 0 && releaseCallback != null) {
        releaseCallback.run();
      }
    }
  }

  @Override
  public VideoFrame.Buffer cropAndScale(
      int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
    return VideoFrame.cropAndScaleI420(
        this, cropX, cropY, cropWidth, cropHeight, scaleWidth, scaleHeight);
  }
}
