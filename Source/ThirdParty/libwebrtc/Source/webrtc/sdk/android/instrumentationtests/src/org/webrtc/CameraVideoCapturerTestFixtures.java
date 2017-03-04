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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.runner.RunWith;
import org.webrtc.CameraEnumerationAndroid.CaptureFormat;
import org.webrtc.VideoRenderer.I420Frame;

class CameraVideoCapturerTestFixtures {
  static final String TAG = "CameraVideoCapturerTestFixtures";
  // Default values used for starting capturing
  static final int DEFAULT_WIDTH = 640;
  static final int DEFAULT_HEIGHT = 480;
  static final int DEFAULT_FPS = 15;

  static private class RendererCallbacks implements VideoRenderer.Callbacks {
    private int framesRendered = 0;
    private Object frameLock = 0;
    private int width = 0;
    private int height = 0;

    @Override
    public void renderFrame(I420Frame frame) {
      synchronized (frameLock) {
        ++framesRendered;
        width = frame.rotatedWidth();
        height = frame.rotatedHeight();
        frameLock.notify();
      }
      VideoRenderer.renderFrameDone(frame);
    }

    public int frameWidth() {
      synchronized (frameLock) {
        return width;
      }
    }

    public int frameHeight() {
      synchronized (frameLock) {
        return height;
      }
    }

    public int waitForNextFrameToRender() throws InterruptedException {
      Logging.d(TAG, "Waiting for the next frame to render");
      synchronized (frameLock) {
        frameLock.wait();
        return framesRendered;
      }
    }
  }

  static private class FakeAsyncRenderer implements VideoRenderer.Callbacks {
    private final List<I420Frame> pendingFrames = new ArrayList<I420Frame>();

    @Override
    public void renderFrame(I420Frame frame) {
      synchronized (pendingFrames) {
        pendingFrames.add(frame);
        pendingFrames.notifyAll();
      }
    }

    // Wait until at least one frame have been received, before returning them.
    public List<I420Frame> waitForPendingFrames() throws InterruptedException {
      Logging.d(TAG, "Waiting for pending frames");
      synchronized (pendingFrames) {
        while (pendingFrames.isEmpty()) {
          pendingFrames.wait();
        }
        return new ArrayList<I420Frame>(pendingFrames);
      }
    }
  }

  static private class FakeCapturerObserver implements CameraVideoCapturer.CapturerObserver {
    private int framesCaptured = 0;
    private int frameSize = 0;
    private int frameWidth = 0;
    private int frameHeight = 0;
    final private Object frameLock = new Object();
    final private Object capturerStartLock = new Object();
    private boolean capturerStartResult = false;
    final private List<Long> timestamps = new ArrayList<Long>();

    @Override
    public void onCapturerStarted(boolean success) {
      Logging.d(TAG, "onCapturerStarted: " + success);

      synchronized (capturerStartLock) {
        capturerStartResult = success;
        capturerStartLock.notifyAll();
      }
    }

    @Override
    public void onCapturerStopped() {
      Logging.d(TAG, "onCapturerStopped");
    }

    @Override
    public void onByteBufferFrameCaptured(
        byte[] frame, int width, int height, int rotation, long timeStamp) {
      synchronized (frameLock) {
        ++framesCaptured;
        frameSize = frame.length;
        frameWidth = width;
        frameHeight = height;
        timestamps.add(timeStamp);
        frameLock.notify();
      }
    }
    @Override
    public void onTextureFrameCaptured(int width, int height, int oesTextureId,
        float[] transformMatrix, int rotation, long timeStamp) {
      synchronized (frameLock) {
        ++framesCaptured;
        frameWidth = width;
        frameHeight = height;
        frameSize = 0;
        timestamps.add(timeStamp);
        frameLock.notify();
      }
    }

