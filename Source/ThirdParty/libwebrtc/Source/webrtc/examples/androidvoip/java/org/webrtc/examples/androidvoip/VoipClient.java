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
import org.webrtc.CalledByNative;

public class VoipClient {
  private long nativeClient;
  private OnVoipClientTaskCompleted listener;

  public VoipClient(Context applicationContext, OnVoipClientTaskCompleted listener) {
    this.listener = listener;
    nativeClient = nativeCreateClient(applicationContext, this);
  }

  private boolean isInitialized() {
    return nativeClient != 0;
  }

  public void getAndSetUpSupportedCodecs() {
    if (isInitialized()) {
      nativeGetSupportedCodecs(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void getAndSetUpLocalIPAddress() {
    if (isInitialized()) {
      nativeGetLocalIPAddress(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void setEncoder(String encoder) {
    if (isInitialized()) {
      nativeSetEncoder(nativeClient, encoder);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void setDecoders(List<String> decoders) {
    if (isInitialized()) {
      nativeSetDecoders(nativeClient, decoders);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void setLocalAddress(String ipAddress, int portNumber) {
    if (isInitialized()) {
      nativeSetLocalAddress(nativeClient, ipAddress, portNumber);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void setRemoteAddress(String ipAddress, int portNumber) {
    if (isInitialized()) {
      nativeSetRemoteAddress(nativeClient, ipAddress, portNumber);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void startSession() {
    if (isInitialized()) {
      nativeStartSession(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void stopSession() {
    if (isInitialized()) {
      nativeStopSession(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void startSend() {
    if (isInitialized()) {
      nativeStartSend(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void stopSend() {
    if (isInitialized()) {
      nativeStopSend(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void startPlayout() {
    if (isInitialized()) {
      nativeStartPlayout(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void stopPlayout() {
    if (isInitialized()) {
      nativeStopPlayout(nativeClient);
    } else {
      listener.onUninitializedVoipClient();
    }
  }

  public void close() {
    nativeDelete(nativeClient);
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

  private static native long nativeCreateClient(
      Context applicationContext, VoipClient javaVoipClient);
  private static native void nativeGetSupportedCodecs(long nativeAndroidVoipClient);
  private static native void nativeGetLocalIPAddress(long nativeAndroidVoipClient);
  private static native void nativeSetEncoder(long nativeAndroidVoipClient, String encoder);
  private static native void nativeSetDecoders(long nativeAndroidVoipClient, List<String> decoders);
  private static native void nativeSetLocalAddress(
      long nativeAndroidVoipClient, String ipAddress, int portNumber);
  private static native void nativeSetRemoteAddress(
      long nativeAndroidVoipClient, String ipAddress, int portNumber);
  private static native void nativeStartSession(long nativeAndroidVoipClient);
  private static native void nativeStopSession(long nativeAndroidVoipClient);
  private static native void nativeStartSend(long nativeAndroidVoipClient);
  private static native void nativeStopSend(long nativeAndroidVoipClient);
  private static native void nativeStartPlayout(long nativeAndroidVoipClient);
  private static native void nativeStopPlayout(long nativeAndroidVoipClient);
  private static native void nativeDelete(long nativeAndroidVoipClient);
}
