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

import java.util.ArrayList;
import java.util.List;
import java.util.Iterator;

/** Java wrapper for a C++ MediaStreamInterface. */
public class MediaStream {
  private static final String TAG = "MediaStream";

  public final List<AudioTrack> audioTracks = new ArrayList<>();
  public final List<VideoTrack> videoTracks = new ArrayList<>();
  public final List<VideoTrack> preservedVideoTracks = new ArrayList<>();
  // Package-protected for PeerConnection.
  final long nativeStream;

  @CalledByNative
  public MediaStream(long nativeStream) {
    this.nativeStream = nativeStream;
  }

  public boolean addTrack(AudioTrack track) {
    if (nativeAddAudioTrackToNativeStream(nativeStream, track.nativeTrack)) {
      audioTracks.add(track);
      return true;
    }
    return false;
  }

  public boolean addTrack(VideoTrack track) {
    if (nativeAddVideoTrackToNativeStream(nativeStream, track.nativeTrack)) {
      videoTracks.add(track);
      return true;
    }
    return false;
  }

  // Tracks added in addTrack() call will be auto released once MediaStream.dispose()
  // is called. If video track need to be preserved after MediaStream is destroyed it
  // should be added to MediaStream using addPreservedTrack() call.
  public boolean addPreservedTrack(VideoTrack track) {
    if (nativeAddVideoTrackToNativeStream(nativeStream, track.nativeTrack)) {
      preservedVideoTracks.add(track);
      return true;
    }
    return false;
  }

  public boolean removeTrack(AudioTrack track) {
    audioTracks.remove(track);
    return nativeRemoveAudioTrack(nativeStream, track.nativeTrack);
  }

  public boolean removeTrack(VideoTrack track) {
    videoTracks.remove(track);
    preservedVideoTracks.remove(track);
    return nativeRemoveVideoTrack(nativeStream, track.nativeTrack);
  }

  @CalledByNative
  public void dispose() {
    // Remove and release previously added audio and video tracks.
    while (!audioTracks.isEmpty()) {
      AudioTrack track = audioTracks.get(0 /* index */);
      removeTrack(track);
      track.dispose();
    }
    while (!videoTracks.isEmpty()) {
      VideoTrack track = videoTracks.get(0 /* index */);
      removeTrack(track);
      track.dispose();
    }
    // Remove, but do not release preserved video tracks.
    while (!preservedVideoTracks.isEmpty()) {
      removeTrack(preservedVideoTracks.get(0 /* index */));
    }
    JniCommon.nativeReleaseRef(nativeStream);
  }

  public String getId() {
    return nativeGetId(nativeStream);
  }

  @Override
  public String toString() {
    return "[" + getId() + ":A=" + audioTracks.size() + ":V=" + videoTracks.size() + "]";
  }

  @CalledByNative
  void addNativeAudioTrack(long nativeTrack) {
    audioTracks.add(new AudioTrack(nativeTrack));
  }

  @CalledByNative
  void addNativeVideoTrack(long nativeTrack) {
    videoTracks.add(new VideoTrack(nativeTrack));
  }

  @CalledByNative
  void removeAudioTrack(long nativeTrack) {
    removeMediaStreamTrack(audioTracks, nativeTrack);
  }

  @CalledByNative
  void removeVideoTrack(long nativeTrack) {
    removeMediaStreamTrack(videoTracks, nativeTrack);
  }

  private static void removeMediaStreamTrack(
      List<? extends MediaStreamTrack> tracks, long nativeTrack) {
    final Iterator<? extends MediaStreamTrack> it = tracks.iterator();
    while (it.hasNext()) {
      MediaStreamTrack track = it.next();
      if (track.nativeTrack == nativeTrack) {
        track.dispose();
        it.remove();
        return;
      }
    }
    Logging.e(TAG, "Couldn't not find track");
  }

  private static native boolean nativeAddAudioTrackToNativeStream(
      long stream, long nativeAudioTrack);
  private static native boolean nativeAddVideoTrackToNativeStream(
      long stream, long nativeVideoTrack);
  private static native boolean nativeRemoveAudioTrack(long stream, long nativeAudioTrack);
  private static native boolean nativeRemoveVideoTrack(long stream, long nativeVideoTrack);
  private static native String nativeGetId(long stream);
}
