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
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.opengl.GLES20;
import android.os.SystemClock;
import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(BaseJUnit4ClassRunner.class)
public class SurfaceTextureHelperTest {
  /**
   * Mock texture listener with blocking wait functionality.
   */
  public static final class MockTextureListener implements VideoSink {
    private final Object lock = new Object();
    private @Nullable VideoFrame.TextureBuffer textureBuffer;
    // Thread where frames are expected to be received on.
    private final @Nullable Thread expectedThread;

    MockTextureListener() {
      this.expectedThread = null;
    }

    MockTextureListener(Thread expectedThread) {
      this.expectedThread = expectedThread;
    }

    @Override
    public void onFrame(VideoFrame frame) {
      if (expectedThread != null && Thread.currentThread() != expectedThread) {
        throw new IllegalStateException("onTextureFrameAvailable called on wrong thread.");
      }
      synchronized (lock) {
        this.textureBuffer = (VideoFrame.TextureBuffer) frame.getBuffer();
        textureBuffer.retain();
        lock.notifyAll();
      }
    }

    /** Wait indefinitely for a new textureBuffer. */
    public VideoFrame.TextureBuffer waitForTextureBuffer() throws InterruptedException {
      synchronized (lock) {
        while (true) {
          final VideoFrame.TextureBuffer textureBufferToReturn = textureBuffer;
          if (textureBufferToReturn != null) {
            textureBuffer = null;
            return textureBufferToReturn;
          }
          lock.wait();
        }
      }
    }

    /** Make sure we get no frame in the specified time period. */
    public void assertNoFrameIsDelivered(final long waitPeriodMs) throws InterruptedException {
      final long startTimeMs = SystemClock.elapsedRealtime();
      long timeRemainingMs = waitPeriodMs;
      synchronized (lock) {
        while (textureBuffer == null && timeRemainingMs > 0) {
          lock.wait(timeRemainingMs);
          final long elapsedTimeMs = SystemClock.elapsedRealtime() - startTimeMs;
          timeRemainingMs = waitPeriodMs - elapsedTimeMs;
        }
        assertTrue(textureBuffer == null);
      }
    }
  }

  /** Assert that two integers are close, with difference at most
   * {@code threshold}. */
  public static void assertClose(int threshold, int expected, int actual) {
    if (Math.abs(expected - actual) <= threshold)
      return;
    fail("Not close enough, threshold " + threshold + ". Expected: " + expected + " Actual: "
        + actual);
  }

