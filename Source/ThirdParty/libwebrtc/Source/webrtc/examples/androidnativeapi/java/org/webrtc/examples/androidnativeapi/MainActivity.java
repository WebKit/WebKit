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

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.widget.Button;
import androidx.annotation.Nullable;
import org.webrtc.Camera1Enumerator;
import org.webrtc.Camera2Enumerator;
import org.webrtc.CameraEnumerator;
import org.webrtc.ContextUtils;
import org.webrtc.EglBase;
import org.webrtc.GlRectDrawer;
import org.webrtc.SurfaceTextureHelper;
import org.webrtc.SurfaceViewRenderer;
import org.webrtc.VideoCapturer;

public class MainActivity extends Activity {
  private @Nullable CallClient callClient;
  private @Nullable EglBase eglBase;
  private @Nullable SurfaceViewRenderer localRenderer;
  private @Nullable SurfaceViewRenderer remoteRenderer;
  private @Nullable SurfaceTextureHelper videoCapturerSurfaceTextureHelper;
  private @Nullable VideoCapturer videoCapturer;

  @Override
  protected void onCreate(Bundle savedInstance) {
    ContextUtils.initialize(getApplicationContext());

    super.onCreate(savedInstance);
    setContentView(R.layout.activity_main);

    System.loadLibrary("examples_androidnativeapi_jni");
    callClient = new CallClient(getApplicationContext());

    Button callButton = (Button) findViewById(R.id.call_button);
    callButton.setOnClickListener((view) -> {
      if (videoCapturer == null) {
        videoCapturer = createVideoCapturer(getApplicationContext());
      }
      callClient.call(
          localRenderer, remoteRenderer, videoCapturer, videoCapturerSurfaceTextureHelper);
    });

    Button hangupButton = (Button) findViewById(R.id.hangup_button);
    hangupButton.setOnClickListener((view) -> { hangup(); });
  }

  @Override
  protected void onStart() {
    super.onStart();

    eglBase = EglBase.create(null /* sharedContext */, EglBase.CONFIG_PLAIN);
    localRenderer = (SurfaceViewRenderer) findViewById(R.id.local_renderer);
    remoteRenderer = (SurfaceViewRenderer) findViewById(R.id.remote_renderer);

    localRenderer.init(eglBase.getEglBaseContext(), null /* rendererEvents */, EglBase.CONFIG_PLAIN,
        new GlRectDrawer());
    remoteRenderer.init(eglBase.getEglBaseContext(), null /* rendererEvents */,
        EglBase.CONFIG_PLAIN, new GlRectDrawer());

    videoCapturerSurfaceTextureHelper =
        SurfaceTextureHelper.create("VideoCapturerThread", eglBase.getEglBaseContext());
  }

  @Override
  protected void onStop() {
    hangup();

    localRenderer.release();
    remoteRenderer.release();
    videoCapturerSurfaceTextureHelper.dispose();
    eglBase.release();

    localRenderer = null;
    remoteRenderer = null;
    videoCapturerSurfaceTextureHelper = null;
    eglBase = null;

    super.onStop();
  }

  @Override
  protected void onDestroy() {
    callClient.close();
    callClient = null;

    super.onDestroy();
  }

  private void hangup() {
    if (videoCapturer != null) {
      try {
        videoCapturer.stopCapture();
      } catch (InterruptedException e) {
        throw new RuntimeException(e);
      }
      videoCapturer.dispose();
      videoCapturer = null;
    }
    callClient.hangup();
  }

  private static VideoCapturer createVideoCapturer(Context context) {
    CameraEnumerator enumerator = Camera2Enumerator.isSupported(context)
        ? new Camera2Enumerator(context)
        : new Camera1Enumerator();
    return enumerator.createCapturer(enumerator.getDeviceNames()[0], null /* eventsHandler */);
  }
}
