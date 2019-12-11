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
  public enum State {
    INITIALIZING,
    LIVE,
    ENDED,
    MUTED;

    @CalledByNative("State")
    static State fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  private long nativeSource;

  public MediaSource(long nativeSource) {
    this.nativeSource = nativeSource;
  }

  public State state() {
    checkMediaSourceExists();
    return nativeGetState(nativeSource);
  }

  public void dispose() {
    checkMediaSourceExists();
    JniCommon.nativeReleaseRef(nativeSource);
    nativeSource = 0;
  }

  /** Returns a pointer to webrtc::MediaSourceInterface. */
  protected long getNativeMediaSource() {
    checkMediaSourceExists();
    return nativeSource;
  }

  private void checkMediaSourceExists() {
    if (nativeSource == 0) {
      throw new IllegalStateException("MediaSource has been disposed.");
    }
  }

  private static native State nativeGetState(long pointer);
}
