/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RefCountDelegateTest {
  @Mock Runnable mockReleaseCallback;
  private RefCountDelegate refCountDelegate;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);

    refCountDelegate = new RefCountDelegate(mockReleaseCallback);
  }

  @Test
  public void testReleaseRunsReleaseCallback() {
    refCountDelegate.release();
    verify(mockReleaseCallback).run();
  }

  @Test
  public void testRetainIncreasesRefCount() {
    refCountDelegate.retain();

    refCountDelegate.release();
    verify(mockReleaseCallback, never()).run();

    refCountDelegate.release();
    verify(mockReleaseCallback).run();
  }

  @Test(expected = IllegalStateException.class)
  public void testReleaseAfterFreeThrowsIllegalStateException() {
    refCountDelegate.release();
    refCountDelegate.release();
  }

  @Test(expected = IllegalStateException.class)
  public void testRetainAfterFreeThrowsIllegalStateException() {
    refCountDelegate.release();
    refCountDelegate.retain();
  }

  @Test
  public void testSafeRetainBeforeFreeReturnsTrueAndIncreasesRefCount() {
    assertThat(refCountDelegate.safeRetain()).isTrue();

    refCountDelegate.release();
    verify(mockReleaseCallback, never()).run();

    refCountDelegate.release();
    verify(mockReleaseCallback).run();
  }

  @Test
  public void testSafeRetainAfterFreeReturnsFalse() {
    refCountDelegate.release();
    assertThat(refCountDelegate.safeRetain()).isFalse();
  }
}
