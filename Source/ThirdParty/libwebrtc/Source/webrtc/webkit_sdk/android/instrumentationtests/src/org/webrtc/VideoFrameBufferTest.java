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

import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import android.graphics.Matrix;
import android.opengl.GLES20;
import android.os.Handler;
import android.os.HandlerThread;
import androidx.test.filters.SmallTest;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;
import org.webrtc.VideoFrame;

/**
 * Test VideoFrame buffers of different kind of formats: I420, RGB, OES, NV12, NV21, and verify
 * toI420() and cropAndScale() behavior. Creating RGB/OES textures involves VideoFrameDrawer and
 * GlRectDrawer and we are testing the full chain I420 -> OES/RGB texture -> I420, with and without
 * cropping in the middle. Reading textures back to I420 also exercises the YuvConverter code.
 */
@RunWith(Parameterized.class)
public class VideoFrameBufferTest {
  /**
   * These tests are parameterized on this enum which represents the different VideoFrame.Buffers.
   */
  private static enum BufferType { I420_JAVA, I420_NATIVE, RGB_TEXTURE, OES_TEXTURE, NV21, NV12 }

  @Parameters(name = "{0}")
  public static Collection<BufferType> parameters() {
    return Arrays.asList(BufferType.values());
  }

  @BeforeClass
  public static void setUp() {
    // Needed for JniCommon.nativeAllocateByteBuffer() to work, which is used from JavaI420Buffer.
    System.loadLibrary(TestConstants.NATIVE_LIBRARY);
  }

  private final BufferType bufferType;

  public VideoFrameBufferTest(BufferType bufferType) {
    this.bufferType = bufferType;
  }

  /**
   * Create a VideoFrame.Buffer of the given type with the same pixel content as the given I420
   * buffer.
   */
  private static VideoFrame.Buffer createBufferWithType(
      BufferType bufferType, VideoFrame.I420Buffer i420Buffer) {
    VideoFrame.Buffer buffer;
    switch (bufferType) {
      case I420_JAVA:
        buffer = i420Buffer;
        buffer.retain();
        assertEquals(VideoFrameBufferType.I420, buffer.getBufferType());
        assertEquals(VideoFrameBufferType.I420, nativeGetBufferType(buffer));
        return buffer;
      case I420_NATIVE:
        buffer = nativeGetNativeI420Buffer(i420Buffer);
        assertEquals(VideoFrameBufferType.I420, buffer.getBufferType());
        assertEquals(VideoFrameBufferType.I420, nativeGetBufferType(buffer));
        return buffer;
      case RGB_TEXTURE:
        buffer = createRgbTextureBuffer(/* eglContext= */ null, i420Buffer);
        assertEquals(VideoFrameBufferType.NATIVE, buffer.getBufferType());
        assertEquals(VideoFrameBufferType.NATIVE, nativeGetBufferType(buffer));
        return buffer;
      case OES_TEXTURE:
        buffer = createOesTextureBuffer(/* eglContext= */ null, i420Buffer);
        assertEquals(VideoFrameBufferType.NATIVE, buffer.getBufferType());
        assertEquals(VideoFrameBufferType.NATIVE, nativeGetBufferType(buffer));
        return buffer;
      case NV21:
        buffer = createNV21Buffer(i420Buffer);
        assertEquals(VideoFrameBufferType.NATIVE, buffer.getBufferType());
        assertEquals(VideoFrameBufferType.NATIVE, nativeGetBufferType(buffer));
        return buffer;
      case NV12:
        buffer = createNV12Buffer(i420Buffer);
        assertEquals(VideoFrameBufferType.NATIVE, buffer.getBufferType());
        assertEquals(VideoFrameBufferType.NATIVE, nativeGetBufferType(buffer));
        return buffer;
      default:
        throw new IllegalArgumentException("Unknown buffer type: " + bufferType);
    }
  }

  private VideoFrame.Buffer createBufferToTest(VideoFrame.I420Buffer i420Buffer) {
    return createBufferWithType(this.bufferType, i420Buffer);
  }

