/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import android.os.Build;
import android.os.Process;
import java.lang.System;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import javax.annotation.Nullable;
import org.webrtc.CalledByNative;
import org.webrtc.Logging;
import org.webrtc.ThreadUtils;
import org.webrtc.audio.JavaAudioDeviceModule.AudioRecordErrorCallback;
import org.webrtc.audio.JavaAudioDeviceModule.AudioRecordStartErrorCode;
import org.webrtc.audio.JavaAudioDeviceModule.SamplesReadyCallback;

class WebRtcAudioRecord {
  private static final String TAG = "WebRtcAudioRecordExternal";

  // Default audio data format is PCM 16 bit per sample.
  // Guaranteed to be supported by all devices.
  private static final int BITS_PER_SAMPLE = 16;

  // Requested size of each recorded buffer provided to the client.
  private static final int CALLBACK_BUFFER_SIZE_MS = 10;

  // Average number of callbacks per second.
  private static final int BUFFERS_PER_SECOND = 1000 / CALLBACK_BUFFER_SIZE_MS;

  // We ask for a native buffer size of BUFFER_SIZE_FACTOR * (minimum required
  // buffer size). The extra space is allocated to guard against glitches under
  // high load.
  private static final int BUFFER_SIZE_FACTOR = 2;

  // The AudioRecordJavaThread is allowed to wait for successful call to join()
  // but the wait times out afther this amount of time.
  private static final long AUDIO_RECORD_THREAD_JOIN_TIMEOUT_MS = 2000;

  public static final int DEFAULT_AUDIO_SOURCE = AudioSource.VOICE_COMMUNICATION;

  private final Context context;
  private final AudioManager audioManager;
  private final int audioSource;

  private long nativeAudioRecord;

  private final WebRtcAudioEffects effects = new WebRtcAudioEffects();

  private @Nullable ByteBuffer byteBuffer;

  private @Nullable AudioRecord audioRecord;
  private @Nullable AudioRecordThread audioThread;

  private volatile boolean microphoneMute;
  private byte[] emptyBytes;

  private final @Nullable AudioRecordErrorCallback errorCallback;
  private final @Nullable SamplesReadyCallback audioSamplesReadyCallback;
  private final boolean isAcousticEchoCancelerSupported;
  private final boolean isNoiseSuppressorSupported;

  /**
   * Audio thread which keeps calling ByteBuffer.read() waiting for audio
   * to be recorded. Feeds recorded data to the native counterpart as a
   * periodic sequence of callbacks using DataIsRecorded().
   * This thread uses a Process.THREAD_PRIORITY_URGENT_AUDIO priority.
   */
  private class AudioRecordThread extends Thread {
    private volatile boolean keepAlive = true;

    public AudioRecordThread(String name) {
      super(name);
    }

    @Override
    public void run() {
      Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
      Logging.d(TAG, "AudioRecordThread" + WebRtcAudioUtils.getThreadInfo());
      assertTrue(audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING);

      long lastTime = System.nanoTime();
      while (keepAlive) {
        int bytesRead = audioRecord.read(byteBuffer, byteBuffer.capacity());
        if (bytesRead == byteBuffer.capacity()) {
          if (microphoneMute) {
            byteBuffer.clear();
            byteBuffer.put(emptyBytes);
          }
          // It's possible we've been shut down during the read, and stopRecording() tried and
          // failed to join this thread. To be a bit safer, try to avoid calling any native methods
          // in case they've been unregistered after stopRecording() returned.
          if (keepAlive) {
            nativeDataIsRecorded(nativeAudioRecord, bytesRead);
          }
          if (audioSamplesReadyCallback != null) {
            // Copy the entire byte buffer array. The start of the byteBuffer is not necessarily
            // at index 0.
            byte[] data = Arrays.copyOfRange(byteBuffer.array(), byteBuffer.arrayOffset(),
                byteBuffer.capacity() + byteBuffer.arrayOffset());
            audioSamplesReadyCallback.onWebRtcAudioRecordSamplesReady(
                new JavaAudioDeviceModule.AudioSamples(audioRecord.getAudioFormat(),
                    audioRecord.getChannelCount(), audioRecord.getSampleRate(), data));
          }
        } else {
          String errorMessage = "AudioRecord.read failed: " + bytesRead;
          Logging.e(TAG, errorMessage);
          if (bytesRead == AudioRecord.ERROR_INVALID_OPERATION) {
            keepAlive = false;
            reportWebRtcAudioRecordError(errorMessage);
          }
        }
      }

      try {
        if (audioRecord != null) {
          audioRecord.stop();
        }
      } catch (IllegalStateException e) {
        Logging.e(TAG, "AudioRecord.stop failed: " + e.getMessage());
      }
    }

