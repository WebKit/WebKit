/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidnativeapi;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import org.jni_zero.NativeMethods;
import org.webrtc.CapturerObserver;
import org.webrtc.SurfaceTextureHelper;
import org.webrtc.VideoCapturer;
import org.webrtc.VideoSink;

public class CallClient {
  private static final String TAG = "CallClient";
  private static final int CAPTURE_WIDTH = 640;
  private static final int CAPTURE_HEIGHT = 480;
  private static final int CAPTURE_FPS = 30;

  private final Context applicationContext;
  private final HandlerThread thread;
  private final Handler handler;

  private long nativeClient;
  private SurfaceTextureHelper surfaceTextureHelper;
  private VideoCapturer videoCapturer;

  public CallClient(Context applicationContext) {
    this.applicationContext = applicationContext;
    thread = new HandlerThread(TAG + "Thread");
    thread.start();
    handler = new Handler(thread.getLooper());
    handler.post(() -> { nativeClient = CallClientJni.get().createClient(); });
  }

  public void call(VideoSink localSink, VideoSink remoteSink, VideoCapturer videoCapturer,
      SurfaceTextureHelper videoCapturerSurfaceTextureHelper) {
    handler.post(() -> {
      CallClientJni.get().call(nativeClient, localSink, remoteSink);
      videoCapturer.initialize(videoCapturerSurfaceTextureHelper, applicationContext,
          CallClientJni.get().getJavaVideoCapturerObserver(nativeClient));
      videoCapturer.startCapture(CAPTURE_WIDTH, CAPTURE_HEIGHT, CAPTURE_FPS);
    });
  }

  public void hangup() {
    handler.post(() -> { CallClientJni.get().hangup(nativeClient); });
  }

  public void close() {
    handler.post(() -> {
      CallClientJni.get().delete(nativeClient);
      nativeClient = 0;
    });
    thread.quitSafely();
  }

  @NativeMethods
  interface Natives {
    long createClient();

    void call(long nativeAndroidCallClient, VideoSink localSink, VideoSink remoteSink);

    void hangup(long nativeAndroidCallClient);

    void delete(long nativeAndroidCallClient);

    CapturerObserver getJavaVideoCapturerObserver(long nativeAndroidCallClient);
  }
}
