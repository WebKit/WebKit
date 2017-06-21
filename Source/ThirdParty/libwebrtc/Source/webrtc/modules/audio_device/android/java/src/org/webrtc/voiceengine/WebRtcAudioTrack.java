/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.voiceengine;

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Process;
import java.lang.Thread;
import java.nio.ByteBuffer;
import org.webrtc.ContextUtils;
import org.webrtc.Logging;

public class WebRtcAudioTrack {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioTrack";

  // Default audio data format is PCM 16 bit per sample.
  // Guaranteed to be supported by all devices.
  private static final int BITS_PER_SAMPLE = 16;

  // Requested size of each recorded buffer provided to the client.
  private static final int CALLBACK_BUFFER_SIZE_MS = 10;

  // Average number of callbacks per second.
  private static final int BUFFERS_PER_SECOND = 1000 / CALLBACK_BUFFER_SIZE_MS;

  private final long nativeAudioTrack;
  private final AudioManager audioManager;

  private ByteBuffer byteBuffer;

  private AudioTrack audioTrack = null;
  private AudioTrackThread audioThread = null;

  // Samples to be played are replaced by zeros if |speakerMute| is set to true.
  // Can be used to ensure that the speaker is fully muted.
  private static volatile boolean speakerMute = false;
  private byte[] emptyBytes;

  public static interface WebRtcAudioTrackErrorCallback {
    void onWebRtcAudioTrackInitError(String errorMessage);
    void onWebRtcAudioTrackStartError(String errorMessage);
    void onWebRtcAudioTrackError(String errorMessage);
  }

  private static WebRtcAudioTrackErrorCallback errorCallback = null;

  public static void setErrorCallback(WebRtcAudioTrackErrorCallback errorCallback) {
    Logging.d(TAG, "Set error callback");
    WebRtcAudioTrack.errorCallback = errorCallback;
  }

  /**
   * Audio thread which keeps calling AudioTrack.write() to stream audio.
   * Data is periodically acquired from the native WebRTC layer using the
   * nativeGetPlayoutData callback function.
   * This thread uses a Process.THREAD_PRIORITY_URGENT_AUDIO priority.
   */
  private class AudioTrackThread extends Thread {
    private volatile boolean keepAlive = true;

    public AudioTrackThread(String name) {
      super(name);
    }

    @Override
    public void run() {
      Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
      Logging.d(TAG, "AudioTrackThread" + WebRtcAudioUtils.getThreadInfo());

      try {
        // In MODE_STREAM mode we can optionally prime the output buffer by
        // writing up to bufferSizeInBytes (from constructor) before starting.
        // This priming will avoid an immediate underrun, but is not required.
        // TODO(henrika): initial tests have shown that priming is not required.
        audioTrack.play();
        assertTrue(audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING);
      } catch (IllegalStateException e) {
        reportWebRtcAudioTrackStartError("AudioTrack.play failed: " + e.getMessage());
        releaseAudioResources();
        return;
      }

      // Fixed size in bytes of each 10ms block of audio data that we ask for
      // using callbacks to the native WebRTC client.
      final int sizeInBytes = byteBuffer.capacity();

      while (keepAlive) {
        // Get 10ms of PCM data from the native WebRTC client. Audio data is
        // written into the common ByteBuffer using the address that was
        // cached at construction.
        nativeGetPlayoutData(sizeInBytes, nativeAudioTrack);
        // Write data until all data has been written to the audio sink.
        // Upon return, the buffer position will have been advanced to reflect
        // the amount of data that was successfully written to the AudioTrack.
        assertTrue(sizeInBytes <= byteBuffer.remaining());
        if (speakerMute) {
          byteBuffer.clear();
          byteBuffer.put(emptyBytes);
          byteBuffer.position(0);
        }
        int bytesWritten = 0;
        if (WebRtcAudioUtils.runningOnLollipopOrHigher()) {
          bytesWritten = writeOnLollipop(audioTrack, byteBuffer, sizeInBytes);
        } else {
          bytesWritten = writePreLollipop(audioTrack, byteBuffer, sizeInBytes);
        }
        if (bytesWritten != sizeInBytes) {
          Logging.e(TAG, "AudioTrack.write failed: " + bytesWritten);
          if (bytesWritten == AudioTrack.ERROR_INVALID_OPERATION) {
            keepAlive = false;
            reportWebRtcAudioTrackError("AudioTrack.write failed: " + bytesWritten);
          }
        }
        // The byte buffer must be rewinded since byteBuffer.position() is
        // increased at each call to AudioTrack.write(). If we don't do this,
        // next call to AudioTrack.write() will fail.
        byteBuffer.rewind();

        // TODO(henrika): it is possible to create a delay estimate here by
        // counting number of written frames and subtracting the result from
        // audioTrack.getPlaybackHeadPosition().
      }

      try {
        audioTrack.stop();
      } catch (IllegalStateException e) {
        Logging.e(TAG, "AudioTrack.stop failed: " + e.getMessage());
      }
      assertTrue(audioTrack.getPlayState() == AudioTrack.PLAYSTATE_STOPPED);
      audioTrack.flush();
    }