    public boolean waitForCapturerToStart() throws InterruptedException {
      Logging.d(TAG, "Waiting for the capturer to start");
      synchronized (capturerStartLock) {
        capturerStartLock.wait();
        return capturerStartResult;
      }
    }

    public int waitForNextCapturedFrame() throws InterruptedException {
      Logging.d(TAG, "Waiting for the next captured frame");
      synchronized (frameLock) {
        frameLock.wait();
        return framesCaptured;
      }
    }

    int frameSize() {
      synchronized (frameLock) {
        return frameSize;
      }
    }

    int frameWidth() {
      synchronized (frameLock) {
        return frameWidth;
      }
    }

    int frameHeight() {
      synchronized (frameLock) {
        return frameHeight;
      }
    }

    List<Long> getCopyAndResetListOftimeStamps() {
      synchronized (frameLock) {
        ArrayList<Long> list = new ArrayList<Long>(timestamps);
        timestamps.clear();
        return list;
      }
    }
  }

  static class CameraEvents implements CameraVideoCapturer.CameraEventsHandler {
    public boolean onCameraOpeningCalled;
    public boolean onFirstFrameAvailableCalled;
    public final Object onCameraFreezedLock = new Object();
    private String onCameraFreezedDescription;
    public final Object cameraClosedLock = new Object();
    private boolean cameraClosed = true;

    @Override
    public void onCameraError(String errorDescription) {
      Logging.w(TAG, "Camera error: " + errorDescription);
      cameraClosed = true;
    }

    @Override
    public void onCameraDisconnected() {}

    @Override
    public void onCameraFreezed(String errorDescription) {
      synchronized (onCameraFreezedLock) {
        onCameraFreezedDescription = errorDescription;
        onCameraFreezedLock.notifyAll();
      }
    }

    @Override
    public void onCameraOpening(String cameraName) {
      onCameraOpeningCalled = true;
      synchronized (cameraClosedLock) {
        cameraClosed = false;
      }
    }

    @Override
    public void onFirstFrameAvailable() {
      onFirstFrameAvailableCalled = true;
    }

    @Override
    public void onCameraClosed() {
      synchronized (cameraClosedLock) {
        cameraClosed = true;
        cameraClosedLock.notifyAll();
      }
    }

    public String waitForCameraFreezed() throws InterruptedException {
      Logging.d(TAG, "Waiting for the camera to freeze");
      synchronized (onCameraFreezedLock) {
        onCameraFreezedLock.wait();
        return onCameraFreezedDescription;
      }
    }

    public void waitForCameraClosed() throws InterruptedException {
      synchronized (cameraClosedLock) {
        while (!cameraClosed) {
          Logging.d(TAG, "Waiting for the camera to close.");
          cameraClosedLock.wait();
        }
      }
    }
  }

  /**
   * Class to collect all classes related to single capturer instance.
   */
  static private class CapturerInstance {
    public CameraVideoCapturer capturer;
    public CameraEvents cameraEvents;
    public SurfaceTextureHelper surfaceTextureHelper;
    public FakeCapturerObserver observer;
    public List<CaptureFormat> supportedFormats;
    public CaptureFormat format;
  }

  /**
   * Class used for collecting a VideoSource, a VideoTrack and a renderer. The class
   * is used for testing local rendering from a capturer.
   */
  static private class VideoTrackWithRenderer {
    public VideoSource source;
    public VideoTrack track;
    public RendererCallbacks rendererCallbacks;
    public FakeAsyncRenderer fakeAsyncRenderer;
  }

  public abstract static class TestObjectFactory {
    final CameraEnumerator cameraEnumerator;

    TestObjectFactory() {
      cameraEnumerator = getCameraEnumerator();
    }

    public CameraVideoCapturer createCapturer(
        String name, CameraVideoCapturer.CameraEventsHandler eventsHandler) {
      return cameraEnumerator.createCapturer(name, eventsHandler);
    }

