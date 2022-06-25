/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.graphics.Bitmap;
import android.graphics.SurfaceTexture;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.support.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

// EmptyActivity is needed for the surface.
@RunWith(BaseJUnit4ClassRunner.class)
public class EglRendererTest {
  private final static String TAG = "EglRendererTest";
  private final static int RENDER_WAIT_MS = 1000;
  private final static int SURFACE_WAIT_MS = 1000;
  private final static int TEST_FRAME_WIDTH = 4;
  private final static int TEST_FRAME_HEIGHT = 4;
  private final static int REMOVE_FRAME_LISTENER_RACY_NUM_TESTS = 10;
  // Some arbitrary frames.
  private final static byte[][][] TEST_FRAMES_DATA = {
      {
          new byte[] {
              -99, -93, -88, -83, -78, -73, -68, -62, -56, -52, -46, -41, -36, -31, -26, -20},
          new byte[] {110, 113, 116, 118}, new byte[] {31, 45, 59, 73},
      },
      {
          new byte[] {
              -108, -103, -98, -93, -87, -82, -77, -72, -67, -62, -56, -50, -45, -40, -35, -30},
          new byte[] {120, 123, 125, -127}, new byte[] {87, 100, 114, 127},
      },
      {
          new byte[] {
              -117, -112, -107, -102, -97, -92, -87, -81, -75, -71, -65, -60, -55, -50, -44, -39},
          new byte[] {113, 116, 118, 120}, new byte[] {45, 59, 73, 87},
      },
  };
  private final static ByteBuffer[][] TEST_FRAMES =
      copyTestDataToDirectByteBuffers(TEST_FRAMES_DATA);

  private static class TestFrameListener implements EglRenderer.FrameListener {
    final private ArrayList<Bitmap> bitmaps = new ArrayList<Bitmap>();
    boolean bitmapReceived;
    Bitmap storedBitmap;

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onFrame(Bitmap bitmap) {
      if (bitmapReceived) {
        fail("Unexpected bitmap was received.");
      }

      bitmapReceived = true;
      storedBitmap = bitmap;
      notify();
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized boolean waitForBitmap(int timeoutMs) throws InterruptedException {
      final long endTimeMs = System.currentTimeMillis() + timeoutMs;
      while (!bitmapReceived) {
        final long waitTimeMs = endTimeMs - System.currentTimeMillis();
        if (waitTimeMs < 0) {
          return false;
        }
        wait(timeoutMs);
      }
      return true;
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized Bitmap resetAndGetBitmap() {
      bitmapReceived = false;
      return storedBitmap;
    }
  }

  final TestFrameListener testFrameListener = new TestFrameListener();

  EglRenderer eglRenderer;
  CountDownLatch surfaceReadyLatch = new CountDownLatch(1);
  int oesTextureId;
  SurfaceTexture surfaceTexture;

  @Before
  public void setUp() throws Exception {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setNativeLibraryName(TestConstants.NATIVE_LIBRARY)
                                         .createInitializationOptions());
    eglRenderer = new EglRenderer("TestRenderer: ");
    eglRenderer.init(null /* sharedContext */, EglBase.CONFIG_RGBA, new GlRectDrawer());
    oesTextureId = GlUtil.generateTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES);
    surfaceTexture = new SurfaceTexture(oesTextureId);
    surfaceTexture.setDefaultBufferSize(1 /* width */, 1 /* height */);
    eglRenderer.createEglSurface(surfaceTexture);
  }

  @After
  public void tearDown() {
    surfaceTexture.release();
    GLES20.glDeleteTextures(1 /* n */, new int[] {oesTextureId}, 0 /* offset */);
    eglRenderer.release();
  }

  /** Checks the bitmap is not null and the correct size. */
  private static void checkBitmap(Bitmap bitmap, float scale) {
    assertNotNull(bitmap);
    assertEquals((int) (TEST_FRAME_WIDTH * scale), bitmap.getWidth());
    assertEquals((int) (TEST_FRAME_HEIGHT * scale), bitmap.getHeight());
  }