  /**
   * Creates a 16x16 I420 buffer that varies smoothly and spans all RGB values.
   */
  public static VideoFrame.I420Buffer createTestI420Buffer() {
    final int width = 16;
    final int height = 16;
    final int[] yData = new int[] {156, 162, 167, 172, 177, 182, 187, 193, 199, 203, 209, 214, 219,
        224, 229, 235, 147, 152, 157, 162, 168, 173, 178, 183, 188, 193, 199, 205, 210, 215, 220,
        225, 138, 143, 148, 153, 158, 163, 168, 174, 180, 184, 190, 195, 200, 205, 211, 216, 128,
        133, 138, 144, 149, 154, 159, 165, 170, 175, 181, 186, 191, 196, 201, 206, 119, 124, 129,
        134, 140, 145, 150, 156, 161, 166, 171, 176, 181, 187, 192, 197, 109, 114, 119, 126, 130,
        136, 141, 146, 151, 156, 162, 167, 172, 177, 182, 187, 101, 105, 111, 116, 121, 126, 132,
        137, 142, 147, 152, 157, 162, 168, 173, 178, 90, 96, 101, 107, 112, 117, 122, 127, 132, 138,
        143, 148, 153, 158, 163, 168, 82, 87, 92, 97, 102, 107, 113, 118, 123, 128, 133, 138, 144,
        149, 154, 159, 72, 77, 83, 88, 93, 98, 103, 108, 113, 119, 124, 129, 134, 139, 144, 150, 63,
        68, 73, 78, 83, 89, 94, 99, 104, 109, 114, 119, 125, 130, 135, 140, 53, 58, 64, 69, 74, 79,
        84, 89, 95, 100, 105, 110, 115, 120, 126, 131, 44, 49, 54, 59, 64, 70, 75, 80, 85, 90, 95,
        101, 106, 111, 116, 121, 34, 40, 45, 50, 55, 60, 65, 71, 76, 81, 86, 91, 96, 101, 107, 113,
        25, 30, 35, 40, 46, 51, 56, 61, 66, 71, 77, 82, 87, 92, 98, 103, 16, 21, 26, 31, 36, 41, 46,
        52, 57, 62, 67, 72, 77, 83, 89, 94};
    final int[] uData = new int[] {110, 113, 116, 118, 120, 123, 125, 128, 113, 116, 118, 120, 123,
        125, 128, 130, 116, 118, 120, 123, 125, 128, 130, 132, 118, 120, 123, 125, 128, 130, 132,
        135, 120, 123, 125, 128, 130, 132, 135, 138, 123, 125, 128, 130, 132, 135, 138, 139, 125,
        128, 130, 132, 135, 138, 139, 142, 128, 130, 132, 135, 138, 139, 142, 145};
    final int[] vData = new int[] {31, 45, 59, 73, 87, 100, 114, 127, 45, 59, 73, 87, 100, 114, 128,
        141, 59, 73, 87, 100, 114, 127, 141, 155, 73, 87, 100, 114, 127, 141, 155, 168, 87, 100,
        114, 128, 141, 155, 168, 182, 100, 114, 128, 141, 155, 168, 182, 197, 114, 127, 141, 155,
        168, 182, 196, 210, 127, 141, 155, 168, 182, 196, 210, 224};
    return JavaI420Buffer.wrap(width, height, toByteBuffer(yData),
        /* strideY= */ width, toByteBuffer(uData), /* strideU= */ width / 2, toByteBuffer(vData),
        /* strideV= */ width / 2,
        /* releaseCallback= */ null);
  }

