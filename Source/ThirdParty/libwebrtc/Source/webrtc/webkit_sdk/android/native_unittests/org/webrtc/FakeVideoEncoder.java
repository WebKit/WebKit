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

import org.webrtc.VideoEncoder;

/**
 * An implementation of VideoEncoder that is used for testing of functionalities of
 * VideoEncoderWrapper.
 */
class FakeVideoEncoder implements VideoEncoder {
  @Override
  public VideoCodecStatus initEncode(Settings settings, Callback encodeCallback) {
    return VideoCodecStatus.OK;
  }

  @Override
  public VideoCodecStatus release() {
    return VideoCodecStatus.OK;
  }

  @Override
  public VideoCodecStatus encode(VideoFrame frame, EncodeInfo info) {
    return VideoCodecStatus.OK;
  }

  @Override
  public VideoCodecStatus setRateAllocation(BitrateAllocation allocation, int framerate) {
    return VideoCodecStatus.OK;
  }

  @Override
  public ScalingSettings getScalingSettings() {
    return ScalingSettings.OFF;
  }

  @Override
  public ResolutionBitrateLimits[] getResolutionBitrateLimits() {
    ResolutionBitrateLimits resolution_bitrate_limits[] = {
        new ResolutionBitrateLimits(/* frameSizePixels = */ 640 * 360,
            /* minStartBitrateBps = */ 300000,
            /* minBitrateBps = */ 200000,
            /* maxBitrateBps = */ 1000000)};

    return resolution_bitrate_limits;
  }

  @Override
  public String getImplementationName() {
    return "FakeVideoEncoder";
  }
}
