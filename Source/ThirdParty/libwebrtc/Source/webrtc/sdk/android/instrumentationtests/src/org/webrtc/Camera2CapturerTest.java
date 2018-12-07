/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
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
import static org.junit.Assert.fail;

import android.annotation.TargetApi;
import android.content.Context;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.os.Handler;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import javax.annotation.Nullable;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@TargetApi(21)
@RunWith(BaseJUnit4ClassRunner.class)
public class Camera2CapturerTest {
  static final String TAG = "Camera2CapturerTest";

  /**
   * Simple camera2 implementation that only knows how to open the camera and close it.
   */
  private class SimpleCamera2 {
    final CameraManager cameraManager;
    final LooperThread looperThread;
    final CountDownLatch openDoneSignal;
    final Object cameraDeviceLock;
    @Nullable CameraDevice cameraDevice; // Guarded by cameraDeviceLock
    boolean openSucceeded; // Guarded by cameraDeviceLock

    private class LooperThread extends Thread {
      final CountDownLatch startedSignal = new CountDownLatch(1);
      private Handler handler;

      @Override
      public void run() {
        Looper.prepare();
        handler = new Handler();
        startedSignal.countDown();
        Looper.loop();
      }

      public void waitToStart() {
        ThreadUtils.awaitUninterruptibly(startedSignal);
      }

      public void requestStop() {
        handler.getLooper().quit();
      }

      public Handler getHandler() {
        return handler;
      }
    }

    private class CameraStateCallback extends CameraDevice.StateCallback {
      @Override
      public void onClosed(CameraDevice cameraDevice) {
        Logging.d(TAG, "Simple camera2 closed.");

        synchronized (cameraDeviceLock) {
          SimpleCamera2.this.cameraDevice = null;
        }
      }

      @Override
      public void onDisconnected(CameraDevice cameraDevice) {
        Logging.d(TAG, "Simple camera2 disconnected.");

        synchronized (cameraDeviceLock) {
          SimpleCamera2.this.cameraDevice = null;
        }
      }

      @Override
      public void onError(CameraDevice cameraDevice, int errorCode) {
        Logging.w(TAG, "Simple camera2 error: " + errorCode);

        synchronized (cameraDeviceLock) {
          SimpleCamera2.this.cameraDevice = cameraDevice;
          openSucceeded = false;
        }

        openDoneSignal.countDown();
      }

      @Override
      public void onOpened(CameraDevice cameraDevice) {
        Logging.d(TAG, "Simple camera2 opened.");

        synchronized (cameraDeviceLock) {
          SimpleCamera2.this.cameraDevice = cameraDevice;
          openSucceeded = true;
        }

        openDoneSignal.countDown();
      }
    }

    SimpleCamera2(Context context, String deviceName) {
      cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
      looperThread = new LooperThread();
      looperThread.start();
      looperThread.waitToStart();
      cameraDeviceLock = new Object();
      openDoneSignal = new CountDownLatch(1);
      cameraDevice = null;
      Logging.d(TAG, "Opening simple camera2.");
      try {
        cameraManager.openCamera(deviceName, new CameraStateCallback(), looperThread.getHandler());
      } catch (CameraAccessException e) {
        fail("Simple camera2 CameraAccessException: " + e.getMessage());
      }

      Logging.d(TAG, "Waiting for simple camera2 to open.");
      ThreadUtils.awaitUninterruptibly(openDoneSignal);
      synchronized (cameraDeviceLock) {
        if (!openSucceeded) {
          fail("Opening simple camera2 failed.");
        }
      }
    }

    public void close() {
      Logging.d(TAG, "Closing simple camera2.");
      synchronized (cameraDeviceLock) {
        if (cameraDevice != null) {
          cameraDevice.close();
        }
      }

      looperThread.requestStop();
      ThreadUtils.joinUninterruptibly(looperThread);
    }
  }

  private class TestObjectFactory extends CameraVideoCapturerTestFixtures.TestObjectFactory {
    @Override
    public CameraEnumerator getCameraEnumerator() {
      return new Camera2Enumerator(getAppContext());
    }

    @Override
    public Context getAppContext() {
      return InstrumentationRegistry.getTargetContext();
    }

    @SuppressWarnings("deprecation")
    @Override
    public Object rawOpenCamera(String cameraName) {
      return new SimpleCamera2(getAppContext(), cameraName);
    }

    @SuppressWarnings("deprecation")
    @Override
    public void rawCloseCamera(Object camera) {
      ((SimpleCamera2) camera).close();
    }
  }

  private CameraVideoCapturerTestFixtures fixtures;

  @Before
  public void setUp() {
    fixtures = new CameraVideoCapturerTestFixtures(new TestObjectFactory());
  }

