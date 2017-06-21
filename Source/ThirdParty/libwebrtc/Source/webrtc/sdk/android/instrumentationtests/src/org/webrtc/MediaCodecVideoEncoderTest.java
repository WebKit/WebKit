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
  final static String TAG = "MediaCodecVideoEncoderTest";
  final static int profile = MediaCodecVideoEncoder.H264Profile.CONSTRAINED_BASELINE.getValue();

  @Test
  @SmallTest
  public void testInitializeUsingByteBuffer() {
    if (!MediaCodecVideoEncoder.isVp8HwSupported()) {
      Log.i(TAG, "Hardware does not support VP8 encoding, skipping testInitReleaseUsingByteBuffer");
      return;
    }
    MediaCodecVideoEncoder encoder = new MediaCodecVideoEncoder();
    assertTrue(encoder.initEncode(
        MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile, 640, 480, 300, 30, null));
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
    MediaCodecVideoEncoder encoder = new MediaCodecVideoEncoder();
    assertTrue(encoder.initEncode(MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile,
        640, 480, 300, 30, eglBase.getEglBaseContext()));
    encoder.release();
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
    assertTrue(encoder.initEncode(
        MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile, 640, 480, 300, 30, null));
    encoder.release();
    EglBase14 eglBase = new EglBase14(null, EglBase.CONFIG_PLAIN);
    assertTrue(encoder.initEncode(MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile,
        640, 480, 300, 30, eglBase.getEglBaseContext()));
    encoder.release();
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
        width, height, 300, 30, null));
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

  @Test
  @SmallTest
  public void testEncoderUsingTextures() throws InterruptedException {
    if (!MediaCodecVideoEncoder.isVp8HwSupportedUsingTextures()) {
      Log.i(TAG, "Hardware does not support VP8 encoding, skipping testEncoderUsingTextures");
      return;
    }

    final int width = 640;
    final int height = 480;
    final long presentationTs = 2;

    final EglBase14 eglOesBase = new EglBase14(null, EglBase.CONFIG_PIXEL_BUFFER);
    eglOesBase.createDummyPbufferSurface();
    eglOesBase.makeCurrent();
    int oesTextureId = GlUtil.generateTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES);

    // TODO(perkj): This test is week since we don't fill the texture with valid data with correct
    // width and height and verify the encoded data. Fill the OES texture and figure out a way to
    // verify that the output make sense.

    MediaCodecVideoEncoder encoder = new MediaCodecVideoEncoder();

    assertTrue(encoder.initEncode(MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, profile,
        width, height, 300, 30, eglOesBase.getEglBaseContext()));
    assertTrue(
        encoder.encodeTexture(true, oesTextureId, RendererCommon.identityMatrix(), presentationTs));
    GlUtil.checkNoGLES2Error("encodeTexture");

    // It should be Ok to delete the texture after calling encodeTexture.
    GLES20.glDeleteTextures(1, new int[] {oesTextureId}, 0);

    OutputBufferInfo info = encoder.dequeueOutputBuffer();
    while (info == null) {
      info = encoder.dequeueOutputBuffer();
      Thread.sleep(20);
    }
    assertTrue(info.index != -1);
    assertTrue(info.buffer.capacity() > 0);
    assertEquals(presentationTs, info.presentationTimestampUs);
    encoder.releaseOutputBuffer(info.index);

    encoder.release();
    eglOesBase.release();
  }
}
