/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.gt;
import static org.mockito.AdditionalMatchers.lt;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.media.AudioTrack;
import android.os.Build;
import androidx.test.runner.AndroidJUnit4;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.webrtc.audio.LowLatencyAudioBufferManager;

/**
 * Tests for LowLatencyAudioBufferManager.
 */
@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.O)
public class LowLatencyAudioBufferManagerTest {
  @Mock private AudioTrack mockAudioTrack;
  private LowLatencyAudioBufferManager bufferManager;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    bufferManager = new LowLatencyAudioBufferManager();
  }

  @Test
  public void testBufferSizeDecrease() {
    when(mockAudioTrack.getUnderrunCount()).thenReturn(0);
    when(mockAudioTrack.getBufferSizeInFrames()).thenReturn(100);
    when(mockAudioTrack.getPlaybackRate()).thenReturn(1000);
    for (int i = 0; i < 9; i++) {
      bufferManager.maybeAdjustBufferSize(mockAudioTrack);
    }
    // Check that the buffer size was not changed yet.
    verify(mockAudioTrack, times(0)).setBufferSizeInFrames(anyInt());
    // After the 10th call without underruns, we expect the buffer size to decrease.
    bufferManager.maybeAdjustBufferSize(mockAudioTrack);
    // The expected size is 10ms below the existing size, which works out to 100 - (1000 / 100)
    // = 90.
    verify(mockAudioTrack, times(1)).setBufferSizeInFrames(90);
  }

  @Test
  public void testBufferSizeNeverBelow10ms() {
    when(mockAudioTrack.getUnderrunCount()).thenReturn(0);
    when(mockAudioTrack.getBufferSizeInFrames()).thenReturn(11);
    when(mockAudioTrack.getPlaybackRate()).thenReturn(1000);
    for (int i = 0; i < 10; i++) {
      bufferManager.maybeAdjustBufferSize(mockAudioTrack);
    }
    // Check that the buffer size was not set to a value below 10 ms.
    verify(mockAudioTrack, times(0)).setBufferSizeInFrames(lt(10));
  }

  @Test
  public void testUnderrunBehavior() {
    when(mockAudioTrack.getUnderrunCount()).thenReturn(1);
    when(mockAudioTrack.getBufferSizeInFrames()).thenReturn(100);
    when(mockAudioTrack.getPlaybackRate()).thenReturn(1000);
    bufferManager.maybeAdjustBufferSize(mockAudioTrack);
    // Check that the buffer size was increased after the underrrun.
    verify(mockAudioTrack, times(1)).setBufferSizeInFrames(gt(100));
    when(mockAudioTrack.getUnderrunCount()).thenReturn(0);
    for (int i = 0; i < 10; i++) {
      bufferManager.maybeAdjustBufferSize(mockAudioTrack);
    }
    // Check that the buffer size was not changed again, even though there were no underruns for
    // 10 calls.
    verify(mockAudioTrack, times(1)).setBufferSizeInFrames(anyInt());
  }

  @Test
  public void testBufferIncrease() {
    when(mockAudioTrack.getBufferSizeInFrames()).thenReturn(100);
    when(mockAudioTrack.getPlaybackRate()).thenReturn(1000);
    for (int i = 1; i < 30; i++) {
      when(mockAudioTrack.getUnderrunCount()).thenReturn(i);
      bufferManager.maybeAdjustBufferSize(mockAudioTrack);
    }
    // Check that the buffer size was not increased more than 5 times.
    verify(mockAudioTrack, times(5)).setBufferSizeInFrames(gt(100));
  }
}
