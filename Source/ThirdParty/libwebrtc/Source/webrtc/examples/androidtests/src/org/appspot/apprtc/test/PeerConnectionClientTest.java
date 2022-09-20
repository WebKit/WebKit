/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc.test;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.util.Log;
import androidx.test.filters.SmallTest;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import org.appspot.apprtc.AppRTCClient.SignalingParameters;
import org.appspot.apprtc.PeerConnectionClient;
import org.appspot.apprtc.PeerConnectionClient.PeerConnectionEvents;
import org.appspot.apprtc.PeerConnectionClient.PeerConnectionParameters;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.Camera1Enumerator;
import org.webrtc.Camera2Enumerator;
import org.webrtc.CameraEnumerator;
import org.webrtc.EglBase;
import org.webrtc.IceCandidate;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.RTCStatsReport;
import org.webrtc.SessionDescription;
import org.webrtc.VideoCapturer;
import org.webrtc.VideoFrame;
import org.webrtc.VideoSink;

@RunWith(AndroidJUnit4.class)
public class PeerConnectionClientTest implements PeerConnectionEvents {
  private static final String TAG = "RTCClientTest";
  private static final int ICE_CONNECTION_WAIT_TIMEOUT = 10000;
  private static final int WAIT_TIMEOUT = 7000;
  private static final int CAMERA_SWITCH_ATTEMPTS = 3;
  private static final int VIDEO_RESTART_ATTEMPTS = 3;
  private static final int CAPTURE_FORMAT_CHANGE_ATTEMPTS = 3;
  private static final int VIDEO_RESTART_TIMEOUT = 500;
  private static final int EXPECTED_VIDEO_FRAMES = 10;
  private static final String VIDEO_CODEC_VP8 = "VP8";
  private static final String VIDEO_CODEC_VP9 = "VP9";
  private static final String VIDEO_CODEC_H264 = "H264";
  private static final int AUDIO_RUN_TIMEOUT = 1000;
  private static final String LOCAL_RENDERER_NAME = "Local renderer";
  private static final String REMOTE_RENDERER_NAME = "Remote renderer";

  private static final int MAX_VIDEO_FPS = 30;
  private static final int WIDTH_VGA = 640;
  private static final int HEIGHT_VGA = 480;
  private static final int WIDTH_QVGA = 320;
  private static final int HEIGHT_QVGA = 240;

  // The peer connection client is assumed to be thread safe in itself; the
  // reference is written by the test thread and read by worker threads.
  private volatile PeerConnectionClient pcClient;
  private volatile boolean loopback;

  // These are protected by their respective event objects.
  private ExecutorService signalingExecutor;
  private boolean isClosed;
  private boolean isIceConnected;
  private SessionDescription localDesc;
  private List<IceCandidate> iceCandidates = new ArrayList<>();
  private final Object localDescEvent = new Object();
  private final Object iceCandidateEvent = new Object();
  private final Object iceConnectedEvent = new Object();
  private final Object closeEvent = new Object();

  // Mock VideoSink implementation.
  private static class MockSink implements VideoSink {
    // These are protected by 'this' since we gets called from worker threads.
    private String rendererName;
    private boolean renderFrameCalled;

    // Thread-safe in itself.
    private CountDownLatch doneRendering;

    public MockSink(int expectedFrames, String rendererName) {
      this.rendererName = rendererName;
      reset(expectedFrames);
    }

    // Resets render to wait for new amount of video frames.
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void reset(int expectedFrames) {
      renderFrameCalled = false;
      doneRendering = new CountDownLatch(expectedFrames);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onFrame(VideoFrame frame) {
      if (!renderFrameCalled) {
        if (rendererName != null) {
          Log.d(TAG,
              rendererName + " render frame: " + frame.getRotatedWidth() + " x "
                  + frame.getRotatedHeight());
        } else {
          Log.d(TAG, "Render frame: " + frame.getRotatedWidth() + " x " + frame.getRotatedHeight());
        }
      }
      renderFrameCalled = true;
      doneRendering.countDown();
    }

    // This method shouldn't hold any locks or touch member variables since it
    // blocks.
    public boolean waitForFramesRendered(int timeoutMs) throws InterruptedException {
      doneRendering.await(timeoutMs, TimeUnit.MILLISECONDS);
      return (doneRendering.getCount() <= 0);
    }
  }

