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
import static org.junit.Assert.fail;

import android.annotation.TargetApi;
import android.graphics.Matrix;
import android.opengl.GLES11Ext;
import android.support.test.filters.SmallTest;
import android.util.Log;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import javax.annotation.Nullable;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

@TargetApi(16)
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class HardwareVideoEncoderTest {
  @ClassParameter private static List<ParameterSet> CLASS_PARAMS = new ArrayList<>();

  static {
    CLASS_PARAMS.add(new ParameterSet()
                         .value(false /* useTextures */, false /* useEglContext */)
                         .name("I420WithoutEglContext"));
    CLASS_PARAMS.add(new ParameterSet()
                         .value(true /* useTextures */, false /* useEglContext */)
                         .name("TextureWithoutEglContext"));
    CLASS_PARAMS.add(new ParameterSet()
                         .value(true /* useTextures */, true /* useEglContext */)
                         .name("TextureWithEglContext"));
  }

  private final boolean useTextures;
  private final boolean useEglContext;

  public HardwareVideoEncoderTest(boolean useTextures, boolean useEglContext) {
    this.useTextures = useTextures;
    this.useEglContext = useEglContext;
  }

  final static String TAG = "HwVideoEncoderTest";

  private static final boolean ENABLE_INTEL_VP8_ENCODER = true;
  private static final boolean ENABLE_H264_HIGH_PROFILE = true;
  private static final VideoEncoder.Settings SETTINGS =
      new VideoEncoder.Settings(1 /* core */, 640 /* width */, 480 /* height */, 300 /* kbps */,
          30 /* fps */, true /* automaticResizeOn */);
  private static final int ENCODE_TIMEOUT_MS = 1000;
  private static final int NUM_TEST_FRAMES = 10;
  private static final int NUM_ENCODE_TRIES = 100;
  private static final int ENCODE_RETRY_SLEEP_MS = 1;

  // # Mock classes
  /**
   * Mock encoder callback that allows easy verification of the general properties of the encoded
   * frame such as width and height. Also used from HardwareVideoDecoderTest.
   */
  static class MockEncoderCallback implements VideoEncoder.Callback {
    private BlockingQueue<EncodedImage> frameQueue = new LinkedBlockingQueue<>();

    @Override
    public void onEncodedFrame(EncodedImage frame, VideoEncoder.CodecSpecificInfo info) {
      assertNotNull(frame);
      assertNotNull(info);

      // Make a copy because keeping a reference to the buffer is not allowed.
      final ByteBuffer bufferCopy = ByteBuffer.allocateDirect(frame.buffer.remaining());
      bufferCopy.put(frame.buffer);
      bufferCopy.rewind();

      frameQueue.offer(EncodedImage.builder()
                           .setBuffer(bufferCopy)
                           .setEncodedWidth(frame.encodedWidth)
                           .setEncodedHeight(frame.encodedHeight)
                           .setCaptureTimeNs(frame.captureTimeNs)
                           .setFrameType(frame.frameType)
                           .setRotation(frame.rotation)
                           .setCompleteFrame(frame.completeFrame)
                           .setQp(frame.qp)
                           .createEncodedImage());
    }

    public EncodedImage poll() {
      try {
        EncodedImage image = frameQueue.poll(ENCODE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        assertNotNull("Timed out waiting for the frame to be encoded.", image);
        return image;
      } catch (InterruptedException e) {
        throw new RuntimeException(e);
      }
    }

    public void assertFrameEncoded(VideoFrame frame) {
      final VideoFrame.Buffer buffer = frame.getBuffer();
      final EncodedImage image = poll();
      assertTrue(image.buffer.capacity() > 0);
      assertEquals(image.encodedWidth, buffer.getWidth());
      assertEquals(image.encodedHeight, buffer.getHeight());
      assertEquals(image.captureTimeNs, frame.getTimestampNs());
      assertEquals(image.rotation, frame.getRotation());
    }
  }

  /** A common base class for the texture and I420 buffer that implements reference counting. */
  private static abstract class MockBufferBase implements VideoFrame.Buffer {
    protected final int width;
    protected final int height;
    private final Runnable releaseCallback;
    private final Object refCountLock = new Object();
    private int refCount = 1;

    public MockBufferBase(int width, int height, Runnable releaseCallback) {
      this.width = width;
      this.height = height;
      this.releaseCallback = releaseCallback;
    }

    @Override
    public int getWidth() {
      return width;
    }

    @Override
    public int getHeight() {
      return height;
    }

    @Override
    public void retain() {
      synchronized (refCountLock) {
        assertTrue("Buffer retained after being destroyed.", refCount > 0);
        ++refCount;
      }
    }

    @Override
    public void release() {
      synchronized (refCountLock) {
        assertTrue("Buffer released too many times.", --refCount >= 0);
        if (refCount == 0) {
          releaseCallback.run();
        }
      }
    }
  }

  private static class MockTextureBuffer
      extends MockBufferBase implements VideoFrame.TextureBuffer {
    private final int textureId;

    public MockTextureBuffer(int textureId, int width, int height, Runnable releaseCallback) {
      super(width, height, releaseCallback);
      this.textureId = textureId;
    }

    @Override
    public VideoFrame.TextureBuffer.Type getType() {
      return VideoFrame.TextureBuffer.Type.OES;
    }

    @Override
    public int getTextureId() {
      return textureId;
    }

    @Override
    public Matrix getTransformMatrix() {
      return new Matrix();
    }

    @Override
    public VideoFrame.I420Buffer toI420() {
      return JavaI420Buffer.allocate(width, height);
    }

    @Override
    public VideoFrame.Buffer cropAndScale(
        int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
      retain();
      return new MockTextureBuffer(textureId, scaleWidth, scaleHeight, this ::release);
    }
  }

  private static class MockI420Buffer extends MockBufferBase implements VideoFrame.I420Buffer {
    private final JavaI420Buffer realBuffer;

    public MockI420Buffer(int width, int height, Runnable releaseCallback) {
      super(width, height, releaseCallback);
      realBuffer = JavaI420Buffer.allocate(width, height);
    }

    @Override
    public ByteBuffer getDataY() {
      return realBuffer.getDataY();
    }

    @Override
    public ByteBuffer getDataU() {
      return realBuffer.getDataU();
    }

    @Override
    public ByteBuffer getDataV() {
      return realBuffer.getDataV();
    }

    @Override
    public int getStrideY() {
      return realBuffer.getStrideY();
    }

    @Override
    public int getStrideU() {
      return realBuffer.getStrideU();
    }

    @Override
    public int getStrideV() {
      return realBuffer.getStrideV();
    }

    @Override
    public VideoFrame.I420Buffer toI420() {
      retain();
      return this;
    }

    @Override
    public void retain() {
      super.retain();
      realBuffer.retain();
    }

    @Override
    public void release() {
      super.release();
      realBuffer.release();
    }

    @Override
    public VideoFrame.Buffer cropAndScale(
        int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
      return realBuffer.cropAndScale(cropX, cropY, cropWidth, cropHeight, scaleWidth, scaleHeight);
    }
  }

  // # Test fields
  private final Object referencedFramesLock = new Object();
  private int referencedFrames = 0;

  private Runnable releaseFrameCallback = new Runnable() {
    @Override
    public void run() {
      synchronized (referencedFramesLock) {
        --referencedFrames;
      }
    }
  };

  private EglBase14 eglBase;
  private long lastTimestampNs;

  // # Helper methods
  private VideoEncoderFactory createEncoderFactory(EglBase.Context eglContext) {
    return new HardwareVideoEncoderFactory(
        eglContext, ENABLE_INTEL_VP8_ENCODER, ENABLE_H264_HIGH_PROFILE);
  }

  private @Nullable VideoEncoder createEncoder() {
    VideoEncoderFactory factory =
        createEncoderFactory(useEglContext ? eglBase.getEglBaseContext() : null);
    VideoCodecInfo[] supportedCodecs = factory.getSupportedCodecs();
    return factory.createEncoder(supportedCodecs[0]);
  }

  private VideoFrame generateI420Frame(int width, int height) {
    synchronized (referencedFramesLock) {
      ++referencedFrames;
    }
    lastTimestampNs += TimeUnit.SECONDS.toNanos(1) / SETTINGS.maxFramerate;
    VideoFrame.Buffer buffer = new MockI420Buffer(width, height, releaseFrameCallback);
    return new VideoFrame(buffer, 0 /* rotation */, lastTimestampNs);
  }

  private VideoFrame generateTextureFrame(int width, int height) {
    synchronized (referencedFramesLock) {
      ++referencedFrames;
    }
    final int textureId = GlUtil.generateTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES);
    lastTimestampNs += TimeUnit.SECONDS.toNanos(1) / SETTINGS.maxFramerate;
    VideoFrame.Buffer buffer =
        new MockTextureBuffer(textureId, width, height, releaseFrameCallback);
    return new VideoFrame(buffer, 0 /* rotation */, lastTimestampNs);
  }

  private VideoFrame generateFrame(int width, int height) {
    return useTextures ? generateTextureFrame(width, height) : generateI420Frame(width, height);
  }

  static void testEncodeFrame(
      VideoEncoder encoder, VideoFrame frame, VideoEncoder.EncodeInfo info) {
    int numTries = 0;

    // It takes a while for the encoder to become ready so try until it accepts the frame.
    while (true) {
      ++numTries;

      final VideoCodecStatus returnValue = encoder.encode(frame, info);
      switch (returnValue) {
        case OK:
          return; // Success
        case NO_OUTPUT:
          if (numTries >= NUM_ENCODE_TRIES) {
            fail("encoder.encode keeps returning NO_OUTPUT");
          }
          try {
            Thread.sleep(ENCODE_RETRY_SLEEP_MS); // Try again.
          } catch (InterruptedException e) {
            throw new RuntimeException(e);
          }
          break;
        default:
          fail("encoder.encode returned: " + returnValue); // Error
      }
    }
  }

  // # Tests
  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader(), TestConstants.NATIVE_LIBRARY);

    eglBase = new EglBase14(null, EglBase.CONFIG_PLAIN);
    eglBase.createDummyPbufferSurface();
    eglBase.makeCurrent();
    lastTimestampNs = System.nanoTime();
  }

  @After
  public void tearDown() {
    eglBase.release();
    synchronized (referencedFramesLock) {
      assertEquals("All frames were not released", 0, referencedFrames);
    }
  }

  @Test
  @SmallTest
  public void testInitialize() {
    VideoEncoder encoder = createEncoder();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, null));
    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  @SmallTest
  public void testEncode() {
    VideoEncoder encoder = createEncoder();
    MockEncoderCallback callback = new MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, callback));

    for (int i = 0; i < NUM_TEST_FRAMES; i++) {
      Log.d(TAG, "Test frame: " + i);
      VideoFrame frame = generateFrame(SETTINGS.width, SETTINGS.height);
      VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
          new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});
      testEncodeFrame(encoder, frame, info);

      callback.assertFrameEncoded(frame);
      frame.release();
    }

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  @SmallTest
  public void testEncodeAltenatingBuffers() {
    VideoEncoder encoder = createEncoder();
    MockEncoderCallback callback = new MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, callback));

    for (int i = 0; i < NUM_TEST_FRAMES; i++) {
      Log.d(TAG, "Test frame: " + i);
      VideoFrame frame;
      VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
          new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});

      frame = generateTextureFrame(SETTINGS.width, SETTINGS.height);
      testEncodeFrame(encoder, frame, info);
      callback.assertFrameEncoded(frame);
      frame.release();

      frame = generateI420Frame(SETTINGS.width, SETTINGS.height);
      testEncodeFrame(encoder, frame, info);
      callback.assertFrameEncoded(frame);
      frame.release();
    }

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  @SmallTest
  public void testEncodeDifferentSizes() {
    VideoEncoder encoder = createEncoder();
    MockEncoderCallback callback = new MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, callback));

    VideoFrame frame;
    VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
        new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});

    frame = generateFrame(SETTINGS.width / 2, SETTINGS.height / 2);
    testEncodeFrame(encoder, frame, info);
    callback.assertFrameEncoded(frame);
    frame.release();

    frame = generateFrame(SETTINGS.width, SETTINGS.height);
    testEncodeFrame(encoder, frame, info);
    callback.assertFrameEncoded(frame);
    frame.release();

    frame = generateFrame(SETTINGS.width / 4, SETTINGS.height / 4);
    testEncodeFrame(encoder, frame, info);
    callback.assertFrameEncoded(frame);
    frame.release();

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }
}
