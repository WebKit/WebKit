/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import java.io.File;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.Metrics.HistogramInfo;
import org.webrtc.PeerConnection.IceConnectionState;
import org.webrtc.PeerConnection.IceGatheringState;
import org.webrtc.PeerConnection.SignalingState;

/** End-to-end tests for PeerConnection.java. */
@RunWith(BaseJUnit4ClassRunner.class)
public class PeerConnectionTest {
  private static final int TIMEOUT_SECONDS = 20;
  private TreeSet<String> threadsBeforeTest = null;

  @Before
  public void setUp() {
    PeerConnectionFactory.initializeAndroidGlobals(InstrumentationRegistry.getContext(), true);
  }

  private static class ObserverExpectations
      implements PeerConnection.Observer, VideoRenderer.Callbacks, DataChannel.Observer,
                 StatsObserver, RTCStatsCollectorCallback, RtpReceiver.Observer {
    private final String name;
    private int expectedIceCandidates = 0;
    private int expectedErrors = 0;
    private int expectedRenegotiations = 0;
    private int expectedWidth = 0;
    private int expectedHeight = 0;
    private int expectedFramesDelivered = 0;
    private int expectedTracksAdded = 0;
    private LinkedList<SignalingState> expectedSignalingChanges = new LinkedList<SignalingState>();
    private LinkedList<IceConnectionState> expectedIceConnectionChanges =
        new LinkedList<IceConnectionState>();
    private LinkedList<IceGatheringState> expectedIceGatheringChanges =
        new LinkedList<IceGatheringState>();
    private LinkedList<String> expectedAddStreamLabels = new LinkedList<String>();
    private LinkedList<String> expectedRemoveStreamLabels = new LinkedList<String>();
    private final LinkedList<IceCandidate> gotIceCandidates = new LinkedList<IceCandidate>();
    private Map<MediaStream, WeakReference<VideoRenderer>> renderers =
        new IdentityHashMap<MediaStream, WeakReference<VideoRenderer>>();
    private DataChannel dataChannel;
    private LinkedList<DataChannel.Buffer> expectedBuffers = new LinkedList<DataChannel.Buffer>();
    private LinkedList<DataChannel.State> expectedStateChanges =
        new LinkedList<DataChannel.State>();
    private LinkedList<String> expectedRemoteDataChannelLabels = new LinkedList<String>();
    private int expectedOldStatsCallbacks = 0;
    private int expectedNewStatsCallbacks = 0;
    private LinkedList<StatsReport[]> gotStatsReports = new LinkedList<StatsReport[]>();
    private final HashSet<MediaStream> gotRemoteStreams = new HashSet<MediaStream>();
    private int expectedFirstAudioPacket = 0;
    private int expectedFirstVideoPacket = 0;

    public ObserverExpectations(String name) {
      this.name = name;
    }

    public synchronized void setDataChannel(DataChannel dataChannel) {
      assertNull(this.dataChannel);
      this.dataChannel = dataChannel;
      this.dataChannel.registerObserver(this);
      assertNotNull(this.dataChannel);
    }

    public synchronized void expectIceCandidates(int count) {
      expectedIceCandidates += count;
    }

    @Override
    public synchronized void onIceCandidate(IceCandidate candidate) {
      --expectedIceCandidates;

      // We don't assert expectedIceCandidates >= 0 because it's hard to know
      // how many to expect, in general.  We only use expectIceCandidates to
      // assert a minimal count.
      synchronized (gotIceCandidates) {
        gotIceCandidates.add(candidate);
        gotIceCandidates.notifyAll();
      }
    }

    @Override
    public void onIceCandidatesRemoved(IceCandidate[] candidates) {}

    public synchronized void setExpectedResolution(int width, int height) {
      expectedWidth = width;
      expectedHeight = height;
    }

    public synchronized void expectFramesDelivered(int count) {
      expectedFramesDelivered += count;
    }

    @Override
    public synchronized void renderFrame(VideoRenderer.I420Frame frame) {
      assertTrue(expectedWidth > 0);
      assertTrue(expectedHeight > 0);
      assertEquals(expectedWidth, frame.rotatedWidth());
      assertEquals(expectedHeight, frame.rotatedHeight());
      --expectedFramesDelivered;
      VideoRenderer.renderFrameDone(frame);
    }

    public synchronized void expectSignalingChange(SignalingState newState) {
      expectedSignalingChanges.add(newState);
    }

    @Override
    public synchronized void onSignalingChange(SignalingState newState) {
      assertEquals(expectedSignalingChanges.removeFirst(), newState);
    }

    public synchronized void expectIceConnectionChange(IceConnectionState newState) {
      expectedIceConnectionChanges.add(newState);
    }

    @Override
    public synchronized void onIceConnectionChange(IceConnectionState newState) {
      // TODO(bemasc): remove once delivery of ICECompleted is reliable
      // (https://code.google.com/p/webrtc/issues/detail?id=3021).
      if (newState.equals(IceConnectionState.COMPLETED)) {
        return;
      }

      if (expectedIceConnectionChanges.isEmpty()) {
        System.out.println(name + "Got an unexpected ice connection change " + newState);
        return;
      }

      assertEquals(expectedIceConnectionChanges.removeFirst(), newState);
    }

    @Override
    public synchronized void onIceConnectionReceivingChange(boolean receiving) {
      System.out.println(name + "Got an ice connection receiving change " + receiving);
    }

    public synchronized void expectIceGatheringChange(IceGatheringState newState) {
      expectedIceGatheringChanges.add(newState);
    }

    @Override
    public synchronized void onIceGatheringChange(IceGatheringState newState) {
      // It's fine to get a variable number of GATHERING messages before
      // COMPLETE fires (depending on how long the test runs) so we don't assert
      // any particular count.
      if (newState == IceGatheringState.GATHERING) {
        return;
      }
      assertEquals(expectedIceGatheringChanges.removeFirst(), newState);
    }

    public synchronized void expectAddStream(String label) {
      expectedAddStreamLabels.add(label);
    }

    @Override
    public synchronized void onAddStream(MediaStream stream) {
      assertEquals(expectedAddStreamLabels.removeFirst(), stream.label());
      assertEquals(1, stream.videoTracks.size());
      assertEquals(1, stream.audioTracks.size());
      assertTrue(stream.videoTracks.get(0).id().endsWith("VideoTrack"));
      assertTrue(stream.audioTracks.get(0).id().endsWith("AudioTrack"));
      assertEquals("video", stream.videoTracks.get(0).kind());
      assertEquals("audio", stream.audioTracks.get(0).kind());
      VideoRenderer renderer = createVideoRenderer(this);
      stream.videoTracks.get(0).addRenderer(renderer);
      assertNull(renderers.put(stream, new WeakReference<VideoRenderer>(renderer)));
      gotRemoteStreams.add(stream);
    }

    public synchronized void expectRemoveStream(String label) {
      expectedRemoveStreamLabels.add(label);
    }

    @Override
    public synchronized void onRemoveStream(MediaStream stream) {
      assertEquals(expectedRemoveStreamLabels.removeFirst(), stream.label());
      WeakReference<VideoRenderer> renderer = renderers.remove(stream);
      assertNotNull(renderer);
      assertNotNull(renderer.get());
      assertEquals(1, stream.videoTracks.size());
      stream.videoTracks.get(0).removeRenderer(renderer.get());
      gotRemoteStreams.remove(stream);
    }

    public synchronized void expectDataChannel(String label) {
      expectedRemoteDataChannelLabels.add(label);
    }

    @Override
    public synchronized void onDataChannel(DataChannel remoteDataChannel) {
      assertEquals(expectedRemoteDataChannelLabels.removeFirst(), remoteDataChannel.label());
      setDataChannel(remoteDataChannel);
      assertEquals(DataChannel.State.CONNECTING, dataChannel.state());
    }

    public synchronized void expectRenegotiationNeeded() {
      ++expectedRenegotiations;
    }

    @Override
    public synchronized void onRenegotiationNeeded() {
      assertTrue(--expectedRenegotiations >= 0);
    }

    public synchronized void expectAddTrack(int expectedTracksAdded) {
      this.expectedTracksAdded = expectedTracksAdded;
    }

    @Override
    public synchronized void onAddTrack(RtpReceiver receiver, MediaStream[] mediaStreams) {
      expectedTracksAdded--;
    }

    public synchronized void expectMessage(ByteBuffer expectedBuffer, boolean expectedBinary) {
      expectedBuffers.add(new DataChannel.Buffer(expectedBuffer, expectedBinary));
    }

    @Override
    public synchronized void onMessage(DataChannel.Buffer buffer) {
      DataChannel.Buffer expected = expectedBuffers.removeFirst();
      assertEquals(expected.binary, buffer.binary);
      assertTrue(expected.data.equals(buffer.data));
    }

    @Override
    public synchronized void onBufferedAmountChange(long previousAmount) {
      assertFalse(previousAmount == dataChannel.bufferedAmount());
    }

    @Override
    public synchronized void onStateChange() {
      assertEquals(expectedStateChanges.removeFirst(), dataChannel.state());
    }

    public synchronized void expectStateChange(DataChannel.State state) {
      expectedStateChanges.add(state);
    }

    // Old getStats callback.
    @Override
    public synchronized void onComplete(StatsReport[] reports) {
      if (--expectedOldStatsCallbacks < 0) {
        throw new RuntimeException("Unexpected stats report: " + reports);
      }
      gotStatsReports.add(reports);
    }

    // New getStats callback.
    @Override
    public synchronized void onStatsDelivered(RTCStatsReport report) {
      if (--expectedNewStatsCallbacks < 0) {
        throw new RuntimeException("Unexpected stats report: " + report);
      }
    }

    @Override
    public synchronized void onFirstPacketReceived(MediaStreamTrack.MediaType mediaType) {
      if (mediaType == MediaStreamTrack.MediaType.MEDIA_TYPE_AUDIO) {
        expectedFirstAudioPacket--;
      } else {
        expectedFirstVideoPacket--;
      }
      if (expectedFirstAudioPacket < 0 || expectedFirstVideoPacket < 0) {
        throw new RuntimeException("Unexpected call of onFirstPacketReceived");
      }
    }

    public synchronized void expectFirstPacketReceived() {
      expectedFirstAudioPacket = 1;
      expectedFirstVideoPacket = 1;
    }

    public synchronized void expectOldStatsCallback() {
      ++expectedOldStatsCallbacks;
    }

    public synchronized void expectNewStatsCallback() {
      ++expectedNewStatsCallbacks;
    }

    public synchronized LinkedList<StatsReport[]> takeStatsReports() {
      LinkedList<StatsReport[]> got = gotStatsReports;
      gotStatsReports = new LinkedList<StatsReport[]>();
      return got;
    }

    // Return a set of expectations that haven't been satisfied yet, possibly
    // empty if no such expectations exist.
    public synchronized TreeSet<String> unsatisfiedExpectations() {
      TreeSet<String> stillWaitingForExpectations = new TreeSet<String>();
      if (expectedIceCandidates > 0) { // See comment in onIceCandidate.
        stillWaitingForExpectations.add("expectedIceCandidates");
      }
      if (expectedErrors != 0) {
        stillWaitingForExpectations.add("expectedErrors: " + expectedErrors);
      }
      if (expectedSignalingChanges.size() != 0) {
        stillWaitingForExpectations.add(
            "expectedSignalingChanges: " + expectedSignalingChanges.size());
      }
      if (expectedIceConnectionChanges.size() != 0) {
        stillWaitingForExpectations.add(
            "expectedIceConnectionChanges: " + expectedIceConnectionChanges.size());
      }
      if (expectedIceGatheringChanges.size() != 0) {
        stillWaitingForExpectations.add(
            "expectedIceGatheringChanges: " + expectedIceGatheringChanges.size());
      }
      if (expectedAddStreamLabels.size() != 0) {
        stillWaitingForExpectations.add(
            "expectedAddStreamLabels: " + expectedAddStreamLabels.size());
      }
      if (expectedRemoveStreamLabels.size() != 0) {
        stillWaitingForExpectations.add(
            "expectedRemoveStreamLabels: " + expectedRemoveStreamLabels.size());
      }
      if (expectedFramesDelivered > 0) {
        stillWaitingForExpectations.add("expectedFramesDelivered: " + expectedFramesDelivered);
      }
      if (!expectedBuffers.isEmpty()) {
        stillWaitingForExpectations.add("expectedBuffers: " + expectedBuffers.size());
      }
      if (!expectedStateChanges.isEmpty()) {
        stillWaitingForExpectations.add("expectedStateChanges: " + expectedStateChanges.size());
      }
      if (!expectedRemoteDataChannelLabels.isEmpty()) {
        stillWaitingForExpectations.add(
            "expectedRemoteDataChannelLabels: " + expectedRemoteDataChannelLabels.size());
      }
      if (expectedOldStatsCallbacks != 0) {
        stillWaitingForExpectations.add("expectedOldStatsCallbacks: " + expectedOldStatsCallbacks);
      }
      if (expectedNewStatsCallbacks != 0) {
        stillWaitingForExpectations.add("expectedNewStatsCallbacks: " + expectedNewStatsCallbacks);
      }
      if (expectedFirstAudioPacket > 0) {
        stillWaitingForExpectations.add("expectedFirstAudioPacket: " + expectedFirstAudioPacket);
      }
      if (expectedFirstVideoPacket > 0) {
        stillWaitingForExpectations.add("expectedFirstVideoPacket: " + expectedFirstVideoPacket);
      }
      if (expectedTracksAdded != 0) {
        stillWaitingForExpectations.add("expectedAddedTrack: " + expectedTracksAdded);
      }
      return stillWaitingForExpectations;
    }

    public boolean waitForAllExpectationsToBeSatisfied(int timeoutSeconds) {
      // TODO(fischman): problems with this approach:
      // - come up with something better than a poll loop
      // - avoid serializing expectations explicitly; the test is not as robust
      //   as it could be because it must place expectations between wait
      //   statements very precisely (e.g. frame must not arrive before its
      //   expectation, and expectation must not be registered so early as to
      //   stall a wait).  Use callbacks to fire off dependent steps instead of
      //   explicitly waiting, so there can be just a single wait at the end of
      //   the test.
      long endTime = System.currentTimeMillis() + 1000 * timeoutSeconds;
      TreeSet<String> prev = null;
      TreeSet<String> stillWaitingForExpectations = unsatisfiedExpectations();
      while (!stillWaitingForExpectations.isEmpty()) {
        if (!stillWaitingForExpectations.equals(prev)) {
          System.out.println(name + " still waiting at\n    " + (new Throwable()).getStackTrace()[1]
              + "\n    for: " + Arrays.toString(stillWaitingForExpectations.toArray()));
        }
        if (endTime < System.currentTimeMillis()) {
          System.out.println(name + " timed out waiting for: "
              + Arrays.toString(stillWaitingForExpectations.toArray()));
          return false;
        }
        try {
          Thread.sleep(10);
        } catch (InterruptedException e) {
          throw new RuntimeException(e);
        }
        prev = stillWaitingForExpectations;
        stillWaitingForExpectations = unsatisfiedExpectations();
      }
      if (prev == null) {
        System.out.println(
            name + " didn't need to wait at\n    " + (new Throwable()).getStackTrace()[1]);
      }
      return true;
    }

    // This methods return a list of all currently gathered ice candidates or waits until
    // 1 candidate have been gathered.
    public List<IceCandidate> getAtLeastOneIceCandidate() throws InterruptedException {
      synchronized (gotIceCandidates) {
        while (gotIceCandidates.isEmpty()) {
          gotIceCandidates.wait();
        }
        return new LinkedList<IceCandidate>(gotIceCandidates);
      }
    }
  }

