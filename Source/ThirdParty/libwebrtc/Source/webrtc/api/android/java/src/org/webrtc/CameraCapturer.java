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

import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import java.util.Arrays;

@SuppressWarnings("deprecation")
public abstract class CameraCapturer implements CameraVideoCapturer {
  enum SwitchState {
    IDLE, // No switch requested.
    PENDING, // Waiting for previous capture session to open.
    IN_PROGRESS, // Waiting for new switched capture session to start.
  }

  private static final String TAG = "CameraCapturer";
  private final static int MAX_OPEN_CAMERA_ATTEMPTS = 3;
  private final static int OPEN_CAMERA_DELAY_MS = 500;
  private final static int OPEN_CAMERA_TIMEOUT = 10000;

  private final CameraEnumerator cameraEnumerator;
  private final CameraEventsHandler eventsHandler;
  private final Handler uiThreadHandler;

  private final CameraSession.CreateSessionCallback createSessionCallback =
      new CameraSession.CreateSessionCallback() {
        @Override
        public void onDone(CameraSession session) {
          checkIsOnCameraThread();
          Logging.d(TAG, "Create session done");
          uiThreadHandler.removeCallbacks(openCameraTimeoutRunnable);
          synchronized (stateLock) {
            capturerObserver.onCapturerStarted(true /* success */);
            sessionOpening = false;
            currentSession = session;
            cameraStatistics = new CameraStatistics(surfaceHelper, eventsHandler);
            firstFrameObserved = false;
            stateLock.notifyAll();

            if (switchState == SwitchState.IN_PROGRESS) {
              if (switchEventsHandler != null) {
                switchEventsHandler.onCameraSwitchDone(cameraEnumerator.isFrontFacing(cameraName));
                switchEventsHandler = null;
              }
              switchState = SwitchState.IDLE;
            } else if (switchState == SwitchState.PENDING) {
              switchState = SwitchState.IDLE;
              switchCameraInternal(switchEventsHandler);
            }
          }
        }

        @Override
        public void onFailure(String error) {
          checkIsOnCameraThread();
          uiThreadHandler.removeCallbacks(openCameraTimeoutRunnable);
          synchronized (stateLock) {
            capturerObserver.onCapturerStarted(false /* success */);
            openAttemptsRemaining--;

            if (openAttemptsRemaining <= 0) {
              Logging.w(TAG, "Opening camera failed, passing: " + error);
              sessionOpening = false;
              stateLock.notifyAll();

              if (switchState != SwitchState.IDLE) {
                if (switchEventsHandler != null) {
                  switchEventsHandler.onCameraSwitchError(error);
                  switchEventsHandler = null;
                }
                switchState = SwitchState.IDLE;
              }

              eventsHandler.onCameraError(error);
            } else {
              Logging.w(TAG, "Opening camera failed, retry: " + error);

              createSessionInternal(OPEN_CAMERA_DELAY_MS);
            }
          }
        }
      };

  private final CameraSession.Events cameraSessionEventsHandler = new CameraSession.Events() {
    @Override
    public void onCameraOpening() {
      checkIsOnCameraThread();
      synchronized (stateLock) {
        if (currentSession != null) {
          Logging.w(TAG, "onCameraOpening while session was open.");
          return;
        }
        eventsHandler.onCameraOpening(cameraName);
      }
    }

    @Override
    public void onCameraError(CameraSession session, String error) {
      checkIsOnCameraThread();
      synchronized (stateLock) {
        if (session != currentSession) {
          Logging.w(TAG, "onCameraError from another session: " + error);
          return;
        }
        eventsHandler.onCameraError(error);
        stopCapture();
      }
    }

    @Override
    public void onCameraDisconnected(CameraSession session) {
      checkIsOnCameraThread();
      synchronized (stateLock) {
        if (session != currentSession) {
          Logging.w(TAG, "onCameraDisconnected from another session.");
          return;
        }
        eventsHandler.onCameraDisconnected();
        stopCapture();
      }
    }

    @Override
    public void onCameraClosed(CameraSession session) {
      checkIsOnCameraThread();
      synchronized (stateLock) {
        if (session != currentSession && currentSession != null) {
          Logging.d(TAG, "onCameraClosed from another session.");
          return;
        }
        eventsHandler.onCameraClosed();
      }
    }

    @Override
    public void onByteBufferFrameCaptured(
        CameraSession session, byte[] data, int width, int height, int rotation, long timestamp) {
      checkIsOnCameraThread();
      synchronized (stateLock) {
        if (session != currentSession) {
          Logging.w(TAG, "onByteBufferFrameCaptured from another session.");
          return;
        }
        if (!firstFrameObserved) {
          eventsHandler.onFirstFrameAvailable();
          firstFrameObserved = true;
        }
        cameraStatistics.addFrame();
        capturerObserver.onByteBufferFrameCaptured(data, width, height, rotation, timestamp);
      }
    }

    @Override
    public void onTextureFrameCaptured(CameraSession session, int width, int height,
        int oesTextureId, float[] transformMatrix, int rotation, long timestamp) {
      checkIsOnCameraThread();
      synchronized (stateLock) {
        if (session != currentSession) {
          Logging.w(TAG, "onTextureFrameCaptured from another session.");
          surfaceHelper.returnTextureFrame();
          return;
        }
        if (!firstFrameObserved) {
          eventsHandler.onFirstFrameAvailable();
          firstFrameObserved = true;
        }
        cameraStatistics.addFrame();
        capturerObserver.onTextureFrameCaptured(
            width, height, oesTextureId, transformMatrix, rotation, timestamp);
      }
    }
  };

