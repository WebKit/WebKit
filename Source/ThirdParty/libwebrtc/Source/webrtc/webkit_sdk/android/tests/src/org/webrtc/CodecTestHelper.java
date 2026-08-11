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
import static com.google.common.truth.Truth.assertWithMessage;

import java.nio.ByteBuffer;
import java.util.Random;

/**
 * Helper methods for {@link HardwareVideoEncoderTest} and {@link AndroidVideoDecoderTest}.
 */
class CodecTestHelper {
  static void assertEqualContents(byte[] expected, ByteBuffer actual, int offset, int size) {
    assertThat(size).isEqualTo(expected.length);
    assertThat(actual.capacity()).isAtLeast(offset + size);
    for (int i = 0; i < expected.length; i++) {
      assertWithMessage("At index: " + i).that(actual.get(offset + i)).isEqualTo(expected[i]);
    }
  }

  static byte[] generateRandomData(int length) {
    Random random = new Random();
    byte[] data = new byte[length];
    random.nextBytes(data);
    return data;
  }

  static VideoFrame.I420Buffer wrapI420(int width, int height, byte[] data) {
    final int posY = 0;
    final int posU = width * height;
    final int posV = posU + width * height / 4;
    final int endV = posV + width * height / 4;

    ByteBuffer buffer = ByteBuffer.allocateDirect(data.length);
    buffer.put(data);

    buffer.limit(posU);
    buffer.position(posY);
    ByteBuffer dataY = buffer.slice();

    buffer.limit(posV);
    buffer.position(posU);
    ByteBuffer dataU = buffer.slice();

    buffer.limit(endV);
    buffer.position(posV);
    ByteBuffer dataV = buffer.slice();

    return JavaI420Buffer.wrap(width, height, dataY, width, dataU, width / 2, dataV, width / 2,
        /* releaseCallback= */ null);
  }
}