  // Sets the expected resolution for an ObserverExpectations once a frame
  // has been captured.
  private static class ExpectedResolutionSetter implements VideoRenderer.Callbacks {
    private ObserverExpectations observer;

    public ExpectedResolutionSetter(ObserverExpectations observer) {
      this.observer = observer;
    }

    @Override
    public synchronized void renderFrame(VideoRenderer.I420Frame frame) {
      // Because different camera devices (fake & physical) produce different
      // resolutions, we only sanity-check the set sizes,
      assertTrue(frame.rotatedWidth() > 0);
      assertTrue(frame.rotatedHeight() > 0);
      observer.setExpectedResolution(frame.rotatedWidth(), frame.rotatedHeight());
    }
  }

  private static class SdpObserverLatch implements SdpObserver {
    private boolean success = false;
    private SessionDescription sdp = null;
    private String error = null;
    private CountDownLatch latch = new CountDownLatch(1);

    public SdpObserverLatch() {}

    @Override
    public void onCreateSuccess(SessionDescription sdp) {
      this.sdp = sdp;
      onSetSuccess();
    }

    @Override
    public void onSetSuccess() {
      success = true;
      latch.countDown();
    }

    @Override
    public void onCreateFailure(String error) {
      onSetFailure(error);
    }

    @Override
    public void onSetFailure(String error) {
      this.error = error;
      latch.countDown();
    }

