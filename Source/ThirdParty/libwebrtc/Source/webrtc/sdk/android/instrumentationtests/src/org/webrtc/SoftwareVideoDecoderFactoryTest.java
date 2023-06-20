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

/** Unit tests for {@link SoftwareVideoDecoderFactory}. */
public class SoftwareVideoDecoderFactoryTest {
  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader(), TestConstants.NATIVE_LIBRARY);
  }

  @SmallTest
  @Test
  public void getSupportedCodecs_returnsDefaultCodecs() {
    VideoDecoderFactory factory = new SoftwareVideoDecoderFactory();
    VideoCodecInfo[] codecs = factory.getSupportedCodecs();
    assertThat(codecs.length).isEqualTo(6);
    assertThat(codecs[0].name).isEqualTo("VP8");
    assertThat(codecs[1].name).isEqualTo("VP9");
    assertThat(codecs[2].name).isEqualTo("VP9");
    assertThat(codecs[3].name).isEqualTo("VP9");
    assertThat(codecs[4].name).isEqualTo("AV1");
    assertThat(codecs[5].name).isEqualTo("AV1");
  }

  @SmallTest
  @Test
  public void createDecoder_supportedCodec_returnsNotNull() {
    VideoDecoderFactory factory = new SoftwareVideoDecoderFactory();
    VideoCodecInfo[] codecs = factory.getSupportedCodecs();
    assertThat(codecs.length).isGreaterThan(0);
    for (VideoCodecInfo codec : codecs) {
      VideoDecoder decoder = factory.createDecoder(codec);
      assertThat(decoder).isNotNull();
    }
  }

  @SmallTest
  @Test
  public void createDecoder_unsupportedCodec_returnsNull() {
    VideoDecoderFactory factory = new SoftwareVideoDecoderFactory();
    VideoCodecInfo codec = new VideoCodecInfo("unsupported", new HashMap<String, String>());
    VideoDecoder decoder = factory.createDecoder(codec);
    assertThat(decoder).isNull();
  }
}
