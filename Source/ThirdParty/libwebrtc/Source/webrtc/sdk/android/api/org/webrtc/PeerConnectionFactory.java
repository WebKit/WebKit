/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
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
import javax.annotation.Nullable;
import org.webrtc.Logging.Severity;
import org.webrtc.audio.AudioDeviceModule;
import org.webrtc.audio.LegacyAudioDeviceModule;

/**
 * Java wrapper for a C++ PeerConnectionFactoryInterface.  Main entry point to
 * the PeerConnection API for clients.
 */
public class PeerConnectionFactory {
  public static final String TRIAL_ENABLED = "Enabled";
  @Deprecated public static final String VIDEO_FRAME_EMIT_TRIAL = "VideoFrameEmit";

  private static final String TAG = "PeerConnectionFactory";
  private static final String VIDEO_CAPTURER_THREAD_NAME = "VideoCapturerThread";

  private long nativeFactory;
  private static volatile boolean internalTracerInitialized;
  @Nullable private static Thread networkThread;
  @Nullable private static Thread workerThread;
  @Nullable private static Thread signalingThread;

  public static class InitializationOptions {
    final Context applicationContext;
    final String fieldTrials;
    final boolean enableInternalTracer;
    final NativeLibraryLoader nativeLibraryLoader;
    final String nativeLibraryName;
    @Nullable Loggable loggable;
    @Nullable Severity loggableSeverity;

    private InitializationOptions(Context applicationContext, String fieldTrials,
        boolean enableInternalTracer, NativeLibraryLoader nativeLibraryLoader,
        String nativeLibraryName, @Nullable Loggable loggable,
        @Nullable Severity loggableSeverity) {
      this.applicationContext = applicationContext;
      this.fieldTrials = fieldTrials;
      this.enableInternalTracer = enableInternalTracer;
      this.nativeLibraryLoader = nativeLibraryLoader;
      this.nativeLibraryName = nativeLibraryName;
      this.loggable = loggable;
      this.loggableSeverity = loggableSeverity;
    }

    public static Builder builder(Context applicationContext) {
      return new Builder(applicationContext);
    }

    public static class Builder {
      private final Context applicationContext;
      private String fieldTrials = "";
      private boolean enableInternalTracer;
      private NativeLibraryLoader nativeLibraryLoader = new NativeLibrary.DefaultLoader();
      private String nativeLibraryName = "jingle_peerconnection_so";
      @Nullable private Loggable loggable;
      @Nullable private Severity loggableSeverity;

      Builder(Context applicationContext) {
        this.applicationContext = applicationContext;
      }

      public Builder setFieldTrials(String fieldTrials) {
        this.fieldTrials = fieldTrials;
        return this;
      }

      public Builder setEnableInternalTracer(boolean enableInternalTracer) {
        this.enableInternalTracer = enableInternalTracer;
        return this;
      }

      public Builder setNativeLibraryLoader(NativeLibraryLoader nativeLibraryLoader) {
        this.nativeLibraryLoader = nativeLibraryLoader;
        return this;
      }

      public Builder setNativeLibraryName(String nativeLibraryName) {
        this.nativeLibraryName = nativeLibraryName;
        return this;
      }

      public Builder setInjectableLogger(Loggable loggable, Severity severity) {
        this.loggable = loggable;
        this.loggableSeverity = severity;
        return this;
      }

      public PeerConnectionFactory.InitializationOptions createInitializationOptions() {
        return new PeerConnectionFactory.InitializationOptions(applicationContext, fieldTrials,
            enableInternalTracer, nativeLibraryLoader, nativeLibraryName, loggable,
            loggableSeverity);
      }
    }
  }