    public boolean await() {
      try {
        assertTrue(latch.await(1000, TimeUnit.MILLISECONDS));
        return getSuccess();
      } catch (Exception e) {
        throw new RuntimeException(e);
      }
    }

    public boolean getSuccess() {
      return success;
    }

    public SessionDescription getSdp() {
      return sdp;
    }

    public String getError() {
      return error;
    }
  }

  static int videoWindowsMapped = -1;

  private static VideoRenderer createVideoRenderer(VideoRenderer.Callbacks videoCallbacks) {
    return new VideoRenderer(videoCallbacks);
  }

  // Return a weak reference to test that ownership is correctly held by
  // PeerConnection, not by test code.
  private static WeakReference<MediaStream> addTracksToPC(PeerConnectionFactory factory,
      PeerConnection pc, VideoSource videoSource, String streamLabel, String videoTrackId,
      String audioTrackId, VideoRenderer.Callbacks videoCallbacks) {
    MediaStream lMS = factory.createLocalMediaStream(streamLabel);
    VideoTrack videoTrack = factory.createVideoTrack(videoTrackId, videoSource);
    assertNotNull(videoTrack);
    VideoRenderer videoRenderer = createVideoRenderer(videoCallbacks);
    assertNotNull(videoRenderer);
    videoTrack.addRenderer(videoRenderer);
    lMS.addTrack(videoTrack);
    // Just for fun, let's remove and re-add the track.
    lMS.removeTrack(videoTrack);
    lMS.addTrack(videoTrack);
    lMS.addTrack(
        factory.createAudioTrack(audioTrackId, factory.createAudioSource(new MediaConstraints())));
    pc.addStream(lMS);
    return new WeakReference<MediaStream>(lMS);
  }