    @TargetApi(21)
    private int writeOnLollipop(AudioTrack audioTrack, ByteBuffer byteBuffer, int sizeInBytes) {
      return audioTrack.write(byteBuffer, sizeInBytes, AudioTrack.WRITE_BLOCKING);
    }

    private int writePreLollipop(AudioTrack audioTrack, ByteBuffer byteBuffer, int sizeInBytes) {
      return audioTrack.write(byteBuffer.array(), byteBuffer.arrayOffset(), sizeInBytes);
    }

    public void joinThread() {
      keepAlive = false;
      while (isAlive()) {
        try {
          join();
        } catch (InterruptedException e) {
          // Ignore.
        }
      }
    }
  }

  WebRtcAudioTrack(long nativeAudioTrack) {
    Logging.d(TAG, "ctor" + WebRtcAudioUtils.getThreadInfo());
    this.nativeAudioTrack = nativeAudioTrack;
    audioManager =
        (AudioManager) ContextUtils.getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
    if (DEBUG) {
      WebRtcAudioUtils.logDeviceInfo(TAG);
    }
  }

  private boolean initPlayout(int sampleRate, int channels) {
    Logging.d(TAG, "initPlayout(sampleRate=" + sampleRate + ", channels=" + channels + ")");
    final int bytesPerFrame = channels * (BITS_PER_SAMPLE / 8);
    byteBuffer = byteBuffer.allocateDirect(bytesPerFrame * (sampleRate / BUFFERS_PER_SECOND));
    Logging.d(TAG, "byteBuffer.capacity: " + byteBuffer.capacity());
    emptyBytes = new byte[byteBuffer.capacity()];
    // Rather than passing the ByteBuffer with every callback (requiring
    // the potentially expensive GetDirectBufferAddress) we simply have the
    // the native class cache the address to the memory once.
    nativeCacheDirectBufferAddress(byteBuffer, nativeAudioTrack);

    // Get the minimum buffer size required for the successful creation of an
    // AudioTrack object to be created in the MODE_STREAM mode.
    // Note that this size doesn't guarantee a smooth playback under load.
    // TODO(henrika): should we extend the buffer size to avoid glitches?
    final int channelConfig = channelCountToConfiguration(channels);
    final int minBufferSizeInBytes =
        AudioTrack.getMinBufferSize(sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT);
    Logging.d(TAG, "AudioTrack.getMinBufferSize: " + minBufferSizeInBytes);
    // For the streaming mode, data must be written to the audio sink in
    // chunks of size (given by byteBuffer.capacity()) less than or equal
    // to the total buffer size |minBufferSizeInBytes|. But, we have seen
    // reports of "getMinBufferSize(): error querying hardware". Hence, it
    // can happen that |minBufferSizeInBytes| contains an invalid value.
    if (minBufferSizeInBytes < byteBuffer.capacity()) {
      reportWebRtcAudioTrackInitError("AudioTrack.getMinBufferSize returns an invalid value.");
      return false;
    }

    // Ensure that prevision audio session was stopped correctly before trying
    // to create a new AudioTrack.
    if (audioTrack != null) {
      reportWebRtcAudioTrackInitError("Conflict with existing AudioTrack.");
      return false;
    }
    try {
      // Create an AudioTrack object and initialize its associated audio buffer.
      // The size of this buffer determines how long an AudioTrack can play
      // before running out of data.
      if (WebRtcAudioUtils.runningOnLollipopOrHigher()) {
        // If we are on API level 21 or higher, it is possible to use a special AudioTrack
        // constructor that uses AudioAttributes and AudioFormat as input. It allows us to
        // supersede the notion of stream types for defining the behavior of audio playback,
        // and to allow certain platforms or routing policies to use this information for more
        // refined volume or routing decisions.
        audioTrack = createAudioTrackOnLollipopOrHigher(
            sampleRate, channelConfig, minBufferSizeInBytes);
      } else {
        // Use default constructor for API levels below 21.
        // Note that, this constructor will be deprecated in API level O (25).
        audioTrack = new AudioTrack(AudioManager.STREAM_VOICE_CALL, sampleRate, channelConfig,
            AudioFormat.ENCODING_PCM_16BIT, minBufferSizeInBytes, AudioTrack.MODE_STREAM);
      }
    } catch (IllegalArgumentException e) {
      reportWebRtcAudioTrackInitError(e.getMessage());
      releaseAudioResources();
      return false;
    }

    // It can happen that an AudioTrack is created but it was not successfully
    // initialized upon creation. Seems to be the case e.g. when the maximum
    // number of globally available audio tracks is exceeded.
    if (audioTrack == null || audioTrack.getState() != AudioTrack.STATE_INITIALIZED) {
      reportWebRtcAudioTrackInitError("Initialization of audio track failed.");
      releaseAudioResources();
      return false;
    }
    logMainParameters();
    logMainParametersExtended();
    return true;
  }