  @Before
  public void setUp() {
    // Load the JNI library for textureToYuv.
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader(), TestConstants.NATIVE_LIBRARY);
  }

  /**
   * Test normal use by receiving three uniform texture frames. Texture frames are returned as early
   * as possible. The texture pixel values are inspected by drawing the texture frame to a pixel
   * buffer and reading it back with glReadPixels().
   */
  @Test
  @MediumTest
  public void testThreeConstantColorFrames() throws InterruptedException {
    final int width = 16;
    final int height = 16;
    // Create EGL base with a pixel buffer as display output.
    final EglBase eglBase = EglBase.create(null, EglBase.CONFIG_PIXEL_BUFFER);
    eglBase.createPbufferSurface(width, height);
    final GlRectDrawer drawer = new GlRectDrawer();

    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, eglBase.getEglBaseContext());
    final MockTextureListener listener = new MockTextureListener();
    surfaceTextureHelper.startListening(listener);
    surfaceTextureHelper.setTextureSize(width, height);

    // Create resources for stubbing an OES texture producer. `eglOesBase` has the SurfaceTexture in
    // `surfaceTextureHelper` as the target EGLSurface.
    final EglBase eglOesBase = EglBase.create(eglBase.getEglBaseContext(), EglBase.CONFIG_PLAIN);
    eglOesBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    assertEquals(eglOesBase.surfaceWidth(), width);
    assertEquals(eglOesBase.surfaceHeight(), height);

    final int red[] = new int[] {79, 144, 185};
    final int green[] = new int[] {66, 210, 162};
    final int blue[] = new int[] {161, 117, 158};
    // Draw three frames.
    for (int i = 0; i < 3; ++i) {
      // Draw a constant color frame onto the SurfaceTexture.
      eglOesBase.makeCurrent();
      GLES20.glClearColor(red[i] / 255.0f, green[i] / 255.0f, blue[i] / 255.0f, 1.0f);
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      // swapBuffers() will ultimately trigger onTextureFrameAvailable().
      eglOesBase.swapBuffers();

      // Wait for an OES texture to arrive and draw it onto the pixel buffer.
      final VideoFrame.TextureBuffer textureBuffer = listener.waitForTextureBuffer();
      eglBase.makeCurrent();
      drawer.drawOes(textureBuffer.getTextureId(),
          RendererCommon.convertMatrixFromAndroidGraphicsMatrix(textureBuffer.getTransformMatrix()),
          width, height, 0, 0, width, height);
      textureBuffer.release();

      // Download the pixels in the pixel buffer as RGBA. Not all platforms support RGB, e.g.
      // Nexus 9.
      final ByteBuffer rgbaData = ByteBuffer.allocateDirect(width * height * 4);
      GLES20.glReadPixels(0, 0, width, height, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, rgbaData);
      GlUtil.checkNoGLES2Error("glReadPixels");

      // Assert rendered image is expected constant color.
      while (rgbaData.hasRemaining()) {
        assertEquals(rgbaData.get() & 0xFF, red[i]);
        assertEquals(rgbaData.get() & 0xFF, green[i]);
        assertEquals(rgbaData.get() & 0xFF, blue[i]);
        assertEquals(rgbaData.get() & 0xFF, 255);
      }
    }

    drawer.release();
    surfaceTextureHelper.dispose();
    eglBase.release();
  }

  /**
   * Test disposing the SurfaceTextureHelper while holding a pending texture frame. The pending
   * texture frame should still be valid, and this is tested by drawing the texture frame to a pixel
   * buffer and reading it back with glReadPixels().
   */
  @Test
  @MediumTest
  public void testLateReturnFrame() throws InterruptedException {
    final int width = 16;
    final int height = 16;
    // Create EGL base with a pixel buffer as display output.
    final EglBase eglBase = EglBase.create(null, EglBase.CONFIG_PIXEL_BUFFER);
    eglBase.createPbufferSurface(width, height);

    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, eglBase.getEglBaseContext());
    final MockTextureListener listener = new MockTextureListener();
    surfaceTextureHelper.startListening(listener);
    surfaceTextureHelper.setTextureSize(width, height);

    // Create resources for stubbing an OES texture producer. `eglOesBase` has the SurfaceTexture in
    // `surfaceTextureHelper` as the target EGLSurface.
    final EglBase eglOesBase = EglBase.create(eglBase.getEglBaseContext(), EglBase.CONFIG_PLAIN);
    eglOesBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    assertEquals(eglOesBase.surfaceWidth(), width);
    assertEquals(eglOesBase.surfaceHeight(), height);

    final int red = 79;
    final int green = 66;
    final int blue = 161;
    // Draw a constant color frame onto the SurfaceTexture.
    eglOesBase.makeCurrent();
    GLES20.glClearColor(red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f);
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    // swapBuffers() will ultimately trigger onTextureFrameAvailable().
    eglOesBase.swapBuffers();
    eglOesBase.release();

    // Wait for OES texture frame.
    final VideoFrame.TextureBuffer textureBuffer = listener.waitForTextureBuffer();
    // Diconnect while holding the frame.
    surfaceTextureHelper.dispose();

    // Draw the pending texture frame onto the pixel buffer.
    eglBase.makeCurrent();
    final GlRectDrawer drawer = new GlRectDrawer();
    drawer.drawOes(textureBuffer.getTextureId(),
        RendererCommon.convertMatrixFromAndroidGraphicsMatrix(textureBuffer.getTransformMatrix()),
        width, height, 0, 0, width, height);
    drawer.release();

    // Download the pixels in the pixel buffer as RGBA. Not all platforms support RGB, e.g. Nexus 9.
    final ByteBuffer rgbaData = ByteBuffer.allocateDirect(width * height * 4);
    GLES20.glReadPixels(0, 0, width, height, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, rgbaData);
    GlUtil.checkNoGLES2Error("glReadPixels");
    eglBase.release();

    // Assert rendered image is expected constant color.
    while (rgbaData.hasRemaining()) {
      assertEquals(rgbaData.get() & 0xFF, red);
      assertEquals(rgbaData.get() & 0xFF, green);
      assertEquals(rgbaData.get() & 0xFF, blue);
      assertEquals(rgbaData.get() & 0xFF, 255);
    }
    // Late frame return after everything has been disposed and released.
    textureBuffer.release();
  }

  /**
   * Test disposing the SurfaceTextureHelper, but keep trying to produce more texture frames. No
   * frames should be delivered to the listener.
   */
  @Test
  @MediumTest
  public void testDispose() throws InterruptedException {
    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper test" /* threadName */, null);
    final MockTextureListener listener = new MockTextureListener();
    surfaceTextureHelper.startListening(listener);
    // Create EglBase with the SurfaceTexture as target EGLSurface.
    final EglBase eglBase = EglBase.create(null, EglBase.CONFIG_PLAIN);
    surfaceTextureHelper.setTextureSize(/* textureWidth= */ 32, /* textureHeight= */ 32);
    eglBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    eglBase.makeCurrent();
    // Assert no frame has been received yet.
    listener.assertNoFrameIsDelivered(/* waitPeriodMs= */ 1);
    // Draw and wait for one frame.
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    // swapBuffers() will ultimately trigger onTextureFrameAvailable().
    eglBase.swapBuffers();
    listener.waitForTextureBuffer().release();

    // Dispose - we should not receive any textures after this.
    surfaceTextureHelper.dispose();

    // Draw one frame.
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    eglBase.swapBuffers();
    // swapBuffers() should not trigger onTextureFrameAvailable() because disposed has been called.
    // Assert that no OES texture was delivered.
    listener.assertNoFrameIsDelivered(/* waitPeriodMs= */ 500);

    eglBase.release();
  }

  /**
   * Test disposing the SurfaceTextureHelper immediately after is has been setup to use a
   * shared context. No frames should be delivered to the listener.
   */
  @Test
  @SmallTest
  public void testDisposeImmediately() {
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper test" /* threadName */, null);
    surfaceTextureHelper.dispose();
  }

  /**
   * Call stopListening(), but keep trying to produce more texture frames. No frames should be
   * delivered to the listener.
   */
  @Test
  @MediumTest
  public void testStopListening() throws InterruptedException {
    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper test" /* threadName */, null);
    surfaceTextureHelper.setTextureSize(/* textureWidth= */ 32, /* textureHeight= */ 32);
    final MockTextureListener listener = new MockTextureListener();
    surfaceTextureHelper.startListening(listener);
    // Create EglBase with the SurfaceTexture as target EGLSurface.
    final EglBase eglBase = EglBase.create(null, EglBase.CONFIG_PLAIN);
    eglBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    eglBase.makeCurrent();
    // Assert no frame has been received yet.
    listener.assertNoFrameIsDelivered(/* waitPeriodMs= */ 1);
    // Draw and wait for one frame.
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    // swapBuffers() will ultimately trigger onTextureFrameAvailable().
    eglBase.swapBuffers();
    listener.waitForTextureBuffer().release();

    // Stop listening - we should not receive any textures after this.
    surfaceTextureHelper.stopListening();

    // Draw one frame.
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    eglBase.swapBuffers();
    // swapBuffers() should not trigger onTextureFrameAvailable() because disposed has been called.
    // Assert that no OES texture was delivered.
    listener.assertNoFrameIsDelivered(/* waitPeriodMs= */ 500);

    surfaceTextureHelper.dispose();
    eglBase.release();
  }

  /**
   * Test stopListening() immediately after the SurfaceTextureHelper has been setup.
   */
  @Test
  @SmallTest
  public void testStopListeningImmediately() throws InterruptedException {
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper test" /* threadName */, null);
    final MockTextureListener listener = new MockTextureListener();
    surfaceTextureHelper.startListening(listener);
    surfaceTextureHelper.stopListening();
    surfaceTextureHelper.dispose();
  }

  /**
   * Test stopListening() immediately after the SurfaceTextureHelper has been setup on the handler
   * thread.
   */
  @Test
  @SmallTest
  public void testStopListeningImmediatelyOnHandlerThread() throws InterruptedException {
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper test" /* threadName */, null);
    final MockTextureListener listener = new MockTextureListener();

    final CountDownLatch stopListeningBarrier = new CountDownLatch(1);
    final CountDownLatch stopListeningBarrierDone = new CountDownLatch(1);
    // Start by posting to the handler thread to keep it occupied.
    surfaceTextureHelper.getHandler().post(new Runnable() {
      @Override
      public void run() {
        ThreadUtils.awaitUninterruptibly(stopListeningBarrier);
        surfaceTextureHelper.stopListening();
        stopListeningBarrierDone.countDown();
      }
    });

    // startListening() is asynchronous and will post to the occupied handler thread.
    surfaceTextureHelper.startListening(listener);
    // Wait for stopListening() to be called on the handler thread.
    stopListeningBarrier.countDown();
    stopListeningBarrierDone.await();
    // Wait until handler thread is idle to try to catch late startListening() call.
    final CountDownLatch barrier = new CountDownLatch(1);
    surfaceTextureHelper.getHandler().post(new Runnable() {
      @Override
      public void run() {
        barrier.countDown();
      }
    });
    ThreadUtils.awaitUninterruptibly(barrier);
    // Previous startListening() call should never have taken place and it should be ok to call it
    // again.
    surfaceTextureHelper.startListening(listener);

    surfaceTextureHelper.dispose();
  }

  /**
   * Test calling startListening() with a new listener after stopListening() has been called.
   */
  @Test
  @MediumTest
  public void testRestartListeningWithNewListener() throws InterruptedException {
    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper test" /* threadName */, null);
    surfaceTextureHelper.setTextureSize(/* textureWidth= */ 32, /* textureHeight= */ 32);
    final MockTextureListener listener1 = new MockTextureListener();
    surfaceTextureHelper.startListening(listener1);
    // Create EglBase with the SurfaceTexture as target EGLSurface.
    final EglBase eglBase = EglBase.create(null, EglBase.CONFIG_PLAIN);
    eglBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    eglBase.makeCurrent();
    // Assert no frame has been received yet.
    listener1.assertNoFrameIsDelivered(/* waitPeriodMs= */ 1);
    // Draw and wait for one frame.
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    // swapBuffers() will ultimately trigger onTextureFrameAvailable().
    eglBase.swapBuffers();
    listener1.waitForTextureBuffer().release();

    // Stop listening - `listener1` should not receive any textures after this.
    surfaceTextureHelper.stopListening();

    // Connect different listener.
    final MockTextureListener listener2 = new MockTextureListener();
    surfaceTextureHelper.startListening(listener2);
    // Assert no frame has been received yet.
    listener2.assertNoFrameIsDelivered(/* waitPeriodMs= */ 1);

    // Draw one frame.
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    eglBase.swapBuffers();

    // Check that `listener2` received the frame, and not `listener1`.
    listener2.waitForTextureBuffer().release();
    listener1.assertNoFrameIsDelivered(/* waitPeriodMs= */ 1);

    surfaceTextureHelper.dispose();
    eglBase.release();
  }

  @Test
  @MediumTest
  public void testTexturetoYuv() throws InterruptedException {
    final int width = 16;
    final int height = 16;

    final EglBase eglBase = EglBase.create(null, EglBase.CONFIG_PLAIN);

    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, eglBase.getEglBaseContext());
    final MockTextureListener listener = new MockTextureListener();
    surfaceTextureHelper.startListening(listener);
    surfaceTextureHelper.setTextureSize(width, height);

    // Create resources for stubbing an OES texture producer. `eglBase` has the SurfaceTexture in
    // `surfaceTextureHelper` as the target EGLSurface.

    eglBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    assertEquals(eglBase.surfaceWidth(), width);
    assertEquals(eglBase.surfaceHeight(), height);

    final int red[] = new int[] {79, 144, 185};
    final int green[] = new int[] {66, 210, 162};
    final int blue[] = new int[] {161, 117, 158};

    final int ref_y[] = new int[] {85, 170, 161};
    final int ref_u[] = new int[] {168, 97, 123};
    final int ref_v[] = new int[] {127, 106, 138};

    // Draw three frames.
    for (int i = 0; i < 3; ++i) {
      // Draw a constant color frame onto the SurfaceTexture.
      eglBase.makeCurrent();
      GLES20.glClearColor(red[i] / 255.0f, green[i] / 255.0f, blue[i] / 255.0f, 1.0f);
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      // swapBuffers() will ultimately trigger onTextureFrameAvailable().
      eglBase.swapBuffers();

      // Wait for an OES texture to arrive.
      final VideoFrame.TextureBuffer textureBuffer = listener.waitForTextureBuffer();
      final VideoFrame.I420Buffer i420 = textureBuffer.toI420();
      textureBuffer.release();

      // Memory layout: Lines are 16 bytes. First 16 lines are
      // the Y data. These are followed by 8 lines with 8 bytes of U
      // data on the left and 8 bytes of V data on the right.
      //
      // Offset
      //      0 YYYYYYYY YYYYYYYY
      //     16 YYYYYYYY YYYYYYYY
      //    ...
      //    240 YYYYYYYY YYYYYYYY
      //    256 UUUUUUUU VVVVVVVV
      //    272 UUUUUUUU VVVVVVVV
      //    ...
      //    368 UUUUUUUU VVVVVVVV
      //    384 buffer end

      // Allow off-by-one differences due to different rounding.
      final ByteBuffer dataY = i420.getDataY();
      final int strideY = i420.getStrideY();
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          assertClose(1, ref_y[i], dataY.get(y * strideY + x) & 0xFF);
        }
      }

      final int chromaWidth = width / 2;
      final int chromaHeight = height / 2;

      final ByteBuffer dataU = i420.getDataU();
      final ByteBuffer dataV = i420.getDataV();
      final int strideU = i420.getStrideU();
      final int strideV = i420.getStrideV();
      for (int y = 0; y < chromaHeight; y++) {
        for (int x = 0; x < chromaWidth; x++) {
          assertClose(1, ref_u[i], dataU.get(y * strideU + x) & 0xFF);
          assertClose(1, ref_v[i], dataV.get(y * strideV + x) & 0xFF);
        }
      }
      i420.release();
    }

    surfaceTextureHelper.dispose();
    eglBase.release();
  }
}
