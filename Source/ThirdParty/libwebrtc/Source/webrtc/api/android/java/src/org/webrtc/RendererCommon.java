/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.graphics.Point;
import android.opengl.GLES20;
import android.opengl.Matrix;
import android.view.View;

import java.nio.ByteBuffer;

/**
 * Static helper functions for renderer implementations.
 */
public class RendererCommon {
  /** Interface for reporting rendering events. */
  public static interface RendererEvents {
    /**
     * Callback fired once first frame is rendered.
     */
    public void onFirstFrameRendered();

    /**
     * Callback fired when rendered frame resolution or rotation has changed.
     */
    public void onFrameResolutionChanged(int videoWidth, int videoHeight, int rotation);
  }

  /** Interface for rendering frames on an EGLSurface. */
  public static interface GlDrawer {
    /**
     * Functions for drawing frames with different sources. The rendering surface target is
     * implied by the current EGL context of the calling thread and requires no explicit argument.
     * The coordinates specify the viewport location on the surface target.
     */
    void drawOes(int oesTextureId, float[] texMatrix, int frameWidth, int frameHeight,
        int viewportX, int viewportY, int viewportWidth, int viewportHeight);
    void drawRgb(int textureId, float[] texMatrix, int frameWidth, int frameHeight, int viewportX,
        int viewportY, int viewportWidth, int viewportHeight);
    void drawYuv(int[] yuvTextures, float[] texMatrix, int frameWidth, int frameHeight,
        int viewportX, int viewportY, int viewportWidth, int viewportHeight);

    /**
     * Release all GL resources. This needs to be done manually, otherwise resources may leak.
     */
    void release();
  }

  /**
   * Helper class for uploading YUV bytebuffer frames to textures that handles stride > width. This
   * class keeps an internal ByteBuffer to avoid unnecessary allocations for intermediate copies.
   */
  public static class YuvUploader {
    // Intermediate copy buffer for uploading yuv frames that are not packed, i.e. stride > width.
    // TODO(magjed): Investigate when GL_UNPACK_ROW_LENGTH is available, or make a custom shader
    // that handles stride and compare performance with intermediate copy.
    private ByteBuffer copyBuffer;