  private final Runnable openCameraTimeoutRunnable = new Runnable() {
    @Override
    public void run() {
      eventsHandler.onCameraError("Camera failed to start within timeout.");
    }
  };

  // Initialized on initialize
  // -------------------------
  private Handler cameraThreadHandler;
  private Context applicationContext;
  private CapturerObserver capturerObserver;
  private SurfaceTextureHelper surfaceHelper;

  private final Object stateLock = new Object();
  private boolean sessionOpening; /* guarded by stateLock */
  private CameraSession currentSession; /* guarded by stateLock */
  private String cameraName; /* guarded by stateLock */
  private int width; /* guarded by stateLock */
  private int height; /* guarded by stateLock */
  private int framerate; /* guarded by stateLock */
  private int openAttemptsRemaining; /* guarded by stateLock */
  private SwitchState switchState = SwitchState.IDLE; /* guarded by stateLock */
  private CameraSwitchHandler switchEventsHandler; /* guarded by stateLock */
  // Valid from onDone call until stopCapture, otherwise null.
  private CameraStatistics cameraStatistics; /* guarded by stateLock */
  private boolean firstFrameObserved; /* guarded by stateLock */

  public CameraCapturer(
      String cameraName, CameraEventsHandler eventsHandler, CameraEnumerator cameraEnumerator) {
    if (eventsHandler == null) {
      eventsHandler = new CameraEventsHandler() {
        @Override
        public void onCameraError(String errorDescription) {}
        @Override
        public void onCameraDisconnected() {}
        @Override
        public void onCameraFreezed(String errorDescription) {}
        @Override
        public void onCameraOpening(String cameraName) {}
        @Override
        public void onFirstFrameAvailable() {}
        @Override
        public void onCameraClosed() {}
      };
    }

    this.eventsHandler = eventsHandler;
    this.cameraEnumerator = cameraEnumerator;
    this.cameraName = cameraName;
    uiThreadHandler = new Handler(Looper.getMainLooper());

    final String[] deviceNames = cameraEnumerator.getDeviceNames();

    if (deviceNames.length == 0) {
      throw new RuntimeException("No cameras attached.");
    }
    if (!Arrays.asList(deviceNames).contains(this.cameraName)) {
      throw new IllegalArgumentException(
          "Camera name " + this.cameraName + " does not match any known camera device.");
    }
  }

  @Override
  public void initialize(SurfaceTextureHelper surfaceTextureHelper, Context applicationContext,
      CapturerObserver capturerObserver) {
    this.applicationContext = applicationContext;
    this.capturerObserver = capturerObserver;
    this.surfaceHelper = surfaceTextureHelper;
    this.cameraThreadHandler =
        surfaceTextureHelper == null ? null : surfaceTextureHelper.getHandler();
  }

  @Override
  public void startCapture(int width, int height, int framerate) {
    Logging.d(TAG, "startCapture: " + width + "x" + height + "@" + framerate);

    synchronized (stateLock) {
      if (sessionOpening || currentSession != null) {
        Logging.w(TAG, "Session already open");
        return;
      }

      this.width = width;
      this.height = height;
      this.framerate = framerate;

      sessionOpening = true;
      openAttemptsRemaining = MAX_OPEN_CAMERA_ATTEMPTS;
      createSessionInternal(0);
    }
  }

  private void createSessionInternal(int delayMs) {
    uiThreadHandler.postDelayed(openCameraTimeoutRunnable, delayMs + OPEN_CAMERA_TIMEOUT);
    cameraThreadHandler.postDelayed(new Runnable() {
      @Override
      public void run() {
        createCameraSession(createSessionCallback, cameraSessionEventsHandler, applicationContext,
            surfaceHelper, cameraName, width, height, framerate);
      }
    }, delayMs);
  }

