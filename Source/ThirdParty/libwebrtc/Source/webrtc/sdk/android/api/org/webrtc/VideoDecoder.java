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

/**
 * Interface for a video decoder that can be used in WebRTC. All calls to the class will be made on
 * a single decoding thread.
 */
public interface VideoDecoder {
  /** Settings passed to the decoder by WebRTC. */
  public class Settings {
    public final int numberOfCores;
    public final int width;
    public final int height;

    @CalledByNative("Settings")
    public Settings(int numberOfCores, int width, int height) {
      this.numberOfCores = numberOfCores;
      this.width = width;
      this.height = height;
    }
  }

  /** Additional info for decoding. */
  public class DecodeInfo {
    public final boolean isMissingFrames;
    public final long renderTimeMs;

    public DecodeInfo(boolean isMissingFrames, long renderTimeMs) {
      this.isMissingFrames = isMissingFrames;
      this.renderTimeMs = renderTimeMs;
    }
  }

  public interface Callback {
    /**
     * Call to return a decoded frame. Can be called on any thread.
     *
     * @param frame Decoded frame
     * @param decodeTimeMs Time it took to decode the frame in milliseconds or null if not available
     * @param qp QP value of the decoded frame or null if not available
     */
    void onDecodedFrame(VideoFrame frame, Integer decodeTimeMs, Integer qp);
  }

  /**
   * Initializes the decoding process with specified settings. Will be called on the decoding thread
   * before any decode calls.
   */
  @CalledByNative VideoCodecStatus initDecode(Settings settings, Callback decodeCallback);
  /**
   * Called when the decoder is no longer needed. Any more calls to decode will not be made.
   */
  @CalledByNative VideoCodecStatus release();
  /**
   * Request the decoder to decode a frame.
   */
  @CalledByNative VideoCodecStatus decode(EncodedImage frame, DecodeInfo info);
  /**
   * The decoder should return true if it prefers late decoding. That is, it can not decode
   * infinite number of frames before the decoded frame is consumed.
   */
  @CalledByNative boolean getPrefersLateDecoding();
  /**
   * Should return a descriptive name for the implementation. Gets called once and cached. May be
   * called from arbitrary thread.
   */
  @CalledByNative String getImplementationName();
}
