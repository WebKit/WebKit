/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import org.webrtc.VideoRenderer.I420Frame;

import android.annotation.SuppressLint;
import android.graphics.Point;
import android.graphics.Rect;
import android.opengl.EGL14;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;

import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.opengles.GL10;

/**
 * Efficiently renders YUV frames using the GPU for CSC.
 * Clients will want first to call setView() to pass GLSurfaceView
 * and then for each video stream either create instance of VideoRenderer using
 * createGui() call or VideoRenderer.Callbacks interface using create() call.
 * Only one instance of the class can be created.
 */
public class VideoRendererGui implements GLSurfaceView.Renderer {
  // |instance|, |instance.surface|, |eglContext|, and |eglContextReady| are synchronized on
  // |VideoRendererGui.class|.
  private static VideoRendererGui instance = null;
  private static Runnable eglContextReady = null;
  private static final String TAG = "VideoRendererGui";
  private GLSurfaceView surface;
  private static EglBase.Context eglContext = null;
  // Indicates if SurfaceView.Renderer.onSurfaceCreated was called.
  // If true then for every newly created yuv image renderer createTexture()
  // should be called. The variable is accessed on multiple threads and
  // all accesses are synchronized on yuvImageRenderers' object lock.
  private boolean onSurfaceCreatedCalled;
  private int screenWidth;
  private int screenHeight;
  // List of yuv renderers.
  private final ArrayList<YuvImageRenderer> yuvImageRenderers;
  // Render and draw threads.
  private static Thread renderFrameThread;
  private static Thread drawThread;

  private VideoRendererGui(GLSurfaceView surface) {
    this.surface = surface;
    // Create an OpenGL ES 2.0 context.
    surface.setPreserveEGLContextOnPause(true);
    surface.setEGLContextClientVersion(2);
    surface.setRenderer(this);
    surface.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);