  private boolean startPlayout() {
    Logging.d(TAG, "startPlayout");
    assertTrue(audioTrack != null);
    assertTrue(audioThread == null);
    if (audioTrack.getState() != AudioTrack.STATE_INITIALIZED) {
      reportWebRtcAudioTrackStartError("AudioTrack instance is not successfully initialized.");
      return false;
    }
    audioThread = new AudioTrackThread("AudioTrackJavaThread");
    audioThread.start();
    return true;
  }

  private boolean stopPlayout() {
    Logging.d(TAG, "stopPlayout");
    assertTrue(audioThread != null);
    logUnderrunCount();
    audioThread.joinThread();
    audioThread = null;
    releaseAudioResources();
    return true;
  }

  // Get max possible volume index for a phone call audio stream.
  private int getStreamMaxVolume() {
    Logging.d(TAG, "getStreamMaxVolume");
    assertTrue(audioManager != null);
    return audioManager.getStreamMaxVolume(AudioManager.STREAM_VOICE_CALL);
  }

  // Set current volume level for a phone call audio stream.
  private boolean setStreamVolume(int volume) {
    Logging.d(TAG, "setStreamVolume(" + volume + ")");
    assertTrue(audioManager != null);
    if (isVolumeFixed()) {
      Logging.e(TAG, "The device implements a fixed volume policy.");
      return false;
    }
    audioManager.setStreamVolume(AudioManager.STREAM_VOICE_CALL, volume, 0);
    return true;
  }

  private boolean isVolumeFixed() {
    if (!WebRtcAudioUtils.runningOnLollipopOrHigher())
      return false;
    return audioManager.isVolumeFixed();
  }

