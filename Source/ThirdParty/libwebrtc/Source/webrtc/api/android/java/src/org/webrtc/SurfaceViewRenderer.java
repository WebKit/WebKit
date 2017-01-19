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

import android.content.Context;
import android.content.res.Resources.NotFoundException;
import android.graphics.Point;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import java.util.concurrent.CountDownLatch;

/**
 * Implements org.webrtc.VideoRenderer.Callbacks by displaying the video stream on a SurfaceView.
 * renderFrame() is asynchronous to avoid blocking the calling thread.
 * This class is thread safe and handles access from potentially four different threads:
 * Interaction from the main app in init, release, setMirror, and setScalingtype.
 * Interaction from C++ rtc::VideoSinkInterface in renderFrame.
 * Interaction from the Activity lifecycle in surfaceCreated, surfaceChanged, and surfaceDestroyed.
 * Interaction with the layout framework in onMeasure and onSizeChanged.
 */
public class SurfaceViewRenderer
    extends SurfaceView implements SurfaceHolder.Callback, VideoRenderer.Callbacks {
  private static final String TAG = "SurfaceViewRenderer";

  // Cached resource name.
  private final String resourceName;
  private final RendererCommon.VideoLayoutMeasure videoLayoutMeasure =
      new RendererCommon.VideoLayoutMeasure();
  private final EglRenderer eglRenderer;

  // Callback for reporting renderer events. Read-only after initilization so no lock required.
  private RendererCommon.RendererEvents rendererEvents;

  private final Object layoutLock = new Object();
  private boolean isFirstFrameRendered;
  private int rotatedFrameWidth;
  private int rotatedFrameHeight;
  private int frameRotation;

  /**
   * Standard View constructor. In order to render something, you must first call init().
   */
  public SurfaceViewRenderer(Context context) {
    super(context);
    this.resourceName = getResourceName();
    eglRenderer = new EglRenderer(resourceName);
    getHolder().addCallback(this);
  }

  /**
   * Standard View constructor. In order to render something, you must first call init().
   */
  public SurfaceViewRenderer(Context context, AttributeSet attrs) {
    super(context, attrs);
    this.resourceName = getResourceName();
    eglRenderer = new EglRenderer(resourceName);
    getHolder().addCallback(this);
  }

  /**
   * Initialize this class, sharing resources with |sharedContext|. It is allowed to call init() to
   * reinitialize the renderer after a previous init()/release() cycle.
   */
  public void init(EglBase.Context sharedContext, RendererCommon.RendererEvents rendererEvents) {
    init(sharedContext, rendererEvents, EglBase.CONFIG_PLAIN, new GlRectDrawer());
  }

  /**
   * Initialize this class, sharing resources with |sharedContext|. The custom |drawer| will be used
   * for drawing frames on the EGLSurface. This class is responsible for calling release() on
   * |drawer|. It is allowed to call init() to reinitialize the renderer after a previous
   * init()/release() cycle.
   */
  public void init(final EglBase.Context sharedContext,
      RendererCommon.RendererEvents rendererEvents, final int[] configAttributes,
      RendererCommon.GlDrawer drawer) {
    ThreadUtils.checkIsOnMainThread();
    this.rendererEvents = rendererEvents;
    synchronized (layoutLock) {
      rotatedFrameWidth = 0;
      rotatedFrameHeight = 0;
      frameRotation = 0;
    }
    eglRenderer.init(sharedContext, configAttributes, drawer);
  }

  /**
   * Block until any pending frame is returned and all GL resources released, even if an interrupt
   * occurs. If an interrupt occurs during release(), the interrupt flag will be set. This function
   * should be called before the Activity is destroyed and the EGLContext is still valid. If you
   * don't call this function, the GL resources might leak.
   */
  public void release() {
    eglRenderer.release();
  }

  /**
   * Set if the video stream should be mirrored or not.
   */
  public void setMirror(final boolean mirror) {
    eglRenderer.setMirror(mirror);
  }

  /**
   * Set how the video will fill the allowed layout area.
   */
  public void setScalingType(RendererCommon.ScalingType scalingType) {
    ThreadUtils.checkIsOnMainThread();
    videoLayoutMeasure.setScalingType(scalingType);
  }

  public void setScalingType(RendererCommon.ScalingType scalingTypeMatchOrientation,
      RendererCommon.ScalingType scalingTypeMismatchOrientation) {
    ThreadUtils.checkIsOnMainThread();
    videoLayoutMeasure.setScalingType(scalingTypeMatchOrientation, scalingTypeMismatchOrientation);
  }

  // VideoRenderer.Callbacks interface.
  @Override
  public void renderFrame(VideoRenderer.I420Frame frame) {
    updateFrameDimensionsAndReportEvents(frame);
    eglRenderer.renderFrame(frame);
  }

  // View layout interface.
  @Override
  protected void onMeasure(int widthSpec, int heightSpec) {
    ThreadUtils.checkIsOnMainThread();
    final Point size;
    synchronized (layoutLock) {
      size =
          videoLayoutMeasure.measure(widthSpec, heightSpec, rotatedFrameWidth, rotatedFrameHeight);
    }
    setMeasuredDimension(size.x, size.y);
    logD("onMeasure(). New size: " + size.x + "x" + size.y);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    ThreadUtils.checkIsOnMainThread();
    eglRenderer.setLayoutAspectRatio((right - left) / (float) (bottom - top));
  }

  // SurfaceHolder.Callback interface.
  @Override
  public void surfaceCreated(final SurfaceHolder holder) {
    ThreadUtils.checkIsOnMainThread();
    eglRenderer.createEglSurface(holder.getSurface());
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder) {
    ThreadUtils.checkIsOnMainThread();
    final CountDownLatch completionLatch = new CountDownLatch(1);
    eglRenderer.releaseEglSurface(new Runnable() {
      @Override
      public void run() {
        completionLatch.countDown();
      }
    });
    ThreadUtils.awaitUninterruptibly(completionLatch);
  }

  @Override
  public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    ThreadUtils.checkIsOnMainThread();
    eglRenderer.surfaceSizeChanged(width, height);
  }

  private String getResourceName() {
    try {
      return getResources().getResourceEntryName(getId()) + ": ";
    } catch (NotFoundException e) {
      return "";
    }
  }

  // Update frame dimensions and report any changes to |rendererEvents|.
  private void updateFrameDimensionsAndReportEvents(VideoRenderer.I420Frame frame) {
    synchronized (layoutLock) {
      if (!isFirstFrameRendered) {
        isFirstFrameRendered = true;
        logD("Reporting first rendered frame.");
        if (rendererEvents != null) {
          rendererEvents.onFirstFrameRendered();
        }
      }
      if (rotatedFrameWidth != frame.rotatedWidth() || rotatedFrameHeight != frame.rotatedHeight()
          || frameRotation != frame.rotationDegree) {
        logD("Reporting frame resolution changed to " + frame.width + "x" + frame.height
            + " with rotation " + frame.rotationDegree);
        if (rendererEvents != null) {
          rendererEvents.onFrameResolutionChanged(frame.width, frame.height, frame.rotationDegree);
        }
        rotatedFrameWidth = frame.rotatedWidth();
        rotatedFrameHeight = frame.rotatedHeight();
        frameRotation = frame.rotationDegree;
        post(new Runnable() {
          @Override
          public void run() {
            requestLayout();
          }
        });
      }
    }
  }

  private void logD(String string) {
    Logging.d(TAG, resourceName + string);
  }
}