  /**
   * Does linear sampling on U/V plane of test data.
   *
   * @param data Plane data to be sampled from.
   * @param planeWidth Width of the plane data. This is also assumed to be the stride.
   * @param planeHeight Height of the plane data.
   * @param x X-coordinate in range [0, 1].
   * @param y Y-coordinate in range [0, 1].
   */
  private static float linearSample(
      ByteBuffer plane, int planeWidth, int planeHeight, float x, float y) {
    final int stride = planeWidth;

    final float coordX = x * planeWidth;
    final float coordY = y * planeHeight;

    int lowIndexX = (int) Math.floor(coordX - 0.5f);
    int lowIndexY = (int) Math.floor(coordY - 0.5f);
    int highIndexX = lowIndexX + 1;
    int highIndexY = lowIndexY + 1;

    final float highWeightX = coordX - lowIndexX - 0.5f;
    final float highWeightY = coordY - lowIndexY - 0.5f;
    final float lowWeightX = 1f - highWeightX;
    final float lowWeightY = 1f - highWeightY;

    // Clamp on the edges.
    lowIndexX = Math.max(0, lowIndexX);
    lowIndexY = Math.max(0, lowIndexY);
    highIndexX = Math.min(planeWidth - 1, highIndexX);
    highIndexY = Math.min(planeHeight - 1, highIndexY);

    float lowYValue = (plane.get(lowIndexY * stride + lowIndexX) & 0xFF) * lowWeightX
        + (plane.get(lowIndexY * stride + highIndexX) & 0xFF) * highWeightX;
    float highYValue = (plane.get(highIndexY * stride + lowIndexX) & 0xFF) * lowWeightX
        + (plane.get(highIndexY * stride + highIndexX) & 0xFF) * highWeightX;

    return lowWeightY * lowYValue + highWeightY * highYValue;
  }

  private static byte saturatedFloatToByte(float c) {
    return (byte) Math.round(255f * Math.max(0f, Math.min(1f, c)));
  }

  /**
   * Converts test data YUV frame to expected RGBA frame. Tries to match the behavior of OpenGL
   * YUV drawer shader. Does linear sampling on the U- and V-planes.
   *
   * @param yuvFrame Array of size 3 containing Y-, U-, V-planes for image of size
   *                 (TEST_FRAME_WIDTH, TEST_FRAME_HEIGHT). U- and V-planes should be half the size
   *                 of the Y-plane.
   */
  private static byte[] convertYUVFrameToRGBA(ByteBuffer[] yuvFrame) {
    final byte[] argbFrame = new byte[TEST_FRAME_WIDTH * TEST_FRAME_HEIGHT * 4];
    final int argbStride = TEST_FRAME_WIDTH * 4;
    final int yStride = TEST_FRAME_WIDTH;

    final int vStride = TEST_FRAME_WIDTH / 2;

    for (int y = 0; y < TEST_FRAME_HEIGHT; y++) {
      for (int x = 0; x < TEST_FRAME_WIDTH; x++) {
        final float yC = ((yuvFrame[0].get(y * yStride + x) & 0xFF) - 16f) / 219f;
        final float uC = (linearSample(yuvFrame[1], TEST_FRAME_WIDTH / 2, TEST_FRAME_HEIGHT / 2,
                              (x + 0.5f) / TEST_FRAME_WIDTH, (y + 0.5f) / TEST_FRAME_HEIGHT)
                             - 16f)
                / 224f
            - 0.5f;
        final float vC = (linearSample(yuvFrame[2], TEST_FRAME_WIDTH / 2, TEST_FRAME_HEIGHT / 2,
                              (x + 0.5f) / TEST_FRAME_WIDTH, (y + 0.5f) / TEST_FRAME_HEIGHT)
                             - 16f)
                / 224f
            - 0.5f;
        final float rC = yC + 1.403f * vC;
        final float gC = yC - 0.344f * uC - 0.714f * vC;
        final float bC = yC + 1.77f * uC;

        argbFrame[y * argbStride + x * 4 + 0] = saturatedFloatToByte(rC);
        argbFrame[y * argbStride + x * 4 + 1] = saturatedFloatToByte(gC);
        argbFrame[y * argbStride + x * 4 + 2] = saturatedFloatToByte(bC);
        argbFrame[y * argbStride + x * 4 + 3] = (byte) 255;
      }
    }

    return argbFrame;
  }

  /** Checks that the bitmap content matches the test frame with the given index. */
  // TODO(titovartem) make correct fix during webrtc:9175
  @SuppressWarnings("ByteBufferBackingArray")
  private static void checkBitmapContent(Bitmap bitmap, int frame) {
    checkBitmap(bitmap, 1f);

    byte[] expectedRGBA = convertYUVFrameToRGBA(TEST_FRAMES[frame]);
    ByteBuffer bitmapBuffer = ByteBuffer.allocateDirect(bitmap.getByteCount());
    bitmap.copyPixelsToBuffer(bitmapBuffer);

    for (int i = 0; i < expectedRGBA.length; i++) {
      int expected = expectedRGBA[i] & 0xFF;
      int value = bitmapBuffer.get(i) & 0xFF;
      // Due to unknown conversion differences check value matches +-1.
      if (Math.abs(value - expected) > 1) {
        Logging.d(TAG, "Expected bitmap content: " + Arrays.toString(expectedRGBA));
        Logging.d(TAG, "Bitmap content: " + Arrays.toString(bitmapBuffer.array()));
        fail("Frame doesn't match original frame on byte " + i + ". Expected: " + expected
            + " Result: " + value);
      }
    }
  }

