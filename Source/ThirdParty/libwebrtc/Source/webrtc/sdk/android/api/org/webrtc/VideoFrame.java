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
 * Java version of webrtc::VideoFrame and webrtc::VideoFrameBuffer. A difference from the C++
 * version is that no explicit tag is used, and clients are expected to use 'instanceof' to find the
 * right subclass of the buffer. This allows clients to create custom VideoFrame.Buffer in
 * arbitrary format in their custom VideoSources, and then cast it back to the correct subclass in
 * their custom VideoSinks. All implementations must also implement the toI420() function,
 * converting from the underlying representation if necessary. I420 is the most widely accepted
 * format and serves as a fallback for video sinks that can only handle I420, e.g. the internal
 * WebRTC software encoders.
 */
public class VideoFrame {
  public interface Buffer {
    /**
     * Resolution of the buffer in pixels.
     */
    int getWidth();
    int getHeight();

    /**
     * Returns a memory-backed frame in I420 format. If the pixel data is in another format, a
     * conversion will take place. All implementations must provide a fallback to I420 for
     * compatibility with e.g. the internal WebRTC software encoders.
     */
    I420Buffer toI420();

    /**
     * Reference counting is needed since a video buffer can be shared between multiple VideoSinks,
     * and the buffer needs to be returned to the VideoSource as soon as all references are gone.
     */
    void retain();
    void release();
  }

  /**
   * Interface for I420 buffers.
   */
  public interface I420Buffer extends Buffer {
    ByteBuffer getDataY();
    ByteBuffer getDataU();
    ByteBuffer getDataV();

    int getStrideY();
    int getStrideU();
    int getStrideV();
  }

  /**
   * Interface for buffers that are stored as a single texture, either in OES or RGB format.
   */
  public interface TextureBuffer extends Buffer {
    enum Type { OES, RGB }

    Type getType();
    int getTextureId();
  }

  private final Buffer buffer;
  private final int rotation;
  private final long timestampNs;
  private final Matrix transformMatrix;

  public VideoFrame(Buffer buffer, int rotation, long timestampNs, Matrix transformMatrix) {
    if (buffer == null) {
      throw new IllegalArgumentException("buffer not allowed to be null");
    }
    if (transformMatrix == null) {
      throw new IllegalArgumentException("transformMatrix not allowed to be null");
    }
    this.buffer = buffer;
    this.rotation = rotation;
    this.timestampNs = timestampNs;
    this.transformMatrix = transformMatrix;
  }

  public Buffer getBuffer() {
    return buffer;
  }

  /**
   * Rotation of the frame in degrees.
   */
  public int getRotation() {
    return rotation;
  }

  /**
   * Timestamp of the frame in nano seconds.
   */
  public long getTimestampNs() {
    return timestampNs;
  }

  /**
   * Retrieve the transform matrix associated with the frame. This transform matrix maps 2D
   * homogeneous coordinates of the form (s, t, 1) with s and t in the inclusive range [0, 1] to the
   * coordinate that should be used to sample that location from the buffer.
   */
  public Matrix getTransformMatrix() {
    return transformMatrix;
  }

  /**
   * Resolution of the frame in pixels.
   */
  public int getWidth() {
    return buffer.getWidth();
  }

  public int getHeight() {
    return buffer.getHeight();
  }

  /**
   * Reference counting of the underlying buffer.
   */
  public void retain() {
    buffer.retain();
  }

  public void release() {
    buffer.release();
  }
}