    // Stops the inner thread loop and also calls AudioRecord.stop().
    // Does not block the calling thread.
    public void stopThread() {
      Logging.d(TAG, "stopThread");
      keepAlive = false;
    }
  }

  @CalledByNative
  WebRtcAudioRecord(Context context, AudioManager audioManager) {
    this(context, audioManager, DEFAULT_AUDIO_SOURCE, null /* errorCallback */,
        null /* audioSamplesReadyCallback */, WebRtcAudioEffects.isAcousticEchoCancelerSupported(),
        WebRtcAudioEffects.isNoiseSuppressorSupported());
  }

  public WebRtcAudioRecord(Context context, AudioManager audioManager, int audioSource,
      @Nullable AudioRecordErrorCallback errorCallback,
      @Nullable SamplesReadyCallback audioSamplesReadyCallback,
      boolean isAcousticEchoCancelerSupported, boolean isNoiseSuppressorSupported) {
    if (isAcousticEchoCancelerSupported && !WebRtcAudioEffects.isAcousticEchoCancelerSupported()) {
      throw new IllegalArgumentException("HW AEC not supported");
    }
    if (isNoiseSuppressorSupported && !WebRtcAudioEffects.isNoiseSuppressorSupported()) {
      throw new IllegalArgumentException("HW NS not supported");
    }
    this.context = context;
    this.audioManager = audioManager;
    this.audioSource = audioSource;
    this.errorCallback = errorCallback;
    this.audioSamplesReadyCallback = audioSamplesReadyCallback;
    this.isAcousticEchoCancelerSupported = isAcousticEchoCancelerSupported;
    this.isNoiseSuppressorSupported = isNoiseSuppressorSupported;
  }

  @CalledByNative
  public void setNativeAudioRecord(long nativeAudioRecord) {
    this.nativeAudioRecord = nativeAudioRecord;
  }

  @CalledByNative
  boolean isAcousticEchoCancelerSupported() {
    return isAcousticEchoCancelerSupported;
  }

  @CalledByNative
  boolean isNoiseSuppressorSupported() {
    return isNoiseSuppressorSupported;
  }

  @CalledByNative
  private boolean enableBuiltInAEC(boolean enable) {
    Logging.d(TAG, "enableBuiltInAEC(" + enable + ")");
    return effects.setAEC(enable);
  }

  @CalledByNative
  private boolean enableBuiltInNS(boolean enable) {
    Logging.d(TAG, "enableBuiltInNS(" + enable + ")");
    return effects.setNS(enable);
  }

  @CalledByNative
  private int initRecording(int sampleRate, int channels) {
    Logging.d(TAG, "initRecording(sampleRate=" + sampleRate + ", channels=" + channels + ")");
    if (audioRecord != null) {
      reportWebRtcAudioRecordInitError("InitRecording called twice without StopRecording.");
      return -1;
    }
    final int bytesPerFrame = channels * (BITS_PER_SAMPLE / 8);
    final int framesPerBuffer = sampleRate / BUFFERS_PER_SECOND;
    byteBuffer = ByteBuffer.allocateDirect(bytesPerFrame * framesPerBuffer);
    if (!(byteBuffer.hasArray())) {
      reportWebRtcAudioRecordInitError("ByteBuffer does not have backing array.");
      return -1;
    }
    Logging.d(TAG, "byteBuffer.capacity: " + byteBuffer.capacity());
    emptyBytes = new byte[byteBuffer.capacity()];
    // Rather than passing the ByteBuffer with every callback (requiring
    // the potentially expensive GetDirectBufferAddress) we simply have the
    // the native class cache the address to the memory once.
    nativeCacheDirectBufferAddress(nativeAudioRecord, byteBuffer);

    // Get the minimum buffer size required for the successful creation of
    // an AudioRecord object, in byte units.
    // Note that this size doesn't guarantee a smooth recording under load.
    final int channelConfig = channelCountToConfiguration(channels);
    int minBufferSize =
        AudioRecord.getMinBufferSize(sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT);
    if (minBufferSize == AudioRecord.ERROR || minBufferSize == AudioRecord.ERROR_BAD_VALUE) {
      reportWebRtcAudioRecordInitError("AudioRecord.getMinBufferSize failed: " + minBufferSize);
      return -1;
    }
    Logging.d(TAG, "AudioRecord.getMinBufferSize: " + minBufferSize);

    // Use a larger buffer size than the minimum required when creating the
    // AudioRecord instance to ensure smooth recording under load. It has been
    // verified that it does not increase the actual recording latency.
    int bufferSizeInBytes = Math.max(BUFFER_SIZE_FACTOR * minBufferSize, byteBuffer.capacity());
    Logging.d(TAG, "bufferSizeInBytes: " + bufferSizeInBytes);
    try {
      audioRecord = new AudioRecord(audioSource, sampleRate, channelConfig,
          AudioFormat.ENCODING_PCM_16BIT, bufferSizeInBytes);
    } catch (IllegalArgumentException e) {
      reportWebRtcAudioRecordInitError("AudioRecord ctor error: " + e.getMessage());
      releaseAudioResources();
      return -1;
    }
    if (audioRecord == null || audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
      reportWebRtcAudioRecordInitError("Failed to create a new AudioRecord instance");
      releaseAudioResources();
      return -1;
    }
    effects.enable(audioRecord.getAudioSessionId());
    logMainParameters();
    logMainParametersExtended();
    return framesPerBuffer;
  }

