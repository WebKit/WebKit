/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * Wraps a native webrtc::VideoEncoder.
 */
abstract class WrappedNativeVideoEncoder implements VideoEncoder {
  @CalledByNative abstract long createNativeEncoder();

  abstract boolean isSoftwareEncoder();

  @Override
  public VideoCodecStatus initEncode(Settings settings, Callback encodeCallback) {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public VideoCodecStatus release() {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public VideoCodecStatus encode(VideoFrame frame, EncodeInfo info) {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public VideoCodecStatus setChannelParameters(short packetLoss, long roundTripTimeMs) {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public VideoCodecStatus setRateAllocation(BitrateAllocation allocation, int framerate) {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public ScalingSettings getScalingSettings() {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public String getImplementationName() {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @CalledByNative
  static boolean isWrappedSoftwareEncoder(VideoEncoder encoder) {
    return (encoder instanceof WrappedNativeVideoEncoder)
        && ((WrappedNativeVideoEncoder) encoder).isSoftwareEncoder();
  }

  @CalledByNative
  static boolean isInstanceOf(VideoEncoder encoder) {
    return encoder instanceof WrappedNativeVideoEncoder;
  }
}