  // Used for making sure thread handles are not leaked.
  // Call initializeThreadCheck before a test and finalizeThreadCheck after
  // a test.
  void initializeThreadCheck() {
    System.gc(); // Encourage any GC-related threads to start up.
    threadsBeforeTest = allThreads();
  }

  void finalizeThreadCheck() throws Exception {
    // TreeSet<String> threadsAfterTest = allThreads();

    // TODO(tommi): Figure out a more reliable way to do this test.  As is
    // we're seeing three possible 'normal' situations:
    // 1.  before and after sets are equal.
    // 2.  before contains 3 threads that do not exist in after.
    // 3.  after contains 3 threads that do not exist in before.
    //
    // Maybe it would be better to do the thread enumeration from C++ and get
    // the thread names as well, in order to determine what these 3 threads are.

    // assertEquals(threadsBeforeTest, threadsAfterTest);
    // Thread.sleep(100);
  }

  // TODO(fischman) MOAR test ideas:
  // - Test that PC.removeStream() works; requires a second
  //   createOffer/createAnswer dance.
  // - audit each place that uses |constraints| for specifying non-trivial
  //   constraints (and ensure they're honored).
  // - test error cases
  // - ensure reasonable coverage of _jni.cc is achieved.  Coverage is
  //   extra-important because of all the free-text (class/method names, etc)
  //   in JNI-style programming; make sure no typos!
  // - Test that shutdown mid-interaction is crash-free.

  @Test
  @MediumTest
  public void testCompleteSession() throws Exception {
    Metrics.enable();
    // Allow loopback interfaces too since our Android devices often don't
    // have those.
    PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
    options.networkIgnoreMask = 0;
    PeerConnectionFactory factory = new PeerConnectionFactory(options);
    // Uncomment to get ALL WebRTC tracing and SENSITIVE libjingle logging.
    // NOTE: this _must_ happen while |factory| is alive!
    // Logging.enableTracing(
    //     "/tmp/PeerConnectionTest-log.txt",
    //     EnumSet.of(Logging.TraceLevel.TRACE_ALL),
    //     Logging.Severity.LS_SENSITIVE);

    MediaConstraints pcConstraints = new MediaConstraints();
    pcConstraints.mandatory.add(new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));

    LinkedList<PeerConnection.IceServer> iceServers = new LinkedList<PeerConnection.IceServer>();
    iceServers.add(new PeerConnection.IceServer("stun:stun.l.google.com:19302"));
    iceServers.add(
        new PeerConnection.IceServer("turn:fake.example.com", "fakeUsername", "fakePassword"));
    ObserverExpectations offeringExpectations = new ObserverExpectations("PCTest:offerer");
    PeerConnection offeringPC =
        factory.createPeerConnection(iceServers, pcConstraints, offeringExpectations);
    assertNotNull(offeringPC);

    ObserverExpectations answeringExpectations = new ObserverExpectations("PCTest:answerer");
    PeerConnection answeringPC =
        factory.createPeerConnection(iceServers, pcConstraints, answeringExpectations);
    assertNotNull(answeringPC);

    // We want to use the same camera for offerer & answerer, so create it here
    // instead of in addTracksToPC.
    final CameraEnumerator enumerator = new Camera1Enumerator(false /* captureToTexture */);
    final VideoCapturer videoCapturer =
        enumerator.createCapturer(enumerator.getDeviceNames()[0], null);
    final VideoSource videoSource = factory.createVideoSource(videoCapturer);
    videoCapturer.startCapture(640, 480, 30);

    offeringExpectations.expectRenegotiationNeeded();
    WeakReference<MediaStream> oLMS =
        addTracksToPC(factory, offeringPC, videoSource, "offeredMediaStream", "offeredVideoTrack",
            "offeredAudioTrack", new ExpectedResolutionSetter(answeringExpectations));

    offeringExpectations.expectAddTrack(2);
    answeringExpectations.expectAddTrack(2);

    offeringExpectations.expectRenegotiationNeeded();
    DataChannel offeringDC = offeringPC.createDataChannel("offeringDC", new DataChannel.Init());
    assertEquals("offeringDC", offeringDC.label());

