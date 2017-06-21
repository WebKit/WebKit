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

import android.graphics.SurfaceTexture;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/**
 * Helper class to create and synchronize access to a SurfaceTexture. The caller will get notified
 * of new frames in onTextureFrameAvailable(), and should call returnTextureFrame() when done with
 * the frame. Only one texture frame can be in flight at once, so returnTextureFrame() must be
 * called in order to receive a new frame. Call stopListening() to stop receiveing new frames. Call
 * dispose to release all resources once the texture frame is returned.
 * Note that there is a C++ counter part of this class that optionally can be used. It is used for
 * wrapping texture frames into webrtc::VideoFrames and also handles calling returnTextureFrame()
 * when the webrtc::VideoFrame is no longer used.
 */
public class SurfaceTextureHelper {
  private static final String TAG = "SurfaceTextureHelper";
  /**
   * Callback interface for being notified that a new texture frame is available. The calls will be
   * made on a dedicated thread with a bound EGLContext. The thread will be the same throughout the
   * lifetime of the SurfaceTextureHelper instance, but different from the thread calling the
   * SurfaceTextureHelper constructor. The callee is not allowed to make another EGLContext current
   * on the calling thread.
   */
  public interface OnTextureFrameAvailableListener {
    abstract void onTextureFrameAvailable(
        int oesTextureId, float[] transformMatrix, long timestampNs);
  }

  /**
   * Construct a new SurfaceTextureHelper sharing OpenGL resources with |sharedContext|. A dedicated
   * thread and handler is created for handling the SurfaceTexture. May return null if EGL fails to
   * initialize a pixel buffer surface and make it current.
   */
  public static SurfaceTextureHelper create(
      final String threadName, final EglBase.Context sharedContext) {
    final HandlerThread thread = new HandlerThread(threadName);
    thread.start();
    final Handler handler = new Handler(thread.getLooper());

    // The onFrameAvailable() callback will be executed on the SurfaceTexture ctor thread. See:
    // http://grepcode.com/file/repository.grepcode.com/java/ext/com.google.android/android/5.1.1_r1/android/graphics/SurfaceTexture.java#195.
    // Therefore, in order to control the callback thread on API lvl < 21, the SurfaceTextureHelper
    // is constructed on the |handler| thread.
    return ThreadUtils.invokeAtFrontUninterruptibly(handler, new Callable<SurfaceTextureHelper>() {
      @Override
      public SurfaceTextureHelper call() {
        try {
          return new SurfaceTextureHelper(sharedContext, handler);
        } catch (RuntimeException e) {
          Logging.e(TAG, threadName + " create failure", e);
          return null;
        }
      }
    });
  }

  private final Handler handler;
  private final EglBase eglBase;
  private final SurfaceTexture surfaceTexture;
  private final int oesTextureId;
  private YuvConverter yuvConverter;

  // These variables are only accessed from the |handler| thread.
  private OnTextureFrameAvailableListener listener;
  // The possible states of this class.
  private boolean hasPendingTexture = false;
  private volatile boolean isTextureInUse = false;
  private boolean isQuitting = false;
  // |pendingListener| is set in setListener() and the runnable is posted to the handler thread.
  // setListener() is not allowed to be called again before stopListening(), so this is thread safe.
  private OnTextureFrameAvailableListener pendingListener;
  final Runnable setListenerRunnable = new Runnable() {
    @Override
    public void run() {
      Logging.d(TAG, "Setting listener to " + pendingListener);
      listener = pendingListener;
      pendingListener = null;
      // May have a pending frame from the previous capture session - drop it.
      if (hasPendingTexture) {
        // Calling updateTexImage() is neccessary in order to receive new frames.
        updateTexImage();
        hasPendingTexture = false;
      }
    }
  };

