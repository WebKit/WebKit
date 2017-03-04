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

import java.util.LinkedList;

/** Java wrapper for a C++ MediaStreamInterface. */
public class MediaStream {
  public final LinkedList<AudioTrack> audioTracks;
  public final LinkedList<VideoTrack> videoTracks;
  public final LinkedList<VideoTrack> preservedVideoTracks;
  // Package-protected for PeerConnection.
  final long nativeStream;

  public MediaStream(long nativeStream) {
    audioTracks = new LinkedList<AudioTrack>();
    videoTracks = new LinkedList<VideoTrack>();
    preservedVideoTracks = new LinkedList<VideoTrack>();
    this.nativeStream = nativeStream;
  }

  public boolean addTrack(AudioTrack track) {
    if (nativeAddAudioTrack(nativeStream, track.nativeTrack)) {
      audioTracks.add(track);
      return true;
    }
    return false;
  }

  public boolean addTrack(VideoTrack track) {
    if (nativeAddVideoTrack(nativeStream, track.nativeTrack)) {
      videoTracks.add(track);
      return true;
    }
    return false;
  }

  // Tracks added in addTrack() call will be auto released once MediaStream.dispose()
  // is called. If video track need to be preserved after MediaStream is destroyed it
  // should be added to MediaStream using addPreservedTrack() call.
  public boolean addPreservedTrack(VideoTrack track) {
    if (nativeAddVideoTrack(nativeStream, track.nativeTrack)) {
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

  public void dispose() {
    // Remove and release previously added audio and video tracks.
    while (!audioTracks.isEmpty()) {
      AudioTrack track = audioTracks.getFirst();
      removeTrack(track);
      track.dispose();
    }
    while (!videoTracks.isEmpty()) {
      VideoTrack track = videoTracks.getFirst();
      removeTrack(track);
      track.dispose();
    }
    // Remove, but do not release preserved video tracks.
    while (!preservedVideoTracks.isEmpty()) {
      removeTrack(preservedVideoTracks.getFirst());
    }
    free(nativeStream);
  }

  public String label() {
    return nativeLabel(nativeStream);
  }

  public String toString() {
    return "[" + label() + ":A=" + audioTracks.size() + ":V=" + videoTracks.size() + "]";
  }

  private static native boolean nativeAddAudioTrack(long nativeStream, long nativeAudioTrack);

  private static native boolean nativeAddVideoTrack(long nativeStream, long nativeVideoTrack);

  private static native boolean nativeRemoveAudioTrack(long nativeStream, long nativeAudioTrack);

  private static native boolean nativeRemoveVideoTrack(long nativeStream, long nativeVideoTrack);

  private static native String nativeLabel(long nativeStream);

  private static native void free(long nativeStream);
}
