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
import android.view.Surface;
import javax.annotation.Nullable;
import javax.microedition.khronos.egl.EGL10;

/**
 * Holds EGL state and utility methods for handling an egl 1.0 EGLContext, an EGLDisplay,
 * and an EGLSurface.
 */
public interface EglBase {
  // EGL wrapper for an actual EGLContext.
  public interface Context {
    /**
     * Returns an EGL context that can be used by native code. Returns 0 if the method is
     * unsupported.
     *
     * @note This is currently only supported for EGL 1.4 and not for EGL 1.0.
     */
    long getNativeEglContext();
  }

  // According to the documentation, EGL can be used from multiple threads at the same time if each
  // thread has its own EGLContext, but in practice it deadlocks on some devices when doing this.
  // Therefore, synchronize on this global lock before calling dangerous EGL functions that might
  // deadlock. See https://bugs.chromium.org/p/webrtc/issues/detail?id=5702 for more info.
  public static final Object lock = new Object();

  // These constants are taken from EGL14.EGL_OPENGL_ES2_BIT and EGL14.EGL_CONTEXT_CLIENT_VERSION.
  // https://android.googlesource.com/platform/frameworks/base/+/master/opengl/java/android/opengl/EGL14.java
  // This is similar to how GlSurfaceView does:
  // http://grepcode.com/file/repository.grepcode.com/java/ext/com.google.android/android/5.1.1_r1/android/opengl/GLSurfaceView.java#760
  public static final int EGL_OPENGL_ES2_BIT = 4;
  // Android-specific extension.
  public static final int EGL_RECORDABLE_ANDROID = 0x3142;

  // clang-format off
  public static final int[] CONFIG_PLAIN = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL10.EGL_NONE
  };
  public static final int[] CONFIG_RGBA = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_ALPHA_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL10.EGL_NONE
  };
  public static final int[] CONFIG_PIXEL_BUFFER = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL10.EGL_SURFACE_TYPE, EGL10.EGL_PBUFFER_BIT,
    EGL10.EGL_NONE
  };
  public static final int[] CONFIG_PIXEL_RGBA_BUFFER = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_ALPHA_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL10.EGL_SURFACE_TYPE, EGL10.EGL_PBUFFER_BIT,
    EGL10.EGL_NONE
  };
  public static final int[] CONFIG_RECORDABLE = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RECORDABLE_ANDROID, 1,
    EGL10.EGL_NONE
  };
  // clang-format on

  /**
   * Create a new context with the specified config attributes, sharing data with |sharedContext|.
   * If |sharedContext| is null, a root context is created. This function will try to create an EGL
   * 1.4 context if possible, and an EGL 1.0 context otherwise.
   */
  public static EglBase create(@Nullable Context sharedContext, int[] configAttributes) {
    return (EglBase14.isEGL14Supported()
               && (sharedContext == null || sharedContext instanceof EglBase14.Context))
        ? new EglBase14((EglBase14.Context) sharedContext, configAttributes)
        : new EglBase10((EglBase10.Context) sharedContext, configAttributes);
  }

  /**
   * Helper function for creating a plain root context. This function will try to create an EGL 1.4
   * context if possible, and an EGL 1.0 context otherwise.
   */
  public static EglBase create() {
    return create(null /* shaderContext */, CONFIG_PLAIN);
  }

  /**
   * Helper function for creating a plain context, sharing data with |sharedContext|. This function
   * will try to create an EGL 1.4 context if possible, and an EGL 1.0 context otherwise.
   */
  public static EglBase create(Context sharedContext) {
    return create(sharedContext, CONFIG_PLAIN);
  }

  /**
   * Explicitly create a root EGl 1.0 context with the specified config attributes.
   */
  public static EglBase createEgl10(int[] configAttributes) {
    return new EglBase10(null /* shaderContext */, configAttributes);
  }

  /**
   * Explicitly create a root EGl 1.0 context with the specified config attributes
   * and shared context.
   */
  public static EglBase createEgl10(
      javax.microedition.khronos.egl.EGLContext sharedContext, int[] configAttributes) {
    return new EglBase10(new EglBase10.Context(sharedContext), configAttributes);
  }

  /**
   * Explicitly create a root EGl 1.4 context with the specified config attributes.
   */
  public static EglBase createEgl14(int[] configAttributes) {
    return new EglBase14(null /* shaderContext */, configAttributes);
  }

  /**
   * Explicitly create a root EGl 1.4 context with the specified config attributes
   * and shared context.
   */
  public static EglBase createEgl14(
      android.opengl.EGLContext sharedContext, int[] configAttributes) {
    return new EglBase14(new EglBase14.Context(sharedContext), configAttributes);
  }

  void createSurface(Surface surface);

  // Create EGLSurface from the Android SurfaceTexture.
  void createSurface(SurfaceTexture surfaceTexture);

  // Create dummy 1x1 pixel buffer surface so the context can be made current.
  void createDummyPbufferSurface();

  void createPbufferSurface(int width, int height);

  Context getEglBaseContext();

  boolean hasSurface();

  int surfaceWidth();

  int surfaceHeight();

  void releaseSurface();

  void release();

  void makeCurrent();

  // Detach the current EGL context, so that it can be made current on another thread.
  void detachCurrent();

  void swapBuffers();

  void swapBuffers(long presentationTimeStampNs);
}
