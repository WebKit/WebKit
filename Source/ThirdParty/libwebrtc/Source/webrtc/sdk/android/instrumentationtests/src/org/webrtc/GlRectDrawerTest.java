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
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import java.nio.ByteBuffer;
import java.util.Random;
import org.junit.Test;

public class GlRectDrawerTest {
  // Resolution of the test image.
  private static final int WIDTH = 16;
  private static final int HEIGHT = 16;
  // Seed for random pixel creation.
  private static final int SEED = 42;
  // When comparing pixels, allow some slack for float arithmetic and integer rounding.
  private static final float MAX_DIFF = 1.5f;

  // clang-format off
  private static final float[] IDENTITY_MATRIX = {
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1};
  // clang-format on

  private static float normalizedByte(byte b) {
    return (b & 0xFF) / 255.0f;
  }

  private static float saturatedConvert(float c) {
    return 255.0f * Math.max(0, Math.min(c, 1));
  }

  // Assert RGB ByteBuffers are pixel perfect identical.
  private static void assertByteBufferEquals(
      int width, int height, ByteBuffer actual, ByteBuffer expected) {
    actual.rewind();
    expected.rewind();
    assertEquals(actual.remaining(), width * height * 3);
    assertEquals(expected.remaining(), width * height * 3);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        final int actualR = actual.get() & 0xFF;
        final int actualG = actual.get() & 0xFF;
        final int actualB = actual.get() & 0xFF;
        final int expectedR = expected.get() & 0xFF;
        final int expectedG = expected.get() & 0xFF;
        final int expectedB = expected.get() & 0xFF;
        if (actualR != expectedR || actualG != expectedG || actualB != expectedB) {
          fail("ByteBuffers of size " + width + "x" + height + " not equal at position "
              + "(" + x + ", " + y + "). Expected color (R,G,B): "
              + "(" + expectedR + ", " + expectedG + ", " + expectedB + ")"
              + " but was: "
              + "(" + actualR + ", " + actualG + ", " + actualB + ").");
        }
      }
    }
  }

  // Convert RGBA ByteBuffer to RGB ByteBuffer.
  private static ByteBuffer stripAlphaChannel(ByteBuffer rgbaBuffer) {
    rgbaBuffer.rewind();
    assertEquals(rgbaBuffer.remaining() % 4, 0);
    final int numberOfPixels = rgbaBuffer.remaining() / 4;
    final ByteBuffer rgbBuffer = ByteBuffer.allocateDirect(numberOfPixels * 3);
    while (rgbaBuffer.hasRemaining()) {
      // Copy RGB.
      for (int channel = 0; channel < 3; ++channel) {
        rgbBuffer.put(rgbaBuffer.get());
      }
      // Drop alpha.
      rgbaBuffer.get();
    }
    return rgbBuffer;
  }

  // TODO(titovartem) make correct fix during webrtc:9175
  @SuppressWarnings("ByteBufferBackingArray")
  @Test
  @SmallTest
  public void testRgbRendering() {
    // Create EGL base with a pixel buffer as display output.
    final EglBase eglBase = EglBase.create(null, EglBase.CONFIG_PIXEL_BUFFER);
    eglBase.createPbufferSurface(WIDTH, HEIGHT);
    eglBase.makeCurrent();

    // Create RGB byte buffer plane with random content.
    final ByteBuffer rgbPlane = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 3);
    final Random random = new Random(SEED);
    random.nextBytes(rgbPlane.array());

    // Upload the RGB byte buffer data as a texture.
    final int rgbTexture = GlUtil.generateTexture(GLES20.GL_TEXTURE_2D);
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, rgbTexture);
    GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGB, WIDTH, HEIGHT, 0, GLES20.GL_RGB,
        GLES20.GL_UNSIGNED_BYTE, rgbPlane);
    GlUtil.checkNoGLES2Error("glTexImage2D");

    // Draw the RGB frame onto the pixel buffer.
    final GlRectDrawer drawer = new GlRectDrawer();
    drawer.drawRgb(rgbTexture, IDENTITY_MATRIX, WIDTH, HEIGHT, 0 /* viewportX */, 0 /* viewportY */,
        WIDTH, HEIGHT);

    // Download the pixels in the pixel buffer as RGBA. Not all platforms support RGB, e.g. Nexus 9.
    final ByteBuffer rgbaData = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 4);
    GLES20.glReadPixels(0, 0, WIDTH, HEIGHT, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, rgbaData);
    GlUtil.checkNoGLES2Error("glReadPixels");

    // Assert rendered image is pixel perfect to source RGB.
    assertByteBufferEquals(WIDTH, HEIGHT, stripAlphaChannel(rgbaData), rgbPlane);

    drawer.release();
    GLES20.glDeleteTextures(1, new int[] {rgbTexture}, 0);
    eglBase.release();
  }

  // TODO(titovartem) make correct fix during webrtc:9175
  @SuppressWarnings("ByteBufferBackingArray")
  @Test
  @SmallTest
  public void testYuvRendering() {
    // Create EGL base with a pixel buffer as display output.
    EglBase eglBase = EglBase.create(null, EglBase.CONFIG_PIXEL_BUFFER);
    eglBase.createPbufferSurface(WIDTH, HEIGHT);
    eglBase.makeCurrent();

    // Create YUV byte buffer planes with random content.
    final ByteBuffer[] yuvPlanes = new ByteBuffer[3];
    final Random random = new Random(SEED);
    for (int i = 0; i < 3; ++i) {
      yuvPlanes[i] = ByteBuffer.allocateDirect(WIDTH * HEIGHT);
      random.nextBytes(yuvPlanes[i].array());
    }

    // Generate 3 texture ids for Y/U/V.
    final int yuvTextures[] = new int[3];
    for (int i = 0; i < 3; i++) {
      yuvTextures[i] = GlUtil.generateTexture(GLES20.GL_TEXTURE_2D);
    }

    // Upload the YUV byte buffer data as textures.
    for (int i = 0; i < 3; ++i) {
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
      GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, WIDTH, HEIGHT, 0,
          GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, yuvPlanes[i]);
      GlUtil.checkNoGLES2Error("glTexImage2D");
    }

    // Draw the YUV frame onto the pixel buffer.
    final GlRectDrawer drawer = new GlRectDrawer();
    drawer.drawYuv(yuvTextures, IDENTITY_MATRIX, WIDTH, HEIGHT, 0 /* viewportX */,
        0 /* viewportY */, WIDTH, HEIGHT);

    // Download the pixels in the pixel buffer as RGBA. Not all platforms support RGB, e.g. Nexus 9.
    final ByteBuffer data = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 4);
    GLES20.glReadPixels(0, 0, WIDTH, HEIGHT, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, data);
    GlUtil.checkNoGLES2Error("glReadPixels");

    // Compare the YUV data with the RGBA result.
    for (int y = 0; y < HEIGHT; ++y) {
      for (int x = 0; x < WIDTH; ++x) {
        // YUV color space. Y in [0, 1], UV in [-0.5, 0.5]. The constants are taken from the YUV
        // fragment shader code in GlGenericDrawer.
        final float y_luma = normalizedByte(yuvPlanes[0].get());
        final float u_chroma = normalizedByte(yuvPlanes[1].get());
        final float v_chroma = normalizedByte(yuvPlanes[2].get());
        // Expected color in unrounded RGB [0.0f, 255.0f].
        final float expectedRed =
            saturatedConvert(1.16438f * y_luma + 1.59603f * v_chroma - 0.874202f);
        final float expectedGreen = saturatedConvert(
            1.16438f * y_luma - 0.391762f * u_chroma - 0.812968f * v_chroma + 0.531668f);
        final float expectedBlue =
            saturatedConvert(1.16438f * y_luma + 2.01723f * u_chroma - 1.08563f);

        // Actual color in RGB8888.
        final int actualRed = data.get() & 0xFF;
        final int actualGreen = data.get() & 0xFF;
        final int actualBlue = data.get() & 0xFF;
        final int actualAlpha = data.get() & 0xFF;

        // Assert rendered image is close to pixel perfect from source YUV.
        assertTrue(Math.abs(actualRed - expectedRed) < MAX_DIFF);
        assertTrue(Math.abs(actualGreen - expectedGreen) < MAX_DIFF);
        assertTrue(Math.abs(actualBlue - expectedBlue) < MAX_DIFF);
        assertEquals(actualAlpha, 255);
      }
    }

    drawer.release();
    GLES20.glDeleteTextures(3, yuvTextures, 0);
    eglBase.release();
  }

  /**
   * The purpose here is to test GlRectDrawer.oesDraw(). Unfortunately, there is no easy way to
   * create an OES texture, which is needed for input to oesDraw(). Most of the test is concerned
   * with creating OES textures in the following way:
   *  - Create SurfaceTexture with help from SurfaceTextureHelper.
   *  - Create an EglBase with the SurfaceTexture as EGLSurface.
   *  - Upload RGB texture with known content.
   *  - Draw the RGB texture onto the EglBase with the SurfaceTexture as target.
   *  - Wait for an OES texture to be produced.
   * The actual oesDraw() test is this:
   *  - Create an EglBase with a pixel buffer as target.
   *  - Render the OES texture onto the pixel buffer.
   *  - Read back the pixel buffer and compare it with the known RGB data.
   */
  // TODO(titovartem) make correct fix during webrtc:9175
  @SuppressWarnings("ByteBufferBackingArray")
  @Test
  @MediumTest
  public void testOesRendering() throws InterruptedException {
    /**
     * Stub class to convert RGB ByteBuffers to OES textures by drawing onto a SurfaceTexture.
     */
    class StubOesTextureProducer {
      private final EglBase eglBase;
      private final GlRectDrawer drawer;
      private final int rgbTexture;

      public StubOesTextureProducer(EglBase.Context sharedContext,
          SurfaceTextureHelper surfaceTextureHelper, int width, int height) {
        eglBase = EglBase.create(sharedContext, EglBase.CONFIG_PLAIN);
        surfaceTextureHelper.setTextureSize(width, height);
        eglBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
        assertEquals(eglBase.surfaceWidth(), width);
        assertEquals(eglBase.surfaceHeight(), height);

        drawer = new GlRectDrawer();

        eglBase.makeCurrent();
        rgbTexture = GlUtil.generateTexture(GLES20.GL_TEXTURE_2D);
      }

      public void draw(ByteBuffer rgbPlane) {
        eglBase.makeCurrent();

        // Upload RGB data to texture.
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, rgbTexture);
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGB, WIDTH, HEIGHT, 0, GLES20.GL_RGB,
            GLES20.GL_UNSIGNED_BYTE, rgbPlane);
        // Draw the RGB data onto the SurfaceTexture.
        drawer.drawRgb(rgbTexture, IDENTITY_MATRIX, WIDTH, HEIGHT, 0 /* viewportX */,
            0 /* viewportY */, WIDTH, HEIGHT);
        eglBase.swapBuffers();
      }

      public void release() {
        eglBase.makeCurrent();
        drawer.release();
        GLES20.glDeleteTextures(1, new int[] {rgbTexture}, 0);
        eglBase.release();
      }
    }

    // Create EGL base with a pixel buffer as display output.
    final EglBase eglBase = EglBase.create(null, EglBase.CONFIG_PIXEL_BUFFER);
    eglBase.createPbufferSurface(WIDTH, HEIGHT);

    // Create resources for generating OES textures.
    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, eglBase.getEglBaseContext());
    final StubOesTextureProducer oesProducer = new StubOesTextureProducer(
        eglBase.getEglBaseContext(), surfaceTextureHelper, WIDTH, HEIGHT);
    final SurfaceTextureHelperTest.MockTextureListener listener =
        new SurfaceTextureHelperTest.MockTextureListener();
    surfaceTextureHelper.startListening(listener);

    // Create RGB byte buffer plane with random content.
    final ByteBuffer rgbPlane = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 3);
    final Random random = new Random(SEED);
    random.nextBytes(rgbPlane.array());

    // Draw the frame and block until an OES texture is delivered.
    oesProducer.draw(rgbPlane);
    final VideoFrame.TextureBuffer textureBuffer = listener.waitForTextureBuffer();

    // Real test starts here.
    // Draw the OES texture on the pixel buffer.
    eglBase.makeCurrent();
    final GlRectDrawer drawer = new GlRectDrawer();
    drawer.drawOes(textureBuffer.getTextureId(),
        RendererCommon.convertMatrixFromAndroidGraphicsMatrix(textureBuffer.getTransformMatrix()),
        WIDTH, HEIGHT, 0 /* viewportX */, 0 /* viewportY */, WIDTH, HEIGHT);

    // Download the pixels in the pixel buffer as RGBA. Not all platforms support RGB, e.g. Nexus 9.
    final ByteBuffer rgbaData = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 4);
    GLES20.glReadPixels(0, 0, WIDTH, HEIGHT, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, rgbaData);
    GlUtil.checkNoGLES2Error("glReadPixels");

    // Assert rendered image is pixel perfect to source RGB.
    assertByteBufferEquals(WIDTH, HEIGHT, stripAlphaChannel(rgbaData), rgbPlane);

    drawer.release();
    textureBuffer.release();
    oesProducer.release();
    surfaceTextureHelper.dispose();
    eglBase.release();
  }
}
