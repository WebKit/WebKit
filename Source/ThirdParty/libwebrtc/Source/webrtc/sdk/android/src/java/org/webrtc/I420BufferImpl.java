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

/** Implementation of an I420 VideoFrame buffer. */
class I420BufferImpl implements VideoFrame.I420Buffer {
  private final int width;
  private final int height;
  private final int strideUV;
  private final ByteBuffer y;
  private final ByteBuffer u;
  private final ByteBuffer v;

  I420BufferImpl(int width, int height) {
    this.width = width;
    this.height = height;
    this.strideUV = (width + 1) / 2;
    int halfHeight = (height + 1) / 2;
    this.y = ByteBuffer.allocateDirect(width * height);
    this.u = ByteBuffer.allocateDirect(strideUV * halfHeight);
    this.v = ByteBuffer.allocateDirect(strideUV * halfHeight);
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
    return y;
  }

  @Override
  public ByteBuffer getDataU() {
    return u;
  }

  @Override
  public ByteBuffer getDataV() {
    return v;
  }

  @Override
  public int getStrideY() {
    return width;
  }

  @Override
  public int getStrideU() {
    return strideUV;
  }

  @Override
  public int getStrideV() {
    return strideUV;
  }

  @Override
  public I420Buffer toI420() {
    return this;
  }

  @Override
  public void retain() {}

  @Override
  public void release() {}
}
