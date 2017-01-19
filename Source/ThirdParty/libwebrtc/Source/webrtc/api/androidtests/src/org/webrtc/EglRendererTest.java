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

import android.graphics.Bitmap;
import android.graphics.SurfaceTexture;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

// EmptyActivity is needed for the surface.
public class EglRendererTest extends InstrumentationTestCase {
  final static String TAG = "EglRendererTest";
  final static int RENDER_WAIT_MS = 1000;
  final static int SURFACE_WAIT_MS = 1000;
  final static int TEST_FRAME_WIDTH = 4;
  final static int TEST_FRAME_HEIGHT = 4;
  // Some arbitrary frames.
  final static ByteBuffer[][] TEST_FRAMES = {
      {
          ByteBuffer.wrap(new byte[] {
              11, -12, 13, -14, -15, 16, -17, 18, 19, -110, 111, -112, -113, 114, -115, 116}),
          ByteBuffer.wrap(new byte[] {117, 118, 119, 120}),
          ByteBuffer.wrap(new byte[] {121, 122, 123, 124}),
      },
      {
          ByteBuffer.wrap(new byte[] {-11, -12, -13, -14, -15, -16, -17, -18, -19, -110, -111, -112,
              -113, -114, -115, -116}),
          ByteBuffer.wrap(new byte[] {-121, -122, -123, -124}),
          ByteBuffer.wrap(new byte[] {-117, -118, -119, -120}),
      },
      {
          ByteBuffer.wrap(new byte[] {-11, -12, -13, -14, -15, -16, -17, -18, -19, -110, -111, -112,
              -113, -114, -115, -116}),
          ByteBuffer.wrap(new byte[] {117, 118, 119, 120}),
          ByteBuffer.wrap(new byte[] {121, 122, 123, 124}),
      },
  };

  private class TestFrameListener implements EglRenderer.FrameListener {
    final private ArrayList<Bitmap> bitmaps = new ArrayList<Bitmap>();
    boolean bitmapReceived;
    Bitmap storedBitmap;

    @Override
    public synchronized void onFrame(Bitmap bitmap) {
      if (bitmapReceived) {
        fail("Unexpected bitmap was received.");
      }

      bitmapReceived = true;
      storedBitmap = bitmap;
      notify();
    }

    public synchronized boolean waitForBitmap(int timeoutMs) throws InterruptedException {
      if (!bitmapReceived) {
        wait(timeoutMs);
      }
      return bitmapReceived;
    }

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

  @Override
  protected void setUp() throws Exception {
    PeerConnectionFactory.initializeAndroidGlobals(getInstrumentation().getTargetContext(),
        true /* initializeAudio */, true /* initializeVideo */, true /* videoHwAcceleration */);
    eglRenderer = new EglRenderer("TestRenderer: ");
    eglRenderer.init(null /* sharedContext */, EglBase.CONFIG_RGBA, new GlRectDrawer());
    oesTextureId = GlUtil.generateTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES);
    surfaceTexture = new SurfaceTexture(oesTextureId);
    surfaceTexture.setDefaultBufferSize(1 /* width */, 1 /* height */);
    eglRenderer.createEglSurface(surfaceTexture);
    eglRenderer.surfaceSizeChanged(1 /* width */, 1 /* height */);
  }

  @Override
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

    return (lowWeightY * lowYValue + highWeightY * highYValue) / 255f;
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
        final int x2 = x / 2;
        final int y2 = y / 2;

        final float yC = (yuvFrame[0].get(y * yStride + x) & 0xFF) / 255f;
        final float uC = linearSample(yuvFrame[1], TEST_FRAME_WIDTH / 2, TEST_FRAME_HEIGHT / 2,
                             (x + 0.5f) / TEST_FRAME_WIDTH, (y + 0.5f) / TEST_FRAME_HEIGHT)
            - 0.5f;
        final float vC = linearSample(yuvFrame[2], TEST_FRAME_WIDTH / 2, TEST_FRAME_HEIGHT / 2,
                             (x + 0.5f) / TEST_FRAME_WIDTH, (y + 0.5f) / TEST_FRAME_HEIGHT)
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
    eglRenderer.renderFrame(new VideoRenderer.I420Frame(TEST_FRAME_WIDTH, TEST_FRAME_HEIGHT, 0,
        new int[] {TEST_FRAME_WIDTH, TEST_FRAME_WIDTH / 2, TEST_FRAME_WIDTH / 2}, TEST_FRAMES[i],
        0));
  }

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
}
