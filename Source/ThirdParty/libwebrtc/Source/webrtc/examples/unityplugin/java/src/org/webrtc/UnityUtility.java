/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.content.Context;
import java.util.List;

public class UnityUtility {
  private static final String VIDEO_CAPTURER_THREAD_NAME = "VideoCapturerThread";

  public static SurfaceTextureHelper LoadSurfaceTextureHelper() {
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create(VIDEO_CAPTURER_THREAD_NAME, null);
    return surfaceTextureHelper;
  }

  private static boolean useCamera2() {
    return Camera2Enumerator.isSupported(ContextUtils.getApplicationContext());
  }

  private static VideoCapturer createCameraCapturer(CameraEnumerator enumerator) {
    final String[] deviceNames = enumerator.getDeviceNames();

    for (String deviceName : deviceNames) {
      if (enumerator.isFrontFacing(deviceName)) {
        VideoCapturer videoCapturer = enumerator.createCapturer(deviceName, null);

        if (videoCapturer != null) {
          return videoCapturer;
        }
      }
    }

    return null;
  }

  public static VideoCapturer LinkCamera(
      long nativeTrackSource, SurfaceTextureHelper surfaceTextureHelper) {
    VideoCapturer capturer =
        createCameraCapturer(new Camera2Enumerator(ContextUtils.getApplicationContext()));

    VideoCapturer.CapturerObserver capturerObserver =
        new AndroidVideoTrackSourceObserver(nativeTrackSource);

    capturer.initialize(
        surfaceTextureHelper, ContextUtils.getApplicationContext(), capturerObserver);

    capturer.startCapture(720, 480, 30);
    return capturer;
  }

  public static void StopCamera(VideoCapturer camera) throws InterruptedException {
    camera.stopCapture();
    camera.dispose();
  }
}
