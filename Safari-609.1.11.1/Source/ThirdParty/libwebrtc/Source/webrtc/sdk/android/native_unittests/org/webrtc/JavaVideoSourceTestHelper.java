/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import org.webrtc.VideoCapturer;

public class JavaVideoSourceTestHelper {
  @CalledByNative
  public static void startCapture(CapturerObserver observer, boolean success) {
    observer.onCapturerStarted(success);
  }

  @CalledByNative
  public static void stopCapture(CapturerObserver observer) {
    observer.onCapturerStopped();
  }

  @CalledByNative
  public static void deliverFrame(
      int width, int height, int rotation, long timestampNs, CapturerObserver observer) {
    observer.onFrameCaptured(
        new VideoFrame(JavaI420Buffer.allocate(width, height), rotation, timestampNs));
  }
}