  public static class Options {
    // Keep in sync with webrtc/rtc_base/network.h!
    //
    // These bit fields are defined for |networkIgnoreMask| below.
    static final int ADAPTER_TYPE_UNKNOWN = 0;
    static final int ADAPTER_TYPE_ETHERNET = 1 << 0;
    static final int ADAPTER_TYPE_WIFI = 1 << 1;
    static final int ADAPTER_TYPE_CELLULAR = 1 << 2;
    static final int ADAPTER_TYPE_VPN = 1 << 3;
    static final int ADAPTER_TYPE_LOOPBACK = 1 << 4;
    static final int ADAPTER_TYPE_ANY = 1 << 5;

    public int networkIgnoreMask;
    public boolean disableEncryption;
    public boolean disableNetworkMonitor;
    public boolean enableAes128Sha1_32CryptoCipher;
    public boolean enableGcmCryptoSuites;

    @CalledByNative("Options")
    int getNetworkIgnoreMask() {
      return networkIgnoreMask;
    }

    @CalledByNative("Options")
    boolean getDisableEncryption() {
      return disableEncryption;
    }

    @CalledByNative("Options")
    boolean getDisableNetworkMonitor() {
      return disableNetworkMonitor;
    }

    @CalledByNative("Options")
    boolean getEnableAes128Sha1_32CryptoCipher() {
      return enableAes128Sha1_32CryptoCipher;
    }

    @CalledByNative("Options")
    boolean getEnableGcmCryptoSuites() {
      return enableGcmCryptoSuites;
    }
  }

  public static class Builder {
    private @Nullable Options options;
    private @Nullable AudioDeviceModule audioDeviceModule = new LegacyAudioDeviceModule();
    private @Nullable VideoEncoderFactory encoderFactory;
    private @Nullable VideoDecoderFactory decoderFactory;
    private @Nullable AudioProcessingFactory audioProcessingFactory;
    private @Nullable FecControllerFactoryFactoryInterface fecControllerFactoryFactory;

    private Builder() {}

    public Builder setOptions(Options options) {
      this.options = options;
      return this;
    }

    public Builder setAudioDeviceModule(AudioDeviceModule audioDeviceModule) {
      this.audioDeviceModule = audioDeviceModule;
      return this;
    }

    public Builder setVideoEncoderFactory(VideoEncoderFactory encoderFactory) {
      this.encoderFactory = encoderFactory;
      return this;
    }

    public Builder setVideoDecoderFactory(VideoDecoderFactory decoderFactory) {
      this.decoderFactory = decoderFactory;
      return this;
    }

    public Builder setAudioProcessingFactory(AudioProcessingFactory audioProcessingFactory) {
      if (audioProcessingFactory == null) {
        throw new NullPointerException(
            "PeerConnectionFactory builder does not accept a null AudioProcessingFactory.");
      }
      this.audioProcessingFactory = audioProcessingFactory;
      return this;
    }

    public Builder setFecControllerFactoryFactoryInterface(
        FecControllerFactoryFactoryInterface fecControllerFactoryFactory) {
      this.fecControllerFactoryFactory = fecControllerFactoryFactory;
      return this;
    }

    public PeerConnectionFactory createPeerConnectionFactory() {
      return new PeerConnectionFactory(options, audioDeviceModule, encoderFactory, decoderFactory,
          audioProcessingFactory, fecControllerFactoryFactory);
    }
  }

  public static Builder builder() {
    return new Builder();
  }

  /**
   * Loads and initializes WebRTC. This must be called at least once before creating a
   * PeerConnectionFactory. Replaces all the old initialization methods. Must not be called while
   * a PeerConnectionFactory is alive.
   */
  public static void initialize(InitializationOptions options) {
    ContextUtils.initialize(options.applicationContext);
    NativeLibrary.initialize(options.nativeLibraryLoader, options.nativeLibraryName);
    nativeInitializeAndroidGlobals();
    nativeInitializeFieldTrials(options.fieldTrials);
    if (options.enableInternalTracer && !internalTracerInitialized) {
      initializeInternalTracer();
    }
    if (options.loggable != null) {
      Logging.injectLoggable(options.loggable, options.loggableSeverity);
      nativeInjectLoggable(new JNILogging(options.loggable), options.loggableSeverity.ordinal());
    } else {
      Logging.d(TAG,
          "PeerConnectionFactory was initialized without an injected Loggable. "
              + "Any existing Loggable will be deleted.");
      Logging.deleteInjectedLoggable();
      nativeDeleteLoggable();
    }
  }

