/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.annotation.TargetApi;
import android.graphics.Matrix;
import android.support.test.filters.SmallTest;
import android.util.Log;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Test;
import org.junit.runner.RunWith;

@TargetApi(16)
@RunWith(BaseJUnit4ClassRunner.class)
public class HardwareVideoEncoderTest {
  final static String TAG = "MediaCodecVideoEncoderTest";

  private static final boolean ENABLE_INTEL_VP8_ENCODER = true;
  private static final boolean ENABLE_H264_HIGH_PROFILE = true;
  private static final VideoEncoder.Settings SETTINGS = new VideoEncoder.Settings(
      1 /* core */, 640 /* width */, 480 /* height */, 300 /* kbps */, 30 /* fps */);

  @Test
  @SmallTest
  public void testInitializeUsingYuvBuffer() {
    HardwareVideoEncoderFactory factory =
        new HardwareVideoEncoderFactory(ENABLE_INTEL_VP8_ENCODER, ENABLE_H264_HIGH_PROFILE);
    VideoCodecInfo[] supportedCodecs = factory.getSupportedCodecs();
    if (supportedCodecs.length == 0) {
      Log.w(TAG, "No hardware encoding support, skipping testInitializeUsingYuvBuffer");
      return;
    }
    VideoEncoder encoder = factory.createEncoder(supportedCodecs[0]);
    assertEquals(encoder.initEncode(SETTINGS, null), VideoCodecStatus.OK);
    assertEquals(encoder.release(), VideoCodecStatus.OK);
  }

  @Test
  @SmallTest
  public void testEncodeYuvBuffer() throws InterruptedException {
    HardwareVideoEncoderFactory factory =
        new HardwareVideoEncoderFactory(ENABLE_INTEL_VP8_ENCODER, ENABLE_H264_HIGH_PROFILE);
    VideoCodecInfo[] supportedCodecs = factory.getSupportedCodecs();
    if (supportedCodecs.length == 0) {
      Log.w(TAG, "No hardware encoding support, skipping testEncodeYuvBuffer");
      return;
    }

    VideoEncoder encoder = factory.createEncoder(supportedCodecs[0]);

    final long presentationTimestampUs = 20000;
    final CountDownLatch encodeDone = new CountDownLatch(1);

    VideoEncoder.Callback callback = new VideoEncoder.Callback() {
      @Override
      public void onEncodedFrame(EncodedImage image, VideoEncoder.CodecSpecificInfo info) {
        assertTrue(image.buffer.capacity() > 0);
        assertEquals(image.encodedWidth, SETTINGS.width);
        assertEquals(image.encodedHeight, SETTINGS.height);
        assertEquals(image.captureTimeMs, presentationTimestampUs / 1000);
        assertEquals(image.frameType, EncodedImage.FrameType.VideoFrameKey);
        assertEquals(image.rotation, 0);
        assertTrue(image.completeFrame);

        encodeDone.countDown();
      }
    };

    assertEquals(encoder.initEncode(SETTINGS, callback), VideoCodecStatus.OK);

    VideoFrame.I420Buffer buffer = new I420BufferImpl(SETTINGS.width, SETTINGS.height);
    VideoFrame frame =
        new VideoFrame(buffer, 0 /* rotation */, presentationTimestampUs * 1000, new Matrix());
    VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
        new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameKey});

    assertEquals(encoder.encode(frame, info), VideoCodecStatus.OK);

    ThreadUtils.awaitUninterruptibly(encodeDone);

    assertEquals(encoder.release(), VideoCodecStatus.OK);
  }
}
