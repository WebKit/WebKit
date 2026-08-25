/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidvoip;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import java.util.ArrayList;
import java.util.List;
import org.jni_zero.NativeMethods;
import org.webrtc.CalledByNative;

public class VoipClient {
  private long nativeClient;
  private final OnVoipClientTaskCompleted listener;

  public VoipClient(Context applicationContext, OnVoipClientTaskCompleted listener) {
    this.listener = listener;
    nativeClient = VoipClientJni.get().createClient(applicationContext, this);
  }

  private boolean isInitialized() {
    return nativeClient != 0;
  }

  public void getAndSetUpSupportedCodecs() {
    if (isInitialized()) {
      VoipClientJni.get().getSupportedCodecs(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void getAndSetUpLocalIPAddress() {
    if (isInitialized()) {
      VoipClientJni.get().getLocalIPAddress(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void setEncoder(String encoder) {
    if (isInitialized()) {
      VoipClientJni.get().setEncoder(nativeClient, encoder);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void setDecoders(List<String> decoders) {
    if (isInitialized()) {
      VoipClientJni.get().setDecoders(nativeClient, decoders);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void setLocalAddress(String ipAddress, int portNumber) {
    if (isInitialized()) {
      VoipClientJni.get().setLocalAddress(nativeClient, ipAddress, portNumber);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void setRemoteAddress(String ipAddress, int portNumber) {
    if (isInitialized()) {
      VoipClientJni.get().setRemoteAddress(nativeClient, ipAddress, portNumber);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void startSession() {
    if (isInitialized()) {
      VoipClientJni.get().startSession(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void stopSession() {
    if (isInitialized()) {
      VoipClientJni.get().stopSession(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void startSend() {
    if (isInitialized()) {
      VoipClientJni.get().startSend(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void stopSend() {
    if (isInitialized()) {
      VoipClientJni.get().stopSend(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void startPlayout() {
    if (isInitialized()) {
      VoipClientJni.get().startPlayout(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void stopPlayout() {
    if (isInitialized()) {
      VoipClientJni.get().stopPlayout(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void close() {
    VoipClientJni.get().delete(nativeClient);
    nativeClient = 0;
  }

  @CalledByNative
  public void onGetLocalIPAddressCompleted(String localIPAddress) {
    listener.onGetLocalIPAddressCompleted(localIPAddress);
  }

  @CalledByNative
  public void onGetSupportedCodecsCompleted(List<String> supportedCodecs) {
    listener.onGetSupportedCodecsCompleted(supportedCodecs);
  }

  @CalledByNative
  public void onStartSessionCompleted(boolean isSuccessful) {
    listener.onStartSessionCompleted(isSuccessful);
  }

  @CalledByNative
  public void onStopSessionCompleted(boolean isSuccessful) {
    listener.onStopSessionCompleted(isSuccessful);
  }

  @CalledByNative
  public void onStartSendCompleted(boolean isSuccessful) {
    listener.onStartSendCompleted(isSuccessful);
  }

  @CalledByNative
  public void onStopSendCompleted(boolean isSuccessful) {
    listener.onStopSendCompleted(isSuccessful);
  }

  @CalledByNative
  public void onStartPlayoutCompleted(boolean isSuccessful) {
    listener.onStartPlayoutCompleted(isSuccessful);
  }

  @CalledByNative
  public void onStopPlayoutCompleted(boolean isSuccessful) {
    listener.onStopPlayoutCompleted(isSuccessful);
  }

  @NativeMethods
  interface Natives {
    long createClient(Context applicationContext, VoipClient javaVoipClient);

    void getSupportedCodecs(long nativeAndroidVoipClient);

    void getLocalIPAddress(long nativeAndroidVoipClient);

    void setEncoder(long nativeAndroidVoipClient, String encoder);

    void setDecoders(long nativeAndroidVoipClient, List<String> decoders);

    void setLocalAddress(long nativeAndroidVoipClient, String ipAddress, int portNumber);

    void setRemoteAddress(long nativeAndroidVoipClient, String ipAddress, int portNumber);

    void startSession(long nativeAndroidVoipClient);

    void stopSession(long nativeAndroidVoipClient);

    void startSend(long nativeAndroidVoipClient);

    void stopSend(long nativeAndroidVoipClient);

    void startPlayout(long nativeAndroidVoipClient);

    void stopPlayout(long nativeAndroidVoipClient);

    void delete(long nativeAndroidVoipClient);
  }
}
