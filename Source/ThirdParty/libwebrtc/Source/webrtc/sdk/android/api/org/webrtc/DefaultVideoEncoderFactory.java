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

import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;

public class DefaultVideoEncoderFactory implements VideoEncoderFactory {
  private final VideoEncoderFactory hardwareVideoEncoderFactory;
  private final VideoEncoderFactory softwareVideoEncoderFactory;

  public DefaultVideoEncoderFactory(
      EglBase.Context eglContext, boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    hardwareVideoEncoderFactory = new HardwareVideoEncoderFactory(
        eglContext, enableIntelVp8Encoder, enableH264HighProfile, false /* fallbackToSoftware */);
    softwareVideoEncoderFactory = new SoftwareVideoEncoderFactory();
  }

  /* This is used for testing. */
  DefaultVideoEncoderFactory(VideoEncoderFactory hardwareVideoEncoderFactory) {
    this.hardwareVideoEncoderFactory = hardwareVideoEncoderFactory;
    softwareVideoEncoderFactory = new SoftwareVideoEncoderFactory();
  }

  @Override
  public VideoEncoder createEncoder(VideoCodecInfo info) {
    final VideoEncoder videoEncoder = hardwareVideoEncoderFactory.createEncoder(info);
    if (videoEncoder != null) {
      return videoEncoder;
    }
    return softwareVideoEncoderFactory.createEncoder(info);
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    LinkedHashSet<VideoCodecInfo> supportedCodecInfos = new LinkedHashSet<VideoCodecInfo>();

    supportedCodecInfos.addAll(Arrays.asList(softwareVideoEncoderFactory.getSupportedCodecs()));
    supportedCodecInfos.addAll(Arrays.asList(hardwareVideoEncoderFactory.getSupportedCodecs()));

    return supportedCodecInfos.toArray(new VideoCodecInfo[supportedCodecInfos.size()]);
  }
}
