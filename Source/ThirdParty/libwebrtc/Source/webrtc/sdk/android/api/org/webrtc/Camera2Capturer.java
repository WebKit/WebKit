/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.annotation.TargetApi;
import android.content.Context;
import android.hardware.camera2.CameraManager;
import android.media.MediaRecorder;

@TargetApi(21)
public class Camera2Capturer extends CameraCapturer {
  private final Context context;
  private final CameraManager cameraManager;

  public Camera2Capturer(Context context, String cameraName, CameraEventsHandler eventsHandler) {
    super(cameraName, eventsHandler, new Camera2Enumerator(context));

    this.context = context;
    cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
  }

  @Override
  protected void createCameraSession(CameraSession.CreateSessionCallback createSessionCallback,
      CameraSession.Events events, Context applicationContext,
      SurfaceTextureHelper surfaceTextureHelper, MediaRecorder mediaRecoder, String cameraName,
      int width, int height, int framerate) {
    Camera2Session.create(createSessionCallback, events, applicationContext, cameraManager,
        surfaceTextureHelper, mediaRecoder, cameraName, width, height, framerate);
  }
}