  /** Tells eglRenderer to render test frame with given index. */
  private void feedFrame(int i) {
    final VideoFrame.I420Buffer buffer = JavaI420Buffer.wrap(TEST_FRAME_WIDTH, TEST_FRAME_HEIGHT,
        TEST_FRAMES[i][0], TEST_FRAME_WIDTH, TEST_FRAMES[i][1], TEST_FRAME_WIDTH / 2,
        TEST_FRAMES[i][2], TEST_FRAME_WIDTH / 2, null /* releaseCallback */);
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestamp */);
    eglRenderer.onFrame(frame);
    frame.release();
  }

  @Test
  @SmallTest
  public void testAddFrameListener() throws Exception {
    eglRenderer.addFrameListener(testFrameListener, 0f /* scaleFactor */);
    feedFrame(0);
    assertTrue(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
    assertNull(testFrameListener.resetAndGetBitmap());
    eglRenderer.addFrameListener(testFrameListener, 0f /* scaleFactor */);
    feedFrame(1);
    assertTrue(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
    assertNull(testFrameListener.resetAndGetBitmap());
    feedFrame(2);
    // Check we get no more bitmaps than two.
    assertFalse(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
  }

  @Test
  @SmallTest
  public void testAddFrameListenerBitmap() throws Exception {
    eglRenderer.addFrameListener(testFrameListener, 1f /* scaleFactor */);
    feedFrame(0);
    assertTrue(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
    checkBitmapContent(testFrameListener.resetAndGetBitmap(), 0);
    eglRenderer.addFrameListener(testFrameListener, 1f /* scaleFactor */);
    feedFrame(1);
    assertTrue(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
    checkBitmapContent(testFrameListener.resetAndGetBitmap(), 1);
  }

  @Test
  @SmallTest
  public void testAddFrameListenerBitmapScale() throws Exception {
    for (int i = 0; i < 3; ++i) {
      float scale = i * 0.5f + 0.5f;
      eglRenderer.addFrameListener(testFrameListener, scale);
      feedFrame(i);
      assertTrue(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
      checkBitmap(testFrameListener.resetAndGetBitmap(), scale);
    }
  }

  /**
   * Checks that the frame listener will not be called with a frame that was delivered before the
   * frame listener was added.
   */
  @Test
  @SmallTest
  public void testFrameListenerNotCalledWithOldFrames() throws Exception {
    feedFrame(0);
    eglRenderer.addFrameListener(testFrameListener, 0f);
    // Check the old frame does not trigger frame listener.
    assertFalse(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
  }

  /** Checks that the frame listener will not be called after it is removed. */
  @Test
  @SmallTest
  public void testRemoveFrameListenerNotRacy() throws Exception {
    for (int i = 0; i < REMOVE_FRAME_LISTENER_RACY_NUM_TESTS; i++) {
      feedFrame(0);
      eglRenderer.addFrameListener(testFrameListener, 0f);
      eglRenderer.removeFrameListener(testFrameListener);
      feedFrame(1);
    }
    // Check the frame listener hasn't triggered.
    assertFalse(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
  }

  @Test
  @SmallTest
  public void testFrameListenersFpsReduction() throws Exception {
    // Test that normal frame listeners receive frames while the renderer is paused.
    eglRenderer.pauseVideo();
    eglRenderer.addFrameListener(testFrameListener, 1f /* scaleFactor */);
    feedFrame(0);
    assertTrue(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
    checkBitmapContent(testFrameListener.resetAndGetBitmap(), 0);

    // Test that frame listeners with FPS reduction applied receive frames while the renderer is not
    // paused.
    eglRenderer.disableFpsReduction();
    eglRenderer.addFrameListener(
        testFrameListener, 1f /* scaleFactor */, null, true /* applyFpsReduction */);
    feedFrame(1);
    assertTrue(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
    checkBitmapContent(testFrameListener.resetAndGetBitmap(), 1);

    // Test that frame listeners with FPS reduction applied will not receive frames while the
    // renderer is paused.
    eglRenderer.pauseVideo();
    eglRenderer.addFrameListener(
        testFrameListener, 1f /* scaleFactor */, null, true /* applyFpsReduction */);
    feedFrame(1);
    assertFalse(testFrameListener.waitForBitmap(RENDER_WAIT_MS));
  }

  private static ByteBuffer[][] copyTestDataToDirectByteBuffers(byte[][][] testData) {
    final ByteBuffer[][] result = new ByteBuffer[testData.length][];

    for (int i = 0; i < testData.length; i++) {
      result[i] = new ByteBuffer[testData[i].length];
      for (int j = 0; j < testData[i].length; j++) {
        result[i][j] = ByteBuffer.allocateDirect(testData[i][j].length);
        result[i][j].put(testData[i][j]);
        result[i][j].rewind();
      }
    }
    return result;
  }
}
