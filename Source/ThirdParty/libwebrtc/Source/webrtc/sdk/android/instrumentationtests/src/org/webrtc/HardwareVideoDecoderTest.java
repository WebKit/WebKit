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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Unit tests for {@link HardwareVideoDecoder}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public final class HardwareVideoDecoderTest {
  @ClassParameter private static List<ParameterSet> CLASS_PARAMS = new ArrayList<>();

  static {
    CLASS_PARAMS.add(new ParameterSet()
                         .value("VP8" /* codecType */, false /* useEglContext */)
                         .name("VP8WithoutEglContext"));
    CLASS_PARAMS.add(new ParameterSet()
                         .value("VP8" /* codecType */, true /* useEglContext */)
                         .name("VP8WithEglContext"));
  }

  private final String codecType;
  private final boolean useEglContext;

  public HardwareVideoDecoderTest(String codecType, boolean useEglContext) {
    this.codecType = codecType;
    this.useEglContext = useEglContext;
  }

  private static final String TAG = "HardwareVideoDecoderTest";

  private static final int TEST_FRAME_COUNT = 10;
  private static final int TEST_FRAME_WIDTH = 640;
  private static final int TEST_FRAME_HEIGHT = 360;
  private static final VideoFrame.I420Buffer[] TEST_FRAMES = generateTestFrames();

  private static final boolean ENABLE_INTEL_VP8_ENCODER = true;
  private static final boolean ENABLE_H264_HIGH_PROFILE = true;
  private static final VideoEncoder.Settings ENCODER_SETTINGS =
      new VideoEncoder.Settings(1 /* core */, 640 /* width */, 480 /* height */, 300 /* kbps */,
          30 /* fps */, true /* automaticResizeOn */);

  private static final int DECODE_TIMEOUT_MS = 1000;
  private static final VideoDecoder.Settings SETTINGS =
      new VideoDecoder.Settings(1 /* core */, 640 /* width */, 480 /* height */);

  private static class MockDecodeCallback implements VideoDecoder.Callback {
    private BlockingQueue<VideoFrame> frameQueue = new LinkedBlockingQueue<>();

    @Override
    public void onDecodedFrame(VideoFrame frame, Integer decodeTimeMs, Integer qp) {
      assertNotNull(frame);
      frameQueue.offer(frame);
    }

    public void assertFrameDecoded(EncodedImage testImage, VideoFrame.I420Buffer testBuffer) {
      VideoFrame decodedFrame = poll();
      VideoFrame.Buffer decodedBuffer = decodedFrame.getBuffer();
      assertEquals(testImage.encodedWidth, decodedBuffer.getWidth());
      assertEquals(testImage.encodedHeight, decodedBuffer.getHeight());
      // TODO(sakal): Decoder looses the nanosecond precision. This is not a problem in practice
      // because C++ EncodedImage stores the timestamp in milliseconds.
      assertEquals(testImage.captureTimeNs / 1000, decodedFrame.getTimestampNs() / 1000);
      assertEquals(testImage.rotation, decodedFrame.getRotation());
    }

    public VideoFrame poll() {
      try {
        VideoFrame frame = frameQueue.poll(DECODE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        assertNotNull("Timed out waiting for the frame to be decoded.", frame);
        return frame;
      } catch (InterruptedException e) {
        throw new RuntimeException(e);
      }
    }
  }

  private static VideoFrame.I420Buffer[] generateTestFrames() {
    VideoFrame.I420Buffer[] result = new VideoFrame.I420Buffer[TEST_FRAME_COUNT];
    for (int i = 0; i < TEST_FRAME_COUNT; i++) {
      result[i] = JavaI420Buffer.allocate(TEST_FRAME_WIDTH, TEST_FRAME_HEIGHT);
      // TODO(sakal): Generate content for the test frames.
    }
    return result;
  }

  private final EncodedImage[] encodedTestFrames = new EncodedImage[TEST_FRAME_COUNT];
  private EglBase14 eglBase;

  private VideoDecoderFactory createDecoderFactory(EglBase.Context eglContext) {
    return new HardwareVideoDecoderFactory(eglContext);
  }

  private VideoDecoder createDecoder() {
    VideoDecoderFactory factory =
        createDecoderFactory(useEglContext ? eglBase.getEglBaseContext() : null);
    return factory.createDecoder(codecType);
  }

  private void encodeTestFrames() {
    VideoEncoderFactory encoderFactory = new HardwareVideoEncoderFactory(
        eglBase.getEglBaseContext(), ENABLE_INTEL_VP8_ENCODER, ENABLE_H264_HIGH_PROFILE);
    VideoEncoder encoder =
        encoderFactory.createEncoder(new VideoCodecInfo(codecType, new HashMap<>()));
    HardwareVideoEncoderTest.MockEncoderCallback encodeCallback =
        new HardwareVideoEncoderTest.MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(ENCODER_SETTINGS, encodeCallback));

    long lastTimestampNs = 0;
    for (int i = 0; i < TEST_FRAME_COUNT; i++) {
      lastTimestampNs += TimeUnit.SECONDS.toNanos(1) / ENCODER_SETTINGS.maxFramerate;
      VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
          new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});
      HardwareVideoEncoderTest.testEncodeFrame(
          encoder, new VideoFrame(TEST_FRAMES[i], 0 /* rotation */, lastTimestampNs), info);
      encodedTestFrames[i] = encodeCallback.poll();
    }

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader());

    eglBase = new EglBase14(null, EglBase.CONFIG_PLAIN);
    eglBase.createDummyPbufferSurface();
    eglBase.makeCurrent();

    encodeTestFrames();
  }

  @After
  public void tearDown() {
    eglBase.release();
  }

  @Test
  @SmallTest
  public void testInitialize() {
    VideoDecoder decoder = createDecoder();
    assertEquals(VideoCodecStatus.OK, decoder.initDecode(SETTINGS, null /* decodeCallback */));
    assertEquals(VideoCodecStatus.OK, decoder.release());
  }

  @Test
  @SmallTest
  public void testDecode() {
    VideoDecoder decoder = createDecoder();
    MockDecodeCallback callback = new MockDecodeCallback();
    assertEquals(VideoCodecStatus.OK, decoder.initDecode(SETTINGS, callback));

    for (int i = 0; i < TEST_FRAME_COUNT; i++) {
      assertEquals(VideoCodecStatus.OK,
          decoder.decode(encodedTestFrames[i],
              new VideoDecoder.DecodeInfo(false /* isMissingFrames */, 0 /* renderTimeMs */)));
      callback.assertFrameDecoded(encodedTestFrames[i], TEST_FRAMES[i]);
    }

    assertEquals(VideoCodecStatus.OK, decoder.release());
  }
}