  // Peer connection events implementation.
  @Override
  public void onLocalDescription(SessionDescription desc) {
    Log.d(TAG, "Local description type: " + desc.type);
    synchronized (localDescEvent) {
      localDesc = desc;
      localDescEvent.notifyAll();
    }
  }

  @Override
  public void onIceCandidate(final IceCandidate candidate) {
    synchronized (iceCandidateEvent) {
      Log.d(TAG, "IceCandidate #" + iceCandidates.size() + " : " + candidate.toString());
      if (loopback) {
        // Loopback local ICE candidate in a separate thread to avoid adding
        // remote ICE candidate in a local ICE candidate callback.
        signalingExecutor.execute(new Runnable() {
          @Override
          public void run() {
            pcClient.addRemoteIceCandidate(candidate);
          }
        });
      }
      iceCandidates.add(candidate);
      iceCandidateEvent.notifyAll();
    }
  }

  @Override
  public void onIceCandidatesRemoved(final IceCandidate[] candidates) {
    // TODO(honghaiz): Add this for tests.
  }

  @Override
  public void onIceConnected() {
    Log.d(TAG, "ICE Connected");
    synchronized (iceConnectedEvent) {
      isIceConnected = true;
      iceConnectedEvent.notifyAll();
    }
  }

  @Override
  public void onIceDisconnected() {
    Log.d(TAG, "ICE Disconnected");
    synchronized (iceConnectedEvent) {
      isIceConnected = false;
      iceConnectedEvent.notifyAll();
    }
  }

  @Override
  public void onConnected() {
    Log.d(TAG, "DTLS Connected");
  }

  @Override
  public void onDisconnected() {
    Log.d(TAG, "DTLS Disconnected");
  }

  @Override
  public void onPeerConnectionClosed() {
    Log.d(TAG, "PeerConnection closed");
    synchronized (closeEvent) {
      isClosed = true;
      closeEvent.notifyAll();
    }
  }

  @Override
  public void onPeerConnectionError(String description) {
    fail("PC Error: " + description);
  }

  @Override
  public void onPeerConnectionStatsReady(final RTCStatsReport report) {}

  // Helper wait functions.
  private boolean waitForLocalDescription(int timeoutMs) throws InterruptedException {
    synchronized (localDescEvent) {
      final long endTimeMs = System.currentTimeMillis() + timeoutMs;
      while (localDesc == null) {
        final long waitTimeMs = endTimeMs - System.currentTimeMillis();
        if (waitTimeMs < 0) {
          return false;
        }
        localDescEvent.wait(waitTimeMs);
      }
      return true;
    }
  }

  private boolean waitForIceCandidates(int timeoutMs) throws InterruptedException {
    synchronized (iceCandidateEvent) {
      final long endTimeMs = System.currentTimeMillis() + timeoutMs;
      while (iceCandidates.size() == 0) {
        final long waitTimeMs = endTimeMs - System.currentTimeMillis();
        if (waitTimeMs < 0) {
          return false;
        }
        iceCandidateEvent.wait(timeoutMs);
      }
      return true;
    }
  }

  private boolean waitForIceConnected(int timeoutMs) throws InterruptedException {
    synchronized (iceConnectedEvent) {
      final long endTimeMs = System.currentTimeMillis() + timeoutMs;
      while (!isIceConnected) {
        final long waitTimeMs = endTimeMs - System.currentTimeMillis();
        if (waitTimeMs < 0) {
          Log.e(TAG, "ICE connection failure");
          return false;
        }
        iceConnectedEvent.wait(timeoutMs);
      }
      return true;
    }
  }

