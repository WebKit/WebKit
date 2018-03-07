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

/**
 * Representation of a single ICE Candidate, mirroring
 * {@code IceCandidateInterface} in the C++ API.
 */
public class IceCandidate {
  public final String sdpMid;
  public final int sdpMLineIndex;
  public final String sdp;
  public final String serverUrl;

  public IceCandidate(String sdpMid, int sdpMLineIndex, String sdp) {
    this.sdpMid = sdpMid;
    this.sdpMLineIndex = sdpMLineIndex;
    this.sdp = sdp;
    this.serverUrl = "";
  }

  @CalledByNative
  IceCandidate(String sdpMid, int sdpMLineIndex, String sdp, String serverUrl) {
    this.sdpMid = sdpMid;
    this.sdpMLineIndex = sdpMLineIndex;
    this.sdp = sdp;
    this.serverUrl = serverUrl;
  }

  @Override
  public String toString() {
    return sdpMid + ":" + sdpMLineIndex + ":" + sdp + ":" + serverUrl;
  }

  @CalledByNative
  String getSdpMid() {
    return sdpMid;
  }

  @CalledByNative
  String getSdp() {
    return sdp;
  }
}