  @CalledByNative
  private boolean startRecording() {
    Logging.d(TAG, "startRecording");
    assertTrue(audioRecord != null);
    assertTrue(audioThread == null);
    try {
      audioRecord.startRecording();
    } catch (IllegalStateException e) {
      reportWebRtcAudioRecordStartError(AudioRecordStartErrorCode.AUDIO_RECORD_START_EXCEPTION,
          "AudioRecord.startRecording failed: " + e.getMessage());
      return false;
    }
    if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
      reportWebRtcAudioRecordStartError(AudioRecordStartErrorCode.AUDIO_RECORD_START_STATE_MISMATCH,
          "AudioRecord.startRecording failed - incorrect state :"
              + audioRecord.getRecordingState());
      return false;
    }
    audioThread = new AudioRecordThread("AudioRecordJavaThread");
    audioThread.start();
    return true;
  }

  @CalledByNative
  private boolean stopRecording() {
    Logging.d(TAG, "stopRecording");
    assertTrue(audioThread != null);
    audioThread.stopThread();
    if (!ThreadUtils.joinUninterruptibly(audioThread, AUDIO_RECORD_THREAD_JOIN_TIMEOUT_MS)) {
      Logging.e(TAG, "Join of AudioRecordJavaThread timed out");
      WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    }
    audioThread = null;
    effects.release();
    releaseAudioResources();
    return true;
  }

  private void logMainParameters() {
    Logging.d(TAG,
        "AudioRecord: "
            + "session ID: " + audioRecord.getAudioSessionId() + ", "
            + "channels: " + audioRecord.getChannelCount() + ", "
            + "sample rate: " + audioRecord.getSampleRate());
  }

  private void logMainParametersExtended() {
    if (Build.VERSION.SDK_INT >= 23) {
      Logging.d(TAG,
          "AudioRecord: "
              // The frame count of the native AudioRecord buffer.
              + "buffer size in frames: " + audioRecord.getBufferSizeInFrames());
    }
  }

  // Helper method which throws an exception  when an assertion has failed.
  private static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private int channelCountToConfiguration(int channels) {
    return (channels == 1 ? AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO);
  }

  private native void nativeCacheDirectBufferAddress(
      long nativeAudioRecordJni, ByteBuffer byteBuffer);
  private native void nativeDataIsRecorded(long nativeAudioRecordJni, int bytes);

  // Sets all recorded samples to zero if |mute| is true, i.e., ensures that
  // the microphone is muted.
  public void setMicrophoneMute(boolean mute) {
    Logging.w(TAG, "setMicrophoneMute(" + mute + ")");
    microphoneMute = mute;
  }

  // Releases the native AudioRecord resources.
  private void releaseAudioResources() {
    Logging.d(TAG, "releaseAudioResources");
    if (audioRecord != null) {
      audioRecord.release();
      audioRecord = null;
    }
  }

  private void reportWebRtcAudioRecordInitError(String errorMessage) {
    Logging.e(TAG, "Init recording error: " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioRecordInitError(errorMessage);
    }
  }

  private void reportWebRtcAudioRecordStartError(
      AudioRecordStartErrorCode errorCode, String errorMessage) {
    Logging.e(TAG, "Start recording error: " + errorCode + ". " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioRecordStartError(errorCode, errorMessage);
    }
  }

  private void reportWebRtcAudioRecordError(String errorMessage) {
    Logging.e(TAG, "Run-time recording error: " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioRecordError(errorMessage);
    }
  }
}
