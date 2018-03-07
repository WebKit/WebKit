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

  // Must be kept in sync with cricket::MediaType.
  public enum MediaType {
    MEDIA_TYPE_AUDIO(0),
    MEDIA_TYPE_VIDEO(1);

    private final int nativeIndex;

    private MediaType(int nativeIndex) {
      this.nativeIndex = nativeIndex;
    }

    @CalledByNative("MediaType")
    int getNative() {
      return nativeIndex;
    }

    @CalledByNative("MediaType")
    static MediaType fromNativeIndex(int nativeIndex) {
      for (MediaType type : MediaType.values()) {
        if (type.getNative() == nativeIndex) {
          return type;
        }
      }
      throw new IllegalArgumentException("Unknown native media type: " + nativeIndex);
    }
  }

  final long nativeTrack;

  public MediaStreamTrack(long nativeTrack) {
    this.nativeTrack = nativeTrack;
  }

  public String id() {
    return getNativeId(nativeTrack);
  }

  public String kind() {
    return getNativeKind(nativeTrack);
  }

  public boolean enabled() {
    return getNativeEnabled(nativeTrack);
  }

  public boolean setEnabled(boolean enable) {
    return setNativeEnabled(nativeTrack, enable);
  }

  public State state() {
    return getNativeState(nativeTrack);
  }

  public void dispose() {
    JniCommon.nativeReleaseRef(nativeTrack);
  }

  private static native String getNativeId(long nativeTrack);

  private static native String getNativeKind(long nativeTrack);

  private static native boolean getNativeEnabled(long nativeTrack);

  private static native boolean setNativeEnabled(long nativeTrack, boolean enabled);

  private static native State getNativeState(long nativeTrack);
}
