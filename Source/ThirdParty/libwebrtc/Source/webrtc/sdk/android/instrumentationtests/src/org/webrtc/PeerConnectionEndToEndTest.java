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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.TreeSet;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.junit.Before;
import org.junit.Test;
import org.webrtc.PeerConnection.IceConnectionState;
import org.webrtc.PeerConnection.IceGatheringState;
import org.webrtc.PeerConnection.PeerConnectionState;
import org.webrtc.PeerConnection.SignalingState;

/** End-to-end tests for {@link PeerConnection}. */
public class PeerConnectionEndToEndTest {
  private static final String TAG = "PeerConnectionEndToEndTest";
  private static final int DEFAULT_TIMEOUT_SECONDS = 20;
  private static final int SHORT_TIMEOUT_SECONDS = 5;

  @Before
  public void setUp() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setNativeLibraryName(TestConstants.NATIVE_LIBRARY)
                                         .createInitializationOptions());
  }

  private static class ObserverExpectations
      implements PeerConnection.Observer, VideoSink, DataChannel.Observer, StatsObserver,
                 RTCStatsCollectorCallback, RtpReceiver.Observer {
    private final String name;
    private int expectedIceCandidates;
    private int expectedErrors;
    private int expectedRenegotiations;
    private int expectedWidth;
    private int expectedHeight;
    private int expectedFramesDelivered;
    private int expectedTracksAdded;
    private Queue<SignalingState> expectedSignalingChanges = new ArrayDeque<>();
    private Queue<IceConnectionState> expectedIceConnectionChanges = new ArrayDeque<>();
    private Queue<IceConnectionState> expectedStandardizedIceConnectionChanges = new ArrayDeque<>();
    private Queue<PeerConnectionState> expectedConnectionChanges = new ArrayDeque<>();
    private Queue<IceGatheringState> expectedIceGatheringChanges = new ArrayDeque<>();
    private Queue<String> expectedAddStreamLabels = new ArrayDeque<>();
    private Queue<String> expectedRemoveStreamLabels = new ArrayDeque<>();
    private final List<IceCandidate> gotIceCandidates = new ArrayList<>();
    private Map<MediaStream, WeakReference<VideoSink>> videoSinks = new IdentityHashMap<>();
    private DataChannel dataChannel;
    private Queue<DataChannel.Buffer> expectedBuffers = new ArrayDeque<>();
    private Queue<DataChannel.State> expectedStateChanges = new ArrayDeque<>();
    private Queue<String> expectedRemoteDataChannelLabels = new ArrayDeque<>();
    private int expectedOldStatsCallbacks;
    private int expectedNewStatsCallbacks;
    private List<StatsReport[]> gotStatsReports = new ArrayList<>();
    private final HashSet<MediaStream> gotRemoteStreams = new HashSet<>();
    private int expectedFirstAudioPacket;
    private int expectedFirstVideoPacket;

    public ObserverExpectations(String name) {
      this.name = name;
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void setDataChannel(DataChannel dataChannel) {
      assertNull(this.dataChannel);
      this.dataChannel = dataChannel;
      this.dataChannel.registerObserver(this);
      assertNotNull(this.dataChannel);
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectIceCandidates(int count) {
      expectedIceCandidates += count;
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onIceCandidate(IceCandidate candidate) {
      Logging.d(TAG, "onIceCandidate: " + candidate.toString());
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
    public void onIceCandidateError(IceCandidateErrorEvent event) {}

    @Override
    public void onIceCandidatesRemoved(IceCandidate[] candidates) {}

    @Override
    public void onSelectedCandidatePairChanged(CandidatePairChangeEvent event) {}

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void setExpectedResolution(int width, int height) {
      expectedWidth = width;
      expectedHeight = height;
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectFramesDelivered(int count) {
      expectedFramesDelivered += count;
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onFrame(VideoFrame frame) {
      if (expectedFramesDelivered <= 0) {
        return;
      }
      assertTrue(expectedWidth > 0);
      assertTrue(expectedHeight > 0);
      assertEquals(expectedWidth, frame.getRotatedWidth());
      assertEquals(expectedHeight, frame.getRotatedHeight());
      --expectedFramesDelivered;
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectSignalingChange(SignalingState newState) {
      expectedSignalingChanges.add(newState);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onSignalingChange(SignalingState newState) {
      assertEquals(expectedSignalingChanges.remove(), newState);
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectIceConnectionChange(IceConnectionState newState) {
      expectedIceConnectionChanges.add(newState);
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectStandardizedIceConnectionChange(IceConnectionState newState) {
      expectedStandardizedIceConnectionChanges.add(newState);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onIceConnectionChange(IceConnectionState newState) {
      // TODO(bemasc): remove once delivery of ICECompleted is reliable
      // (https://code.google.com/p/webrtc/issues/detail?id=3021).
      if (newState.equals(IceConnectionState.COMPLETED)) {
        return;
      }

      if (expectedIceConnectionChanges.isEmpty()) {
        Logging.d(TAG, name + "Got an unexpected ICE connection change " + newState);
        return;
      }

      assertEquals(expectedIceConnectionChanges.remove(), newState);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onStandardizedIceConnectionChange(IceConnectionState newState) {
      if (newState.equals(IceConnectionState.COMPLETED)) {
        return;
      }

      if (expectedIceConnectionChanges.isEmpty()) {
        Logging.d(TAG, name + "Got an unexpected standardized ICE connection change " + newState);
        return;
      }

      assertEquals(expectedStandardizedIceConnectionChanges.remove(), newState);
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectConnectionChange(PeerConnectionState newState) {
      expectedConnectionChanges.add(newState);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onConnectionChange(PeerConnectionState newState) {
      if (expectedConnectionChanges.isEmpty()) {
        Logging.d(TAG, name + " got an unexpected DTLS connection change " + newState);
        return;
      }

      assertEquals(expectedConnectionChanges.remove(), newState);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onIceConnectionReceivingChange(boolean receiving) {
      Logging.d(TAG, name + " got an ICE connection receiving change " + receiving);
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectIceGatheringChange(IceGatheringState newState) {
      expectedIceGatheringChanges.add(newState);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onIceGatheringChange(IceGatheringState newState) {
      // It's fine to get a variable number of GATHERING messages before
      // COMPLETE fires (depending on how long the test runs) so we don't assert
      // any particular count.
      if (newState == IceGatheringState.GATHERING) {
        return;
      }
      if (expectedIceGatheringChanges.isEmpty()) {
        Logging.d(TAG, name + "Got an unexpected ICE gathering change " + newState);
      }
      assertEquals(expectedIceGatheringChanges.remove(), newState);
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectAddStream(String label) {
      expectedAddStreamLabels.add(label);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onAddStream(MediaStream stream) {
      assertEquals(expectedAddStreamLabels.remove(), stream.getId());
      for (AudioTrack track : stream.audioTracks) {
        assertEquals("audio", track.kind());
      }
      for (VideoTrack track : stream.videoTracks) {
        assertEquals("video", track.kind());
        track.addSink(this);
        assertNull(videoSinks.put(stream, new WeakReference<VideoSink>(this)));
      }
      gotRemoteStreams.add(stream);
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectRemoveStream(String label) {
      expectedRemoveStreamLabels.add(label);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onRemoveStream(MediaStream stream) {
      assertEquals(expectedRemoveStreamLabels.remove(), stream.getId());
      WeakReference<VideoSink> videoSink = videoSinks.remove(stream);
      assertNotNull(videoSink);
      assertNotNull(videoSink.get());
      for (VideoTrack videoTrack : stream.videoTracks) {
        videoTrack.removeSink(videoSink.get());
      }
      gotRemoteStreams.remove(stream);
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectDataChannel(String label) {
      expectedRemoteDataChannelLabels.add(label);
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onDataChannel(DataChannel remoteDataChannel) {
      assertEquals(expectedRemoteDataChannelLabels.remove(), remoteDataChannel.label());
      setDataChannel(remoteDataChannel);
      assertEquals(DataChannel.State.CONNECTING, dataChannel.state());
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectRenegotiationNeeded() {
      ++expectedRenegotiations;
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onRenegotiationNeeded() {
      assertTrue(--expectedRenegotiations >= 0);
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectAddTrack(int expectedTracksAdded) {
      this.expectedTracksAdded = expectedTracksAdded;
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onAddTrack(RtpReceiver receiver, MediaStream[] mediaStreams) {
      expectedTracksAdded--;
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectMessage(ByteBuffer expectedBuffer, boolean expectedBinary) {
      expectedBuffers.add(new DataChannel.Buffer(expectedBuffer, expectedBinary));
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onMessage(DataChannel.Buffer buffer) {
      DataChannel.Buffer expected = expectedBuffers.remove();
      assertEquals(expected.binary, buffer.binary);
      assertTrue(expected.data.equals(buffer.data));
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onBufferedAmountChange(long previousAmount) {
      assertFalse(previousAmount == dataChannel.bufferedAmount());
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onStateChange() {
      assertEquals(expectedStateChanges.remove(), dataChannel.state());
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectStateChange(DataChannel.State state) {
      expectedStateChanges.add(state);
    }

    // Old getStats callback.
    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onComplete(StatsReport[] reports) {
      if (--expectedOldStatsCallbacks < 0) {
        throw new RuntimeException("Unexpected stats report: " + Arrays.toString(reports));
      }
      gotStatsReports.add(reports);
    }

    // New getStats callback.
    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onStatsDelivered(RTCStatsReport report) {
      if (--expectedNewStatsCallbacks < 0) {
        throw new RuntimeException("Unexpected stats report: " + report);
      }
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
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

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectFirstPacketReceived() {
      expectedFirstAudioPacket = 1;
      expectedFirstVideoPacket = 1;
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectOldStatsCallback() {
      ++expectedOldStatsCallbacks;
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void expectNewStatsCallback() {
      ++expectedNewStatsCallbacks;
    }

    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized List<StatsReport[]> takeStatsReports() {
      List<StatsReport[]> got = gotStatsReports;
      gotStatsReports = new ArrayList<StatsReport[]>();
      return got;
    }

    // Return a set of expectations that haven't been satisfied yet, possibly
    // empty if no such expectations exist.
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
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
          Logging.d(TAG,
              name + " still waiting at\n    " + (new Throwable()).getStackTrace()[1]
                  + "\n    for: " + Arrays.toString(stillWaitingForExpectations.toArray()));
        }
        if (endTime < System.currentTimeMillis()) {
          Logging.d(TAG,
              name + " timed out waiting for: "
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
        Logging.d(
            TAG, name + " didn't need to wait at\n    " + (new Throwable()).getStackTrace()[1]);
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
        return new ArrayList<IceCandidate>(gotIceCandidates);
      }
    }
  }

  // Sets the expected resolution for an ObserverExpectations once a frame
  // has been captured.
  private static class ExpectedResolutionSetter implements VideoSink {
    private ObserverExpectations observer;

    public ExpectedResolutionSetter(ObserverExpectations observer) {
      this.observer = observer;
    }

    @Override
    // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public synchronized void onFrame(VideoFrame frame) {
      // Because different camera devices (fake & physical) produce different
      // resolutions, we only sanity-check the set sizes,
      assertTrue(frame.getRotatedWidth() > 0);
      assertTrue(frame.getRotatedHeight() > 0);
      observer.setExpectedResolution(frame.getRotatedWidth(), frame.getRotatedHeight());
      frame.retain();
    }
  }

  static class SdpObserverLatch implements SdpObserver {
    private boolean success;
    private @Nullable SessionDescription sdp;
    private @Nullable String error;
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

    public @Nullable SessionDescription getSdp() {
      return sdp;
    }

    public @Nullable String getError() {
      return error;
    }
  }

  // Return a weak reference to test that ownership is correctly held by
  // PeerConnection, not by test code.
  private static WeakReference<MediaStream> addTracksToPC(PeerConnectionFactory factory,
      PeerConnection pc, VideoSource videoSource, String streamLabel, String videoTrackId,
      String audioTrackId, VideoSink videoSink) {
    return addTracksToPC(factory, pc, videoSource, streamLabel, videoTrackId, audioTrackId,
        videoSink, /*useAddStream=*/false);
  }
  private static WeakReference<MediaStream> addTracksToPC(PeerConnectionFactory factory,
      PeerConnection pc, VideoSource videoSource, String streamLabel, String videoTrackId,
      String audioTrackId, VideoSink videoSink, boolean useAddStream) {
    MediaStream lMS = factory.createLocalMediaStream(streamLabel);
    VideoTrack videoTrack = factory.createVideoTrack(videoTrackId, videoSource);
    assertNotNull(videoTrack);
    assertNotNull(videoSink);
    videoTrack.addSink(videoSink);
    lMS.addTrack(videoTrack);
    // Just for fun, let's remove and re-add the track.
    lMS.removeTrack(videoTrack);
    lMS.addTrack(videoTrack);
    lMS.addTrack(
        factory.createAudioTrack(audioTrackId, factory.createAudioSource(new MediaConstraints())));
    if (!useAddStream) {
      // In Unified Plan, addTrack() is the preferred way of adding tracks.
      for (AudioTrack track : lMS.audioTracks) {
        pc.addTrack(track, Collections.singletonList(lMS.getId()));
      }
      for (VideoTrack track : lMS.videoTracks) {
        pc.addTrack(track, Collections.singletonList(lMS.getId()));
      }
    } else {
      // Only in Plan B is addStream() supported. Used by a legacy test not yet
      // updated to Unified Plan.
      // TODO(https://crbug.com/webrtc/13528): Remove use of addStream().
      pc.addStream(lMS);
    }
    return new WeakReference<MediaStream>(lMS);
  }

  @Test
  @MediumTest
  public void testCompleteSession() throws Exception {
    Metrics.enable();
    // Allow loopback interfaces too since our Android devices often don't
    // have those.
    PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
    options.networkIgnoreMask = 0;
    PeerConnectionFactory factory = PeerConnectionFactory.builder()
                                        .setOptions(options)
                                        .setVideoEncoderFactory(new SoftwareVideoEncoderFactory())
                                        .setVideoDecoderFactory(new SoftwareVideoDecoderFactory())
                                        .createPeerConnectionFactory();

    List<PeerConnection.IceServer> iceServers = new ArrayList<>();
    iceServers.add(
        PeerConnection.IceServer.builder("stun:stun.l.google.com:19302").createIceServer());
    iceServers.add(PeerConnection.IceServer.builder("turn:fake.example.com")
                       .setUsername("fakeUsername")
                       .setPassword("fakePassword")
                       .createIceServer());

    PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServers);
    rtcConfig.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;

    ObserverExpectations offeringExpectations = new ObserverExpectations("PCTest:offerer");
    PeerConnection offeringPC = factory.createPeerConnection(rtcConfig, offeringExpectations);
    assertNotNull(offeringPC);

    ObserverExpectations answeringExpectations = new ObserverExpectations("PCTest:answerer");
    PeerConnection answeringPC = factory.createPeerConnection(rtcConfig, answeringExpectations);
    assertNotNull(answeringPC);

    // We want to use the same camera for offerer & answerer, so create it here
    // instead of in addTracksToPC.
    final CameraEnumerator enumerator = new Camera1Enumerator(false /* captureToTexture */);
    final VideoCapturer videoCapturer =
        enumerator.createCapturer(enumerator.getDeviceNames()[0], null /* eventsHandler */);
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper", /* sharedContext= */ null);
    final VideoSource videoSource = factory.createVideoSource(/* isScreencast= */ false);
    videoCapturer.initialize(surfaceTextureHelper, InstrumentationRegistry.getTargetContext(),
        videoSource.getCapturerObserver());
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
    answeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTING);
    answeringPC.setLocalDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.HAVE_LOCAL_OFFER);
    offeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTING);
    offeringPC.setLocalDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.STABLE);
    offeringExpectations.expectAddStream("answeredMediaStream");

    offeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    offeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);
    offeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CHECKING);
    offeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CONNECTED);
    offeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTED);
    // TODO(bemasc): uncomment once delivery of ICECompleted is reliable
    // (https://code.google.com/p/webrtc/issues/detail?id=3021).
    //
    // offeringExpectations.expectIceConnectionChange(
    //     IceConnectionState.COMPLETED);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);
    answeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CHECKING);
    answeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CONNECTED);
    answeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTED);

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

    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

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
    assertNull(rtpParameters.encodings.get(0).minBitrateBps);
    assertNull(rtpParameters.encodings.get(0).maxFramerate);
    assertNull(rtpParameters.encodings.get(0).numTemporalLayers);
    assertNull(rtpParameters.encodings.get(0).scaleResolutionDownBy);
    assertTrue(rtpParameters.encodings.get(0).rid.isEmpty());

    rtpParameters.encodings.get(0).maxBitrateBps = 300000;
    rtpParameters.encodings.get(0).minBitrateBps = 100000;
    rtpParameters.encodings.get(0).maxFramerate = 20;
    rtpParameters.encodings.get(0).numTemporalLayers = 2;
    rtpParameters.encodings.get(0).scaleResolutionDownBy = 2.0;
    assertTrue(videoSender.setParameters(rtpParameters));

    // Create a DTMF sender.
    DtmfSender dtmfSender = audioSender.dtmf();
    assertNotNull(dtmfSender);
    assertTrue(dtmfSender.canInsertDtmf());
    assertTrue(dtmfSender.insertDtmf("123", 300, 100));

    // Verify that we can read back the updated value.
    rtpParameters = videoSender.getParameters();
    assertEquals(300000, (int) rtpParameters.encodings.get(0).maxBitrateBps);
    assertEquals(100000, (int) rtpParameters.encodings.get(0).minBitrateBps);
    assertEquals(20, (int) rtpParameters.encodings.get(0).maxFramerate);
    assertEquals(2, (int) rtpParameters.encodings.get(0).numTemporalLayers);
    assertThat(rtpParameters.encodings.get(0).scaleResolutionDownBy).isEqualTo(2.0);

    // Test send & receive UTF-8 text.
    answeringExpectations.expectMessage(
        ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    DataChannel.Buffer buffer =
        new DataChannel.Buffer(ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    assertTrue(offeringExpectations.dataChannel.send(buffer));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

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
    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    offeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    answeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    offeringExpectations.expectStateChange(DataChannel.State.CLOSED);
    answeringExpectations.expectStateChange(DataChannel.State.CLOSED);
    offeringExpectations.dataChannel.close();
    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    // Test SetBitrate.
    assertTrue(offeringPC.setBitrate(100000, 5000000, 500000000));
    assertFalse(offeringPC.setBitrate(3, 2, 1));

    // Test getStats by Sender interface
    offeringExpectations.expectNewStatsCallback();
    offeringPC.getStats(videoSender, offeringExpectations);
    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    // Test getStats by Receiver interface
    RtpReceiver videoReceiver = null;
    for (RtpReceiver receiver : answeringPC.getReceivers()) {
      if (receiver.track().kind().equals("video")) {
        videoReceiver = receiver;
      }
    }
    assertNotNull(videoReceiver);
    answeringExpectations.expectNewStatsCallback();
    answeringPC.getStats(videoReceiver, answeringExpectations);
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    // Free the Java-land objects and collect them.
    shutdownPC(offeringPC, offeringExpectations);
    offeringPC = null;
    shutdownPC(answeringPC, answeringExpectations);
    answeringPC = null;
    videoCapturer.stopCapture();
    videoCapturer.dispose();
    videoSource.dispose();
    surfaceTextureHelper.dispose();
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
    PeerConnectionFactory factory =
        PeerConnectionFactory.builder().setOptions(options).createPeerConnectionFactory();

    List<PeerConnection.IceServer> iceServers = new ArrayList<>();
    iceServers.add(
        PeerConnection.IceServer.builder("stun:stun.l.google.com:19302").createIceServer());
    iceServers.add(PeerConnection.IceServer.builder("turn:fake.example.com")
                       .setUsername("fakeUsername")
                       .setPassword("fakePassword")
                       .createIceServer());

    PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServers);
    rtcConfig.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;

    ObserverExpectations offeringExpectations = new ObserverExpectations("PCTest:offerer");
    PeerConnection offeringPC = factory.createPeerConnection(rtcConfig, offeringExpectations);
    assertNotNull(offeringPC);

    ObserverExpectations answeringExpectations = new ObserverExpectations("PCTest:answerer");
    PeerConnection answeringPC = factory.createPeerConnection(rtcConfig, answeringExpectations);
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
    answeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTING);
    answeringPC.setLocalDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.HAVE_LOCAL_OFFER);
    offeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTING);
    offeringPC.setLocalDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.STABLE);

    offeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    offeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);
    offeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CHECKING);
    offeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CONNECTED);
    offeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTED);
    // TODO(bemasc): uncomment once delivery of ICECompleted is reliable
    // (https://code.google.com/p/webrtc/issues/detail?id=3021).
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);
    answeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CHECKING);
    answeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CONNECTED);
    answeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTED);

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

    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    assertEquals(PeerConnection.SignalingState.STABLE, offeringPC.signalingState());
    assertEquals(PeerConnection.SignalingState.STABLE, answeringPC.signalingState());

    // Test send & receive UTF-8 text.
    answeringExpectations.expectMessage(
        ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    DataChannel.Buffer buffer =
        new DataChannel.Buffer(ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    assertTrue(offeringExpectations.dataChannel.send(buffer));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

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
    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    offeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    answeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    offeringExpectations.expectStateChange(DataChannel.State.CLOSED);
    answeringExpectations.expectStateChange(DataChannel.State.CLOSED);
    offeringExpectations.dataChannel.close();
    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    // Free the Java-land objects and collect them.
    shutdownPC(offeringPC, offeringExpectations);
    offeringPC = null;
    shutdownPC(answeringPC, answeringExpectations);
    answeringPC = null;
    factory.dispose();
    System.gc();
  }

  // Tests that ICE candidates that are not allowed by an ICE transport type, thus not being
  // signaled to the gathering PeerConnection, can be surfaced via configuration if allowed by the
  // new ICE transport type, when RTCConfiguration.surfaceIceCandidatesOnIceTransportTypeChanged is
  // true.
  @Test
  @SmallTest
  public void testSurfaceIceCandidatesWhenIceTransportTypeChanged() throws Exception {
    // For this test, we only need one PeerConnection to observe the behavior of gathering, and we
    // create only the offering PC below.
    //
    // Allow loopback interfaces too since our Android devices often don't
    // have those.
    PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
    options.networkIgnoreMask = 0;
    PeerConnectionFactory factory =
        PeerConnectionFactory.builder().setOptions(options).createPeerConnectionFactory();

    PeerConnection.RTCConfiguration rtcConfig =
        new PeerConnection.RTCConfiguration(Collections.emptyList());
    rtcConfig.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;
    // NONE would prevent any candidate being signaled to the PC.
    rtcConfig.iceTransportsType = PeerConnection.IceTransportsType.NONE;
    // We must have the continual gathering enabled to allow the surfacing of candidates on the ICE
    // transport type change.
    rtcConfig.continualGatheringPolicy = PeerConnection.ContinualGatheringPolicy.GATHER_CONTINUALLY;
    rtcConfig.surfaceIceCandidatesOnIceTransportTypeChanged = true;

    ObserverExpectations offeringExpectations = new ObserverExpectations("PCTest:offerer");
    PeerConnection offeringPC = factory.createPeerConnection(rtcConfig, offeringExpectations);
    assertNotNull(offeringPC);

    // Create a data channel and set local description to kick off the ICE candidate gathering.
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
    offeringExpectations.expectSignalingChange(SignalingState.HAVE_LOCAL_OFFER);
    offeringPC.setLocalDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    assertEquals(offeringPC.getLocalDescription().type, offerSdp.type);

    // Wait until we satisfy all expectations in the setup.
    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    // Add the expectation of gathering at least one candidate, which should however fail because of
    // the transport type NONE.
    offeringExpectations.expectIceCandidates(1);
    assertFalse(offeringExpectations.waitForAllExpectationsToBeSatisfied(SHORT_TIMEOUT_SECONDS));

    // Change the transport type and we should be able to meet the expectation of gathering this
    // time.
    rtcConfig.iceTransportsType = PeerConnection.IceTransportsType.ALL;
    offeringPC.setConfiguration(rtcConfig);
    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));
  }

  @Test
  @MediumTest
  public void testTrackRemovalAndAddition() throws Exception {
    // Allow loopback interfaces too since our Android devices often don't
    // have those.
    PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
    options.networkIgnoreMask = 0;
    PeerConnectionFactory factory = PeerConnectionFactory.builder()
                                        .setOptions(options)
                                        .setVideoEncoderFactory(new SoftwareVideoEncoderFactory())
                                        .setVideoDecoderFactory(new SoftwareVideoDecoderFactory())
                                        .createPeerConnectionFactory();

    List<PeerConnection.IceServer> iceServers = new ArrayList<>();
    iceServers.add(
        PeerConnection.IceServer.builder("stun:stun.l.google.com:19302").createIceServer());

    PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServers);
    // TODO(https://crbug.com/webrtc/13528): Update test not to use Plan B.
    rtcConfig.sdpSemantics = PeerConnection.SdpSemantics.PLAN_B;

    ObserverExpectations offeringExpectations = new ObserverExpectations("PCTest:offerer");
    PeerConnection offeringPC = factory.createPeerConnection(rtcConfig, offeringExpectations);
    assertNotNull(offeringPC);

    ObserverExpectations answeringExpectations = new ObserverExpectations("PCTest:answerer");
    PeerConnection answeringPC = factory.createPeerConnection(rtcConfig, answeringExpectations);
    assertNotNull(answeringPC);

    // We want to use the same camera for offerer & answerer, so create it here
    // instead of in addTracksToPC.
    final CameraEnumerator enumerator = new Camera1Enumerator(false /* captureToTexture */);
    final VideoCapturer videoCapturer =
        enumerator.createCapturer(enumerator.getDeviceNames()[0], null /* eventsHandler */);
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper", /* sharedContext= */ null);
    final VideoSource videoSource = factory.createVideoSource(/* isScreencast= */ false);
    videoCapturer.initialize(surfaceTextureHelper, InstrumentationRegistry.getTargetContext(),
        videoSource.getCapturerObserver());
    videoCapturer.startCapture(640, 480, 30);

    // Add offerer media stream.
    offeringExpectations.expectRenegotiationNeeded();
    WeakReference<MediaStream> oLMS =
        addTracksToPC(factory, offeringPC, videoSource, "offeredMediaStream", "offeredVideoTrack",
            "offeredAudioTrack", new ExpectedResolutionSetter(answeringExpectations),
            /*useAddStream=*/true);

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
    offeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTING);
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
        new ExpectedResolutionSetter(offeringExpectations),
        /*useAddStream=*/true);

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
    answeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTING);
    answeringPC.setLocalDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    // Set remote description for offerer.
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.STABLE);
    offeringExpectations.expectAddStream("answeredMediaStream");

    offeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    offeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);
    offeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CHECKING);
    offeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CONNECTED);
    offeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTED);
    // TODO(bemasc): uncomment once delivery of ICECompleted is reliable
    // (https://code.google.com/p/webrtc/issues/detail?id=3021).
    //
    // offeringExpectations.expectIceConnectionChange(
    //     IceConnectionState.COMPLETED);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CHECKING);
    answeringExpectations.expectIceConnectionChange(IceConnectionState.CONNECTED);
    answeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CHECKING);
    answeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CONNECTED);
    answeringExpectations.expectConnectionChange(PeerConnectionState.CONNECTED);

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

    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));
    assertTrue(answeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    assertEquals(PeerConnection.SignalingState.STABLE, offeringPC.signalingState());
    assertEquals(PeerConnection.SignalingState.STABLE, answeringPC.signalingState());

    // Now do another negotiation, removing the video track from one peer.
    // This previously caused a crash on pc.dispose().
    // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=5128
    VideoTrack offererVideoTrack = oLMS.get().videoTracks.get(0);
    // Note that when we call removeTrack, we regain responsibility for
    // disposing of the track.
    offeringExpectations.expectRenegotiationNeeded();
    oLMS.get().removeTrack(offererVideoTrack);
    negotiate(offeringPC, offeringExpectations, answeringPC, answeringExpectations);

    // Make sure the track was really removed.
    MediaStream aRMS = answeringExpectations.gotRemoteStreams.iterator().next();
    assertTrue(aRMS.videoTracks.isEmpty());

    // Add the video track to test if the answeringPC will create a new track
    // for the updated remote description.
    offeringExpectations.expectRenegotiationNeeded();
    oLMS.get().addTrack(offererVideoTrack);
    // The answeringPC sets the updated remote description with a track added.
    // So the onAddTrack callback is expected to be called once.
    answeringExpectations.expectAddTrack(1);
    offeringExpectations.expectAddTrack(0);
    negotiate(offeringPC, offeringExpectations, answeringPC, answeringExpectations);

    // Finally, remove both the audio and video tracks, which should completely
    // remove the remote stream. This used to trigger an assert.
    // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=5128
    offeringExpectations.expectRenegotiationNeeded();
    oLMS.get().removeTrack(offererVideoTrack);
    AudioTrack offererAudioTrack = oLMS.get().audioTracks.get(0);
    offeringExpectations.expectRenegotiationNeeded();
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
    surfaceTextureHelper.dispose();
    factory.dispose();
    System.gc();
  }

  /**
   * Test that a Java MediaStream is updated when the native stream is.
   * <p>
   * Specifically, test that when remote tracks are indicated as being added or
   * removed from a MediaStream (via "a=ssrc" or "a=msid" in a remote
   * description), the existing remote MediaStream object is updated.
   * <p>
   * This test starts with just an audio track, adds a video track, then
   * removes it. It only applies remote offers, which is sufficient to test
   * this functionality and simplifies the test. This means that no media will
   * actually be sent/received; we're just testing that the Java MediaStream
   * object gets updated when the native object changes.
   */
  @Test
  @MediumTest
  public void testRemoteStreamUpdatedWhenTracksAddedOrRemoved() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder()
                                        .setVideoEncoderFactory(new SoftwareVideoEncoderFactory())
                                        .setVideoDecoderFactory(new SoftwareVideoDecoderFactory())
                                        .createPeerConnectionFactory();

    // TODO(https://crbug.com/webrtc/13528): Update test not to use Plan B.
    PeerConnection.RTCConfiguration planBConfig =
        new PeerConnection.RTCConfiguration(Collections.emptyList());
    planBConfig.sdpSemantics = PeerConnection.SdpSemantics.PLAN_B;

    // Use OfferToReceiveAudio/Video to ensure every offer has an audio and
    // video m= section. Simplifies the test because it means we don't have to
    // actually apply the offer to "offeringPC"; it's just used as an SDP
    // factory.
    MediaConstraints offerConstraints = new MediaConstraints();
    offerConstraints.mandatory.add(
        new MediaConstraints.KeyValuePair("OfferToReceiveAudio", "true"));
    offerConstraints.mandatory.add(
        new MediaConstraints.KeyValuePair("OfferToReceiveVideo", "true"));

    // This PeerConnection will only be used to generate offers.
    ObserverExpectations offeringExpectations = new ObserverExpectations("offerer");
    PeerConnection offeringPC = factory.createPeerConnection(planBConfig, offeringExpectations);
    assertNotNull(offeringPC);

    ObserverExpectations expectations = new ObserverExpectations("PC under test");
    PeerConnection pcUnderTest = factory.createPeerConnection(planBConfig, expectations);
    assertNotNull(pcUnderTest);

    // Add offerer media stream with just an audio track.
    MediaStream localStream = factory.createLocalMediaStream("stream");
    AudioTrack localAudioTrack =
        factory.createAudioTrack("audio", factory.createAudioSource(new MediaConstraints()));
    localStream.addTrack(localAudioTrack);
    offeringExpectations.expectRenegotiationNeeded();
    RtpSender audioSender =
        offeringPC.addTrack(localAudioTrack, Collections.singletonList(localStream.getId()));
    // Create offer.
    SdpObserverLatch sdpLatch = new SdpObserverLatch();
    offeringPC.createOffer(sdpLatch, offerConstraints);
    assertTrue(sdpLatch.await());
    SessionDescription offerSdp = sdpLatch.getSdp();

    // Apply remote offer to PC under test.
    sdpLatch = new SdpObserverLatch();
    expectations.expectSignalingChange(SignalingState.HAVE_REMOTE_OFFER);
    expectations.expectAddStream("stream");
    pcUnderTest.setRemoteDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    // Sanity check that we get one remote stream with one audio track.
    MediaStream remoteStream = expectations.gotRemoteStreams.iterator().next();
    assertEquals(remoteStream.audioTracks.size(), 1);
    assertEquals(remoteStream.videoTracks.size(), 0);

    // Add a video track...
    final CameraEnumerator enumerator = new Camera1Enumerator(false /* captureToTexture */);
    final VideoCapturer videoCapturer =
        enumerator.createCapturer(enumerator.getDeviceNames()[0], null /* eventsHandler */);
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create("SurfaceTextureHelper", /* sharedContext= */ null);
    final VideoSource videoSource = factory.createVideoSource(/* isScreencast= */ false);
    videoCapturer.initialize(surfaceTextureHelper, InstrumentationRegistry.getTargetContext(),
        videoSource.getCapturerObserver());
    VideoTrack videoTrack = factory.createVideoTrack("video", videoSource);
    offeringExpectations.expectRenegotiationNeeded();
    localStream.addTrack(videoTrack);
    offeringPC.addTrack(videoTrack, Collections.singletonList(localStream.getId()));
    // ... and create an updated offer.
    sdpLatch = new SdpObserverLatch();
    offeringPC.createOffer(sdpLatch, offerConstraints);
    assertTrue(sdpLatch.await());
    offerSdp = sdpLatch.getSdp();

    // Apply remote offer with new video track to PC under test.
    sdpLatch = new SdpObserverLatch();
    pcUnderTest.setRemoteDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    // The remote stream should now have a video track.
    assertEquals(remoteStream.audioTracks.size(), 1);
    assertEquals(remoteStream.videoTracks.size(), 1);

    // Finally, create another offer with the audio track removed.
    offeringExpectations.expectRenegotiationNeeded();
    localStream.removeTrack(localAudioTrack);
    localAudioTrack.dispose();
    offeringPC.removeTrack(audioSender);
    sdpLatch = new SdpObserverLatch();
    offeringPC.createOffer(sdpLatch, offerConstraints);
    assertTrue(sdpLatch.await());
    offerSdp = sdpLatch.getSdp();

    // Apply remote offer with just a video track to PC under test.
    sdpLatch = new SdpObserverLatch();
    pcUnderTest.setRemoteDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    // The remote stream should no longer have an audio track.
    assertEquals(remoteStream.audioTracks.size(), 0);
    assertEquals(remoteStream.videoTracks.size(), 1);

    // Free the Java-land objects. Video capturer and source aren't owned by
    // the PeerConnection and need to be disposed separately.
    // TODO(deadbeef): Should all these events really occur on disposal?
    // "Gathering complete" is especially odd since gathering never started.
    // Note that this test isn't meant to test these events, but we must do
    // this or otherwise it will crash.
    offeringExpectations.expectIceConnectionChange(IceConnectionState.CLOSED);
    offeringExpectations.expectStandardizedIceConnectionChange(IceConnectionState.CLOSED);
    offeringExpectations.expectSignalingChange(SignalingState.CLOSED);
    offeringExpectations.expectIceGatheringChange(IceGatheringState.COMPLETE);
    offeringPC.dispose();
    expectations.expectIceConnectionChange(IceConnectionState.CLOSED);
    expectations.expectStandardizedIceConnectionChange(IceConnectionState.CLOSED);
    expectations.expectSignalingChange(SignalingState.CLOSED);
    expectations.expectIceGatheringChange(IceGatheringState.COMPLETE);
    pcUnderTest.dispose();
    videoCapturer.dispose();
    videoSource.dispose();
    surfaceTextureHelper.dispose();
    factory.dispose();
  }

  @Test
  @SmallTest
  public void testRollback() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    PeerConnection.RTCConfiguration config =
        new PeerConnection.RTCConfiguration(Collections.emptyList());
    config.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;

    ObserverExpectations offeringExpectations = new ObserverExpectations("PCTest:offerer");
    PeerConnection pc = factory.createPeerConnection(config, offeringExpectations);

    SdpObserverLatch sdpLatch = new SdpObserverLatch();
    pc.createOffer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription offer = sdpLatch.getSdp();

    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.HAVE_LOCAL_OFFER);
    pc.setLocalDescription(sdpLatch, offer);
    assertTrue(sdpLatch.await());

    SessionDescription rollback = new SessionDescription(SessionDescription.Type.ROLLBACK, "");
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.STABLE);
    // TODO(bugs.webrtc.org/11970): determine if triggering ONN (twice even) is correct.
    offeringExpectations.expectRenegotiationNeeded();
    offeringExpectations.expectRenegotiationNeeded();
    pc.setLocalDescription(sdpLatch, rollback);
    assertTrue(sdpLatch.await());

    assertTrue(offeringExpectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));
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

  @SuppressWarnings("deprecation") // TODO(sakal): getStats is deprecated
  private static void shutdownPC(PeerConnection pc, ObserverExpectations expectations) {
    if (expectations.dataChannel != null) {
      expectations.dataChannel.unregisterObserver();
      expectations.dataChannel.dispose();
    }

    // Call getStats (old implementation) before shutting down PC.
    expectations.expectOldStatsCallback();
    assertTrue(pc.getStats(expectations, null /* track */));
    assertTrue(expectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    // Call the new getStats implementation as well.
    expectations.expectNewStatsCallback();
    pc.getStats(expectations);
    assertTrue(expectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    expectations.expectIceConnectionChange(IceConnectionState.CLOSED);
    expectations.expectStandardizedIceConnectionChange(IceConnectionState.CLOSED);
    expectations.expectConnectionChange(PeerConnectionState.CLOSED);
    expectations.expectSignalingChange(SignalingState.CLOSED);
    pc.close();
    assertTrue(expectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    // Call getStats (old implementation) after calling close(). Should still
    // work.
    expectations.expectOldStatsCallback();
    assertTrue(pc.getStats(expectations, null /* track */));
    assertTrue(expectations.waitForAllExpectationsToBeSatisfied(DEFAULT_TIMEOUT_SECONDS));

    Logging.d(TAG, "FYI stats: ");
    int reportIndex = -1;
    for (StatsReport[] reports : expectations.takeStatsReports()) {
      Logging.d(TAG, " Report #" + (++reportIndex));
      for (int i = 0; i < reports.length; ++i) {
        Logging.d(TAG, "  " + reports[i].toString());
      }
    }
    assertEquals(1, reportIndex);
    Logging.d(TAG, "End stats.");

    pc.dispose();
  }
}
