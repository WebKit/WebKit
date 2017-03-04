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

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

/**
 * Java-land version of the PeerConnection APIs; wraps the C++ API
 * http://www.webrtc.org/reference/native-apis, which in turn is inspired by the
 * JS APIs: http://dev.w3.org/2011/webrtc/editor/webrtc.html and
 * http://www.w3.org/TR/mediacapture-streams/
 */
public class PeerConnection {
  static {
    System.loadLibrary("jingle_peerconnection_so");
  }

  /** Tracks PeerConnectionInterface::IceGatheringState */
  public enum IceGatheringState { NEW, GATHERING, COMPLETE }

  /** Tracks PeerConnectionInterface::IceConnectionState */
  public enum IceConnectionState {
    NEW,
    CHECKING,
    CONNECTED,
    COMPLETED,
    FAILED,
    DISCONNECTED,
    CLOSED
  }

  /** Tracks PeerConnectionInterface::TlsCertPolicy */
  public enum TlsCertPolicy {
    TLS_CERT_POLICY_SECURE,
    TLS_CERT_POLICY_INSECURE_NO_CHECK,
  }

  /** Tracks PeerConnectionInterface::SignalingState */
  public enum SignalingState {
    STABLE,
    HAVE_LOCAL_OFFER,
    HAVE_LOCAL_PRANSWER,
    HAVE_REMOTE_OFFER,
    HAVE_REMOTE_PRANSWER,
    CLOSED
  }

  /** Java version of PeerConnectionObserver. */
  public static interface Observer {
    /** Triggered when the SignalingState changes. */
    public void onSignalingChange(SignalingState newState);

    /** Triggered when the IceConnectionState changes. */
    public void onIceConnectionChange(IceConnectionState newState);

    /** Triggered when the ICE connection receiving status changes. */
    public void onIceConnectionReceivingChange(boolean receiving);

    /** Triggered when the IceGatheringState changes. */
    public void onIceGatheringChange(IceGatheringState newState);

    /** Triggered when a new ICE candidate has been found. */
    public void onIceCandidate(IceCandidate candidate);

    /** Triggered when some ICE candidates have been removed. */
    public void onIceCandidatesRemoved(IceCandidate[] candidates);

    /** Triggered when media is received on a new stream from remote peer. */
    public void onAddStream(MediaStream stream);

    /** Triggered when a remote peer close a stream. */
    public void onRemoveStream(MediaStream stream);

    /** Triggered when a remote peer opens a DataChannel. */
    public void onDataChannel(DataChannel dataChannel);

    /** Triggered when renegotiation is necessary. */
    public void onRenegotiationNeeded();

    /**
     * Triggered when a new track is signaled by the remote peer, as a result of
     * setRemoteDescription.
     */
    public void onAddTrack(RtpReceiver receiver, MediaStream[] mediaStreams);
  }

  /** Java version of PeerConnectionInterface.IceServer. */
  public static class IceServer {
    public final String uri;
    public final String username;
    public final String password;
    public final TlsCertPolicy tlsCertPolicy;

    /** Convenience constructor for STUN servers. */
    public IceServer(String uri) {
      this(uri, "", "");
    }

    public IceServer(String uri, String username, String password) {
      this(uri, username, password, TlsCertPolicy.TLS_CERT_POLICY_SECURE);
    }

    public IceServer(String uri, String username, String password, TlsCertPolicy tlsCertPolicy) {
      this.uri = uri;
      this.username = username;
      this.password = password;
      this.tlsCertPolicy = tlsCertPolicy;
    }

    public String toString() {
      return uri + " [" + username + ":" + password + "] [" + tlsCertPolicy + "]";
    }
  }

  /** Java version of PeerConnectionInterface.IceTransportsType */
  public enum IceTransportsType { NONE, RELAY, NOHOST, ALL }

  /** Java version of PeerConnectionInterface.BundlePolicy */
  public enum BundlePolicy { BALANCED, MAXBUNDLE, MAXCOMPAT }

