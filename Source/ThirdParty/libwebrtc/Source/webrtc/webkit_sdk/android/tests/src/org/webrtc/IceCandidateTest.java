/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
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
import org.webrtc.IceCandidate;

@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE)
public class IceCandidateTest {
  @Test
  public void testIceCandidateEquals() {
    IceCandidate c1 = new IceCandidate(
        "audio", 0, "candidate:1532086002 1 udp 2122194687 192.168.86.144 37138 typ host");
    IceCandidate c2 = new IceCandidate(
        "audio", 0, "candidate:1532086002 1 udp 2122194687 192.168.86.144 37138 typ host");

    // c3 differ by sdpMid
    IceCandidate c3 = new IceCandidate(
        "video", 0, "candidate:1532086002 1 udp 2122194687 192.168.86.144 37138 typ host");
    // c4 differ by sdpMLineIndex
    IceCandidate c4 = new IceCandidate(
        "audio", 1, "candidate:1532086002 1 udp 2122194687 192.168.86.144 37138 typ host");
    // c5 differ by sdp.
    IceCandidate c5 = new IceCandidate(
        "audio", 0, "candidate:1532086002 1 udp 2122194687 192.168.86.144 37139 typ host");

    assertThat(c1.equals(c2)).isTrue();
    assertThat(c2.equals(c1)).isTrue();
    assertThat(c1.equals(null)).isFalse();
    assertThat(c1.equals(c3)).isFalse();
    assertThat(c1.equals(c4)).isFalse();
    assertThat(c5.equals(c1)).isFalse();

    Object o2 = c2;
    assertThat(c1.equals(o2)).isTrue();
    assertThat(o2.equals(c1)).isTrue();
  }
}