  /** Get current volume level for a phone call audio stream. */
  private int getStreamVolume() {
    Logging.d(TAG, "getStreamVolume");
    assertTrue(audioManager != null);
    return audioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL);
  }

  private void logMainParameters() {
    Logging.d(TAG, "AudioTrack: "
            + "session ID: " + audioTrack.getAudioSessionId() + ", "
            + "channels: " + audioTrack.getChannelCount() + ", "
            + "sample rate: " + audioTrack.getSampleRate() + ", "
            // Gain (>=1.0) expressed as linear multiplier on sample values.
            + "max gain: " + audioTrack.getMaxVolume());
  }

  // Creates and AudioTrack instance using AudioAttributes and AudioFormat as input.
  // It allows certain platforms or routing policies to use this information for more
  // refined volume or routing decisions.
  @TargetApi(21)
  private AudioTrack createAudioTrackOnLollipopOrHigher(
    int sampleRateInHz, int channelConfig, int bufferSizeInBytes) {
    Logging.d(TAG, "createAudioTrackOnLollipopOrHigher");
    // TODO(henrika): use setPerformanceMode(int) with PERFORMANCE_MODE_LOW_LATENCY to control
    // performance when Android O is supported. Add some logging in the mean time.
    final int nativeOutputSampleRate =
        AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_VOICE_CALL);
    Logging.d(TAG, "nativeOutputSampleRate: " + nativeOutputSampleRate);
    if (sampleRateInHz != nativeOutputSampleRate) {
      Logging.w(TAG, "Unable to use fast mode since requested sample rate is not native");
    }
    // Create an audio track where the audio usage is for VoIP and the content type is speech.
    return new AudioTrack(
        new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
        .build(),
        new AudioFormat.Builder()
          .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
          .setSampleRate(sampleRateInHz)
          .setChannelMask(channelConfig)
          .build(),
        bufferSizeInBytes,
        AudioTrack.MODE_STREAM,
        AudioManager.AUDIO_SESSION_ID_GENERATE);
  }

  @TargetApi(24)
  private void logMainParametersExtended() {
    if (WebRtcAudioUtils.runningOnMarshmallowOrHigher()) {
      Logging.d(TAG, "AudioTrack: "
              // The effective size of the AudioTrack buffer that the app writes to.
              + "buffer size in frames: " + audioTrack.getBufferSizeInFrames());
    }
    if (WebRtcAudioUtils.runningOnNougatOrHigher()) {
      Logging.d(TAG, "AudioTrack: "
              // Maximum size of the AudioTrack buffer in frames.
              + "buffer capacity in frames: " + audioTrack.getBufferCapacityInFrames());
    }
  }

  // Prints the number of underrun occurrences in the application-level write
  // buffer since the AudioTrack was created. An underrun occurs if the app does
  // not write audio data quickly enough, causing the buffer to underflow and a
  // potential audio glitch.
  // TODO(henrika): keep track of this value in the field and possibly add new
  // UMA stat if needed.
  @TargetApi(24)
  private void logUnderrunCount() {
    if (WebRtcAudioUtils.runningOnNougatOrHigher()) {
      Logging.d(TAG, "underrun count: " + audioTrack.getUnderrunCount());
    }
  }

  // Helper method which throws an exception  when an assertion has failed.
  private static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private int channelCountToConfiguration(int channels) {
    return (channels == 1 ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO);
  }

  private native void nativeCacheDirectBufferAddress(ByteBuffer byteBuffer, long nativeAudioRecord);

  private native void nativeGetPlayoutData(int bytes, long nativeAudioRecord);

  // Sets all samples to be played out to zero if |mute| is true, i.e.,
  // ensures that the speaker is muted.
  public static void setSpeakerMute(boolean mute) {
    Logging.w(TAG, "setSpeakerMute(" + mute + ")");
    speakerMute = mute;
  }

  // Releases the native AudioTrack resources.
  private void releaseAudioResources() {
    if (audioTrack != null) {
      audioTrack.release();
      audioTrack = null;
    }
  }

  private void reportWebRtcAudioTrackInitError(String errorMessage) {
    Logging.e(TAG, "Init error: " + errorMessage);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioTrackInitError(errorMessage);
    }
  }

  private void reportWebRtcAudioTrackStartError(String errorMessage) {
    Logging.e(TAG, "Start error: " + errorMessage);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioTrackStartError(errorMessage);
    }
  }

  private void reportWebRtcAudioTrackError(String errorMessage) {
    Logging.e(TAG, "Run-time playback error: " + errorMessage);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioTrackError(errorMessage);
    }
  }

}