  private boolean waitForPeerConnectionClosed(int timeoutMs) throws InterruptedException {
    synchronized (closeEvent) {
      final long endTimeMs = System.currentTimeMillis() + timeoutMs;
      while (!isClosed) {
        final long waitTimeMs = endTimeMs - System.currentTimeMillis();
        if (waitTimeMs < 0) {
          return false;
        }
        closeEvent.wait(timeoutMs);
      }
      return true;
    }
  }

  PeerConnectionClient createPeerConnectionClient(MockSink localRenderer, MockSink remoteRenderer,
      PeerConnectionParameters peerConnectionParameters, VideoCapturer videoCapturer) {
    List<PeerConnection.IceServer> iceServers = new ArrayList<>();
    SignalingParameters signalingParameters =
        new SignalingParameters(iceServers, true, // iceServers, initiator.
            null, null, null, // clientId, wssUrl, wssPostUrl.
            null, null); // offerSdp, iceCandidates.

    final EglBase eglBase = EglBase.create();
    PeerConnectionClient client =
        new PeerConnectionClient(InstrumentationRegistry.getTargetContext(), eglBase,
            peerConnectionParameters, this /* PeerConnectionEvents */);
    PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
    options.networkIgnoreMask = 0;
    options.disableNetworkMonitor = true;
    client.createPeerConnectionFactory(options);
    client.createPeerConnection(localRenderer, remoteRenderer, videoCapturer, signalingParameters);
    client.createOffer();
    return client;
  }

  private PeerConnectionParameters createParametersForAudioCall() {
    return new PeerConnectionParameters(false, /* videoCallEnabled */
        true, /* loopback */
        false, /* tracing */
        // Video codec parameters.
        0, /* videoWidth */
        0, /* videoHeight */
        0, /* videoFps */
        0, /* videoStartBitrate */
        "", /* videoCodec */
        true, /* videoCodecHwAcceleration */
        false, /* videoFlexfecEnabled */
        // Audio codec parameters.
        0, /* audioStartBitrate */
        "OPUS", /* audioCodec */
        false, /* noAudioProcessing */
        false, /* aecDump */
        false, /* saveInputAudioToFile */
        false /* useOpenSLES */, false /* disableBuiltInAEC */, false /* disableBuiltInAGC */,
        false /* disableBuiltInNS */, false /* disableWebRtcAGC */, false /* enableRtcEventLog */,
        null /* dataChannelParameters */);
  }

  private VideoCapturer createCameraCapturer(boolean captureToTexture) {
    final boolean useCamera2 = captureToTexture
        && Camera2Enumerator.isSupported(InstrumentationRegistry.getTargetContext());

    CameraEnumerator enumerator;
    if (useCamera2) {
      enumerator = new Camera2Enumerator(InstrumentationRegistry.getTargetContext());
    } else {
      enumerator = new Camera1Enumerator(captureToTexture);
    }
    String deviceName = enumerator.getDeviceNames()[0];
    return enumerator.createCapturer(deviceName, null);
  }

  private PeerConnectionParameters createParametersForVideoCall(String videoCodec) {
    return new PeerConnectionParameters(true, /* videoCallEnabled */
        true, /* loopback */
        false, /* tracing */
        // Video codec parameters.
        0, /* videoWidth */
        0, /* videoHeight */
        0, /* videoFps */
        0, /* videoStartBitrate */
        videoCodec, /* videoCodec */
        true, /* videoCodecHwAcceleration */
        false, /* videoFlexfecEnabled */
        // Audio codec parameters.
        0, /* audioStartBitrate */
        "OPUS", /* audioCodec */
        false, /* noAudioProcessing */
        false, /* aecDump */
        false, /* saveInputAudioToFile */
        false /* useOpenSLES */, false /* disableBuiltInAEC */, false /* disableBuiltInAGC */,
        false /* disableBuiltInNS */, false /* disableWebRtcAGC */, false /* enableRtcEventLog */,
        null /* dataChannelParameters */);
  }

  @Before
  public void setUp() {
    signalingExecutor = Executors.newSingleThreadExecutor();
  }

  @After
  public void tearDown() {
    signalingExecutor.shutdown();
  }

