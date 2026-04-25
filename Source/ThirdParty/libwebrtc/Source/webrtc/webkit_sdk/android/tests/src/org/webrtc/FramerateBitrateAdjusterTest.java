/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.runner.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.webrtc.VideoEncoder.ScalingSettings;

@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE)
public class FramerateBitrateAdjusterTest {
  @Test
  public void getAdjustedFramerate_alwaysReturnsDefault() {
    FramerateBitrateAdjuster bitrateAdjuster = new FramerateBitrateAdjuster();
    bitrateAdjuster.setTargets(1000, 15);
    assertThat(bitrateAdjuster.getAdjustedFramerateFps()).isEqualTo(30.0);
  }

  @Test
  public void getAdjustedBitrate_defaultFramerate_returnsTargetBitrate() {
    FramerateBitrateAdjuster bitrateAdjuster = new FramerateBitrateAdjuster();
    bitrateAdjuster.setTargets(1000, 30);
    assertThat(bitrateAdjuster.getAdjustedBitrateBps()).isEqualTo(1000);
  }

  @Test
  public void getAdjustedBitrate_nonDefaultFramerate_returnsAdjustedBitrate() {
    FramerateBitrateAdjuster bitrateAdjuster = new FramerateBitrateAdjuster();
    bitrateAdjuster.setTargets(1000, 7.5);
    // Target frame frame is x4 times smaller than the adjusted one (30fps). Adjusted bitrate should
    // be x4 times larger then the target one.
    assertThat(bitrateAdjuster.getAdjustedBitrateBps()).isEqualTo(4000);
  }
}