  @After
  public void tearDown() {
    fixtures.dispose();
  }

  @Test
  @SmallTest
  public void testCreateAndDispose() throws InterruptedException {
    fixtures.createCapturerAndDispose();
  }

  @Test
  @SmallTest
  public void testCreateNonExistingCamera() throws InterruptedException {
    fixtures.createNonExistingCamera();
  }

  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using a "default" capturer.
  // It tests both the Java and the C++ layer.
  @Test
  @MediumTest
  public void testCreateCapturerAndRender() throws InterruptedException {
    fixtures.createCapturerAndRender();
  }

  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the front facing video capturer.
  // It tests both the Java and the C++ layer.
  @Test
  @MediumTest
  public void testStartFrontFacingVideoCapturer() throws InterruptedException {
    fixtures.createFrontFacingCapturerAndRender();
  }

  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the back facing video capturer.
  // It tests both the Java and the C++ layer.
  @Test
  @MediumTest
  public void testStartBackFacingVideoCapturer() throws InterruptedException {
    fixtures.createBackFacingCapturerAndRender();
  }

  // This test that the default camera can be started and that the camera can
  // later be switched to another camera.
  // It tests both the Java and the C++ layer.
  @Test
  @MediumTest
  public void testSwitchVideoCapturer() throws InterruptedException {
    fixtures.switchCamera();
  }

  @Test
  @MediumTest
  public void testCameraEvents() throws InterruptedException {
    fixtures.cameraEventsInvoked();
  }

  // Test what happens when attempting to call e.g. switchCamera() after camera has been stopped.
  @Test
  @MediumTest
  public void testCameraCallsAfterStop() throws InterruptedException {
    fixtures.cameraCallsAfterStop();
  }

  // This test that the VideoSource that the CameraVideoCapturer is connected to can
  // be stopped and restarted. It tests both the Java and the C++ layer.
  @Test
  @LargeTest
  public void testStopRestartVideoSource() throws InterruptedException {
    fixtures.stopRestartVideoSource();
  }

  // This test that the camera can be started at different resolutions.
  // It does not test or use the C++ layer.
  @Test
  @LargeTest
  public void testStartStopWithDifferentResolutions() throws InterruptedException {
    fixtures.startStopWithDifferentResolutions();
  }

  // This test what happens if buffers are returned after the capturer have
  // been stopped and restarted. It does not test or use the C++ layer.
  @Test
  @LargeTest
  public void testReturnBufferLate() throws InterruptedException {
    fixtures.returnBufferLate();
  }

  // This test that we can capture frames, keep the frames in a local renderer, stop capturing,
  // and then return the frames. The difference between the test testReturnBufferLate() is that we
  // also test the JNI and C++ AndroidVideoCapturer parts.
  @Test
  @MediumTest
  public void testReturnBufferLateEndToEnd() throws InterruptedException {
    fixtures.returnBufferLateEndToEnd();
  }

  // This test that CameraEventsHandler.onError is triggered if video buffers are not returned to
  // the capturer.
  @Test
  @LargeTest
  public void testCameraFreezedEventOnBufferStarvation() throws InterruptedException {
    fixtures.cameraFreezedEventOnBufferStarvation();
  }

  // This test that frames forwarded to a renderer is scaled if adaptOutputFormat is
  // called. This test both Java and C++ parts of of the stack.
  @Test
  @MediumTest
  public void testScaleCameraOutput() throws InterruptedException {
    fixtures.scaleCameraOutput();
  }

  // This test that frames forwarded to a renderer is cropped to a new orientation if
  // adaptOutputFormat is called in such a way. This test both Java and C++ parts of of the stack.
  @Test
  @MediumTest
  public void testCropCameraOutput() throws InterruptedException {
    fixtures.cropCameraOutput();
  }

  // This test that an error is reported if the camera is already opened
  // when CameraVideoCapturer is started.
  @Test
  @LargeTest
  public void testStartWhileCameraIsAlreadyOpen() throws InterruptedException {
    fixtures.startWhileCameraIsAlreadyOpen();
  }

  // This test that CameraVideoCapturer can be started, even if the camera is already opened
  // if the camera is closed while CameraVideoCapturer is re-trying to start.
  @Test
  @LargeTest
  public void testStartWhileCameraIsAlreadyOpenAndCloseCamera() throws InterruptedException {
    fixtures.startWhileCameraIsAlreadyOpenAndCloseCamera();
  }

  // This test that CameraVideoCapturer.stop can be called while CameraVideoCapturer is
  // re-trying to start.
  @Test
  @MediumTest
  public void testStartWhileCameraIsAlreadyOpenAndStop() throws InterruptedException {
    fixtures.startWhileCameraIsAlreadyOpenAndStop();
  }
}