    offeringExpectations.setDataChannel(offeringDC);
    SdpObserverLatch sdpLatch = new SdpObserverLatch();
    offeringPC.createOffer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription offerSdp = sdpLatch.getSdp();
    assertEquals(offerSdp.type, SessionDescription.Type.OFFER);
    assertFalse(offerSdp.description.isEmpty());

    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(SignalingState.HAVE_REMOTE_OFFER);
    answeringExpectations.expectAddStream("offeredMediaStream");
    // SCTP DataChannels are announced via OPEN messages over the established
    // connection (not via SDP), so answeringExpectations can only register
    // expecting the channel during ICE, below.
    answeringPC.setRemoteDescription(sdpLatch, offerSdp);
    assertEquals(PeerConnection.SignalingState.STABLE, offeringPC.signalingState());
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    answeringExpectations.expectRenegotiationNeeded();
    WeakReference<MediaStream> aLMS = addTracksToPC(factory, answeringPC, videoSource,
        "answeredMediaStream", "answeredVideoTrack", "answeredAudioTrack",
        new ExpectedResolutionSetter(offeringExpectations));

    sdpLatch = new SdpObserverLatch();
    answeringPC.createAnswer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription answerSdp = sdpLatch.getSdp();
    assertEquals(answerSdp.type, SessionDescription.Type.ANSWER);
    assertFalse(answerSdp.description.isEmpty());

    offeringExpectations.expectIceCandidates(2);
    answeringExpectations.expectIceCandidates(2);

