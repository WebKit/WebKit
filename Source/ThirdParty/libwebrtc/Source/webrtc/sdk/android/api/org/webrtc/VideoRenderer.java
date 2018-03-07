/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.nio.ByteBuffer;

/**
 * Java version of VideoSinkInterface.  In addition to allowing clients to
 * define their own rendering behavior (by passing in a Callbacks object), this
 * class also provides a createGui() method for creating a GUI-rendering window
 * on various platforms.
 */
public class VideoRenderer {
  /**
   * Java version of webrtc::VideoFrame. Frames are only constructed from native code and test
   * code.
   */
  public static class I420Frame {
    public final int width;
    public final int height;
    public final int[] yuvStrides;
    public ByteBuffer[] yuvPlanes;
    public final boolean yuvFrame;
    // Matrix that transforms standard coordinates to their proper sampling locations in
    // the texture. This transform compensates for any properties of the video source that
    // cause it to appear different from a normalized texture. This matrix does not take
    // |rotationDegree| into account.
    public final float[] samplingMatrix;
    public int textureId;
    // Frame pointer in C++.
    private long nativeFramePointer;

    // rotationDegree is the degree that the frame must be rotated clockwisely
    // to be rendered correctly.
    public int rotationDegree;

    // If this I420Frame was constructed from VideoFrame.Buffer, this points to
    // the backing buffer.
    private final VideoFrame.Buffer backingBuffer;

    /**
     * Construct a frame of the given dimensions with the specified planar data.
     */
    public I420Frame(int width, int height, int rotationDegree, int[] yuvStrides,
        ByteBuffer[] yuvPlanes, long nativeFramePointer) {
      this.width = width;
      this.height = height;
      this.yuvStrides = yuvStrides;
      this.yuvPlanes = yuvPlanes;
      this.yuvFrame = true;
      this.rotationDegree = rotationDegree;
      this.nativeFramePointer = nativeFramePointer;
      backingBuffer = null;
      if (rotationDegree % 90 != 0) {
        throw new IllegalArgumentException("Rotation degree not multiple of 90: " + rotationDegree);
      }
      // The convention in WebRTC is that the first element in a ByteBuffer corresponds to the
      // top-left corner of the image, but in glTexImage2D() the first element corresponds to the
      // bottom-left corner. This discrepancy is corrected by setting a vertical flip as sampling
      // matrix.
      samplingMatrix = RendererCommon.verticalFlipMatrix();
    }

    /**
     * Construct a texture frame of the given dimensions with data in SurfaceTexture
     */
    public I420Frame(int width, int height, int rotationDegree, int textureId,
        float[] samplingMatrix, long nativeFramePointer) {
      this.width = width;
      this.height = height;
      this.yuvStrides = null;
      this.yuvPlanes = null;
      this.samplingMatrix = samplingMatrix;
      this.textureId = textureId;
      this.yuvFrame = false;
      this.rotationDegree = rotationDegree;
      this.nativeFramePointer = nativeFramePointer;
      backingBuffer = null;
      if (rotationDegree % 90 != 0) {
        throw new IllegalArgumentException("Rotation degree not multiple of 90: " + rotationDegree);
      }
    }

    /**
     * Construct a frame from VideoFrame.Buffer.
     */
    public I420Frame(int rotationDegree, VideoFrame.Buffer buffer, long nativeFramePointer) {
      this.width = buffer.getWidth();
      this.height = buffer.getHeight();
      this.rotationDegree = rotationDegree;
      if (rotationDegree % 90 != 0) {
        throw new IllegalArgumentException("Rotation degree not multiple of 90: " + rotationDegree);
      }
      if (buffer instanceof VideoFrame.TextureBuffer
          && ((VideoFrame.TextureBuffer) buffer).getType() == VideoFrame.TextureBuffer.Type.OES) {
        VideoFrame.TextureBuffer textureBuffer = (VideoFrame.TextureBuffer) buffer;
        this.yuvFrame = false;
        this.textureId = textureBuffer.getTextureId();
        this.samplingMatrix = RendererCommon.convertMatrixFromAndroidGraphicsMatrix(
            textureBuffer.getTransformMatrix());

        this.yuvStrides = null;
        this.yuvPlanes = null;
      } else if (buffer instanceof VideoFrame.I420Buffer) {
        VideoFrame.I420Buffer i420Buffer = (VideoFrame.I420Buffer) buffer;
        this.yuvFrame = true;
        this.yuvStrides =
            new int[] {i420Buffer.getStrideY(), i420Buffer.getStrideU(), i420Buffer.getStrideV()};
        this.yuvPlanes =
            new ByteBuffer[] {i420Buffer.getDataY(), i420Buffer.getDataU(), i420Buffer.getDataV()};
        // The convention in WebRTC is that the first element in a ByteBuffer corresponds to the
        // top-left corner of the image, but in glTexImage2D() the first element corresponds to the
        // bottom-left corner. This discrepancy is corrected by multiplying the sampling matrix with
        // a vertical flip matrix.
        this.samplingMatrix = RendererCommon.verticalFlipMatrix();

        this.textureId = 0;
      } else {
        this.yuvFrame = false;
        this.textureId = 0;
        this.samplingMatrix = null;
        this.yuvStrides = null;
        this.yuvPlanes = null;
      }
      this.nativeFramePointer = nativeFramePointer;
      backingBuffer = buffer;
    }

