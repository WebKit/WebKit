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

/** Java wrapper for a C++ MediaStreamTrackInterface. */
public class MediaStreamTrack {
  /** Tracks MediaStreamTrackInterface.TrackState */
  public enum State { LIVE, ENDED }

  final long nativeTrack;

  public MediaStreamTrack(long nativeTrack) {
    this.nativeTrack = nativeTrack;
  }

  public String id() {
    return nativeId(nativeTrack);
  }

  public String kind() {
    return nativeKind(nativeTrack);
  }

  public boolean enabled() {
    return nativeEnabled(nativeTrack);
  }

  public boolean setEnabled(boolean enable) {
    return nativeSetEnabled(nativeTrack, enable);
  }

  public State state() {
    return nativeState(nativeTrack);
  }

  public void dispose() {
    free(nativeTrack);
  }

  private static native String nativeId(long nativeTrack);

  private static native String nativeKind(long nativeTrack);

  private static native boolean nativeEnabled(long nativeTrack);

  private static native boolean nativeSetEnabled(long nativeTrack, boolean enabled);

  private static native State nativeState(long nativeTrack);

  private static native void free(long nativeTrack);
}
