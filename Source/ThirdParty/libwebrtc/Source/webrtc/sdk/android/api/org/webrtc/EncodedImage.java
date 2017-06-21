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

/**
 * An encoded frame from a video stream. Used as an input for decoders and as an output for
 * encoders.
 */
public class EncodedImage {
  public enum FrameType {
    EmptyFrame,
    VideoFrameKey,
    VideoFrameDelta,
  }

  public final ByteBuffer buffer;
  public final int encodedWidth;
  public final int encodedHeight;
  public final long captureTimeMs;
  public final FrameType frameType;
  public final int rotation;
  public final boolean completeFrame;
  public final Integer qp;

  private EncodedImage(ByteBuffer buffer, int encodedWidth, int encodedHeight, long captureTimeMs,
      FrameType frameType, int rotation, boolean completeFrame, Integer qp) {
    this.buffer = buffer;
    this.encodedWidth = encodedWidth;
    this.encodedHeight = encodedHeight;
    this.captureTimeMs = captureTimeMs;
    this.frameType = frameType;
    this.rotation = rotation;
    this.completeFrame = completeFrame;
    this.qp = qp;
  }

  public static Builder builder() {
    return new Builder();
  }

  public static class Builder {
    private ByteBuffer buffer;
    private int encodedWidth;
    private int encodedHeight;
    private long captureTimeMs;
    private EncodedImage.FrameType frameType;
    private int rotation;
    private boolean completeFrame;
    private Integer qp;

    private Builder() {}

    public Builder setBuffer(ByteBuffer buffer) {
      this.buffer = buffer;
      return this;
    }

    public Builder setEncodedWidth(int encodedWidth) {
      this.encodedWidth = encodedWidth;
      return this;
    }

    public Builder setEncodedHeight(int encodedHeight) {
      this.encodedHeight = encodedHeight;
      return this;
    }

    public Builder setCaptureTimeMs(long captureTimeMs) {
      this.captureTimeMs = captureTimeMs;
      return this;
    }

    public Builder setFrameType(EncodedImage.FrameType frameType) {
      this.frameType = frameType;
      return this;
    }

    public Builder setRotation(int rotation) {
      this.rotation = rotation;
      return this;
    }

    public Builder setCompleteFrame(boolean completeFrame) {
      this.completeFrame = completeFrame;
      return this;
    }

    public Builder setQp(Integer qp) {
      this.qp = qp;
      return this;
    }

    public EncodedImage createEncodedImage() {
      return new EncodedImage(buffer, encodedWidth, encodedHeight, captureTimeMs, frameType,
          rotation, completeFrame, qp);
    }
  }
}
