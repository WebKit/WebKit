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

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

public class Camera1CapturerUsingTextureTest extends InstrumentationTestCase {
  static final String TAG = "Camera1CapturerUsingTextureTest";

  private class TestObjectFactory extends CameraVideoCapturerTestFixtures.TestObjectFactory {
    @Override
    public CameraEnumerator getCameraEnumerator() {
      return new Camera1Enumerator();
    }

    @Override
    public Context getAppContext() {
      return getInstrumentation().getTargetContext();
    }

    @SuppressWarnings("deprecation")
    @Override
    public Object rawOpenCamera(String cameraName) {
      return android.hardware.Camera.open(Camera1Enumerator.getCameraIndex(cameraName));
    }

    @SuppressWarnings("deprecation")
    @Override
    public void rawCloseCamera(Object camera) {
      ((android.hardware.Camera) camera).release();
    }
  }

  private CameraVideoCapturerTestFixtures fixtures;

  @Override
  protected void setUp() {
    fixtures = new CameraVideoCapturerTestFixtures(new TestObjectFactory());
  }

  @Override
  protected void tearDown() {
    fixtures.dispose();
  }

  @SmallTest
  public void testCreateAndDispose() throws InterruptedException {
    fixtures.createCapturerAndDispose();
  }

  @SmallTest
  public void testCreateNonExistingCamera() throws InterruptedException {
    fixtures.createNonExistingCamera();
  }

  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using a "default" capturer.
  // It tests both the Java and the C++ layer.
  @MediumTest
  public void testCreateCapturerAndRender() throws InterruptedException {
    fixtures.createCapturerAndRender();
  }

  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the front facing video capturer.
  // It tests both the Java and the C++ layer.
  @MediumTest
  public void testStartFrontFacingVideoCapturer() throws InterruptedException {
    fixtures.createFrontFacingCapturerAndRender();
  }

  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the back facing video capturer.
  // It tests both the Java and the C++ layer.
  @MediumTest
  public void testStartBackFacingVideoCapturer() throws InterruptedException {
    fixtures.createBackFacingCapturerAndRender();
  }

  // This test that the default camera can be started and that the camera can
  // later be switched to another camera.
  // It tests both the Java and the C++ layer.
  @MediumTest
  public void testSwitchVideoCapturer() throws InterruptedException {
    fixtures.switchCamera();
  }

  @MediumTest
  public void testCameraEvents() throws InterruptedException {
    fixtures.cameraEventsInvoked();
  }

  // Test what happens when attempting to call e.g. switchCamera() after camera has been stopped.
  @MediumTest
  public void testCameraCallsAfterStop() throws InterruptedException {
    fixtures.cameraCallsAfterStop();
  }

  // This test that the VideoSource that the CameraVideoCapturer is connected to can
  // be stopped and restarted. It tests both the Java and the C++ layer.
  @LargeTest
  public void testStopRestartVideoSource() throws InterruptedException {
    fixtures.stopRestartVideoSource();
  }

  // This test that the camera can be started at different resolutions.
  // It does not test or use the C++ layer.
  @LargeTest
  public void testStartStopWithDifferentResolutions() throws InterruptedException {
    fixtures.startStopWithDifferentResolutions();
  }

  // This test what happens if buffers are returned after the capturer have
  // been stopped and restarted. It does not test or use the C++ layer.
  @LargeTest
  public void testReturnBufferLate() throws InterruptedException {
    fixtures.returnBufferLate();
  }

  // This test that we can capture frames, keep the frames in a local renderer, stop capturing,
  // and then return the frames. The difference between the test testReturnBufferLate() is that we
  // also test the JNI and C++ AndroidVideoCapturer parts.
  @MediumTest
  public void testReturnBufferLateEndToEnd() throws InterruptedException {
    fixtures.returnBufferLateEndToEnd();
  }

  // This test that CameraEventsHandler.onError is triggered if video buffers are not returned to
  // the capturer.
  @LargeTest
  public void testCameraFreezedEventOnBufferStarvation() throws InterruptedException {
    fixtures.cameraFreezedEventOnBufferStarvation();
  }

  // This test that frames forwarded to a renderer is scaled if adaptOutputFormat is
  // called. This test both Java and C++ parts of of the stack.
  @MediumTest
  public void testScaleCameraOutput() throws InterruptedException {
    fixtures.scaleCameraOutput();
  }

  // This test that an error is reported if the camera is already opened
  // when CameraVideoCapturer is started.
  @LargeTest
  public void testStartWhileCameraIsAlreadyOpen() throws InterruptedException {
    fixtures.startWhileCameraIsAlreadyOpen();
  }

  // This test that CameraVideoCapturer can be started, even if the camera is already opened
  // if the camera is closed while CameraVideoCapturer is re-trying to start.
  @LargeTest
  public void testStartWhileCameraIsAlreadyOpenAndCloseCamera() throws InterruptedException {
    fixtures.startWhileCameraIsAlreadyOpenAndCloseCamera();
  }

  // This test that CameraVideoCapturer.stop can be called while CameraVideoCapturer is
  // re-trying to start.
  @MediumTest
  public void testStartWhileCameraIsAlreadyOpenAndStop() throws InterruptedException {
    fixtures.startWhileCameraIsAlreadyOpenAndStop();
  }
}
