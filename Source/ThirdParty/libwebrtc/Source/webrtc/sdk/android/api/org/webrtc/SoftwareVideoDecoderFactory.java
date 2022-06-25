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

import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class SoftwareVideoDecoderFactory implements VideoDecoderFactory {
  @Nullable
  @Override
  public VideoDecoder createDecoder(VideoCodecInfo codecInfo) {
    String codecName = codecInfo.getName();

    if (codecName.equalsIgnoreCase(VideoCodecMimeType.VP8.name())) {
      return new LibvpxVp8Decoder();
    }
    if (codecName.equalsIgnoreCase(VideoCodecMimeType.VP9.name())
        && LibvpxVp9Decoder.nativeIsSupported()) {
      return new LibvpxVp9Decoder();
    }
    if (codecName.equalsIgnoreCase(VideoCodecMimeType.AV1.name())
        && LibaomAv1Decoder.nativeIsSupported()) {
      return new LibaomAv1Decoder();
    }

    return null;
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    return supportedCodecs();
  }

  static VideoCodecInfo[] supportedCodecs() {
    List<VideoCodecInfo> codecs = new ArrayList<VideoCodecInfo>();

    codecs.add(new VideoCodecInfo(VideoCodecMimeType.VP8.name(), new HashMap<>()));
    if (LibvpxVp9Decoder.nativeIsSupported()) {
      codecs.add(new VideoCodecInfo(VideoCodecMimeType.VP9.name(), new HashMap<>()));
    }
    if (LibaomAv1Decoder.nativeIsSupported()) {
      codecs.add(new VideoCodecInfo(VideoCodecMimeType.AV1.name(), new HashMap<>()));
    }

    return codecs.toArray(new VideoCodecInfo[codecs.size()]);
  }
}
