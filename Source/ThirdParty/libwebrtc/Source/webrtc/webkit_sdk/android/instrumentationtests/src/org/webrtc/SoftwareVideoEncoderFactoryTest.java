/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static com.google.common.truth.Truth.assertThat;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;
import java.util.HashMap;
import org.junit.Before;
import org.junit.Test;

/** Unit tests for {@link SoftwareVideoEncoderFactory}. */
public class SoftwareVideoEncoderFactoryTest {
  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader(), TestConstants.NATIVE_LIBRARY);
  }

  @SmallTest
  @Test
  public void getSupportedCodecs_returnsDefaultCodecs() {
    VideoEncoderFactory factory = new SoftwareVideoEncoderFactory();
    VideoCodecInfo[] codecs = factory.getSupportedCodecs();
    assertThat(codecs.length).isEqualTo(3);
    assertThat(codecs[0].name).isEqualTo("VP8");
    assertThat(codecs[1].name).isEqualTo("AV1");
    assertThat(codecs[2].name).isEqualTo("VP9");
  }

  @SmallTest
  @Test
  public void createEncoder_supportedCodec_returnsNotNull() {
    VideoEncoderFactory factory = new SoftwareVideoEncoderFactory();
    VideoCodecInfo[] codecs = factory.getSupportedCodecs();
    assertThat(codecs.length).isGreaterThan(0);
    for (VideoCodecInfo codec : codecs) {
      VideoEncoder encoder = factory.createEncoder(codec);
      assertThat(encoder).isNotNull();
    }
  }

  @SmallTest
  @Test
  public void createEncoder_unsupportedCodec_returnsNull() {
    VideoEncoderFactory factory = new SoftwareVideoEncoderFactory();
    VideoCodecInfo codec = new VideoCodecInfo("unsupported", new HashMap<String, String>());
    VideoEncoder encoder = factory.createEncoder(codec);
    assertThat(encoder).isNull();
  }
}
