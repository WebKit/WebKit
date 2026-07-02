/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;
import java.util.ArrayList;
import java.util.HashMap;
import org.junit.Before;
import org.junit.Test;

/** Unit tests for {@link DefaultVideoEncoderFactory}. */
public class DefaultVideoEncoderFactoryTest {
  static class CustomHardwareVideoEncoderFactory implements VideoEncoderFactory {
    private VideoCodecInfo supportedCodec;

    public CustomHardwareVideoEncoderFactory(VideoCodecInfo supportedCodec) {
      this.supportedCodec = supportedCodec;
    }

    @Override
    public @Nullable VideoEncoder createEncoder(VideoCodecInfo info) {
      return null;
    }

    @Override
    public VideoCodecInfo[] getSupportedCodecs() {
      return new VideoCodecInfo[] {supportedCodec};
    }
  }

  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader(), TestConstants.NATIVE_LIBRARY);
  }

  @SmallTest
  @Test
  public void getSupportedCodecs_hwVp8SameParamsAsSwVp8_oneVp8() {
    VideoCodecInfo hwVp8Encoder = new VideoCodecInfo("VP8", new HashMap<>());
    VideoEncoderFactory hwFactory = new CustomHardwareVideoEncoderFactory(hwVp8Encoder);
    DefaultVideoEncoderFactory defFactory = new DefaultVideoEncoderFactory(hwFactory);
    VideoCodecInfo[] supportedCodecs = defFactory.getSupportedCodecs();
    assertEquals(3, supportedCodecs.length);
    assertEquals("VP8", supportedCodecs[0].name);
    assertEquals("AV1", supportedCodecs[1].name);
    assertEquals("VP9", supportedCodecs[2].name);
  }

  @SmallTest
  @Test
  public void getSupportedCodecs_hwVp8WithDifferentParams_twoVp8() {
    VideoCodecInfo hwVp8Encoder = new VideoCodecInfo("VP8", new HashMap<String, String>() {
      { put("param", "value"); }
    });
    VideoEncoderFactory hwFactory = new CustomHardwareVideoEncoderFactory(hwVp8Encoder);
    DefaultVideoEncoderFactory defFactory = new DefaultVideoEncoderFactory(hwFactory);
    VideoCodecInfo[] supportedCodecs = defFactory.getSupportedCodecs();
    assertEquals(4, supportedCodecs.length);
    assertEquals("VP8", supportedCodecs[0].name);
    assertEquals("AV1", supportedCodecs[1].name);
    assertEquals("VP9", supportedCodecs[2].name);
    assertEquals("VP8", supportedCodecs[3].name);
    assertEquals(1, supportedCodecs[3].params.size());
    assertEquals("value", supportedCodecs[3].params.get("param"));
  }
}
