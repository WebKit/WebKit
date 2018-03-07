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

import android.support.test.filters.SmallTest;
import android.util.Log;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Unit tests for {@link DefaultVideoEncoderFactory}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class DefaultVideoEncoderFactoryTest {
  static class CustomHardwareVideoEncoderFactory implements VideoEncoderFactory {
    private ArrayList<VideoCodecInfo> codecs = new ArrayList<>();

    public CustomHardwareVideoEncoderFactory(boolean includeVP8, boolean includeH264High) {
      if (includeVP8) {
        codecs.add(new VideoCodecInfo("VP8", new HashMap<>()));
      }
      codecs.add(new VideoCodecInfo("VP9", new HashMap<>()));

      HashMap<String, String> baselineParams = new HashMap<String, String>();
      baselineParams.put("profile-level-id", "42e01f");
      baselineParams.put("level-asymmetry-allowed", "1");
      baselineParams.put("packetization-mode", "1");
      codecs.add(new VideoCodecInfo("H264", baselineParams));

      if (includeH264High) {
        HashMap<String, String> highParams = new HashMap<String, String>();
        highParams.put("profile-level-id", "640c1f");
        highParams.put("level-asymmetry-allowed", "1");
        highParams.put("packetization-mode", "1");
        codecs.add(new VideoCodecInfo("H264", highParams));
      }
    }

    @Override
    public VideoEncoder createEncoder(VideoCodecInfo info) {
      return null;
    }

    @Override
    public VideoCodecInfo[] getSupportedCodecs() {
      return codecs.toArray(new VideoCodecInfo[codecs.size()]);
    }
  }

  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader());
  }

  @SmallTest
  @Test
  public void testGetSupportedCodecsWithHardwareH264HighProfile() {
    VideoEncoderFactory hwFactory = new CustomHardwareVideoEncoderFactory(true, true);
    DefaultVideoEncoderFactory dvef = new DefaultVideoEncoderFactory(hwFactory);
    VideoCodecInfo[] videoCodecs = dvef.getSupportedCodecs();
    assertEquals(4, videoCodecs.length);
    assertEquals("VP8", videoCodecs[0].name);
    assertEquals("VP9", videoCodecs[1].name);
    assertEquals("H264", videoCodecs[2].name);
    assertEquals("42e01f", videoCodecs[2].params.get("profile-level-id"));
    assertEquals("H264", videoCodecs[3].name);
    assertEquals("640c1f", videoCodecs[3].params.get("profile-level-id"));
  }

  @SmallTest
  @Test
  public void testGetSupportedCodecsWithoutHardwareH264HighProfile() {
    VideoEncoderFactory hwFactory = new CustomHardwareVideoEncoderFactory(true, false);
    DefaultVideoEncoderFactory dvef = new DefaultVideoEncoderFactory(hwFactory);
    VideoCodecInfo[] videoCodecs = dvef.getSupportedCodecs();
    assertEquals(3, videoCodecs.length);
    assertEquals("VP8", videoCodecs[0].name);
    assertEquals("VP9", videoCodecs[1].name);
    assertEquals("H264", videoCodecs[2].name);
    assertEquals("42e01f", videoCodecs[2].params.get("profile-level-id"));
  }

  @SmallTest
  @Test
  public void testGetSupportedCodecsWithoutHardwareVP8() {
    VideoEncoderFactory hwFactory = new CustomHardwareVideoEncoderFactory(false, true);
    DefaultVideoEncoderFactory dvef = new DefaultVideoEncoderFactory(hwFactory);
    VideoCodecInfo[] videoCodecs = dvef.getSupportedCodecs();
    assertEquals(4, videoCodecs.length);
    assertEquals("VP8", videoCodecs[0].name);
    assertEquals("VP9", videoCodecs[1].name);
    assertEquals("H264", videoCodecs[2].name);
    assertEquals("42e01f", videoCodecs[2].params.get("profile-level-id"));
    assertEquals("H264", videoCodecs[3].name);
    assertEquals("640c1f", videoCodecs[3].params.get("profile-level-id"));
  }
}