    public int rotatedWidth() {
      return (rotationDegree % 180 == 0) ? width : height;
    }

    public int rotatedHeight() {
      return (rotationDegree % 180 == 0) ? height : width;
    }

    @Override
    public String toString() {
      final String type = yuvFrame
          ? "Y: " + yuvStrides[0] + ", U: " + yuvStrides[1] + ", V: " + yuvStrides[2]
          : "Texture: " + textureId;
      return width + "x" + height + ", " + type;
    }

    /**
     * Convert the frame to VideoFrame. It is no longer safe to use the I420Frame after calling
     * this.
     */
    VideoFrame toVideoFrame() {
      final VideoFrame.Buffer buffer;
      if (backingBuffer != null) {
        // We were construted from a VideoFrame.Buffer, just return it.
        // Make sure webrtc::VideoFrame object is released.
        backingBuffer.retain();
        VideoRenderer.renderFrameDone(this);
        buffer = backingBuffer;
      } else if (yuvFrame) {
        buffer = JavaI420Buffer.wrap(width, height, yuvPlanes[0], yuvStrides[0], yuvPlanes[1],
            yuvStrides[1], yuvPlanes[2], yuvStrides[2],
            () -> { VideoRenderer.renderFrameDone(this); });
      } else {
        // Note: surfaceTextureHelper being null means calling toI420 will crash.
        buffer = new TextureBufferImpl(width, height, VideoFrame.TextureBuffer.Type.OES, textureId,
            RendererCommon.convertMatrixToAndroidGraphicsMatrix(samplingMatrix),
            null /* surfaceTextureHelper */, () -> { VideoRenderer.renderFrameDone(this); });
      }
      return new VideoFrame(buffer, rotationDegree, 0 /* timestampNs */);
    }
  }

  // Helper native function to do a video frame plane copying.
  public static native void nativeCopyPlane(
      ByteBuffer src, int width, int height, int srcStride, ByteBuffer dst, int dstStride);

  /** The real meat of VideoSinkInterface. */
  public static interface Callbacks {
    // |frame| might have pending rotation and implementation of Callbacks
    // should handle that by applying rotation during rendering. The callee
    // is responsible for signaling when it is done with |frame| by calling
    // renderFrameDone(frame).
    public void renderFrame(I420Frame frame);
  }

  /**
   * This must be called after every renderFrame() to release the frame.
   */
  public static void renderFrameDone(I420Frame frame) {
    frame.yuvPlanes = null;
    frame.textureId = 0;
    if (frame.nativeFramePointer != 0) {
      releaseNativeFrame(frame.nativeFramePointer);
      frame.nativeFramePointer = 0;
    }
  }

  long nativeVideoRenderer;

  public VideoRenderer(Callbacks callbacks) {
    nativeVideoRenderer = nativeWrapVideoRenderer(callbacks);
  }

  public void dispose() {
    if (nativeVideoRenderer == 0) {
      // Already disposed.
      return;
    }

    freeWrappedVideoRenderer(nativeVideoRenderer);
    nativeVideoRenderer = 0;
  }

  private static native long nativeWrapVideoRenderer(Callbacks callbacks);
  private static native void freeWrappedVideoRenderer(long nativeVideoRenderer);
  private static native void releaseNativeFrame(long nativeFramePointer);
}
