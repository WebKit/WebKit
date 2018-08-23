/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.annotation.TargetApi;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.os.Build;
import android.support.test.filters.SmallTest;
import android.util.Log;
import java.nio.ByteBuffer;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.MediaCodecVideoEncoder.OutputBufferInfo;

@TargetApi(Build.VERSION_CODES.ICE_CREAM_SANDWICH_MR1)
@RunWith(BaseJUnit4ClassRunner.class)
public class MediaCodecVideoEncoderTest {
  final static String TAG = "MCVideoEncoderTest";
  final static int profile = MediaCodecVideoEncoder.H264Profile.CONSTRAINED_BASELINE.getValue();

  @Test
  @SmallTest
  public void testInitializeUsingByteBuffer() {
    if (!MediaCodecVideoEncoder.isVp8HwSupported()) {
      Log.i(TAG, "Hardware does not support VP8 encoding, skipping testInitReleaseUsingByteBuffer");
      return;
    }
    MediaCodecVideoEncoder encoder = new MediaCodecVideoEncoder();
    assertTrue(encoder.initEncode(MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile,
        640, 480, 300, 30, /* useSurface= */ false));
    encoder.release();
  }

  @Test
  @SmallTest
  public void testInitilizeUsingTextures() {
    if (!MediaCodecVideoEncoder.isVp8HwSupportedUsingTextures()) {
      Log.i(TAG, "hardware does not support VP8 encoding, skipping testEncoderUsingTextures");
      return;
    }
    EglBase14 eglBase = new EglBase14(null, EglBase.CONFIG_PLAIN);
    MediaCodecVideoEncoder.setEglContext(eglBase.getEglBaseContext());
    MediaCodecVideoEncoder encoder = new MediaCodecVideoEncoder();
    assertTrue(encoder.initEncode(MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile,
        640, 480, 300, 30, /* useSurface= */ true));
    encoder.release();
    MediaCodecVideoEncoder.disposeEglContext();
    eglBase.release();
  }

  @Test
  @SmallTest
  public void testInitializeUsingByteBufferReInitilizeUsingTextures() {
    if (!MediaCodecVideoEncoder.isVp8HwSupportedUsingTextures()) {
      Log.i(TAG, "hardware does not support VP8 encoding, skipping testEncoderUsingTextures");
      return;
    }
    MediaCodecVideoEncoder encoder = new MediaCodecVideoEncoder();
    assertTrue(encoder.initEncode(MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile,
        640, 480, 300, 30, /* useSurface= */ false));
    encoder.release();
    EglBase14 eglBase = new EglBase14(null, EglBase.CONFIG_PLAIN);
    MediaCodecVideoEncoder.setEglContext(eglBase.getEglBaseContext());
    assertTrue(encoder.initEncode(MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile,
        640, 480, 300, 30, /* useSurface= */ true));
    encoder.release();
    MediaCodecVideoEncoder.disposeEglContext();
    eglBase.release();
  }

  @Test
  @SmallTest
  public void testEncoderUsingByteBuffer() throws InterruptedException {
    if (!MediaCodecVideoEncoder.isVp8HwSupported()) {
      Log.i(TAG, "Hardware does not support VP8 encoding, skipping testEncoderUsingByteBuffer");
      return;
    }

    final int width = 640;
    final int height = 480;
    final int min_size = width * height * 3 / 2;
    final long presentationTimestampUs = 2;

    MediaCodecVideoEncoder encoder = new MediaCodecVideoEncoder();

    assertTrue(encoder.initEncode(MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile,
        width, height, 300, 30, /* useSurface= */ false));
    ByteBuffer[] inputBuffers = encoder.getInputBuffers();
    assertNotNull(inputBuffers);
    assertTrue(min_size <= inputBuffers[0].capacity());

    int bufferIndex;
    do {
      Thread.sleep(10);
      bufferIndex = encoder.dequeueInputBuffer();
    } while (bufferIndex == -1); // |-1| is returned when there is no buffer available yet.

    assertTrue(bufferIndex >= 0);
    assertTrue(bufferIndex < inputBuffers.length);
    assertTrue(encoder.encodeBuffer(true, bufferIndex, min_size, presentationTimestampUs));

    OutputBufferInfo info;
    do {
      info = encoder.dequeueOutputBuffer();
      Thread.sleep(10);
    } while (info == null);
    assertTrue(info.index >= 0);
    assertEquals(presentationTimestampUs, info.presentationTimestampUs);
    assertTrue(info.buffer.capacity() > 0);
    encoder.releaseOutputBuffer(info.index);

    encoder.release();
  }
}
