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

import android.os.Handler;
import android.os.HandlerThread;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;

/**
 * Can be used to save the video frames to file.
 */
public class VideoFileRenderer implements VideoRenderer.Callbacks {
  private static final String TAG = "VideoFileRenderer";

  private final HandlerThread renderThread;
  private final Object handlerLock = new Object();
  private final Handler renderThreadHandler;
  private final FileOutputStream videoOutFile;
  private final int outputFileWidth;
  private final int outputFileHeight;
  private final int outputFrameSize;
  private final ByteBuffer outputFrameBuffer;
  private EglBase eglBase;
  private YuvConverter yuvConverter;

  public VideoFileRenderer(String outputFile, int outputFileWidth, int outputFileHeight,
      final EglBase.Context sharedContext) throws IOException {
    if ((outputFileWidth % 2) == 1 || (outputFileHeight % 2) == 1) {
      throw new IllegalArgumentException("Does not support uneven width or height");
    }

    this.outputFileWidth = outputFileWidth;
    this.outputFileHeight = outputFileHeight;

    outputFrameSize = outputFileWidth * outputFileHeight * 3 / 2;
    outputFrameBuffer = ByteBuffer.allocateDirect(outputFrameSize);

    videoOutFile = new FileOutputStream(outputFile);
    videoOutFile.write(
        ("YUV4MPEG2 C420 W" + outputFileWidth + " H" + outputFileHeight + " Ip F30:1 A1:1\n")
            .getBytes());

    renderThread = new HandlerThread(TAG);
    renderThread.start();
    renderThreadHandler = new Handler(renderThread.getLooper());

    ThreadUtils.invokeAtFrontUninterruptibly(renderThreadHandler, new Runnable() {
      @Override
      public void run() {
        eglBase = EglBase.create(sharedContext, EglBase.CONFIG_PIXEL_BUFFER);
        eglBase.createDummyPbufferSurface();
        eglBase.makeCurrent();
        yuvConverter = new YuvConverter();
      }
    });
  }

  @Override
  public void renderFrame(final VideoRenderer.I420Frame frame) {
    renderThreadHandler.post(new Runnable() {
      @Override
      public void run() {
        renderFrameOnRenderThread(frame);
      }
    });
  }

  private void renderFrameOnRenderThread(VideoRenderer.I420Frame frame) {
    final float frameAspectRatio = (float) frame.rotatedWidth() / (float) frame.rotatedHeight();

    final float[] rotatedSamplingMatrix =
        RendererCommon.rotateTextureMatrix(frame.samplingMatrix, frame.rotationDegree);
    final float[] layoutMatrix = RendererCommon.getLayoutMatrix(
        false, frameAspectRatio, (float) outputFileWidth / outputFileHeight);
    final float[] texMatrix = RendererCommon.multiplyMatrices(rotatedSamplingMatrix, layoutMatrix);

    try {
      videoOutFile.write("FRAME\n".getBytes());
      if (!frame.yuvFrame) {
        yuvConverter.convert(outputFrameBuffer, outputFileWidth, outputFileHeight, outputFileWidth,
            frame.textureId, texMatrix);

        int stride = outputFileWidth;
        byte[] data = outputFrameBuffer.array();
        int offset = outputFrameBuffer.arrayOffset();

        // Write Y
        videoOutFile.write(data, offset, outputFileWidth * outputFileHeight);

        // Write U
        for (int r = outputFileHeight; r < outputFileHeight * 3 / 2; ++r) {
          videoOutFile.write(data, offset + r * stride, stride / 2);
        }

        // Write V
        for (int r = outputFileHeight; r < outputFileHeight * 3 / 2; ++r) {
          videoOutFile.write(data, offset + r * stride + stride / 2, stride / 2);
        }
      } else {
        nativeI420Scale(frame.yuvPlanes[0], frame.yuvStrides[0], frame.yuvPlanes[1],
            frame.yuvStrides[1], frame.yuvPlanes[2], frame.yuvStrides[2], frame.width, frame.height,
            outputFrameBuffer, outputFileWidth, outputFileHeight);
        videoOutFile.write(
            outputFrameBuffer.array(), outputFrameBuffer.arrayOffset(), outputFrameSize);
      }
    } catch (IOException e) {
      Logging.e(TAG, "Failed to write to file for video out");
      throw new RuntimeException(e);
    } finally {
      VideoRenderer.renderFrameDone(frame);
    }
  }

  /**
   * Release all resources. All already posted frames will be rendered first.
   */
  public void release() {
    final CountDownLatch cleanupBarrier = new CountDownLatch(1);
    renderThreadHandler.post(new Runnable() {
      @Override
      public void run() {
        try {
          videoOutFile.close();
        } catch (IOException e) {
          Logging.d(TAG, "Error closing output video file");
        }
        yuvConverter.release();
        eglBase.release();
        renderThread.quit();
        cleanupBarrier.countDown();
      }
    });
    ThreadUtils.awaitUninterruptibly(cleanupBarrier);
  }

  public static native void nativeI420Scale(ByteBuffer srcY, int strideY, ByteBuffer srcU,
      int strideU, ByteBuffer srcV, int strideV, int width, int height, ByteBuffer dst,
      int dstWidth, int dstHeight);
}
