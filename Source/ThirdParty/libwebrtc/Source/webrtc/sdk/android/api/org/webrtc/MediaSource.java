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

/** Java wrapper for a C++ MediaSourceInterface. */
public class MediaSource {
  /** Tracks MediaSourceInterface.SourceState */
  public enum State { INITIALIZING, LIVE, ENDED, MUTED }

  final long nativeSource; // Package-protected for PeerConnectionFactory.

  public MediaSource(long nativeSource) {
    this.nativeSource = nativeSource;
  }

  public State state() {
    return nativeState(nativeSource);
  }

  public void dispose() {
    JniCommon.nativeReleaseRef(nativeSource);
  }

  private static native State nativeState(long pointer);
}