    public String getNameOfFrontFacingDevice() {
      for (String deviceName : cameraEnumerator.getDeviceNames()) {
        if (cameraEnumerator.isFrontFacing(deviceName)) {
          return deviceName;
        }
      }

      return null;
    }

    public String getNameOfBackFacingDevice() {
      for (String deviceName : cameraEnumerator.getDeviceNames()) {
        if (cameraEnumerator.isBackFacing(deviceName)) {
          return deviceName;
        }
      }

      return null;
    }

    public boolean haveTwoCameras() {
      return cameraEnumerator.getDeviceNames().length >= 2;
    }

    public boolean isCapturingToTexture() {
      // In the future, we plan to only support capturing to texture, so default to true
      return true;
    }

    abstract public CameraEnumerator getCameraEnumerator();
    abstract public Context getAppContext();

    // CameraVideoCapturer API is too slow for some of our tests where we need to open a competing
    // camera. These methods are used instead.
    abstract public Object rawOpenCamera(String cameraName);
    abstract public void rawCloseCamera(Object camera);
  }

  private PeerConnectionFactory peerConnectionFactory;
  private TestObjectFactory testObjectFactory;

  CameraVideoCapturerTestFixtures(TestObjectFactory testObjectFactory) {
    PeerConnectionFactory.initializeAndroidGlobals(
        testObjectFactory.getAppContext(), true, true, true);

    this.peerConnectionFactory = new PeerConnectionFactory(null /* options */);
    this.testObjectFactory = testObjectFactory;
  }

  public void dispose() {
    this.peerConnectionFactory.dispose();
  }

