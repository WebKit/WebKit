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

import android.graphics.Matrix;
import java.nio.ByteBuffer;

/**
 * Android texture buffer backed by a SurfaceTextureHelper's texture. The buffer calls
 * |releaseCallback| when it is released.
 */
class TextureBufferImpl implements VideoFrame.TextureBuffer {
  private final int width;
  private final int height;
  private final Type type;
  private final int id;
  private final Matrix transformMatrix;
  private final SurfaceTextureHelper surfaceTextureHelper;
  private final Runnable releaseCallback;
  private final Object refCountLock = new Object();
  private int refCount;

  public TextureBufferImpl(int width, int height, Type type, int id, Matrix transformMatrix,
      SurfaceTextureHelper surfaceTextureHelper, Runnable releaseCallback) {
    this.width = width;
    this.height = height;
    this.type = type;
    this.id = id;
    this.transformMatrix = transformMatrix;
    this.surfaceTextureHelper = surfaceTextureHelper;
    this.releaseCallback = releaseCallback;
    this.refCount = 1; // Creator implicitly holds a reference.
  }

  @Override
  public VideoFrame.TextureBuffer.Type getType() {
    return type;
  }

  @Override
  public int getTextureId() {
    return id;
  }

  @Override
  public Matrix getTransformMatrix() {
    return transformMatrix;
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
  public VideoFrame.I420Buffer toI420() {
    return surfaceTextureHelper.textureToYuv(this);
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
    retain();
    Matrix newMatrix = new Matrix(transformMatrix);
    newMatrix.postScale(cropWidth / (float) width, cropHeight / (float) height);
    newMatrix.postTranslate(cropX / (float) width, cropY / (float) height);

    return new TextureBufferImpl(
        scaleWidth, scaleHeight, type, id, newMatrix, surfaceTextureHelper, new Runnable() {
          @Override
          public void run() {
            release();
          }
        });
  }
}
