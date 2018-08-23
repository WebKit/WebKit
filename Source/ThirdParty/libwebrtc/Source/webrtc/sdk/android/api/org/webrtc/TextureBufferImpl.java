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
import javax.annotation.Nullable;
import android.os.Handler;

/**
 * Android texture buffer that glues together the necessary information together with a generic
 * release callback. ToI420() is implemented by providing a Handler and a YuvConverter.
 */
public class TextureBufferImpl implements VideoFrame.TextureBuffer {
  private final int width;
  private final int height;
  private final Type type;
  private final int id;
  private final Matrix transformMatrix;
  private final Handler toI420Handler;
  private final YuvConverter yuvConverter;
  private final RefCountDelegate refCountDelegate;

  public TextureBufferImpl(int width, int height, Type type, int id, Matrix transformMatrix,
      Handler toI420Handler, YuvConverter yuvConverter, @Nullable Runnable releaseCallback) {
    this.width = width;
    this.height = height;
    this.type = type;
    this.id = id;
    this.transformMatrix = transformMatrix;
    this.toI420Handler = toI420Handler;
    this.yuvConverter = yuvConverter;
    this.refCountDelegate = new RefCountDelegate(releaseCallback);
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
    return ThreadUtils.invokeAtFrontUninterruptibly(
        toI420Handler, () -> yuvConverter.convert(this));
  }

  @Override
  public void retain() {
    refCountDelegate.retain();
  }

  @Override
  public void release() {
    refCountDelegate.release();
  }

  @Override
  public VideoFrame.Buffer cropAndScale(
      int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
    final Matrix cropAndScaleMatrix = new Matrix();
    // In WebRTC, Y=0 is the top row, while in OpenGL Y=0 is the bottom row. This means that the Y
    // direction is effectively reversed.
    final int cropYFromBottom = height - (cropY + cropHeight);
    cropAndScaleMatrix.preTranslate(cropX / (float) width, cropYFromBottom / (float) height);
    cropAndScaleMatrix.preScale(cropWidth / (float) width, cropHeight / (float) height);

    return applyTransformMatrix(cropAndScaleMatrix, scaleWidth, scaleHeight);
  }

  /**
   * Create a new TextureBufferImpl with an applied transform matrix and a new size. The
   * existing buffer is unchanged. The given transform matrix is applied first when texture
   * coordinates are still in the unmodified [0, 1] range.
   */
  public TextureBufferImpl applyTransformMatrix(
      Matrix transformMatrix, int newWidth, int newHeight) {
    final Matrix newMatrix = new Matrix(this.transformMatrix);
    newMatrix.preConcat(transformMatrix);
    retain();
    return new TextureBufferImpl(
        newWidth, newHeight, type, id, newMatrix, toI420Handler, yuvConverter, this ::release);
  }
}