  private void checkInitializeHasBeenCalled() {
    if (!NativeLibrary.isLoaded() || ContextUtils.getApplicationContext() == null) {
      throw new IllegalStateException(
          "PeerConnectionFactory.initialize was not called before creating a "
          + "PeerConnectionFactory.");
    }
  }

  private static void initializeInternalTracer() {
    internalTracerInitialized = true;
    nativeInitializeInternalTracer();
  }

  public static void shutdownInternalTracer() {
    internalTracerInitialized = false;
    nativeShutdownInternalTracer();
  }

  // Field trial initialization. Must be called before PeerConnectionFactory
  // is created.
  // Deprecated, use PeerConnectionFactory.initialize instead.
  @Deprecated
  public static void initializeFieldTrials(String fieldTrialsInitString) {
    nativeInitializeFieldTrials(fieldTrialsInitString);
  }

  // Wrapper of webrtc::field_trial::FindFullName. Develop the feature with default behaviour off.
  // Example usage:
  // if (PeerConnectionFactory.fieldTrialsFindFullName("WebRTCExperiment").equals("Enabled")) {
  //   method1();
  // } else {
  //   method2();
  // }
  public static String fieldTrialsFindFullName(String name) {
    return NativeLibrary.isLoaded() ? nativeFindFieldTrialsFullName(name) : "";
  }
  // Start/stop internal capturing of internal tracing.
  public static boolean startInternalTracingCapture(String tracingFilename) {
    return nativeStartInternalTracingCapture(tracingFilename);
  }

  public static void stopInternalTracingCapture() {
    nativeStopInternalTracingCapture();
  }

  private PeerConnectionFactory(Options options, @Nullable AudioDeviceModule audioDeviceModule,
      @Nullable VideoEncoderFactory encoderFactory, @Nullable VideoDecoderFactory decoderFactory,
      @Nullable AudioProcessingFactory audioProcessingFactory,
      @Nullable FecControllerFactoryFactoryInterface fecControllerFactoryFactory) {
    checkInitializeHasBeenCalled();
    nativeFactory = nativeCreatePeerConnectionFactory(ContextUtils.getApplicationContext(), options,
        audioDeviceModule == null ? 0 : audioDeviceModule.getNativeAudioDeviceModulePointer(),
        encoderFactory, decoderFactory,
        audioProcessingFactory == null ? 0 : audioProcessingFactory.createNative(),
        fecControllerFactoryFactory == null ? 0 : fecControllerFactoryFactory.createNative());
    if (nativeFactory == 0) {
      throw new RuntimeException("Failed to initialize PeerConnectionFactory!");
    }
  }

  @CalledByNative
  PeerConnectionFactory(long nativeFactory) {
    checkInitializeHasBeenCalled();
    if (nativeFactory == 0) {
      throw new RuntimeException("Failed to initialize PeerConnectionFactory!");
    }
    this.nativeFactory = nativeFactory;
  }

  /**
   * Internal helper function to pass the parameters down into the native JNI bridge.
   */
  @Nullable
  PeerConnection createPeerConnectionInternal(PeerConnection.RTCConfiguration rtcConfig,
      MediaConstraints constraints, PeerConnection.Observer observer,
      SSLCertificateVerifier sslCertificateVerifier) {
    checkPeerConnectionFactoryExists();
    long nativeObserver = PeerConnection.createNativePeerConnectionObserver(observer);
    if (nativeObserver == 0) {
      return null;
    }
    long nativePeerConnection = nativeCreatePeerConnection(
        nativeFactory, rtcConfig, constraints, nativeObserver, sslCertificateVerifier);
    if (nativePeerConnection == 0) {
      return null;
    }
    return new PeerConnection(nativePeerConnection);
  }