    /**
     * Upload |planes| into |outputYuvTextures|, taking stride into consideration.
     * |outputYuvTextures| must have been generated in advance.
     */
    public void uploadYuvData(
        int[] outputYuvTextures, int width, int height, int[] strides, ByteBuffer[] planes) {
      final int[] planeWidths = new int[] {width, width / 2, width / 2};
      final int[] planeHeights = new int[] {height, height / 2, height / 2};
      // Make a first pass to see if we need a temporary copy buffer.
      int copyCapacityNeeded = 0;
      for (int i = 0; i < 3; ++i) {
        if (strides[i] > planeWidths[i]) {
          copyCapacityNeeded = Math.max(copyCapacityNeeded, planeWidths[i] * planeHeights[i]);
        }
      }
      // Allocate copy buffer if necessary.
      if (copyCapacityNeeded > 0
          && (copyBuffer == null || copyBuffer.capacity() < copyCapacityNeeded)) {
        copyBuffer = ByteBuffer.allocateDirect(copyCapacityNeeded);
      }
      // Upload each plane.
      for (int i = 0; i < 3; ++i) {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, outputYuvTextures[i]);
        // GLES only accepts packed data, i.e. stride == planeWidth.
        final ByteBuffer packedByteBuffer;
        if (strides[i] == planeWidths[i]) {
          // Input is packed already.
          packedByteBuffer = planes[i];
        } else {
          VideoRenderer.nativeCopyPlane(
              planes[i], planeWidths[i], planeHeights[i], strides[i], copyBuffer, planeWidths[i]);
          packedByteBuffer = copyBuffer;
        }
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, planeWidths[i],
            planeHeights[i], 0, GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, packedByteBuffer);
      }
    }
  }

  /**
   * Helper class for determining layout size based on layout requirements, scaling type, and video
   * aspect ratio.
   */
  public static class VideoLayoutMeasure {
    // The scaling type determines how the video will fill the allowed layout area in measure(). It
    // can be specified separately for the case when video has matched orientation with layout size
    // and when there is an orientation mismatch.
    private ScalingType scalingTypeMatchOrientation = ScalingType.SCALE_ASPECT_BALANCED;
    private ScalingType scalingTypeMismatchOrientation = ScalingType.SCALE_ASPECT_BALANCED;

    public void setScalingType(ScalingType scalingType) {
      this.scalingTypeMatchOrientation = scalingType;
      this.scalingTypeMismatchOrientation = scalingType;
    }

    public void setScalingType(
        ScalingType scalingTypeMatchOrientation, ScalingType scalingTypeMismatchOrientation) {
      this.scalingTypeMatchOrientation = scalingTypeMatchOrientation;
      this.scalingTypeMismatchOrientation = scalingTypeMismatchOrientation;
    }

    public Point measure(int widthSpec, int heightSpec, int frameWidth, int frameHeight) {
      // Calculate max allowed layout size.
      final int maxWidth = View.getDefaultSize(Integer.MAX_VALUE, widthSpec);
      final int maxHeight = View.getDefaultSize(Integer.MAX_VALUE, heightSpec);
      if (frameWidth == 0 || frameHeight == 0 || maxWidth == 0 || maxHeight == 0) {
        return new Point(maxWidth, maxHeight);
      }
      // Calculate desired display size based on scaling type, video aspect ratio,
      // and maximum layout size.
      final float frameAspect = frameWidth / (float) frameHeight;
      final float displayAspect = maxWidth / (float) maxHeight;
      final ScalingType scalingType = (frameAspect > 1.0f) == (displayAspect > 1.0f)
          ? scalingTypeMatchOrientation
          : scalingTypeMismatchOrientation;
      final Point layoutSize = getDisplaySize(scalingType, frameAspect, maxWidth, maxHeight);

      // If the measure specification is forcing a specific size - yield.
      if (View.MeasureSpec.getMode(widthSpec) == View.MeasureSpec.EXACTLY) {
        layoutSize.x = maxWidth;
      }
      if (View.MeasureSpec.getMode(heightSpec) == View.MeasureSpec.EXACTLY) {
        layoutSize.y = maxHeight;
      }
      return layoutSize;
    }
  }

  // Types of video scaling:
  // SCALE_ASPECT_FIT - video frame is scaled to fit the size of the view by
  //    maintaining the aspect ratio (black borders may be displayed).
  // SCALE_ASPECT_FILL - video frame is scaled to fill the size of the view by
  //    maintaining the aspect ratio. Some portion of the video frame may be
  //    clipped.
  // SCALE_ASPECT_BALANCED - Compromise between FIT and FILL. Video frame will fill as much as
  // possible of the view while maintaining aspect ratio, under the constraint that at least
  // |BALANCED_VISIBLE_FRACTION| of the frame content will be shown.
  public static enum ScalingType { SCALE_ASPECT_FIT, SCALE_ASPECT_FILL, SCALE_ASPECT_BALANCED }
  // The minimum fraction of the frame content that will be shown for |SCALE_ASPECT_BALANCED|.
  // This limits excessive cropping when adjusting display size.
  private static float BALANCED_VISIBLE_FRACTION = 0.5625f;
  // clang-format off
  public static final float[] identityMatrix() {
    return new float[] {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1};
  }
  // Matrix with transform y' = 1 - y.
  public static final float[] verticalFlipMatrix() {
    return new float[] {
        1,  0, 0, 0,
        0, -1, 0, 0,
        0,  0, 1, 0,
        0,  1, 0, 1};
  }

  // Matrix with transform x' = 1 - x.
  public static final float[] horizontalFlipMatrix() {
    return new float[] {
        -1, 0, 0, 0,
         0, 1, 0, 0,
         0, 0, 1, 0,
         1, 0, 0, 1};
  }
  // clang-format on

  /**
   * Returns texture matrix that will have the effect of rotating the frame |rotationDegree|
   * clockwise when rendered.
   */
  public static float[] rotateTextureMatrix(float[] textureMatrix, float rotationDegree) {
    final float[] rotationMatrix = new float[16];
    Matrix.setRotateM(rotationMatrix, 0, rotationDegree, 0, 0, 1);
    adjustOrigin(rotationMatrix);
    return multiplyMatrices(textureMatrix, rotationMatrix);
  }

  /**
   * Returns new matrix with the result of a * b.
   */
  public static float[] multiplyMatrices(float[] a, float[] b) {
    final float[] resultMatrix = new float[16];
    Matrix.multiplyMM(resultMatrix, 0, a, 0, b, 0);
    return resultMatrix;
  }

  /**
   * Returns layout transformation matrix that applies an optional mirror effect and compensates
   * for video vs display aspect ratio.
   */
  public static float[] getLayoutMatrix(
      boolean mirror, float videoAspectRatio, float displayAspectRatio) {
    float scaleX = 1;
    float scaleY = 1;
    // Scale X or Y dimension so that video and display size have same aspect ratio.
    if (displayAspectRatio > videoAspectRatio) {
      scaleY = videoAspectRatio / displayAspectRatio;
    } else {
      scaleX = displayAspectRatio / videoAspectRatio;
    }
    // Apply optional horizontal flip.
    if (mirror) {
      scaleX *= -1;
    }
    final float matrix[] = new float[16];
    Matrix.setIdentityM(matrix, 0);
    Matrix.scaleM(matrix, 0, scaleX, scaleY, 1);
    adjustOrigin(matrix);
    return matrix;
  }

  /**
   * Calculate display size based on scaling type, video aspect ratio, and maximum display size.
   */
  public static Point getDisplaySize(
      ScalingType scalingType, float videoAspectRatio, int maxDisplayWidth, int maxDisplayHeight) {
    return getDisplaySize(convertScalingTypeToVisibleFraction(scalingType), videoAspectRatio,
        maxDisplayWidth, maxDisplayHeight);
  }

  /**
   * Move |matrix| transformation origin to (0.5, 0.5). This is the origin for texture coordinates
   * that are in the range 0 to 1.
   */
  private static void adjustOrigin(float[] matrix) {
    // Note that OpenGL is using column-major order.
    // Pre translate with -0.5 to move coordinates to range [-0.5, 0.5].
    matrix[12] -= 0.5f * (matrix[0] + matrix[4]);
    matrix[13] -= 0.5f * (matrix[1] + matrix[5]);
    // Post translate with 0.5 to move coordinates to range [0, 1].
    matrix[12] += 0.5f;
    matrix[13] += 0.5f;
  }

  /**
   * Each scaling type has a one-to-one correspondence to a numeric minimum fraction of the video
   * that must remain visible.
   */
  private static float convertScalingTypeToVisibleFraction(ScalingType scalingType) {
    switch (scalingType) {
      case SCALE_ASPECT_FIT:
        return 1.0f;
      case SCALE_ASPECT_FILL:
        return 0.0f;
      case SCALE_ASPECT_BALANCED:
        return BALANCED_VISIBLE_FRACTION;
      default:
        throw new IllegalArgumentException();
    }
  }

  /**
   * Calculate display size based on minimum fraction of the video that must remain visible,
   * video aspect ratio, and maximum display size.
   */
  private static Point getDisplaySize(
      float minVisibleFraction, float videoAspectRatio, int maxDisplayWidth, int maxDisplayHeight) {
    // If there is no constraint on the amount of cropping, fill the allowed display area.
    if (minVisibleFraction == 0 || videoAspectRatio == 0) {
      return new Point(maxDisplayWidth, maxDisplayHeight);
    }
    // Each dimension is constrained on max display size and how much we are allowed to crop.
    final int width = Math.min(
        maxDisplayWidth, Math.round(maxDisplayHeight / minVisibleFraction * videoAspectRatio));
    final int height = Math.min(
        maxDisplayHeight, Math.round(maxDisplayWidth / minVisibleFraction / videoAspectRatio));
    return new Point(width, height);
  }
}