  /**
   * Create an RGB texture buffer available in `eglContext` with the same pixel content as the given
   * I420 buffer.
   */
  public static VideoFrame.TextureBuffer createRgbTextureBuffer(
      EglBase.Context eglContext, VideoFrame.I420Buffer i420Buffer) {
    final int width = i420Buffer.getWidth();
    final int height = i420Buffer.getHeight();

    final HandlerThread renderThread = new HandlerThread("RGB texture thread");
    renderThread.start();
    final Handler renderThreadHandler = new Handler(renderThread.getLooper());
    return ThreadUtils.invokeAtFrontUninterruptibly(renderThreadHandler, () -> {
      // Create EGL base with a pixel buffer as display output.
      final EglBase eglBase = EglBase.create(eglContext, EglBase.CONFIG_PIXEL_BUFFER);
      eglBase.createDummyPbufferSurface();
      eglBase.makeCurrent();

      final GlTextureFrameBuffer textureFrameBuffer = new GlTextureFrameBuffer(GLES20.GL_RGBA);
      textureFrameBuffer.setSize(width, height);

      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, textureFrameBuffer.getFrameBufferId());
      drawI420Buffer(i420Buffer);
      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);

      final YuvConverter yuvConverter = new YuvConverter();
      return new TextureBufferImpl(width, height, VideoFrame.TextureBuffer.Type.RGB,
          textureFrameBuffer.getTextureId(),
          /* transformMatrix= */ new Matrix(), renderThreadHandler, yuvConverter,
          /* releaseCallback= */ () -> renderThreadHandler.post(() -> {
            textureFrameBuffer.release();
            yuvConverter.release();
            eglBase.release();
            renderThread.quit();
          }));
    });
  }

  /**
   * Create an OES texture buffer available in `eglContext` with the same pixel content as the given
   * I420 buffer.
   */
  public static VideoFrame.TextureBuffer createOesTextureBuffer(
      EglBase.Context eglContext, VideoFrame.I420Buffer i420Buffer) {
    final int width = i420Buffer.getWidth();
    final int height = i420Buffer.getHeight();

    // Create resources for generating OES textures.
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper test", eglContext);
    surfaceTextureHelper.setTextureSize(width, height);

    final HandlerThread renderThread = new HandlerThread("OES texture thread");
    renderThread.start();
    final Handler renderThreadHandler = new Handler(renderThread.getLooper());
    final VideoFrame.TextureBuffer oesBuffer =
        ThreadUtils.invokeAtFrontUninterruptibly(renderThreadHandler, () -> {
          // Create EGL base with the SurfaceTexture as display output.
          final EglBase eglBase = EglBase.create(eglContext, EglBase.CONFIG_PLAIN);
          eglBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
          eglBase.makeCurrent();
          assertEquals(width, eglBase.surfaceWidth());
          assertEquals(height, eglBase.surfaceHeight());

          final SurfaceTextureHelperTest.MockTextureListener listener =
              new SurfaceTextureHelperTest.MockTextureListener();
          surfaceTextureHelper.startListening(listener);

          // Draw the frame and block until an OES texture is delivered.
          drawI420Buffer(i420Buffer);
          eglBase.swapBuffers();
          final VideoFrame.TextureBuffer textureBuffer = listener.waitForTextureBuffer();
          surfaceTextureHelper.stopListening();
          surfaceTextureHelper.dispose();

          return textureBuffer;
        });
    renderThread.quit();

    return oesBuffer;
  }

  /** Create an NV21Buffer with the same pixel content as the given I420 buffer. */
  public static NV21Buffer createNV21Buffer(VideoFrame.I420Buffer i420Buffer) {
    final int width = i420Buffer.getWidth();
    final int height = i420Buffer.getHeight();
    final int chromaStride = width;
    final int chromaWidth = (width + 1) / 2;
    final int chromaHeight = (height + 1) / 2;
    final int ySize = width * height;

    final ByteBuffer nv21Buffer = ByteBuffer.allocateDirect(ySize + chromaStride * chromaHeight);
    // We don't care what the array offset is since we only want an array that is direct.
    @SuppressWarnings("ByteBufferBackingArray") final byte[] nv21Data = nv21Buffer.array();

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        final byte yValue = i420Buffer.getDataY().get(y * i420Buffer.getStrideY() + x);
        nv21Data[y * width + x] = yValue;
      }
    }
    for (int y = 0; y < chromaHeight; ++y) {
      for (int x = 0; x < chromaWidth; ++x) {
        final byte uValue = i420Buffer.getDataU().get(y * i420Buffer.getStrideU() + x);
        final byte vValue = i420Buffer.getDataV().get(y * i420Buffer.getStrideV() + x);
        nv21Data[ySize + y * chromaStride + 2 * x + 0] = vValue;
        nv21Data[ySize + y * chromaStride + 2 * x + 1] = uValue;
      }
    }
    return new NV21Buffer(nv21Data, width, height, /* releaseCallback= */ null);
  }

  /** Create an NV12Buffer with the same pixel content as the given I420 buffer. */
  public static NV12Buffer createNV12Buffer(VideoFrame.I420Buffer i420Buffer) {
    final int width = i420Buffer.getWidth();
    final int height = i420Buffer.getHeight();
    final int chromaStride = width;
    final int chromaWidth = (width + 1) / 2;
    final int chromaHeight = (height + 1) / 2;
    final int ySize = width * height;

    final ByteBuffer nv12Buffer = ByteBuffer.allocateDirect(ySize + chromaStride * chromaHeight);

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        final byte yValue = i420Buffer.getDataY().get(y * i420Buffer.getStrideY() + x);
        nv12Buffer.put(y * width + x, yValue);
      }
    }
    for (int y = 0; y < chromaHeight; ++y) {
      for (int x = 0; x < chromaWidth; ++x) {
        final byte uValue = i420Buffer.getDataU().get(y * i420Buffer.getStrideU() + x);
        final byte vValue = i420Buffer.getDataV().get(y * i420Buffer.getStrideV() + x);
        nv12Buffer.put(ySize + y * chromaStride + 2 * x + 0, uValue);
        nv12Buffer.put(ySize + y * chromaStride + 2 * x + 1, vValue);
      }
    }
    return new NV12Buffer(width, height, /* stride= */ width, /* sliceHeight= */ height, nv12Buffer,
        /* releaseCallback */ null);
  }

  /** Print the ByteBuffer plane to the StringBuilder. */
  private static void printPlane(
      StringBuilder stringBuilder, int width, int height, ByteBuffer plane, int stride) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        final int value = plane.get(y * stride + x) & 0xFF;
        if (x != 0) {
          stringBuilder.append(", ");
        }
        stringBuilder.append(value);
      }
      stringBuilder.append("\n");
    }
  }

  /** Convert the pixel content of an I420 buffer to a string representation. */
  private static String i420BufferToString(VideoFrame.I420Buffer buffer) {
    final StringBuilder stringBuilder = new StringBuilder();
    stringBuilder.append(
        "I420 buffer with size: " + buffer.getWidth() + "x" + buffer.getHeight() + ".\n");
    stringBuilder.append("Y-plane:\n");
    printPlane(stringBuilder, buffer.getWidth(), buffer.getHeight(), buffer.getDataY(),
        buffer.getStrideY());
    final int chromaWidth = (buffer.getWidth() + 1) / 2;
    final int chromaHeight = (buffer.getHeight() + 1) / 2;
    stringBuilder.append("U-plane:\n");
    printPlane(stringBuilder, chromaWidth, chromaHeight, buffer.getDataU(), buffer.getStrideU());
    stringBuilder.append("V-plane:\n");
    printPlane(stringBuilder, chromaWidth, chromaHeight, buffer.getDataV(), buffer.getStrideV());
    return stringBuilder.toString();
  }

  /**
   * Assert that the given I420 buffers are almost identical, allowing for some difference due to
   * numerical errors. It has limits for both overall PSNR and maximum individual pixel difference.
   */
  public static void assertAlmostEqualI420Buffers(
      VideoFrame.I420Buffer bufferA, VideoFrame.I420Buffer bufferB) {
    final int diff = maxDiff(bufferA, bufferB);
    assertThat("Pixel difference too high: " + diff + "."
            + "\nBuffer A: " + i420BufferToString(bufferA)
            + "Buffer B: " + i420BufferToString(bufferB),
        diff, lessThanOrEqualTo(4));
    final double psnr = calculatePsnr(bufferA, bufferB);
    assertThat("PSNR too low: " + psnr + "."
            + "\nBuffer A: " + i420BufferToString(bufferA)
            + "Buffer B: " + i420BufferToString(bufferB),
        psnr, greaterThanOrEqualTo(50.0));
  }

  /** Returns a flattened list of pixel differences for two ByteBuffer planes. */
  private static List<Integer> getPixelDiffs(
      int width, int height, ByteBuffer planeA, int strideA, ByteBuffer planeB, int strideB) {
    List<Integer> res = new ArrayList<>();
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        final int valueA = planeA.get(y * strideA + x) & 0xFF;
        final int valueB = planeB.get(y * strideB + x) & 0xFF;
        res.add(Math.abs(valueA - valueB));
      }
    }
    return res;
  }

  /** Returns a flattened list of pixel differences for two I420 buffers. */
  private static List<Integer> getPixelDiffs(
      VideoFrame.I420Buffer bufferA, VideoFrame.I420Buffer bufferB) {
    assertEquals(bufferA.getWidth(), bufferB.getWidth());
    assertEquals(bufferA.getHeight(), bufferB.getHeight());
    final int width = bufferA.getWidth();
    final int height = bufferA.getHeight();
    final int chromaWidth = (width + 1) / 2;
    final int chromaHeight = (height + 1) / 2;
    final List<Integer> diffs = getPixelDiffs(width, height, bufferA.getDataY(),
        bufferA.getStrideY(), bufferB.getDataY(), bufferB.getStrideY());
    diffs.addAll(getPixelDiffs(chromaWidth, chromaHeight, bufferA.getDataU(), bufferA.getStrideU(),
        bufferB.getDataU(), bufferB.getStrideU()));
    diffs.addAll(getPixelDiffs(chromaWidth, chromaHeight, bufferA.getDataV(), bufferA.getStrideV(),
        bufferB.getDataV(), bufferB.getStrideV()));
    return diffs;
  }

  /** Returns the maximum pixel difference from any of the Y/U/V planes in the given buffers. */
  private static int maxDiff(VideoFrame.I420Buffer bufferA, VideoFrame.I420Buffer bufferB) {
    return Collections.max(getPixelDiffs(bufferA, bufferB));
  }

  /**
   * Returns the PSNR given a sum of squared error and the number of measurements that were added.
   */
  private static double sseToPsnr(long sse, int count) {
    if (sse == 0) {
      return Double.POSITIVE_INFINITY;
    }
    final double meanSquaredError = (double) sse / (double) count;
    final double maxPixelValue = 255.0;
    return 10.0 * Math.log10(maxPixelValue * maxPixelValue / meanSquaredError);
  }

  /** Returns the PSNR of the given I420 buffers. */
  private static double calculatePsnr(
      VideoFrame.I420Buffer bufferA, VideoFrame.I420Buffer bufferB) {
    final List<Integer> pixelDiffs = getPixelDiffs(bufferA, bufferB);
    long sse = 0;
    for (int pixelDiff : pixelDiffs) {
      sse += pixelDiff * pixelDiff;
    }
    return sseToPsnr(sse, pixelDiffs.size());
  }

  /**
   * Convert an int array to a byte array and make sure the values are within the range [0, 255].
   */
  private static byte[] toByteArray(int[] array) {
    final byte[] res = new byte[array.length];
    for (int i = 0; i < array.length; ++i) {
      final int value = array[i];
      assertThat(value, greaterThanOrEqualTo(0));
      assertThat(value, lessThanOrEqualTo(255));
      res[i] = (byte) value;
    }
    return res;
  }

  /** Convert a byte array to a direct ByteBuffer. */
  private static ByteBuffer toByteBuffer(int[] array) {
    final ByteBuffer buffer = ByteBuffer.allocateDirect(array.length);
    buffer.put(toByteArray(array));
    buffer.rewind();
    return buffer;
  }

  /**
   * Draw an I420 buffer on the currently bound frame buffer, allocating and releasing any
   * resources necessary.
   */
  private static void drawI420Buffer(VideoFrame.I420Buffer i420Buffer) {
    final GlRectDrawer drawer = new GlRectDrawer();
    final VideoFrameDrawer videoFrameDrawer = new VideoFrameDrawer();
    videoFrameDrawer.drawFrame(
        new VideoFrame(i420Buffer, /* rotation= */ 0, /* timestampNs= */ 0), drawer);
    videoFrameDrawer.release();
    drawer.release();
  }

  /**
   * Helper function that tests cropAndScale() with the given cropping and scaling parameters, and
   * compares the pixel content against a reference I420 buffer.
   */
  private void testCropAndScale(
      int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
    final VideoFrame.I420Buffer referenceI420Buffer = createTestI420Buffer();
    final VideoFrame.Buffer bufferToTest = createBufferToTest(referenceI420Buffer);

    final VideoFrame.Buffer croppedReferenceBuffer = referenceI420Buffer.cropAndScale(
        cropX, cropY, cropWidth, cropHeight, scaleWidth, scaleHeight);
    referenceI420Buffer.release();
    final VideoFrame.I420Buffer croppedReferenceI420Buffer = croppedReferenceBuffer.toI420();
    croppedReferenceBuffer.release();

    final VideoFrame.Buffer croppedBufferToTest =
        bufferToTest.cropAndScale(cropX, cropY, cropWidth, cropHeight, scaleWidth, scaleHeight);
    bufferToTest.release();

    final VideoFrame.I420Buffer croppedOutputI420Buffer = croppedBufferToTest.toI420();
    croppedBufferToTest.release();

    assertAlmostEqualI420Buffers(croppedReferenceI420Buffer, croppedOutputI420Buffer);
    croppedReferenceI420Buffer.release();
    croppedOutputI420Buffer.release();
  }

  @Test
  @SmallTest
  /** Test calling toI420() and comparing pixel content against I420 reference. */
  public void testToI420() {
    final VideoFrame.I420Buffer referenceI420Buffer = createTestI420Buffer();
    final VideoFrame.Buffer bufferToTest = createBufferToTest(referenceI420Buffer);

    final VideoFrame.I420Buffer outputI420Buffer = bufferToTest.toI420();
    bufferToTest.release();

    assertEquals(VideoFrameBufferType.I420, nativeGetBufferType(outputI420Buffer));
    assertAlmostEqualI420Buffers(referenceI420Buffer, outputI420Buffer);
    referenceI420Buffer.release();
    outputI420Buffer.release();
  }

  @Test
  @SmallTest
  /** Pure 2x scaling with no cropping. */
  public void testScale2x() {
    testCropAndScale(0 /* cropX= */, 0 /* cropY= */, /* cropWidth= */ 16, /* cropHeight= */ 16,
        /* scaleWidth= */ 8, /* scaleHeight= */ 8);
  }

  @Test
  @SmallTest
  /** Test cropping only X direction, with no scaling. */
  public void testCropX() {
    testCropAndScale(8 /* cropX= */, 0 /* cropY= */, /* cropWidth= */ 8, /* cropHeight= */ 16,
        /* scaleWidth= */ 8, /* scaleHeight= */ 16);
  }

  @Test
  @SmallTest
  /** Test cropping only Y direction, with no scaling. */
  public void testCropY() {
    testCropAndScale(0 /* cropX= */, 8 /* cropY= */, /* cropWidth= */ 16, /* cropHeight= */ 8,
        /* scaleWidth= */ 16, /* scaleHeight= */ 8);
  }

  @Test
  @SmallTest
  /** Test center crop, with no scaling. */
  public void testCenterCrop() {
    testCropAndScale(4 /* cropX= */, 4 /* cropY= */, /* cropWidth= */ 8, /* cropHeight= */ 8,
        /* scaleWidth= */ 8, /* scaleHeight= */ 8);
  }

  @Test
  @SmallTest
  /** Test non-center crop for right bottom corner, with no scaling. */
  public void testRightBottomCornerCrop() {
    testCropAndScale(8 /* cropX= */, 8 /* cropY= */, /* cropWidth= */ 8, /* cropHeight= */ 8,
        /* scaleWidth= */ 8, /* scaleHeight= */ 8);
  }

  @Test
  @SmallTest
  /** Test combined cropping and scaling. */
  public void testCropAndScale() {
    testCropAndScale(4 /* cropX= */, 4 /* cropY= */, /* cropWidth= */ 12, /* cropHeight= */ 12,
        /* scaleWidth= */ 8, /* scaleHeight= */ 8);
  }

  @VideoFrameBufferType private static native int nativeGetBufferType(VideoFrame.Buffer buffer);

  /** Returns the copy of I420Buffer using WrappedNativeI420Buffer. */
  private static native VideoFrame.Buffer nativeGetNativeI420Buffer(
      VideoFrame.I420Buffer i420Buffer);
}