  /**
   * Deprecated. PeerConnection constraints are deprecated. Supply values in rtcConfig struct
   * instead and use the method without constraints in the signature.
   */
  @Nullable
  @Deprecated
  public PeerConnection createPeerConnection(PeerConnection.RTCConfiguration rtcConfig,
      MediaConstraints constraints, PeerConnection.Observer observer) {
    return createPeerConnectionInternal(
        rtcConfig, constraints, observer, /* sslCertificateVerifier= */ null);
  }

  /**
   * Deprecated. PeerConnection constraints are deprecated. Supply values in rtcConfig struct
   * instead and use the method without constraints in the signature.
   */
  @Nullable
  @Deprecated
  public PeerConnection createPeerConnection(List<PeerConnection.IceServer> iceServers,
      MediaConstraints constraints, PeerConnection.Observer observer) {
    PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServers);
    return createPeerConnection(rtcConfig, constraints, observer);
  }

  @Nullable
  public PeerConnection createPeerConnection(
      List<PeerConnection.IceServer> iceServers, PeerConnection.Observer observer) {
    PeerConnection.RTCConfiguration rtcConfig = new PeerConnection.RTCConfiguration(iceServers);
    return createPeerConnection(rtcConfig, observer);
  }

  @Nullable
  public PeerConnection createPeerConnection(
      PeerConnection.RTCConfiguration rtcConfig, PeerConnection.Observer observer) {
    return createPeerConnection(rtcConfig, null /* constraints */, observer);
  }

  @Nullable
  public PeerConnection createPeerConnection(
      PeerConnection.RTCConfiguration rtcConfig, PeerConnectionDependencies dependencies) {
    return createPeerConnectionInternal(rtcConfig, null /* constraints */,
        dependencies.getObserver(), dependencies.getSSLCertificateVerifier());
  }

  public MediaStream createLocalMediaStream(String label) {
    checkPeerConnectionFactoryExists();
    return new MediaStream(nativeCreateLocalMediaStream(nativeFactory, label));
  }

  public VideoSource createVideoSource(boolean isScreencast) {
    checkPeerConnectionFactoryExists();
    return new VideoSource(nativeCreateVideoSource(nativeFactory, isScreencast));
  }

  public VideoTrack createVideoTrack(String id, VideoSource source) {
    checkPeerConnectionFactoryExists();
    return new VideoTrack(
        nativeCreateVideoTrack(nativeFactory, id, source.getNativeVideoTrackSource()));
  }

  public AudioSource createAudioSource(MediaConstraints constraints) {
    checkPeerConnectionFactoryExists();
    return new AudioSource(nativeCreateAudioSource(nativeFactory, constraints));
  }

  public AudioTrack createAudioTrack(String id, AudioSource source) {
    checkPeerConnectionFactoryExists();
    return new AudioTrack(nativeCreateAudioTrack(nativeFactory, id, source.getNativeAudioSource()));
  }

  // Starts recording an AEC dump. Ownership of the file is transfered to the
  // native code. If an AEC dump is already in progress, it will be stopped and
  // a new one will start using the provided file.
  public boolean startAecDump(int file_descriptor, int filesize_limit_bytes) {
    checkPeerConnectionFactoryExists();
    return nativeStartAecDump(nativeFactory, file_descriptor, filesize_limit_bytes);
  }

  // Stops recording an AEC dump. If no AEC dump is currently being recorded,
  // this call will have no effect.
  public void stopAecDump() {
    checkPeerConnectionFactoryExists();
    nativeStopAecDump(nativeFactory);
  }

  public void dispose() {
    checkPeerConnectionFactoryExists();
    nativeFreeFactory(nativeFactory);
    networkThread = null;
    workerThread = null;
    signalingThread = null;
    MediaCodecVideoEncoder.disposeEglContext();
    MediaCodecVideoDecoder.disposeEglContext();
    nativeFactory = 0;
  }

  public void threadsCallbacks() {
    checkPeerConnectionFactoryExists();
    nativeInvokeThreadsCallbacks(nativeFactory);
  }

  /** Returns a pointer to the native webrtc::PeerConnectionFactoryInterface. */
  public long getNativePeerConnectionFactory() {
    checkPeerConnectionFactoryExists();
    return nativeGetNativePeerConnectionFactory(nativeFactory);
  }

  /** Returns a pointer to the native OwnedFactoryAndThreads object */
  public long getNativeOwnedFactoryAndThreads() {
    checkPeerConnectionFactoryExists();
    return nativeFactory;
  }

  private void checkPeerConnectionFactoryExists() {
    if (nativeFactory == 0) {
      throw new IllegalStateException("PeerConnectionFactory has been disposed.");
    }
  }

  private static void printStackTrace(@Nullable Thread thread, String threadName) {
    if (thread != null) {
      StackTraceElement[] stackTraces = thread.getStackTrace();
      if (stackTraces.length > 0) {
        Logging.d(TAG, threadName + " stacks trace:");
        for (StackTraceElement stackTrace : stackTraces) {
          Logging.d(TAG, stackTrace.toString());
        }
      }
    }
  }

  public static void printStackTraces() {
    printStackTrace(networkThread, "Network thread");
    printStackTrace(workerThread, "Worker thread");
    printStackTrace(signalingThread, "Signaling thread");
  }

  @CalledByNative
  private static void onNetworkThreadReady() {
    networkThread = Thread.currentThread();
    Logging.d(TAG, "onNetworkThreadReady");
  }

  @CalledByNative
  private static void onWorkerThreadReady() {
    workerThread = Thread.currentThread();
    Logging.d(TAG, "onWorkerThreadReady");
  }

  @CalledByNative
  private static void onSignalingThreadReady() {
    signalingThread = Thread.currentThread();
    Logging.d(TAG, "onSignalingThreadReady");
  }

  // Must be called at least once before creating a PeerConnectionFactory
  // (for example, at application startup time).
  private static native void nativeInitializeAndroidGlobals();
  private static native void nativeInitializeFieldTrials(String fieldTrialsInitString);
  private static native String nativeFindFieldTrialsFullName(String name);
  private static native void nativeInitializeInternalTracer();
  // Internal tracing shutdown, called to prevent resource leaks. Must be called after
  // PeerConnectionFactory is gone to prevent races with code performing tracing.
  private static native void nativeShutdownInternalTracer();
  private static native boolean nativeStartInternalTracingCapture(String tracingFilename);
  private static native void nativeStopInternalTracingCapture();
  private static native long nativeCreatePeerConnectionFactory(Context context, Options options,
      long nativeAudioDeviceModule, VideoEncoderFactory encoderFactory,
      VideoDecoderFactory decoderFactory, long nativeAudioProcessor,
      long nativeFecControllerFactory);
  private static native long nativeCreatePeerConnection(long factory,
      PeerConnection.RTCConfiguration rtcConfig, MediaConstraints constraints, long nativeObserver,
      SSLCertificateVerifier sslCertificateVerifier);
  private static native long nativeCreateLocalMediaStream(long factory, String label);
  private static native long nativeCreateVideoSource(long factory, boolean is_screencast);
  private static native long nativeCreateVideoTrack(
      long factory, String id, long nativeVideoSource);
  private static native long nativeCreateAudioSource(long factory, MediaConstraints constraints);
  private static native long nativeCreateAudioTrack(long factory, String id, long nativeSource);
  private static native boolean nativeStartAecDump(
      long factory, int file_descriptor, int filesize_limit_bytes);
  private static native void nativeStopAecDump(long factory);
  private static native void nativeInvokeThreadsCallbacks(long factory);
  private static native void nativeFreeFactory(long factory);
  private static native long nativeGetNativePeerConnectionFactory(long factory);
  private static native void nativeInjectLoggable(JNILogging jniLogging, int severity);
  private static native void nativeDeleteLoggable();
}