  @Override
  public void stopCapture() {
    Logging.d(TAG, "Stop capture");

    synchronized (stateLock) {
      while (sessionOpening) {
        Logging.d(TAG, "Stop capture: Waiting for session to open");
        ThreadUtils.waitUninterruptibly(stateLock);
      }

      if (currentSession != null) {
        Logging.d(TAG, "Stop capture: Nulling session");
        cameraStatistics.release();
        cameraStatistics = null;
        final CameraSession oldSession = currentSession;
        cameraThreadHandler.post(new Runnable() {
          @Override
          public void run() {
            oldSession.stop();
          }
        });
        currentSession = null;
        capturerObserver.onCapturerStopped();
      } else {
        Logging.d(TAG, "Stop capture: No session open");
      }
    }

    Logging.d(TAG, "Stop capture done");
  }

  @Override
  public void changeCaptureFormat(int width, int height, int framerate) {
    Logging.d(TAG, "changeCaptureFormat: " + width + "x" + height + "@" + framerate);
    synchronized (stateLock) {
      stopCapture();
      startCapture(width, height, framerate);
    }
  }

  @Override
  public void dispose() {
    Logging.d(TAG, "dispose");
    stopCapture();
  }

  @Override
  public void switchCamera(final CameraSwitchHandler switchEventsHandler) {
    Logging.d(TAG, "switchCamera");
    cameraThreadHandler.post(new Runnable() {
      @Override
      public void run() {
        switchCameraInternal(switchEventsHandler);
      }
    });
  }

  @Override
  public boolean isScreencast() {
    return false;
  }

  public void printStackTrace() {
    Thread cameraThread = null;
    if (cameraThreadHandler != null) {
      cameraThread = cameraThreadHandler.getLooper().getThread();
    }
    if (cameraThread != null) {
      StackTraceElement[] cameraStackTrace = cameraThread.getStackTrace();
      if (cameraStackTrace.length > 0) {
        Logging.d(TAG, "CameraCapturer stack trace:");
        for (StackTraceElement traceElem : cameraStackTrace) {
          Logging.d(TAG, traceElem.toString());
        }
      }
    }
  }

  private void switchCameraInternal(final CameraSwitchHandler switchEventsHandler) {
    Logging.d(TAG, "switchCamera internal");

    final String[] deviceNames = cameraEnumerator.getDeviceNames();

    if (deviceNames.length < 2) {
      if (switchEventsHandler != null) {
        switchEventsHandler.onCameraSwitchError("No camera to switch to.");
      }
      return;
    }

    synchronized (stateLock) {
      if (switchState != SwitchState.IDLE) {
        Logging.d(TAG, "switchCamera switchInProgress");
        if (switchEventsHandler != null) {
          switchEventsHandler.onCameraSwitchError("Camera switch already in progress.");
        }
        return;
      }

      if (!sessionOpening && currentSession == null) {
        Logging.d(TAG, "switchCamera: No session open");
        if (switchEventsHandler != null) {
          switchEventsHandler.onCameraSwitchError("Camera is not running.");
        }
        return;
      }

      this.switchEventsHandler = switchEventsHandler;
      if (sessionOpening) {
        switchState = SwitchState.PENDING;
        return;
      } else {
        switchState = SwitchState.IN_PROGRESS;
      }

      Logging.d(TAG, "switchCamera: Stopping session");
      cameraStatistics.release();
      cameraStatistics = null;
      final CameraSession oldSession = currentSession;
      cameraThreadHandler.post(new Runnable() {
        @Override
        public void run() {
          oldSession.stop();
        }
      });
      currentSession = null;

      int cameraNameIndex = Arrays.asList(deviceNames).indexOf(cameraName);
      cameraName = deviceNames[(cameraNameIndex + 1) % deviceNames.length];

      sessionOpening = true;
      openAttemptsRemaining = 1;
      createSessionInternal(0);
    }
    Logging.d(TAG, "switchCamera done");
  }

  private void checkIsOnCameraThread() {
    if (Thread.currentThread() != cameraThreadHandler.getLooper().getThread()) {
      Logging.e(TAG, "Check is on camera thread failed.");
      throw new RuntimeException("Not on camera thread.");
    }
  }

  protected String getCameraName() {
    synchronized (stateLock) {
      return cameraName;
    }
  }

  abstract protected void createCameraSession(
      CameraSession.CreateSessionCallback createSessionCallback, CameraSession.Events events,
      Context applicationContext, SurfaceTextureHelper surfaceTextureHelper, String cameraName,
      int width, int height, int framerate);
}
