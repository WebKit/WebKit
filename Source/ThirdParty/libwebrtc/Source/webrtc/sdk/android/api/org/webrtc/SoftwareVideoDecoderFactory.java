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

public class SoftwareVideoDecoderFactory implements VideoDecoderFactory {
  @Override
  public VideoDecoder createDecoder(String codecType) {
    if (codecType.equalsIgnoreCase("VP8")) {
      return new VP8Decoder();
    }
    if (codecType.equalsIgnoreCase("VP9") && VP9Decoder.isSupported()) {
      return new VP9Decoder();
    }

    return null;
  }
}