    offeringExpectations.expectIceGatheringChange(IceGatheringState.COMPLETE);
    answeringExpectations.expectIceGatheringChange(IceGatheringState.COMPLETE);

    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(SignalingState.STABLE);
    answeringPC.setLocalDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.HAVE_LOCAL_OFFER);
    offeringPC.setLocalDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.STABLE);
    offeringExpectations.expectAddStream("answeredMediaStream");

    offeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    offeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);
    // TODO(bemasc): uncomment once delivery of ICECompleted is reliable
    // (https://code.google.com/p/webrtc/issues/detail?id=3021).
    //
    // offeringExpectations.expectIceConnectionChange(
    //     IceConnectionState.COMPLETED);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);

    offeringPC.setRemoteDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    assertEquals(offeringPC.getLocalDescription().type, offerSdp.type);
    assertEquals(offeringPC.getRemoteDescription().type, answerSdp.type);
    assertEquals(answeringPC.getLocalDescription().type, answerSdp.type);
    assertEquals(answeringPC.getRemoteDescription().type, offerSdp.type);

    assertEquals(offeringPC.getSenders().size(), 2);
    assertEquals(offeringPC.getReceivers().size(), 2);
    assertEquals(answeringPC.getSenders().size(), 2);
    assertEquals(answeringPC.getReceivers().size(), 2);

    offeringExpectations.expectFirstPacketReceived();
    answeringExpectations.expectFirstPacketReceived();

    for (RtpReceiver receiver : offeringPC.getReceivers()) {
      receiver.SetObserver(offeringExpectations);
    }

    for (RtpReceiver receiver : answeringPC.getReceivers()) {
      receiver.SetObserver(answeringExpectations);
    }

    // Wait for at least some frames to be delivered at each end (number
    // chosen arbitrarily).
    offeringExpectations.expectFramesDelivered(10);
    answeringExpectations.expectFramesDelivered(10);

    offeringExpectations.expectStateChange(DataChannel.State.OPEN);
    // See commentary about SCTP DataChannels above for why this is here.
    answeringExpectations.expectDataChannel("offeringDC");
    answeringExpectations.expectStateChange(DataChannel.State.OPEN);

    // Wait for at least one ice candidate from the offering PC and forward them to the answering
    // PC.
    for (IceCandidate candidate : offeringExpectations.getAtLeastOneIceCandidate()) {
      answeringPC.addIceCandidate(candidate);
    }

    // Wait for at least one ice candidate from the answering PC and forward them to the offering
    // PC.
    for (IceCandidate candidate : answeringExpectations.getAtLeastOneIceCandidate()) {
      offeringPC.addIceCandidate(candidate);
    }

    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    assertEquals(PeerConnection.SignalingState.STABLE, offeringPC.signalingState());
    assertEquals(PeerConnection.SignalingState.STABLE, answeringPC.signalingState());

    // Test some of the RtpSender API.
    RtpSender videoSender = null;
    RtpSender audioSender = null;
    for (RtpSender sender : offeringPC.getSenders()) {
      if (sender.track().kind().equals("video")) {
        videoSender = sender;
      } else {
        audioSender = sender;
      }
    }
    assertNotNull(videoSender);
    assertNotNull(audioSender);

    // Set a bitrate limit for the outgoing video stream for the offerer.
    RtpParameters rtpParameters = videoSender.getParameters();
    assertNotNull(rtpParameters);
    assertEquals(1, rtpParameters.encodings.size());
    assertNull(rtpParameters.encodings.get(0).maxBitrateBps);

    rtpParameters.encodings.get(0).maxBitrateBps = 300000;
    assertTrue(videoSender.setParameters(rtpParameters));

    // Create a DTMF sender.
    DtmfSender dtmfSender = audioSender.dtmf();
    assertNotNull(dtmfSender);
    assertTrue(dtmfSender.canInsertDtmf());
    assertTrue(dtmfSender.insertDtmf("123", 300, 100));

    // Verify that we can read back the updated value.
    rtpParameters = videoSender.getParameters();
    assertEquals(300000, (int) rtpParameters.encodings.get(0).maxBitrateBps);

    // Test send & receive UTF-8 text.
    answeringExpectations.expectMessage(
        ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    DataChannel.Buffer buffer =
        new DataChannel.Buffer(ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    assertTrue(offeringExpectations.dataChannel.send(buffer));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    // Construct this binary message two different ways to ensure no
    // shortcuts are taken.
    ByteBuffer expectedBinaryMessage = ByteBuffer.allocateDirect(5);
    for (byte i = 1; i < 6; ++i) {
      expectedBinaryMessage.put(i);
    }
    expectedBinaryMessage.flip();
    offeringExpectations.expectMessage(expectedBinaryMessage, true);
    assertTrue(answeringExpectations.dataChannel.send(
        new DataChannel.Buffer(ByteBuffer.wrap(new byte[] {1, 2, 3, 4, 5}), true)));
    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    offeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    answeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    offeringExpectations.expectStateChange(DataChannel.State.CLOSED);
    answeringExpectations.expectStateChange(DataChannel.State.CLOSED);
    answeringExpectations.dataChannel.close();
    offeringExpectations.dataChannel.close();

    // Free the Java-land objects and collect them.
    shutdownPC(offeringPC, offeringExpectations);
    offeringPC = null;
    shutdownPC(answeringPC, answeringExpectations);
    answeringPC = null;
    getMetrics();
    videoCapturer.stopCapture();
    videoCapturer.dispose();
    videoSource.dispose();
    factory.dispose();
    System.gc();
  }

  @Test
  @MediumTest
  public void testDataChannelOnlySession() throws Exception {
    // Allow loopback interfaces too since our Android devices often don't
    // have those.
    PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
    options.networkIgnoreMask = 0;
    PeerConnectionFactory factory = new PeerConnectionFactory(options);

    MediaConstraints pcConstraints = new MediaConstraints();
    pcConstraints.mandatory.add(new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));

    LinkedList<PeerConnection.IceServer> iceServers = new LinkedList<PeerConnection.IceServer>();
    iceServers.add(new PeerConnection.IceServer("stun:stun.l.google.com:19302"));
    iceServers.add(
        new PeerConnection.IceServer("turn:fake.example.com", "fakeUsername", "fakePassword"));
    ObserverExpectations offeringExpectations = new ObserverExpectations("PCTest:offerer");
    PeerConnection offeringPC =
        factory.createPeerConnection(iceServers, pcConstraints, offeringExpectations);
    assertNotNull(offeringPC);

    ObserverExpectations answeringExpectations = new ObserverExpectations("PCTest:answerer");
    PeerConnection answeringPC =
        factory.createPeerConnection(iceServers, pcConstraints, answeringExpectations);
    assertNotNull(answeringPC);

    offeringExpectations.expectRenegotiationNeeded();
    DataChannel offeringDC = offeringPC.createDataChannel("offeringDC", new DataChannel.Init());
    assertEquals("offeringDC", offeringDC.label());

    offeringExpectations.setDataChannel(offeringDC);
    SdpObserverLatch sdpLatch = new SdpObserverLatch();
    offeringPC.createOffer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription offerSdp = sdpLatch.getSdp();
    assertEquals(offerSdp.type, SessionDescription.Type.OFFER);
    assertFalse(offerSdp.description.isEmpty());

    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(SignalingState.HAVE_REMOTE_OFFER);
    // SCTP DataChannels are announced via OPEN messages over the established
    // connection (not via SDP), so answeringExpectations can only register
    // expecting the channel during ICE, below.
    answeringPC.setRemoteDescription(sdpLatch, offerSdp);
    assertEquals(PeerConnection.SignalingState.STABLE, offeringPC.signalingState());
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    sdpLatch = new SdpObserverLatch();
    answeringPC.createAnswer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription answerSdp = sdpLatch.getSdp();
    assertEquals(answerSdp.type, SessionDescription.Type.ANSWER);
    assertFalse(answerSdp.description.isEmpty());

    offeringExpectations.expectIceCandidates(2);
    answeringExpectations.expectIceCandidates(2);

    offeringExpectations.expectIceGatheringChange(IceGatheringState.COMPLETE);
    answeringExpectations.expectIceGatheringChange(IceGatheringState.COMPLETE);

    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(SignalingState.STABLE);
    answeringPC.setLocalDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.HAVE_LOCAL_OFFER);
    offeringPC.setLocalDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.STABLE);

    offeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    offeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);
    // TODO(bemasc): uncomment once delivery of ICECompleted is reliable
    // (https://code.google.com/p/webrtc/issues/detail?id=3021).
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);

    offeringPC.setRemoteDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    assertEquals(offeringPC.getLocalDescription().type, offerSdp.type);
    assertEquals(offeringPC.getRemoteDescription().type, answerSdp.type);
    assertEquals(answeringPC.getLocalDescription().type, answerSdp.type);
    assertEquals(answeringPC.getRemoteDescription().type, offerSdp.type);

    offeringExpectations.expectStateChange(DataChannel.State.OPEN);
    // See commentary about SCTP DataChannels above for why this is here.
    answeringExpectations.expectDataChannel("offeringDC");
    answeringExpectations.expectStateChange(DataChannel.State.OPEN);

    // Wait for at least one ice candidate from the offering PC and forward them to the answering
    // PC.
    for (IceCandidate candidate : offeringExpectations.getAtLeastOneIceCandidate()) {
      answeringPC.addIceCandidate(candidate);
    }

    // Wait for at least one ice candidate from the answering PC and forward them to the offering
    // PC.
    for (IceCandidate candidate : answeringExpectations.getAtLeastOneIceCandidate()) {
      offeringPC.addIceCandidate(candidate);
    }

    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    assertEquals(PeerConnection.SignalingState.STABLE, offeringPC.signalingState());
    assertEquals(PeerConnection.SignalingState.STABLE, answeringPC.signalingState());

    // Test send & receive UTF-8 text.
    answeringExpectations.expectMessage(
        ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    DataChannel.Buffer buffer =
        new DataChannel.Buffer(ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    assertTrue(offeringExpectations.dataChannel.send(buffer));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    // Construct this binary message two different ways to ensure no
    // shortcuts are taken.
    ByteBuffer expectedBinaryMessage = ByteBuffer.allocateDirect(5);
    for (byte i = 1; i < 6; ++i) {
      expectedBinaryMessage.put(i);
    }
    expectedBinaryMessage.flip();
    offeringExpectations.expectMessage(expectedBinaryMessage, true);
    assertTrue(answeringExpectations.dataChannel.send(
        new DataChannel.Buffer(ByteBuffer.wrap(new byte[] {1, 2, 3, 4, 5}), true)));
    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    offeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    answeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    offeringExpectations.expectStateChange(DataChannel.State.CLOSED);
    answeringExpectations.expectStateChange(DataChannel.State.CLOSED);
    answeringExpectations.dataChannel.close();
    offeringExpectations.dataChannel.close();

    // Free the Java-land objects and collect them.
    shutdownPC(offeringPC, offeringExpectations);
    offeringPC = null;
    shutdownPC(answeringPC, answeringExpectations);
    answeringPC = null;
    factory.dispose();
    System.gc();
  }

  // Flaky on Android. See webrtc:7761
  @DisabledTest
  @Test
  @MediumTest
  public void testTrackRemovalAndAddition() throws Exception {
    // Allow loopback interfaces too since our Android devices often don't
    // have those.
    PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
    options.networkIgnoreMask = 0;
    PeerConnectionFactory factory = new PeerConnectionFactory(options);

    MediaConstraints pcConstraints = new MediaConstraints();
    pcConstraints.mandatory.add(new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));

    LinkedList<PeerConnection.IceServer> iceServers = new LinkedList<PeerConnection.IceServer>();
    iceServers.add(new PeerConnection.IceServer("stun:stun.l.google.com:19302"));

    ObserverExpectations offeringExpectations = new ObserverExpectations("PCTest:offerer");
    PeerConnection offeringPC =
        factory.createPeerConnection(iceServers, pcConstraints, offeringExpectations);
    assertNotNull(offeringPC);

    ObserverExpectations answeringExpectations = new ObserverExpectations("PCTest:answerer");
    PeerConnection answeringPC =
        factory.createPeerConnection(iceServers, pcConstraints, answeringExpectations);
    assertNotNull(answeringPC);

    // We want to use the same camera for offerer & answerer, so create it here
    // instead of in addTracksToPC.
    final CameraEnumerator enumerator = new Camera1Enumerator(false /* captureToTexture */);
    final VideoCapturer videoCapturer =
        enumerator.createCapturer(enumerator.getDeviceNames()[0], null);
    final VideoSource videoSource = factory.createVideoSource(videoCapturer);
    videoCapturer.startCapture(640, 480, 30);

    // Add offerer media stream.
    offeringExpectations.expectRenegotiationNeeded();
    WeakReference<MediaStream> oLMS =
        addTracksToPC(factory, offeringPC, videoSource, "offeredMediaStream", "offeredVideoTrack",
            "offeredAudioTrack", new ExpectedResolutionSetter(answeringExpectations));

    offeringExpectations.expectAddTrack(2);
    answeringExpectations.expectAddTrack(2);
    // Create offer.
    SdpObserverLatch sdpLatch = new SdpObserverLatch();
    offeringPC.createOffer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription offerSdp = sdpLatch.getSdp();
    assertEquals(offerSdp.type, SessionDescription.Type.OFFER);
    assertFalse(offerSdp.description.isEmpty());

    // Set local description for offerer.
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.HAVE_LOCAL_OFFER);
    offeringExpectations.expectIceCandidates(2);
    offeringExpectations.expectIceGatheringChange(IceGatheringState.COMPLETE);
    offeringPC.setLocalDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    // Set remote description for answerer.
    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(SignalingState.HAVE_REMOTE_OFFER);
    answeringExpectations.expectAddStream("offeredMediaStream");
    answeringPC.setRemoteDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    // Add answerer media stream.
    answeringExpectations.expectRenegotiationNeeded();
    WeakReference<MediaStream> aLMS = addTracksToPC(factory, answeringPC, videoSource,
        "answeredMediaStream", "answeredVideoTrack", "answeredAudioTrack",
        new ExpectedResolutionSetter(offeringExpectations));

    // Create answer.
    sdpLatch = new SdpObserverLatch();
    answeringPC.createAnswer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription answerSdp = sdpLatch.getSdp();
    assertEquals(answerSdp.type, SessionDescription.Type.ANSWER);
    assertFalse(answerSdp.description.isEmpty());

    // Set local description for answerer.
    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(SignalingState.STABLE);
    answeringExpectations.expectIceCandidates(2);
    answeringExpectations.expectIceGatheringChange(IceGatheringState.COMPLETE);
    answeringPC.setLocalDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    // Set remote description for offerer.
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.STABLE);
    offeringExpectations.expectAddStream("answeredMediaStream");

    offeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    offeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);
    // TODO(bemasc): uncomment once delivery of ICECompleted is reliable
    // (https://code.google.com/p/webrtc/issues/detail?id=3021).
    //
    // offeringExpectations.expectIceConnectionChange(
    //     IceConnectionState.COMPLETED);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);

    offeringPC.setRemoteDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    // Wait for at least one ice candidate from the offering PC and forward them to the answering
    // PC.
    for (IceCandidate candidate : offeringExpectations.getAtLeastOneIceCandidate()) {
      answeringPC.addIceCandidate(candidate);
    }

    // Wait for at least one ice candidate from the answering PC and forward them to the offering
    // PC.
    for (IceCandidate candidate : answeringExpectations.getAtLeastOneIceCandidate()) {
      offeringPC.addIceCandidate(candidate);
    }

    // Wait for one frame of the correct size to be delivered.
    // Otherwise we could get a dummy black frame of unexpcted size when the
    // video track is removed.
    offeringExpectations.expectFramesDelivered(1);
    answeringExpectations.expectFramesDelivered(1);

    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    assertEquals(PeerConnection.SignalingState.STABLE, offeringPC.signalingState());
    assertEquals(PeerConnection.SignalingState.STABLE, answeringPC.signalingState());

    // Now do another negotiation, removing the video track from one peer.
    // This previously caused a crash on pc.dispose().
    // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=5128
    VideoTrack offererVideoTrack = oLMS.get().videoTracks.get(0);
    // Note that when we call removeTrack, we regain responsibility for
    // disposing of the track.
    oLMS.get().removeTrack(offererVideoTrack);
    negotiate(offeringPC, offeringExpectations, answeringPC, answeringExpectations);

    // Make sure the track was really removed.
    // TODO(deadbeef): Currently the expectation is that the video track's
    // state will be set to "ended". However, in the future, it's likely that
    // the video track will be completely removed from the remote stream
    // (as it is on the C++ level).
    MediaStream aRMS = answeringExpectations.gotRemoteStreams.iterator().next();
    assertEquals(aRMS.videoTracks.get(0).state(), MediaStreamTrack.State.ENDED);

    // Add the video track to test if the answeringPC will create a new track
    // for the updated remote description.
    oLMS.get().addTrack(offererVideoTrack);
    // The answeringPC sets the updated remote description with a track added.
    // So the onAddTrack callback is expected to be called once.
    answeringExpectations.expectAddTrack(1);
    offeringExpectations.expectAddTrack(0);
    negotiate(offeringPC, offeringExpectations, answeringPC, answeringExpectations);

    // Finally, remove both the audio and video tracks, which should completely
    // remove the remote stream. This used to trigger an assert.
    // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=5128
    oLMS.get().removeTrack(offererVideoTrack);
    AudioTrack offererAudioTrack = oLMS.get().audioTracks.get(0);
    oLMS.get().removeTrack(offererAudioTrack);

    answeringExpectations.expectRemoveStream("offeredMediaStream");
    negotiate(offeringPC, offeringExpectations, answeringPC, answeringExpectations);

    // Make sure the stream was really removed.
    assertTrue(answeringExpectations.gotRemoteStreams.isEmpty());

    // Free the Java-land objects and collect them.
    shutdownPC(offeringPC, offeringExpectations);
    offeringPC = null;
    shutdownPC(answeringPC, answeringExpectations);
    answeringPC = null;
    offererVideoTrack.dispose();
    offererAudioTrack.dispose();
    videoCapturer.stopCapture();
    videoCapturer.dispose();
    videoSource.dispose();
    factory.dispose();
    System.gc();
  }

  private static void negotiate(PeerConnection offeringPC,
      ObserverExpectations offeringExpectations, PeerConnection answeringPC,
      ObserverExpectations answeringExpectations) {
    // Create offer.
    SdpObserverLatch sdpLatch = new SdpObserverLatch();
    offeringPC.createOffer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription offerSdp = sdpLatch.getSdp();
    assertEquals(offerSdp.type, SessionDescription.Type.OFFER);
    assertFalse(offerSdp.description.isEmpty());

    // Set local description for offerer.
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.HAVE_LOCAL_OFFER);
    offeringPC.setLocalDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    // Set remote description for answerer.
    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(SignalingState.HAVE_REMOTE_OFFER);
    answeringPC.setRemoteDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    // Create answer.
    sdpLatch = new SdpObserverLatch();
    answeringPC.createAnswer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription answerSdp = sdpLatch.getSdp();
    assertEquals(answerSdp.type, SessionDescription.Type.ANSWER);
    assertFalse(answerSdp.description.isEmpty());

    // Set local description for answerer.
    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(SignalingState.STABLE);
    answeringPC.setLocalDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    // Set remote description for offerer.
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.STABLE);
    offeringPC.setRemoteDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());
  }

  private static void getMetrics() {
    Metrics metrics = Metrics.getAndReset();
    assertTrue(metrics.map.size() > 0);
    // Test for example that the lifetime of a Call is recorded.
    String name = "WebRTC.Call.LifetimeInSeconds";
    assertTrue(metrics.map.containsKey(name));
    HistogramInfo info = metrics.map.get(name);
    assertTrue(info.samples.size() > 0);
  }

  private static void shutdownPC(PeerConnection pc, ObserverExpectations expectations) {
    if (expectations.dataChannel != null) {
      expectations.dataChannel.unregisterObserver();
      expectations.dataChannel.dispose();
    }

    // Call getStats (old implementation) before shutting down PC.
    expectations.expectOldStatsCallback();
    assertTrue(pc.getStats(expectations, null));
    assertTrue(expectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    // Call the new getStats implementation as well.
    expectations.expectNewStatsCallback();
    pc.getStats(expectations);
    assertTrue(expectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    expectations.expectIceConnectionChange(IceConnectionState.CLOSED);
    expectations.expectSignalingChange(SignalingState.CLOSED);
    pc.close();
    assertTrue(expectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    // Call getStats (old implementation) after calling close(). Should still
    // work.
    expectations.expectOldStatsCallback();
    assertTrue(pc.getStats(expectations, null));
    assertTrue(expectations.waitForAllExpectationsToBeSatisfied(TIMEOUT_SECONDS));

    System.out.println("FYI stats: ");
    int reportIndex = -1;
    for (StatsReport[] reports : expectations.takeStatsReports()) {
      System.out.println(" Report #" + (++reportIndex));
      for (int i = 0; i < reports.length; ++i) {
        System.out.println("  " + reports[i].toString());
      }
    }
    assertEquals(1, reportIndex);
    System.out.println("End stats.");

    pc.dispose();
  }

  // Returns a set of thread IDs belonging to this process, as Strings.
  private static TreeSet<String> allThreads() {
    TreeSet<String> threads = new TreeSet<String>();
    // This pokes at /proc instead of using the Java APIs because we're also
    // looking for libjingle/webrtc native threads, most of which won't have
    // attached to the JVM.
    for (String threadId : (new File("/proc/self/task")).list()) {
      threads.add(threadId);
    }
    return threads;
  }
}
