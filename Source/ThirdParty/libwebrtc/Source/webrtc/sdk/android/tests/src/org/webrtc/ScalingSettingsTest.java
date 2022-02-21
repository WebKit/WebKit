/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.webrtc.VideoEncoder.ScalingSettings;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ScalingSettingsTest {
  @Test
  public void testToString() {
    assertEquals("[ 1, 2 ]", new ScalingSettings(1, 2).toString());
    assertEquals("OFF", ScalingSettings.OFF.toString());
  }
}