  // Internal helper methods
  private CapturerInstance createCapturer(String name, boolean initialize) {
    CapturerInstance instance = new CapturerInstance();
    instance.cameraEvents = new CameraEvents();
    instance.capturer = testObjectFactory.createCapturer(name, instance.cameraEvents);
    instance.surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, null /* sharedContext */);
    instance.observer = new FakeCapturerObserver();
    if (initialize) {
      instance.capturer.initialize(
          instance.surfaceTextureHelper, testObjectFactory.getAppContext(), instance.observer);
    }
    instance.supportedFormats = testObjectFactory.cameraEnumerator.getSupportedFormats(name);
    return instance;
  }

  private CapturerInstance createCapturer(boolean initialize) {
    String name = testObjectFactory.cameraEnumerator.getDeviceNames()[0];
    return createCapturer(name, initialize);
  }

  private void startCapture(CapturerInstance instance) {
    startCapture(instance, 0);
  }

  private void startCapture(CapturerInstance instance, int formatIndex) {
    final CameraEnumerationAndroid.CaptureFormat format =
        instance.supportedFormats.get(formatIndex);

    instance.capturer.startCapture(format.width, format.height, format.framerate.max);
    instance.format = format;
  }

  private void disposeCapturer(CapturerInstance instance) throws InterruptedException {
    instance.capturer.stopCapture();
    instance.cameraEvents.waitForCameraClosed();
    instance.capturer.dispose();
    instance.surfaceTextureHelper.returnTextureFrame();
    instance.surfaceTextureHelper.dispose();
  }

  private VideoTrackWithRenderer createVideoTrackWithRenderer(
      CameraVideoCapturer capturer, VideoRenderer.Callbacks rendererCallbacks) {
    VideoTrackWithRenderer videoTrackWithRenderer = new VideoTrackWithRenderer();
    videoTrackWithRenderer.source = peerConnectionFactory.createVideoSource(capturer);
    capturer.startCapture(DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FPS);
    videoTrackWithRenderer.track =
        peerConnectionFactory.createVideoTrack("dummy", videoTrackWithRenderer.source);
    videoTrackWithRenderer.track.addRenderer(new VideoRenderer(rendererCallbacks));
    return videoTrackWithRenderer;
  }

  private VideoTrackWithRenderer createVideoTrackWithRenderer(CameraVideoCapturer capturer) {
    RendererCallbacks rendererCallbacks = new RendererCallbacks();
    VideoTrackWithRenderer videoTrackWithRenderer =
        createVideoTrackWithRenderer(capturer, rendererCallbacks);
    videoTrackWithRenderer.rendererCallbacks = rendererCallbacks;
    return videoTrackWithRenderer;
  }

  private VideoTrackWithRenderer createVideoTrackWithFakeAsyncRenderer(
      CameraVideoCapturer capturer) {
    FakeAsyncRenderer fakeAsyncRenderer = new FakeAsyncRenderer();
    VideoTrackWithRenderer videoTrackWithRenderer =
        createVideoTrackWithRenderer(capturer, fakeAsyncRenderer);
    videoTrackWithRenderer.fakeAsyncRenderer = fakeAsyncRenderer;
    return videoTrackWithRenderer;
  }

  private void disposeVideoTrackWithRenderer(VideoTrackWithRenderer videoTrackWithRenderer) {
    videoTrackWithRenderer.track.dispose();
    videoTrackWithRenderer.source.dispose();
  }

  private void waitUntilIdle(CapturerInstance capturerInstance) throws InterruptedException {
    final CountDownLatch barrier = new CountDownLatch(1);
    capturerInstance.surfaceTextureHelper.getHandler().post(new Runnable() {
      @Override
      public void run() {
        barrier.countDown();
      }
    });
    barrier.await();
  }

  private void createCapturerAndRender(String name) throws InterruptedException {
    if (name == null) {
      Logging.w(TAG, "Skipping video capturer test because device name is null.");
      return;
    }

    final CapturerInstance capturerInstance = createCapturer(name, false /* initialize */);
    final VideoTrackWithRenderer videoTrackWithRenderer =
        createVideoTrackWithRenderer(capturerInstance.capturer);
    assertTrue(videoTrackWithRenderer.rendererCallbacks.waitForNextFrameToRender() > 0);
    disposeCapturer(capturerInstance);
    disposeVideoTrackWithRenderer(videoTrackWithRenderer);
  }

  // Test methods
  public void createCapturerAndDispose() throws InterruptedException {
    disposeCapturer(createCapturer(true /* initialize */));
  }

  public void createNonExistingCamera() throws InterruptedException {
    try {
      disposeCapturer(createCapturer("non-existing camera", false /* initialize */));
    } catch (IllegalArgumentException e) {
      return;
    }

    fail("Expected illegal argument exception when creating non-existing camera.");
  }

  public void createCapturerAndRender() throws InterruptedException {
    String name = testObjectFactory.cameraEnumerator.getDeviceNames()[0];
    createCapturerAndRender(name);
  }

  public void createFrontFacingCapturerAndRender() throws InterruptedException {
    createCapturerAndRender(testObjectFactory.getNameOfFrontFacingDevice());
  }

  public void createBackFacingCapturerAndRender() throws InterruptedException {
    createCapturerAndRender(testObjectFactory.getNameOfBackFacingDevice());
  }

  public void switchCamera() throws InterruptedException {
    if (!testObjectFactory.haveTwoCameras()) {
      Logging.w(
          TAG, "Skipping test switch video capturer because the device doesn't have two cameras.");
      return;
    }

    final CapturerInstance capturerInstance = createCapturer(false /* initialize */);
    final VideoTrackWithRenderer videoTrackWithRenderer =
        createVideoTrackWithRenderer(capturerInstance.capturer);
    // Wait for the camera to start so we can switch it
    assertTrue(videoTrackWithRenderer.rendererCallbacks.waitForNextFrameToRender() > 0);

    // Array with one element to avoid final problem in nested classes.
    final boolean[] cameraSwitchSuccessful = new boolean[1];
    final CountDownLatch barrier = new CountDownLatch(1);
    capturerInstance.capturer.switchCamera(new CameraVideoCapturer.CameraSwitchHandler() {
      @Override
      public void onCameraSwitchDone(boolean isFrontCamera) {
        cameraSwitchSuccessful[0] = true;
        barrier.countDown();
      }
      @Override
      public void onCameraSwitchError(String errorDescription) {
        cameraSwitchSuccessful[0] = false;
        barrier.countDown();
      }
    });
    // Wait until the camera has been switched.
    barrier.await();

    // Check result.
    assertTrue(cameraSwitchSuccessful[0]);
    // Ensure that frames are received.
    assertTrue(videoTrackWithRenderer.rendererCallbacks.waitForNextFrameToRender() > 0);
    disposeCapturer(capturerInstance);
    disposeVideoTrackWithRenderer(videoTrackWithRenderer);
  }

  public void cameraEventsInvoked() throws InterruptedException {
    final CapturerInstance capturerInstance = createCapturer(true /* initialize */);
    startCapture(capturerInstance);
    // Make sure camera is started and first frame is received and then stop it.
    assertTrue(capturerInstance.observer.waitForCapturerToStart());
    capturerInstance.observer.waitForNextCapturedFrame();
    disposeCapturer(capturerInstance);

    assertTrue(capturerInstance.cameraEvents.onCameraOpeningCalled);
    assertTrue(capturerInstance.cameraEvents.onFirstFrameAvailableCalled);
  }

  public void cameraCallsAfterStop() throws InterruptedException {
    final CapturerInstance capturerInstance = createCapturer(true /* initialize */);
    startCapture(capturerInstance);
    // Make sure camera is started and then stop it.
    assertTrue(capturerInstance.observer.waitForCapturerToStart());
    capturerInstance.capturer.stopCapture();
    capturerInstance.surfaceTextureHelper.returnTextureFrame();

    // We can't change |capturer| at this point, but we should not crash.
    capturerInstance.capturer.switchCamera(null /* switchEventsHandler */);
    capturerInstance.capturer.changeCaptureFormat(DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FPS);

    disposeCapturer(capturerInstance);
  }

  public void stopRestartVideoSource() throws InterruptedException {
    final CapturerInstance capturerInstance = createCapturer(false /* initialize */);
    final VideoTrackWithRenderer videoTrackWithRenderer =
        createVideoTrackWithRenderer(capturerInstance.capturer);

    assertTrue(videoTrackWithRenderer.rendererCallbacks.waitForNextFrameToRender() > 0);
    assertEquals(MediaSource.State.LIVE, videoTrackWithRenderer.source.state());

    capturerInstance.capturer.stopCapture();
    assertEquals(MediaSource.State.ENDED, videoTrackWithRenderer.source.state());

    startCapture(capturerInstance);
    assertTrue(videoTrackWithRenderer.rendererCallbacks.waitForNextFrameToRender() > 0);
    assertEquals(MediaSource.State.LIVE, videoTrackWithRenderer.source.state());

    disposeCapturer(capturerInstance);
    disposeVideoTrackWithRenderer(videoTrackWithRenderer);
  }

  public void startStopWithDifferentResolutions() throws InterruptedException {
    final CapturerInstance capturerInstance = createCapturer(true /* initialize */);

    for (int i = 0; i < 3; ++i) {
      startCapture(capturerInstance, i);
      assertTrue(capturerInstance.observer.waitForCapturerToStart());
      capturerInstance.observer.waitForNextCapturedFrame();

      // Check the frame size. The actual width and height depend on how the capturer is mounted.
      final boolean identicalResolution =
          (capturerInstance.observer.frameWidth() == capturerInstance.format.width
              && capturerInstance.observer.frameHeight() == capturerInstance.format.height);
      final boolean flippedResolution =
          (capturerInstance.observer.frameWidth() == capturerInstance.format.height
              && capturerInstance.observer.frameHeight() == capturerInstance.format.width);
      if (!identicalResolution && !flippedResolution) {
        fail("Wrong resolution, got: " + capturerInstance.observer.frameWidth() + "x"
            + capturerInstance.observer.frameHeight() + " expected: "
            + capturerInstance.format.width + "x" + capturerInstance.format.height + " or "
            + capturerInstance.format.height + "x" + capturerInstance.format.width);
      }

      if (testObjectFactory.isCapturingToTexture()) {
        assertEquals(0, capturerInstance.observer.frameSize());
      } else {
        assertTrue(capturerInstance.format.frameSize() <= capturerInstance.observer.frameSize());
      }
      capturerInstance.capturer.stopCapture();
      capturerInstance.surfaceTextureHelper.returnTextureFrame();
    }
    disposeCapturer(capturerInstance);
  }

  public void returnBufferLate() throws InterruptedException {
    final CapturerInstance capturerInstance = createCapturer(true /* initialize */);
    startCapture(capturerInstance);
    assertTrue(capturerInstance.observer.waitForCapturerToStart());

    capturerInstance.observer.waitForNextCapturedFrame();
    capturerInstance.capturer.stopCapture();
    List<Long> listOftimestamps = capturerInstance.observer.getCopyAndResetListOftimeStamps();
    assertTrue(listOftimestamps.size() >= 1);

    startCapture(capturerInstance, 1);
    capturerInstance.observer.waitForCapturerToStart();
    capturerInstance.surfaceTextureHelper.returnTextureFrame();

    capturerInstance.observer.waitForNextCapturedFrame();
    capturerInstance.capturer.stopCapture();

    listOftimestamps = capturerInstance.observer.getCopyAndResetListOftimeStamps();
    assertTrue(listOftimestamps.size() >= 1);

    disposeCapturer(capturerInstance);
  }

  public void returnBufferLateEndToEnd() throws InterruptedException {
    final CapturerInstance capturerInstance = createCapturer(false /* initialize */);
    final VideoTrackWithRenderer videoTrackWithRenderer =
        createVideoTrackWithFakeAsyncRenderer(capturerInstance.capturer);
    // Wait for at least one frame that has not been returned.
    assertFalse(videoTrackWithRenderer.fakeAsyncRenderer.waitForPendingFrames().isEmpty());

    capturerInstance.capturer.stopCapture();

    // Dispose everything.
    disposeCapturer(capturerInstance);
    disposeVideoTrackWithRenderer(videoTrackWithRenderer);

    // Return the frame(s), on a different thread out of spite.
    final List<I420Frame> pendingFrames =
        videoTrackWithRenderer.fakeAsyncRenderer.waitForPendingFrames();
    final Thread returnThread = new Thread(new Runnable() {
      @Override
      public void run() {
        for (I420Frame frame : pendingFrames) {
          VideoRenderer.renderFrameDone(frame);
        }
      }
    });
    returnThread.start();
    returnThread.join();
  }

  public void cameraFreezedEventOnBufferStarvation() throws InterruptedException {
    final CapturerInstance capturerInstance = createCapturer(true /* initialize */);
    startCapture(capturerInstance);
    // Make sure camera is started.
    assertTrue(capturerInstance.observer.waitForCapturerToStart());
    // Since we don't return the buffer, we should get a starvation message if we are
    // capturing to a texture.
    assertEquals("Camera failure. Client must return video buffers.",
        capturerInstance.cameraEvents.waitForCameraFreezed());

    capturerInstance.capturer.stopCapture();
    disposeCapturer(capturerInstance);
  }

  public void scaleCameraOutput() throws InterruptedException {
    final CapturerInstance capturerInstance = createCapturer(false /* initialize */);
    final VideoTrackWithRenderer videoTrackWithRenderer =
        createVideoTrackWithRenderer(capturerInstance.capturer);
    assertTrue(videoTrackWithRenderer.rendererCallbacks.waitForNextFrameToRender() > 0);

    final int startWidth = videoTrackWithRenderer.rendererCallbacks.frameWidth();
    final int startHeight = videoTrackWithRenderer.rendererCallbacks.frameHeight();
    final int frameRate = 30;
    final int scaledWidth = startWidth / 2;
    final int scaledHeight = startHeight / 2;

    // Request the captured frames to be scaled.
    videoTrackWithRenderer.source.adaptOutputFormat(scaledWidth, scaledHeight, frameRate);

    boolean gotExpectedResolution = false;
    int numberOfInspectedFrames = 0;

    do {
      videoTrackWithRenderer.rendererCallbacks.waitForNextFrameToRender();
      ++numberOfInspectedFrames;

      gotExpectedResolution = (videoTrackWithRenderer.rendererCallbacks.frameWidth() == scaledWidth
          && videoTrackWithRenderer.rendererCallbacks.frameHeight() == scaledHeight);
    } while (!gotExpectedResolution && numberOfInspectedFrames < 30);

    disposeCapturer(capturerInstance);
    disposeVideoTrackWithRenderer(videoTrackWithRenderer);

    assertTrue(gotExpectedResolution);
  }

  public void startWhileCameraIsAlreadyOpen() throws InterruptedException {
    final String cameraName = testObjectFactory.getNameOfBackFacingDevice();
    // At this point camera is not actually opened.
    final CapturerInstance capturerInstance = createCapturer(cameraName, true /* initialize */);

    final Object competingCamera = testObjectFactory.rawOpenCamera(cameraName);

    startCapture(capturerInstance);

    if (android.os.Build.VERSION.SDK_INT > android.os.Build.VERSION_CODES.LOLLIPOP_MR1) {
      // The first opened camera client will be evicted.
      assertTrue(capturerInstance.observer.waitForCapturerToStart());
    } else {
      assertFalse(capturerInstance.observer.waitForCapturerToStart());
    }

    testObjectFactory.rawCloseCamera(competingCamera);
    disposeCapturer(capturerInstance);
  }

  public void startWhileCameraIsAlreadyOpenAndCloseCamera() throws InterruptedException {
    final String cameraName = testObjectFactory.getNameOfBackFacingDevice();
    // At this point camera is not actually opened.
    final CapturerInstance capturerInstance = createCapturer(cameraName, false /* initialize */);

    Logging.d(TAG, "startWhileCameraIsAlreadyOpenAndCloseCamera: Opening competing camera.");
    final Object competingCamera = testObjectFactory.rawOpenCamera(cameraName);

    Logging.d(TAG, "startWhileCameraIsAlreadyOpenAndCloseCamera: Opening camera.");
    final VideoTrackWithRenderer videoTrackWithRenderer =
        createVideoTrackWithRenderer(capturerInstance.capturer);
    waitUntilIdle(capturerInstance);

    Logging.d(TAG, "startWhileCameraIsAlreadyOpenAndCloseCamera: Closing competing camera.");
    testObjectFactory.rawCloseCamera(competingCamera);

    // Make sure camera is started and first frame is received and then stop it.
    Logging.d(TAG, "startWhileCameraIsAlreadyOpenAndCloseCamera: Waiting for capture to start.");
    videoTrackWithRenderer.rendererCallbacks.waitForNextFrameToRender();
    Logging.d(TAG, "startWhileCameraIsAlreadyOpenAndCloseCamera: Stopping capture.");
    disposeCapturer(capturerInstance);
  }

  public void startWhileCameraIsAlreadyOpenAndStop() throws InterruptedException {
    final String cameraName = testObjectFactory.getNameOfBackFacingDevice();
    // At this point camera is not actually opened.
    final CapturerInstance capturerInstance = createCapturer(cameraName, true /* initialize */);

    final Object competingCamera = testObjectFactory.rawOpenCamera(cameraName);

    startCapture(capturerInstance);
    disposeCapturer(capturerInstance);

    testObjectFactory.rawCloseCamera(competingCamera);
  }
}
