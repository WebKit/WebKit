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

public class DefaultVideoDecoderFactory implements VideoDecoderFactory {
  private final HardwareVideoDecoderFactory hardwareVideoDecoderFactory;
  private final SoftwareVideoDecoderFactory softwareVideoDecoderFactory;

  public DefaultVideoDecoderFactory(EglBase.Context eglContext) {
    hardwareVideoDecoderFactory =
        new HardwareVideoDecoderFactory(eglContext, false /* fallbackToSoftware */);
    softwareVideoDecoderFactory = new SoftwareVideoDecoderFactory();
  }

  @Override
  public VideoDecoder createDecoder(String codecType) {
    VideoDecoder decoder = hardwareVideoDecoderFactory.createDecoder(codecType);
    if (decoder != null) {
      return decoder;
    }
    return softwareVideoDecoderFactory.createDecoder(codecType);
  }
}
