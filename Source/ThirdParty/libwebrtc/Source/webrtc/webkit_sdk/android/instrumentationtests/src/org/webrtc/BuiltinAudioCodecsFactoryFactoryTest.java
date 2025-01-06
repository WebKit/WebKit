/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.filters.SmallTest;
import org.junit.Before;
import org.junit.Test;

public final class BuiltinAudioCodecsFactoryFactoryTest {
  @Before
  public void setUp() {
    System.loadLibrary(TestConstants.NATIVE_LIBRARY);
  }

  @Test
  @SmallTest
  public void testAudioEncoderFactoryFactoryTest() throws Exception {
    BuiltinAudioEncoderFactoryFactory factory = new BuiltinAudioEncoderFactoryFactory();
    long aef = 0;
    try {
      aef = factory.createNativeAudioEncoderFactory();
      assertThat(aef).isNotEqualTo(0);
    } finally {
      if (aef != 0) {
        JniCommon.nativeReleaseRef(aef);
      }
    }
  }

  @Test
  @SmallTest
  public void testAudioDecoderFactoryFactoryTest() throws Exception {
    BuiltinAudioDecoderFactoryFactory factory = new BuiltinAudioDecoderFactoryFactory();
    long adf = 0;
    try {
      adf = factory.createNativeAudioDecoderFactory();
      assertThat(adf).isNotEqualTo(0);
    } finally {
      if (adf != 0) {
        JniCommon.nativeReleaseRef(adf);
      }
    }
  }
}