  /** Java version of PeerConnectionInterface.RtcpMuxPolicy */
  public enum RtcpMuxPolicy { NEGOTIATE, REQUIRE }

  /** Java version of PeerConnectionInterface.TcpCandidatePolicy */
  public enum TcpCandidatePolicy { ENABLED, DISABLED }

  /** Java version of PeerConnectionInterface.CandidateNetworkPolicy */
  public enum CandidateNetworkPolicy { ALL, LOW_COST }

  /** Java version of rtc::KeyType */
  public enum KeyType { RSA, ECDSA }

  /** Java version of PeerConnectionInterface.ContinualGatheringPolicy */
  public enum ContinualGatheringPolicy { GATHER_ONCE, GATHER_CONTINUALLY }

  /** Java version of PeerConnectionInterface.RTCConfiguration */
  public static class RTCConfiguration {
    public IceTransportsType iceTransportsType;
    public List<IceServer> iceServers;
    public BundlePolicy bundlePolicy;
    public RtcpMuxPolicy rtcpMuxPolicy;
    public TcpCandidatePolicy tcpCandidatePolicy;
    public CandidateNetworkPolicy candidateNetworkPolicy;
    public int audioJitterBufferMaxPackets;
    public boolean audioJitterBufferFastAccelerate;
    public int iceConnectionReceivingTimeout;
    public int iceBackupCandidatePairPingInterval;
    public KeyType keyType;
    public ContinualGatheringPolicy continualGatheringPolicy;
    public int iceCandidatePoolSize;
    public boolean pruneTurnPorts;
    public boolean presumeWritableWhenFullyRelayed;
    public Integer iceCheckMinInterval;

    public RTCConfiguration(List<IceServer> iceServers) {
      iceTransportsType = IceTransportsType.ALL;
      bundlePolicy = BundlePolicy.BALANCED;
      rtcpMuxPolicy = RtcpMuxPolicy.REQUIRE;
      tcpCandidatePolicy = TcpCandidatePolicy.ENABLED;
      candidateNetworkPolicy = candidateNetworkPolicy.ALL;
      this.iceServers = iceServers;
      audioJitterBufferMaxPackets = 50;
      audioJitterBufferFastAccelerate = false;
      iceConnectionReceivingTimeout = -1;
      iceBackupCandidatePairPingInterval = -1;
      keyType = KeyType.ECDSA;
      continualGatheringPolicy = ContinualGatheringPolicy.GATHER_ONCE;
      iceCandidatePoolSize = 0;
      pruneTurnPorts = false;
      presumeWritableWhenFullyRelayed = false;
      iceCheckMinInterval = null;
    }
  };

  private final List<MediaStream> localStreams;
  private final long nativePeerConnection;
  private final long nativeObserver;
  private List<RtpSender> senders;
  private List<RtpReceiver> receivers;

  PeerConnection(long nativePeerConnection, long nativeObserver) {
    this.nativePeerConnection = nativePeerConnection;
    this.nativeObserver = nativeObserver;
    localStreams = new LinkedList<MediaStream>();
    senders = new LinkedList<RtpSender>();
    receivers = new LinkedList<RtpReceiver>();
  }

  // JsepInterface.
  public native SessionDescription getLocalDescription();

  public native SessionDescription getRemoteDescription();

  public native DataChannel createDataChannel(String label, DataChannel.Init init);

  public native void createOffer(SdpObserver observer, MediaConstraints constraints);

  public native void createAnswer(SdpObserver observer, MediaConstraints constraints);

  public native void setLocalDescription(SdpObserver observer, SessionDescription sdp);

  public native void setRemoteDescription(SdpObserver observer, SessionDescription sdp);

  public boolean setConfiguration(RTCConfiguration config) {
    return nativeSetConfiguration(config, nativeObserver);
  }