    yuvImageRenderers = new ArrayList<YuvImageRenderer>();
  }

  /**
   * Class used to display stream of YUV420 frames at particular location
   * on a screen. New video frames are sent to display using renderFrame()
   * call.
   */
  private static class YuvImageRenderer implements VideoRenderer.Callbacks {
    // |surface| is synchronized on |this|.
    private GLSurfaceView surface;
    private int id;
    // TODO(magjed): Delete GL resources in release(). Must be synchronized with draw(). We are
    // currently leaking resources to avoid a rare crash in release() where the EGLContext has
    // become invalid beforehand.
    private int[] yuvTextures = {0, 0, 0};
    private final RendererCommon.YuvUploader yuvUploader = new RendererCommon.YuvUploader();
    private final RendererCommon.GlDrawer drawer;
    // Resources for making a deep copy of incoming OES texture frame.
    private GlTextureFrameBuffer textureCopy;

    // Pending frame to render. Serves as a queue with size 1. |pendingFrame| is accessed by two
    // threads - frames are received in renderFrame() and consumed in draw(). Frames are dropped in
    // renderFrame() if the previous frame has not been rendered yet.
    private I420Frame pendingFrame;
    private final Object pendingFrameLock = new Object();
    // Type of video frame used for recent frame rendering.
    private static enum RendererType { RENDERER_YUV, RENDERER_TEXTURE }

    private RendererType rendererType;
    private RendererCommon.ScalingType scalingType;
    private boolean mirror;
    private RendererCommon.RendererEvents rendererEvents;
    // Flag if renderFrame() was ever called.
    boolean seenFrame;
    // Total number of video frames received in renderFrame() call.
    private int framesReceived;
    // Number of video frames dropped by renderFrame() because previous
    // frame has not been rendered yet.
    private int framesDropped;
    // Number of rendered video frames.
    private int framesRendered;
    // Time in ns when the first video frame was rendered.
    private long startTimeNs = -1;
    // Time in ns spent in draw() function.
    private long drawTimeNs;
    // Time in ns spent in draw() copying resources from |pendingFrame| - including uploading frame
    // data to rendering planes.
    private long copyTimeNs;
    // The allowed view area in percentage of screen size.
    private final Rect layoutInPercentage;
    // The actual view area in pixels. It is a centered subrectangle of the rectangle defined by
    // |layoutInPercentage|.
    private final Rect displayLayout = new Rect();
    // Cached layout transformation matrix, calculated from current layout parameters.
    private float[] layoutMatrix;
    // Flag if layout transformation matrix update is needed.
    private boolean updateLayoutProperties;
    // Layout properties update lock. Guards |updateLayoutProperties|, |screenWidth|,
    // |screenHeight|, |videoWidth|, |videoHeight|, |rotationDegree|, |scalingType|, and |mirror|.
    private final Object updateLayoutLock = new Object();
    // Texture sampling matrix.
    private float[] rotatedSamplingMatrix;
    // Viewport dimensions.
    private int screenWidth;
    private int screenHeight;
    // Video dimension.
    private int videoWidth;
    private int videoHeight;

    // This is the degree that the frame should be rotated clockwisely to have
    // it rendered up right.
    private int rotationDegree;

    private YuvImageRenderer(GLSurfaceView surface, int id, int x, int y, int width, int height,
        RendererCommon.ScalingType scalingType, boolean mirror, RendererCommon.GlDrawer drawer) {
      Logging.d(TAG, "YuvImageRenderer.Create id: " + id);
      this.surface = surface;
      this.id = id;
      this.scalingType = scalingType;
      this.mirror = mirror;
      this.drawer = drawer;
      layoutInPercentage = new Rect(x, y, Math.min(100, x + width), Math.min(100, y + height));
      updateLayoutProperties = false;
      rotationDegree = 0;
    }

    public synchronized void reset() {
      seenFrame = false;
    }

    private synchronized void release() {
      surface = null;
      drawer.release();
      synchronized (pendingFrameLock) {
        if (pendingFrame != null) {
          VideoRenderer.renderFrameDone(pendingFrame);
          pendingFrame = null;
        }
      }
    }

    private void createTextures() {
      Logging.d(TAG, "  YuvImageRenderer.createTextures " + id + " on GL thread:"
              + Thread.currentThread().getId());

      // Generate 3 texture ids for Y/U/V and place them into |yuvTextures|.
      for (int i = 0; i < 3; i++) {
        yuvTextures[i] = GlUtil.generateTexture(GLES20.GL_TEXTURE_2D);
      }
      // Generate texture and framebuffer for offscreen texture copy.
      textureCopy = new GlTextureFrameBuffer(GLES20.GL_RGB);
    }

    private void updateLayoutMatrix() {
      synchronized (updateLayoutLock) {
        if (!updateLayoutProperties) {
          return;
        }
        // Initialize to maximum allowed area. Round to integer coordinates inwards the layout
        // bounding box (ceil left/top and floor right/bottom) to not break constraints.
        displayLayout.set((screenWidth * layoutInPercentage.left + 99) / 100,
            (screenHeight * layoutInPercentage.top + 99) / 100,
            (screenWidth * layoutInPercentage.right) / 100,
            (screenHeight * layoutInPercentage.bottom) / 100);
        Logging.d(TAG, "ID: " + id + ". AdjustTextureCoords. Allowed display size: "
                + displayLayout.width() + " x " + displayLayout.height() + ". Video: " + videoWidth
                + " x " + videoHeight + ". Rotation: " + rotationDegree + ". Mirror: " + mirror);
        final float videoAspectRatio = (rotationDegree % 180 == 0)
            ? (float) videoWidth / videoHeight
            : (float) videoHeight / videoWidth;
        // Adjust display size based on |scalingType|.
        final Point displaySize = RendererCommon.getDisplaySize(
            scalingType, videoAspectRatio, displayLayout.width(), displayLayout.height());
        displayLayout.inset((displayLayout.width() - displaySize.x) / 2,
            (displayLayout.height() - displaySize.y) / 2);
        Logging.d(TAG,
            "  Adjusted display size: " + displayLayout.width() + " x " + displayLayout.height());
        layoutMatrix = RendererCommon.getLayoutMatrix(
            mirror, videoAspectRatio, (float) displayLayout.width() / displayLayout.height());
        updateLayoutProperties = false;
        Logging.d(TAG, "  AdjustTextureCoords done");
      }
    }

    private void draw() {
      if (!seenFrame) {
        // No frame received yet - nothing to render.
        return;
      }
      long now = System.nanoTime();

      final boolean isNewFrame;
      synchronized (pendingFrameLock) {
        isNewFrame = (pendingFrame != null);
        if (isNewFrame && startTimeNs == -1) {
          startTimeNs = now;
        }

        if (isNewFrame) {
          rotatedSamplingMatrix = RendererCommon.rotateTextureMatrix(
              pendingFrame.samplingMatrix, pendingFrame.rotationDegree);
          if (pendingFrame.yuvFrame) {
            rendererType = RendererType.RENDERER_YUV;
            yuvUploader.uploadYuvData(yuvTextures, pendingFrame.width, pendingFrame.height,
                pendingFrame.yuvStrides, pendingFrame.yuvPlanes);
          } else {
            rendererType = RendererType.RENDERER_TEXTURE;
            // External texture rendering. Make a deep copy of the external texture.
            // Reallocate offscreen texture if necessary.
            textureCopy.setSize(pendingFrame.rotatedWidth(), pendingFrame.rotatedHeight());

            // Bind our offscreen framebuffer.
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, textureCopy.getFrameBufferId());
            GlUtil.checkNoGLES2Error("glBindFramebuffer");

            // Copy the OES texture content. This will also normalize the sampling matrix.
            drawer.drawOes(pendingFrame.textureId, rotatedSamplingMatrix, textureCopy.getWidth(),
                textureCopy.getHeight(), 0, 0, textureCopy.getWidth(), textureCopy.getHeight());
            rotatedSamplingMatrix = RendererCommon.identityMatrix();

            // Restore normal framebuffer.
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
            GLES20.glFinish();
          }
          copyTimeNs += (System.nanoTime() - now);
          VideoRenderer.renderFrameDone(pendingFrame);
          pendingFrame = null;
        }
      }

      updateLayoutMatrix();
      final float[] texMatrix =
          RendererCommon.multiplyMatrices(rotatedSamplingMatrix, layoutMatrix);
      // OpenGL defaults to lower left origin - flip viewport position vertically.
      final int viewportY = screenHeight - displayLayout.bottom;
      if (rendererType == RendererType.RENDERER_YUV) {
        drawer.drawYuv(yuvTextures, texMatrix, videoWidth, videoHeight, displayLayout.left,
            viewportY, displayLayout.width(), displayLayout.height());
      } else {
        drawer.drawRgb(textureCopy.getTextureId(), texMatrix, videoWidth, videoHeight,
            displayLayout.left, viewportY, displayLayout.width(), displayLayout.height());
      }

      if (isNewFrame) {
        framesRendered++;
        drawTimeNs += (System.nanoTime() - now);
        if ((framesRendered % 300) == 0) {
          logStatistics();
        }
      }
    }

    private void logStatistics() {
      long timeSinceFirstFrameNs = System.nanoTime() - startTimeNs;
      Logging.d(TAG, "ID: " + id + ". Type: " + rendererType + ". Frames received: "
              + framesReceived + ". Dropped: " + framesDropped + ". Rendered: " + framesRendered);
      if (framesReceived > 0 && framesRendered > 0) {
        Logging.d(TAG, "Duration: " + (int) (timeSinceFirstFrameNs / 1e6) + " ms. FPS: "
                + framesRendered * 1e9 / timeSinceFirstFrameNs);
        Logging.d(TAG, "Draw time: " + (int) (drawTimeNs / (1000 * framesRendered))
                + " us. Copy time: " + (int) (copyTimeNs / (1000 * framesReceived)) + " us");
      }
    }

    public void setScreenSize(final int screenWidth, final int screenHeight) {
      synchronized (updateLayoutLock) {
        if (screenWidth == this.screenWidth && screenHeight == this.screenHeight) {
          return;
        }
        Logging.d(TAG, "ID: " + id + ". YuvImageRenderer.setScreenSize: " + screenWidth + " x "
                + screenHeight);
        this.screenWidth = screenWidth;
        this.screenHeight = screenHeight;
        updateLayoutProperties = true;
      }
    }

    public void setPosition(int x, int y, int width, int height,
        RendererCommon.ScalingType scalingType, boolean mirror) {
      final Rect layoutInPercentage =
          new Rect(x, y, Math.min(100, x + width), Math.min(100, y + height));
      synchronized (updateLayoutLock) {
        if (layoutInPercentage.equals(this.layoutInPercentage) && scalingType == this.scalingType
            && mirror == this.mirror) {
          return;
        }
        Logging.d(TAG, "ID: " + id + ". YuvImageRenderer.setPosition: (" + x + ", " + y + ") "
                + width + " x " + height + ". Scaling: " + scalingType + ". Mirror: " + mirror);
        this.layoutInPercentage.set(layoutInPercentage);
        this.scalingType = scalingType;
        this.mirror = mirror;
        updateLayoutProperties = true;
      }
    }

    private void setSize(final int videoWidth, final int videoHeight, final int rotation) {
      if (videoWidth == this.videoWidth && videoHeight == this.videoHeight
          && rotation == rotationDegree) {
        return;
      }
      if (rendererEvents != null) {
        Logging.d(TAG, "ID: " + id + ". Reporting frame resolution changed to " + videoWidth + " x "
                + videoHeight);
        rendererEvents.onFrameResolutionChanged(videoWidth, videoHeight, rotation);
      }

      synchronized (updateLayoutLock) {
        Logging.d(TAG, "ID: " + id + ". YuvImageRenderer.setSize: " + videoWidth + " x "
                + videoHeight + " rotation " + rotation);

        this.videoWidth = videoWidth;
        this.videoHeight = videoHeight;
        rotationDegree = rotation;
        updateLayoutProperties = true;
        Logging.d(TAG, "  YuvImageRenderer.setSize done.");
      }
    }

    @Override
    public synchronized void renderFrame(I420Frame frame) {
      if (surface == null) {
        // This object has been released.
        VideoRenderer.renderFrameDone(frame);
        return;
      }
      if (renderFrameThread == null) {
        renderFrameThread = Thread.currentThread();
      }
      if (!seenFrame && rendererEvents != null) {
        Logging.d(TAG, "ID: " + id + ". Reporting first rendered frame.");
        rendererEvents.onFirstFrameRendered();
      }
      framesReceived++;
      synchronized (pendingFrameLock) {
        // Check input frame parameters.
        if (frame.yuvFrame) {
          if (frame.yuvStrides[0] < frame.width || frame.yuvStrides[1] < frame.width / 2
              || frame.yuvStrides[2] < frame.width / 2) {
            Logging.e(TAG, "Incorrect strides " + frame.yuvStrides[0] + ", " + frame.yuvStrides[1]
                    + ", " + frame.yuvStrides[2]);
            VideoRenderer.renderFrameDone(frame);
            return;
          }
        }

        if (pendingFrame != null) {
          // Skip rendering of this frame if previous frame was not rendered yet.
          framesDropped++;
          VideoRenderer.renderFrameDone(frame);
          seenFrame = true;
          return;
        }
        pendingFrame = frame;
      }
      setSize(frame.width, frame.height, frame.rotationDegree);
      seenFrame = true;

      // Request rendering.
      surface.requestRender();
    }
  }

  /** Passes GLSurfaceView to video renderer. */
  public static synchronized void setView(GLSurfaceView surface, Runnable eglContextReadyCallback) {
    Logging.d(TAG, "VideoRendererGui.setView");
    instance = new VideoRendererGui(surface);
    eglContextReady = eglContextReadyCallback;
  }

  public static synchronized EglBase.Context getEglBaseContext() {
    return eglContext;
  }

  /** Releases GLSurfaceView video renderer. */
  public static synchronized void dispose() {
    if (instance == null) {
      return;
    }
    Logging.d(TAG, "VideoRendererGui.dispose");
    synchronized (instance.yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : instance.yuvImageRenderers) {
        yuvImageRenderer.release();
      }
      instance.yuvImageRenderers.clear();
    }
    renderFrameThread = null;
    drawThread = null;
    instance.surface = null;
    eglContext = null;
    eglContextReady = null;
    instance = null;
  }

  /**
   * Creates VideoRenderer with top left corner at (x, y) and resolution
   * (width, height). All parameters are in percentage of screen resolution.
   */
  public static VideoRenderer createGui(int x, int y, int width, int height,
      RendererCommon.ScalingType scalingType, boolean mirror) throws Exception {
    YuvImageRenderer javaGuiRenderer = create(x, y, width, height, scalingType, mirror);
    return new VideoRenderer(javaGuiRenderer);
  }

  public static VideoRenderer.Callbacks createGuiRenderer(
      int x, int y, int width, int height, RendererCommon.ScalingType scalingType, boolean mirror) {
    return create(x, y, width, height, scalingType, mirror);
  }

  /**
   * Creates VideoRenderer.Callbacks with top left corner at (x, y) and
   * resolution (width, height). All parameters are in percentage of
   * screen resolution.
   */
  public static synchronized YuvImageRenderer create(
      int x, int y, int width, int height, RendererCommon.ScalingType scalingType, boolean mirror) {
    return create(x, y, width, height, scalingType, mirror, new GlRectDrawer());
  }

  /**
   * Creates VideoRenderer.Callbacks with top left corner at (x, y) and resolution (width, height).
   * All parameters are in percentage of screen resolution. The custom |drawer| will be used for
   * drawing frames on the EGLSurface. This class is responsible for calling release() on |drawer|.
   */
  public static synchronized YuvImageRenderer create(int x, int y, int width, int height,
      RendererCommon.ScalingType scalingType, boolean mirror, RendererCommon.GlDrawer drawer) {
    // Check display region parameters.
    if (x < 0 || x > 100 || y < 0 || y > 100 || width < 0 || width > 100 || height < 0
        || height > 100 || x + width > 100 || y + height > 100) {
      throw new RuntimeException("Incorrect window parameters.");
    }

    if (instance == null) {
      throw new RuntimeException("Attempt to create yuv renderer before setting GLSurfaceView");
    }
    final YuvImageRenderer yuvImageRenderer = new YuvImageRenderer(instance.surface,
        instance.yuvImageRenderers.size(), x, y, width, height, scalingType, mirror, drawer);
    synchronized (instance.yuvImageRenderers) {
      if (instance.onSurfaceCreatedCalled) {
        // onSurfaceCreated has already been called for VideoRendererGui -
        // need to create texture for new image and add image to the
        // rendering list.
        final CountDownLatch countDownLatch = new CountDownLatch(1);
        instance.surface.queueEvent(new Runnable() {
          @Override
          public void run() {
            yuvImageRenderer.createTextures();
            yuvImageRenderer.setScreenSize(instance.screenWidth, instance.screenHeight);
            countDownLatch.countDown();
          }
        });
        // Wait for task completion.
        try {
          countDownLatch.await();
        } catch (InterruptedException e) {
          throw new RuntimeException(e);
        }
      }
      // Add yuv renderer to rendering list.
      instance.yuvImageRenderers.add(yuvImageRenderer);
    }
    return yuvImageRenderer;
  }

  public static synchronized void update(VideoRenderer.Callbacks renderer, int x, int y, int width,
      int height, RendererCommon.ScalingType scalingType, boolean mirror) {
    Logging.d(TAG, "VideoRendererGui.update");
    if (instance == null) {
      throw new RuntimeException("Attempt to update yuv renderer before setting GLSurfaceView");
    }
    synchronized (instance.yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : instance.yuvImageRenderers) {
        if (yuvImageRenderer == renderer) {
          yuvImageRenderer.setPosition(x, y, width, height, scalingType, mirror);
        }
      }
    }
  }

  public static synchronized void setRendererEvents(
      VideoRenderer.Callbacks renderer, RendererCommon.RendererEvents rendererEvents) {
    Logging.d(TAG, "VideoRendererGui.setRendererEvents");
    if (instance == null) {
      throw new RuntimeException("Attempt to set renderer events before setting GLSurfaceView");
    }
    synchronized (instance.yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : instance.yuvImageRenderers) {
        if (yuvImageRenderer == renderer) {
          yuvImageRenderer.rendererEvents = rendererEvents;
        }
      }
    }
  }

  public static synchronized void remove(VideoRenderer.Callbacks renderer) {
    Logging.d(TAG, "VideoRendererGui.remove");
    if (instance == null) {
      throw new RuntimeException("Attempt to remove renderer before setting GLSurfaceView");
    }
    synchronized (instance.yuvImageRenderers) {
      final int index = instance.yuvImageRenderers.indexOf(renderer);
      if (index == -1) {
        Logging.w(TAG, "Couldn't remove renderer (not present in current list)");
      } else {
        instance.yuvImageRenderers.remove(index).release();
      }
    }
  }

  public static synchronized void reset(VideoRenderer.Callbacks renderer) {
    Logging.d(TAG, "VideoRendererGui.reset");
    if (instance == null) {
      throw new RuntimeException("Attempt to reset renderer before setting GLSurfaceView");
    }
    synchronized (instance.yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : instance.yuvImageRenderers) {
        if (yuvImageRenderer == renderer) {
          yuvImageRenderer.reset();
        }
      }
    }
  }

  private static void printStackTrace(Thread thread, String threadName) {
    if (thread != null) {
      StackTraceElement[] stackTraces = thread.getStackTrace();
      if (stackTraces.length > 0) {
        Logging.d(TAG, threadName + " stacks trace:");
        for (StackTraceElement stackTrace : stackTraces) {
          Logging.d(TAG, stackTrace.toString());
        }
      }
    }
  }

  public static synchronized void printStackTraces() {
    if (instance == null) {
      return;
    }
    printStackTrace(renderFrameThread, "Render frame thread");
    printStackTrace(drawThread, "Draw thread");
  }

  @SuppressLint("NewApi")
  @Override
  public void onSurfaceCreated(GL10 unused, EGLConfig config) {
    Logging.d(TAG, "VideoRendererGui.onSurfaceCreated");
    // Store render EGL context.
    synchronized (VideoRendererGui.class) {
      if (EglBase14.isEGL14Supported()) {
        eglContext = new EglBase14.Context(EGL14.eglGetCurrentContext());
      } else {
        eglContext = new EglBase10.Context(((EGL10) EGLContext.getEGL()).eglGetCurrentContext());
      }

      Logging.d(TAG, "VideoRendererGui EGL Context: " + eglContext);
    }

    synchronized (yuvImageRenderers) {
      // Create textures for all images.
      for (YuvImageRenderer yuvImageRenderer : yuvImageRenderers) {
        yuvImageRenderer.createTextures();
      }
      onSurfaceCreatedCalled = true;
    }
    GlUtil.checkNoGLES2Error("onSurfaceCreated done");
    GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
    GLES20.glClearColor(0.15f, 0.15f, 0.15f, 1.0f);

    // Fire EGL context ready event.
    synchronized (VideoRendererGui.class) {
      if (eglContextReady != null) {
        eglContextReady.run();
      }
    }
  }

  @Override
  public void onSurfaceChanged(GL10 unused, int width, int height) {
    Logging.d(TAG, "VideoRendererGui.onSurfaceChanged: " + width + " x " + height + "  ");
    screenWidth = width;
    screenHeight = height;
    synchronized (yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : yuvImageRenderers) {
        yuvImageRenderer.setScreenSize(screenWidth, screenHeight);
      }
    }
  }

  @Override
  public void onDrawFrame(GL10 unused) {
    if (drawThread == null) {
      drawThread = Thread.currentThread();
    }
    GLES20.glViewport(0, 0, screenWidth, screenHeight);
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    synchronized (yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : yuvImageRenderers) {
        yuvImageRenderer.draw();
      }
    }
  }
}