  @Test
  @SmallTest
  public void testSetLocalOfferMakesVideoFlowLocally() throws InterruptedException {
    Log.d(TAG, "testSetLocalOfferMakesVideoFlowLocally");
    MockSink localRenderer = new MockSink(EXPECTED_VIDEO_FRAMES, LOCAL_RENDERER_NAME);
    pcClient = createPeerConnectionClient(localRenderer,
        new MockSink(/* expectedFrames= */ 0, /* rendererName= */ null),
        createParametersForVideoCall(VIDEO_CODEC_VP8),
        createCameraCapturer(false /* captureToTexture */));

    // Wait for local description and ice candidates set events.
    assertTrue("Local description was not set.", waitForLocalDescription(WAIT_TIMEOUT));
    assertTrue("ICE candidates were not generated.", waitForIceCandidates(WAIT_TIMEOUT));

    // Check that local video frames were rendered.
    assertTrue(
        "Local video frames were not rendered.", localRenderer.waitForFramesRendered(WAIT_TIMEOUT));

    pcClient.close();
    assertTrue(
        "PeerConnection close event was not received.", waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testSetLocalOfferMakesVideoFlowLocally Done.");
  }

  private void doLoopbackTest(PeerConnectionParameters parameters, VideoCapturer videoCapturer,
      boolean decodeToTexture) throws InterruptedException {
    loopback = true;
    MockSink localRenderer = null;
    MockSink remoteRenderer = null;
    if (parameters.videoCallEnabled) {
      Log.d(TAG, "testLoopback for video " + parameters.videoCodec);
      localRenderer = new MockSink(EXPECTED_VIDEO_FRAMES, LOCAL_RENDERER_NAME);
      remoteRenderer = new MockSink(EXPECTED_VIDEO_FRAMES, REMOTE_RENDERER_NAME);
    } else {
      Log.d(TAG, "testLoopback for audio.");
    }
    pcClient = createPeerConnectionClient(localRenderer, remoteRenderer, parameters, videoCapturer);

    // Wait for local description, change type to answer and set as remote description.
    assertTrue("Local description was not set.", waitForLocalDescription(WAIT_TIMEOUT));
    SessionDescription remoteDescription = new SessionDescription(
        SessionDescription.Type.fromCanonicalForm("answer"), localDesc.description);
    pcClient.setRemoteDescription(remoteDescription);

    // Wait for ICE connection.
    assertTrue("ICE connection failure.", waitForIceConnected(ICE_CONNECTION_WAIT_TIMEOUT));

    if (parameters.videoCallEnabled) {
      // Check that local and remote video frames were rendered.
      assertTrue("Local video frames were not rendered.",
          localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
      assertTrue("Remote video frames were not rendered.",
          remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    } else {
      // For audio just sleep for 1 sec.
      // TODO(glaznev): check how we can detect that remote audio was rendered.
      Thread.sleep(AUDIO_RUN_TIMEOUT);
    }

    pcClient.close();
    assertTrue(waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testLoopback done.");
  }

  @Test
  @SmallTest
  public void testLoopbackAudio() throws InterruptedException {
    doLoopbackTest(createParametersForAudioCall(), null, false /* decodeToTexture */);
  }

  @Test
  @SmallTest
  public void testLoopbackVp8() throws InterruptedException {
    doLoopbackTest(createParametersForVideoCall(VIDEO_CODEC_VP8),
        createCameraCapturer(false /* captureToTexture */), false /* decodeToTexture */);
  }

  @Test
  @SmallTest
  public void testLoopbackVp9() throws InterruptedException {
    doLoopbackTest(createParametersForVideoCall(VIDEO_CODEC_VP9),
        createCameraCapturer(false /* captureToTexture */), false /* decodeToTexture */);
  }

  @Test
  @SmallTest
  public void testLoopbackH264() throws InterruptedException {
    doLoopbackTest(createParametersForVideoCall(VIDEO_CODEC_H264),
        createCameraCapturer(false /* captureToTexture */), false /* decodeToTexture */);
  }

  @Test
  @SmallTest
  public void testLoopbackVp8DecodeToTexture() throws InterruptedException {
    doLoopbackTest(createParametersForVideoCall(VIDEO_CODEC_VP8),
        createCameraCapturer(false /* captureToTexture */), true /* decodeToTexture */);
  }

  @Test
  @SmallTest
  public void testLoopbackVp9DecodeToTexture() throws InterruptedException {
    doLoopbackTest(createParametersForVideoCall(VIDEO_CODEC_VP9),
        createCameraCapturer(false /* captureToTexture */), true /* decodeToTexture */);
  }

  @Test
  @SmallTest
  public void testLoopbackH264DecodeToTexture() throws InterruptedException {
    doLoopbackTest(createParametersForVideoCall(VIDEO_CODEC_H264),
        createCameraCapturer(false /* captureToTexture */), true /* decodeToTexture */);
  }

  @Test
  @SmallTest
  public void testLoopbackVp8CaptureToTexture() throws InterruptedException {
    doLoopbackTest(createParametersForVideoCall(VIDEO_CODEC_VP8),
        createCameraCapturer(true /* captureToTexture */), true /* decodeToTexture */);
  }

  @Test
  @SmallTest
  public void testLoopbackH264CaptureToTexture() throws InterruptedException {
    doLoopbackTest(createParametersForVideoCall(VIDEO_CODEC_H264),
        createCameraCapturer(true /* captureToTexture */), true /* decodeToTexture */);
  }

  // Checks if default front camera can be switched to back camera and then
  // again to front camera.
  @Test
  @SmallTest
  public void testCameraSwitch() throws InterruptedException {
    Log.d(TAG, "testCameraSwitch");
    loopback = true;

    MockSink localRenderer = new MockSink(EXPECTED_VIDEO_FRAMES, LOCAL_RENDERER_NAME);
    MockSink remoteRenderer = new MockSink(EXPECTED_VIDEO_FRAMES, REMOTE_RENDERER_NAME);

    pcClient = createPeerConnectionClient(localRenderer, remoteRenderer,
        createParametersForVideoCall(VIDEO_CODEC_VP8),
        createCameraCapturer(false /* captureToTexture */));

    // Wait for local description, set type to answer and set as remote description.
    assertTrue("Local description was not set.", waitForLocalDescription(WAIT_TIMEOUT));
    SessionDescription remoteDescription = new SessionDescription(
        SessionDescription.Type.fromCanonicalForm("answer"), localDesc.description);
    pcClient.setRemoteDescription(remoteDescription);

    // Wait for ICE connection.
    assertTrue("ICE connection failure.", waitForIceConnected(ICE_CONNECTION_WAIT_TIMEOUT));

    // Check that local and remote video frames were rendered.
    assertTrue("Local video frames were not rendered before camera switch.",
        localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    assertTrue("Remote video frames were not rendered before camera switch.",
        remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));

    for (int i = 0; i < CAMERA_SWITCH_ATTEMPTS; i++) {
      // Try to switch camera
      pcClient.switchCamera();

      // Reset video renders and check that local and remote video frames
      // were rendered after camera switch.
      localRenderer.reset(EXPECTED_VIDEO_FRAMES);
      remoteRenderer.reset(EXPECTED_VIDEO_FRAMES);
      assertTrue("Local video frames were not rendered after camera switch.",
          localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
      assertTrue("Remote video frames were not rendered after camera switch.",
          remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    }
    pcClient.close();
    assertTrue(waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testCameraSwitch done.");
  }

  // Checks if video source can be restarted - simulate app goes to
  // background and back to foreground.
  @Test
  @SmallTest
  public void testVideoSourceRestart() throws InterruptedException {
    Log.d(TAG, "testVideoSourceRestart");
    loopback = true;

    MockSink localRenderer = new MockSink(EXPECTED_VIDEO_FRAMES, LOCAL_RENDERER_NAME);
    MockSink remoteRenderer = new MockSink(EXPECTED_VIDEO_FRAMES, REMOTE_RENDERER_NAME);

    pcClient = createPeerConnectionClient(localRenderer, remoteRenderer,
        createParametersForVideoCall(VIDEO_CODEC_VP8),
        createCameraCapturer(false /* captureToTexture */));

    // Wait for local description, set type to answer and set as remote description.
    assertTrue("Local description was not set.", waitForLocalDescription(WAIT_TIMEOUT));
    SessionDescription remoteDescription = new SessionDescription(
        SessionDescription.Type.fromCanonicalForm("answer"), localDesc.description);
    pcClient.setRemoteDescription(remoteDescription);

    // Wait for ICE connection.
    assertTrue("ICE connection failure.", waitForIceConnected(ICE_CONNECTION_WAIT_TIMEOUT));

    // Check that local and remote video frames were rendered.
    assertTrue("Local video frames were not rendered before video restart.",
        localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    assertTrue("Remote video frames were not rendered before video restart.",
        remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));

    // Stop and then start video source a few times.
    for (int i = 0; i < VIDEO_RESTART_ATTEMPTS; i++) {
      pcClient.stopVideoSource();
      Thread.sleep(VIDEO_RESTART_TIMEOUT);
      pcClient.startVideoSource();

      // Reset video renders and check that local and remote video frames
      // were rendered after video restart.
      localRenderer.reset(EXPECTED_VIDEO_FRAMES);
      remoteRenderer.reset(EXPECTED_VIDEO_FRAMES);
      assertTrue("Local video frames were not rendered after video restart.",
          localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
      assertTrue("Remote video frames were not rendered after video restart.",
          remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    }
    pcClient.close();
    assertTrue(waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testVideoSourceRestart done.");
  }

  // Checks if capture format can be changed on fly and decoder can be reset properly.
  @Test
  @SmallTest
  public void testCaptureFormatChange() throws InterruptedException {
    Log.d(TAG, "testCaptureFormatChange");
    loopback = true;

    MockSink localRenderer = new MockSink(EXPECTED_VIDEO_FRAMES, LOCAL_RENDERER_NAME);
    MockSink remoteRenderer = new MockSink(EXPECTED_VIDEO_FRAMES, REMOTE_RENDERER_NAME);

    pcClient = createPeerConnectionClient(localRenderer, remoteRenderer,
        createParametersForVideoCall(VIDEO_CODEC_VP8),
        createCameraCapturer(false /* captureToTexture */));

    // Wait for local description, set type to answer and set as remote description.
    assertTrue("Local description was not set.", waitForLocalDescription(WAIT_TIMEOUT));
    SessionDescription remoteDescription = new SessionDescription(
        SessionDescription.Type.fromCanonicalForm("answer"), localDesc.description);
    pcClient.setRemoteDescription(remoteDescription);

    // Wait for ICE connection.
    assertTrue("ICE connection failure.", waitForIceConnected(ICE_CONNECTION_WAIT_TIMEOUT));

    // Check that local and remote video frames were rendered.
    assertTrue("Local video frames were not rendered before camera resolution change.",
        localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    assertTrue("Remote video frames were not rendered before camera resolution change.",
        remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));

    // Change capture output format a few times.
    for (int i = 0; i < 2 * CAPTURE_FORMAT_CHANGE_ATTEMPTS; i++) {
      if (i % 2 == 0) {
        pcClient.changeCaptureFormat(WIDTH_VGA, HEIGHT_VGA, MAX_VIDEO_FPS);
      } else {
        pcClient.changeCaptureFormat(WIDTH_QVGA, HEIGHT_QVGA, MAX_VIDEO_FPS);
      }

      // Reset video renders and check that local and remote video frames
      // were rendered after capture format change.
      localRenderer.reset(EXPECTED_VIDEO_FRAMES);
      remoteRenderer.reset(EXPECTED_VIDEO_FRAMES);
      assertTrue("Local video frames were not rendered after capture format change.",
          localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
      assertTrue("Remote video frames were not rendered after capture format change.",
          remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    }

    pcClient.close();
    assertTrue(waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testCaptureFormatChange done.");
  }
}