  public boolean addIceCandidate(IceCandidate candidate) {
    return nativeAddIceCandidate(candidate.sdpMid, candidate.sdpMLineIndex, candidate.sdp);
  }

  public boolean removeIceCandidates(final IceCandidate[] candidates) {
    return nativeRemoveIceCandidates(candidates);
  }

  public boolean addStream(MediaStream stream) {
    boolean ret = nativeAddLocalStream(stream.nativeStream);
    if (!ret) {
      return false;
    }
    localStreams.add(stream);
    return true;
  }

  public void removeStream(MediaStream stream) {
    nativeRemoveLocalStream(stream.nativeStream);
    localStreams.remove(stream);
  }

  public RtpSender createSender(String kind, String stream_id) {
    RtpSender new_sender = nativeCreateSender(kind, stream_id);
    if (new_sender != null) {
      senders.add(new_sender);
    }
    return new_sender;
  }

  // Note that calling getSenders will dispose of the senders previously
  // returned (and same goes for getReceivers).
  public List<RtpSender> getSenders() {
    for (RtpSender sender : senders) {
      sender.dispose();
    }
    senders = nativeGetSenders();
    return Collections.unmodifiableList(senders);
  }

  public List<RtpReceiver> getReceivers() {
    for (RtpReceiver receiver : receivers) {
      receiver.dispose();
    }
    receivers = nativeGetReceivers();
    return Collections.unmodifiableList(receivers);
  }

  public boolean getStats(StatsObserver observer, MediaStreamTrack track) {
    return nativeGetStats(observer, (track == null) ? 0 : track.nativeTrack);
  }

  // Starts recording an RTC event log. Ownership of the file is transfered to
  // the native code. If an RTC event log is already being recorded, it will be
  // stopped and a new one will start using the provided file. Logging will
  // continue until the stopRtcEventLog function is called. The max_size_bytes
  // argument is ignored, it is added for future use.
  public boolean startRtcEventLog(int file_descriptor, int max_size_bytes) {
    return nativeStartRtcEventLog(file_descriptor, max_size_bytes);
  }

  // Stops recording an RTC event log. If no RTC event log is currently being
  // recorded, this call will have no effect.
  public void stopRtcEventLog() {
    nativeStopRtcEventLog();
  }

  // TODO(fischman): add support for DTMF-related methods once that API
  // stabilizes.
  public native SignalingState signalingState();

  public native IceConnectionState iceConnectionState();

  public native IceGatheringState iceGatheringState();

  public native void close();

  public void dispose() {
    close();
    for (MediaStream stream : localStreams) {
      nativeRemoveLocalStream(stream.nativeStream);
      stream.dispose();
    }
    localStreams.clear();
    for (RtpSender sender : senders) {
      sender.dispose();
    }
    senders.clear();
    for (RtpReceiver receiver : receivers) {
      receiver.dispose();
    }
    receivers.clear();
    freePeerConnection(nativePeerConnection);
    freeObserver(nativeObserver);
  }

  private static native void freePeerConnection(long nativePeerConnection);

  private static native void freeObserver(long nativeObserver);

  public native boolean nativeSetConfiguration(RTCConfiguration config, long nativeObserver);

  private native boolean nativeAddIceCandidate(
      String sdpMid, int sdpMLineIndex, String iceCandidateSdp);

  private native boolean nativeRemoveIceCandidates(final IceCandidate[] candidates);

  private native boolean nativeAddLocalStream(long nativeStream);

  private native void nativeRemoveLocalStream(long nativeStream);

  private native boolean nativeGetStats(StatsObserver observer, long nativeTrack);

  private native RtpSender nativeCreateSender(String kind, String stream_id);

  private native List<RtpSender> nativeGetSenders();

  private native List<RtpReceiver> nativeGetReceivers();

  private native boolean nativeStartRtcEventLog(int file_descriptor, int max_size_bytes);

  private native void nativeStopRtcEventLog();
}