  private SurfaceTextureHelper(EglBase.Context sharedContext, Handler handler) {
    if (handler.getLooper().getThread() != Thread.currentThread()) {
      throw new IllegalStateException("SurfaceTextureHelper must be created on the handler thread");
    }
    this.handler = handler;

    eglBase = EglBase.create(sharedContext, EglBase.CONFIG_PIXEL_BUFFER);
    try {
      // Both these statements have been observed to fail on rare occasions, see BUG=webrtc:5682.
      eglBase.createDummyPbufferSurface();
      eglBase.makeCurrent();
    } catch (RuntimeException e) {
      // Clean up before rethrowing the exception.
      eglBase.release();
      handler.getLooper().quit();
      throw e;
    }

    oesTextureId = GlUtil.generateTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES);
    surfaceTexture = new SurfaceTexture(oesTextureId);
    surfaceTexture.setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
      @Override
      public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        hasPendingTexture = true;
        tryDeliverTextureFrame();
      }
    });
  }

  /**
   * Start to stream textures to the given |listener|. If you need to change listener, you need to
   * call stopListening() first.
   */
  public void startListening(final OnTextureFrameAvailableListener listener) {
    if (this.listener != null || this.pendingListener != null) {
      throw new IllegalStateException("SurfaceTextureHelper listener has already been set.");
    }
    this.pendingListener = listener;
    handler.post(setListenerRunnable);
  }

  /**
   * Stop listening. The listener set in startListening() is guaranteded to not receive any more
   * onTextureFrameAvailable() callbacks after this function returns.
   */
  public void stopListening() {
    Logging.d(TAG, "stopListening()");
    handler.removeCallbacks(setListenerRunnable);
    ThreadUtils.invokeAtFrontUninterruptibly(handler, new Runnable() {
      @Override
      public void run() {
        listener = null;
        pendingListener = null;
      }
    });
  }

  /**
   * Retrieve the underlying SurfaceTexture. The SurfaceTexture should be passed in to a video
   * producer such as a camera or decoder.
   */
  public SurfaceTexture getSurfaceTexture() {
    return surfaceTexture;
  }

  /**
   * Retrieve the handler that calls onTextureFrameAvailable(). This handler is valid until
   * dispose() is called.
   */
  public Handler getHandler() {
    return handler;
  }

  /**
   * Call this function to signal that you are done with the frame received in
   * onTextureFrameAvailable(). Only one texture frame can be in flight at once, so you must call
   * this function in order to receive a new frame.
   */
  public void returnTextureFrame() {
    handler.post(new Runnable() {
      @Override
      public void run() {
        isTextureInUse = false;
        if (isQuitting) {
          release();
        } else {
          tryDeliverTextureFrame();
        }
      }
    });
  }

  public boolean isTextureInUse() {
    return isTextureInUse;
  }

  /**
   * Call disconnect() to stop receiving frames. OpenGL resources are released and the handler is
   * stopped when the texture frame has been returned by a call to returnTextureFrame(). You are
   * guaranteed to not receive any more onTextureFrameAvailable() after this function returns.
   */
  public void dispose() {
    Logging.d(TAG, "dispose()");
    ThreadUtils.invokeAtFrontUninterruptibly(handler, new Runnable() {
      @Override
      public void run() {
        isQuitting = true;
        if (!isTextureInUse) {
          release();
        }
      }
    });
  }

  public void textureToYUV(final ByteBuffer buf, final int width, final int height,
      final int stride, final int textureId, final float[] transformMatrix) {
    if (textureId != oesTextureId) {
      throw new IllegalStateException("textureToByteBuffer called with unexpected textureId");
    }

    ThreadUtils.invokeAtFrontUninterruptibly(handler, new Runnable() {
      @Override
      public void run() {
        if (yuvConverter == null) {
          yuvConverter = new YuvConverter();
        }
        yuvConverter.convert(buf, width, height, stride, textureId, transformMatrix);
      }
    });
  }

  private void updateTexImage() {
    // SurfaceTexture.updateTexImage apparently can compete and deadlock with eglSwapBuffers,
    // as observed on Nexus 5. Therefore, synchronize it with the EGL functions.
    // See https://bugs.chromium.org/p/webrtc/issues/detail?id=5702 for more info.
    synchronized (EglBase.lock) {
      surfaceTexture.updateTexImage();
    }
  }

  private void tryDeliverTextureFrame() {
    if (handler.getLooper().getThread() != Thread.currentThread()) {
      throw new IllegalStateException("Wrong thread.");
    }
    if (isQuitting || !hasPendingTexture || isTextureInUse || listener == null) {
      return;
    }
    isTextureInUse = true;
    hasPendingTexture = false;

    updateTexImage();

    final float[] transformMatrix = new float[16];
    surfaceTexture.getTransformMatrix(transformMatrix);
    final long timestampNs = surfaceTexture.getTimestamp();
    listener.onTextureFrameAvailable(oesTextureId, transformMatrix, timestampNs);
  }

  private void release() {
    if (handler.getLooper().getThread() != Thread.currentThread()) {
      throw new IllegalStateException("Wrong thread.");
    }
    if (isTextureInUse || !isQuitting) {
      throw new IllegalStateException("Unexpected release.");
    }
    if (yuvConverter != null) {
      yuvConverter.release();
    }
    GLES20.glDeleteTextures(1, new int[] {oesTextureId}, 0);
    surfaceTexture.release();
    eglBase.release();
    handler.getLooper().quit();
  }
}
