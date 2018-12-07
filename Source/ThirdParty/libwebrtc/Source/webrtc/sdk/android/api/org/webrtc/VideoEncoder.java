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

import javax.annotation.Nullable;
import org.webrtc.EncodedImage;

/**
 * Interface for a video encoder that can be used with WebRTC. All calls will be made on the
 * encoding thread. The encoder may be constructed on a different thread and changing thread after
 * calling release is allowed.
 */
public interface VideoEncoder {
  /** Settings passed to the encoder by WebRTC. */
  public class Settings {
    public final int numberOfCores;
    public final int width;
    public final int height;
    public final int startBitrate; // Kilobits per second.
    public final int maxFramerate;
    public final int numberOfSimulcastStreams;
    public final boolean automaticResizeOn;

    @CalledByNative("Settings")
    public Settings(int numberOfCores, int width, int height, int startBitrate, int maxFramerate,
        int numberOfSimulcastStreams, boolean automaticResizeOn) {
      this.numberOfCores = numberOfCores;
      this.width = width;
      this.height = height;
      this.startBitrate = startBitrate;
      this.maxFramerate = maxFramerate;
      this.numberOfSimulcastStreams = numberOfSimulcastStreams;
      this.automaticResizeOn = automaticResizeOn;
    }
  }

  /** Additional info for encoding. */
  public class EncodeInfo {
    public final EncodedImage.FrameType[] frameTypes;

    @CalledByNative("EncodeInfo")
    public EncodeInfo(EncodedImage.FrameType[] frameTypes) {
      this.frameTypes = frameTypes;
    }
  }

  // TODO(sakal): Add values to these classes as necessary.
  /** Codec specific information about the encoded frame. */
  public class CodecSpecificInfo {}

  public class CodecSpecificInfoVP8 extends CodecSpecificInfo {}

  public class CodecSpecificInfoVP9 extends CodecSpecificInfo {}

  public class CodecSpecificInfoH264 extends CodecSpecificInfo {}

  /**
   * Represents bitrate allocated for an encoder to produce frames. Bitrate can be divided between
   * spatial and temporal layers.
   */
  public class BitrateAllocation {
    // First index is the spatial layer and second the temporal layer.
    public final int[][] bitratesBbs;

    /**
     * Initializes the allocation with a two dimensional array of bitrates. The first index of the
     * array is the spatial layer and the second index in the temporal layer.
     */
    @CalledByNative("BitrateAllocation")
    public BitrateAllocation(int[][] bitratesBbs) {
      this.bitratesBbs = bitratesBbs;
    }

    /**
     * Gets the total bitrate allocated for all layers.
     */
    public int getSum() {
      int sum = 0;
      for (int[] spatialLayer : bitratesBbs) {
        for (int bitrate : spatialLayer) {
          sum += bitrate;
        }
      }
      return sum;
    }
  }

  /** Settings for WebRTC quality based scaling. */
  public class ScalingSettings {
    public final boolean on;
    @Nullable public final Integer low;
    @Nullable public final Integer high;

    /**
     * Settings to disable quality based scaling.
     */
    public static final ScalingSettings OFF = new ScalingSettings();

    /**
     * Creates settings to enable quality based scaling.
     *
     * @param low Average QP at which to scale up the resolution.
     * @param high Average QP at which to scale down the resolution.
     */
    public ScalingSettings(int low, int high) {
      this.on = true;
      this.low = low;
      this.high = high;
    }

    private ScalingSettings() {
      this.on = false;
      this.low = null;
      this.high = null;
    }

    // TODO(bugs.webrtc.org/8830): Below constructors are deprecated.
    // Default thresholds are going away, so thresholds have to be set
    // when scaling is on.
    /**
     * Creates quality based scaling setting.
     *
     * @param on True if quality scaling is turned on.
     */
    @Deprecated
    public ScalingSettings(boolean on) {
      this.on = on;
      this.low = null;
      this.high = null;
    }

    /**
     * Creates quality based scaling settings with custom thresholds.
     *
     * @param on True if quality scaling is turned on.
     * @param low Average QP at which to scale up the resolution.
     * @param high Average QP at which to scale down the resolution.
     */
    @Deprecated
    public ScalingSettings(boolean on, int low, int high) {
      this.on = on;
      this.low = low;
      this.high = high;
    }

    @Override
    public String toString() {
      return on ? "[ " + low + ", " + high + " ]" : "OFF";
    }
  }

  public interface Callback {
    /**
     * Call to return an encoded frame. It is safe to assume the byte buffer held by |frame| is not
     * accessed after the call to this method returns.
     */
    void onEncodedFrame(EncodedImage frame, CodecSpecificInfo info);
  }

  /**
   * The encoder implementation backing this interface is either 1) a Java
   * encoder (e.g., an Android platform encoder), or alternatively 2) a native
   * encoder (e.g., a software encoder or a C++ encoder adapter).
   *
   * For case 1), createNativeVideoEncoder() should return zero.
   * In this case, we expect the native library to call the encoder through
   * JNI using the Java interface declared below.
   *
   * For case 2), createNativeVideoEncoder() should return a non-zero value.
   * In this case, we expect the native library to treat the returned value as
   * a raw pointer of type webrtc::VideoEncoder* (ownership is transferred to
   * the caller). The native library should then directly call the
   * webrtc::VideoEncoder interface without going through JNI. All calls to
   * the Java interface methods declared below should thus throw an
   * UnsupportedOperationException.
   */
  @CalledByNative
  default long createNativeVideoEncoder() {
    return 0;
  }

  /**
   * Returns true if the encoder is backed by hardware.
   */
  @CalledByNative
  default boolean isHardwareEncoder() {
    return true;
  }

  /**
   * Initializes the encoding process. Call before any calls to encode.
   */
  @CalledByNative VideoCodecStatus initEncode(Settings settings, Callback encodeCallback);

  /**
   * Releases the encoder. No more calls to encode will be made after this call.
   */
  @CalledByNative VideoCodecStatus release();

  /**
   * Requests the encoder to encode a frame.
   */
  @CalledByNative VideoCodecStatus encode(VideoFrame frame, EncodeInfo info);

  /** Sets the bitrate allocation and the target framerate for the encoder. */
  @CalledByNative VideoCodecStatus setRateAllocation(BitrateAllocation allocation, int framerate);

  /** Any encoder that wants to use WebRTC provided quality scaler must implement this method. */
  @CalledByNative ScalingSettings getScalingSettings();

  /**
   * Should return a descriptive name for the implementation. Gets called once and cached. May be
   * called from arbitrary thread.
   */
  @CalledByNative String getImplementationName();
}
