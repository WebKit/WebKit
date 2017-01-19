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

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.opengl.GLES20;
import android.os.Build;
import android.os.Bundle;
import android.view.Surface;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

// Java-side of peerconnection_jni.cc:MediaCodecVideoEncoder.
// This class is an implementation detail of the Java PeerConnection API.
@TargetApi(19)
@SuppressWarnings("deprecation")
public class MediaCodecVideoEncoder {
  // This class is constructed, operated, and destroyed by its C++ incarnation,
  // so the class and its methods have non-public visibility.  The API this
  // class exposes aims to mimic the webrtc::VideoEncoder API as closely as
  // possibly to minimize the amount of translation work necessary.

  private static final String TAG = "MediaCodecVideoEncoder";

  // Tracks webrtc::VideoCodecType.
  public enum VideoCodecType { VIDEO_CODEC_VP8, VIDEO_CODEC_VP9, VIDEO_CODEC_H264 }

  private static final int MEDIA_CODEC_RELEASE_TIMEOUT_MS = 5000; // Timeout for codec releasing.
  private static final int DEQUEUE_TIMEOUT = 0; // Non-blocking, no wait.
  private static final int BITRATE_ADJUSTMENT_FPS = 30;
  private static final int MAXIMUM_INITIAL_FPS = 30;
  private static final double BITRATE_CORRECTION_SEC = 3.0;
  // Maximum bitrate correction scale - no more than 2 times.
  private static final double BITRATE_CORRECTION_MAX_SCALE = 2;
  // Amount of correction steps to reach correction maximum scale.
  private static final int BITRATE_CORRECTION_STEPS = 10;

  // Active running encoder instance. Set in initEncode() (called from native code)
  // and reset to null in release() call.
  private static MediaCodecVideoEncoder runningInstance = null;
  private static MediaCodecVideoEncoderErrorCallback errorCallback = null;
  private static int codecErrors = 0;
  // List of disabled codec types - can be set from application.
  private static Set<String> hwEncoderDisabledTypes = new HashSet<String>();

  private Thread mediaCodecThread;
  private MediaCodec mediaCodec;
  private ByteBuffer[] outputBuffers;
  private EglBase14 eglBase;
  private int width;
  private int height;
  private Surface inputSurface;
  private GlRectDrawer drawer;

  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  private static final String VP9_MIME_TYPE = "video/x-vnd.on2.vp9";
  private static final String H264_MIME_TYPE = "video/avc";

  // Type of bitrate adjustment for video encoder.
  public enum BitrateAdjustmentType {
    // No adjustment - video encoder has no known bitrate problem.
    NO_ADJUSTMENT,
    // Framerate based bitrate adjustment is required - HW encoder does not use frame
    // timestamps to calculate frame bitrate budget and instead is relying on initial
    // fps configuration assuming that all frames are coming at fixed initial frame rate.
    FRAMERATE_ADJUSTMENT,
    // Dynamic bitrate adjustment is required - HW encoder used frame timestamps, but actual
    // bitrate deviates too much from the target value.
    DYNAMIC_ADJUSTMENT
  }

  // Class describing supported media codec properties.
  private static class MediaCodecProperties {
    public final String codecPrefix;
    // Minimum Android SDK required for this codec to be used.
    public final int minSdk;
    // Flag if encoder implementation does not use frame timestamps to calculate frame bitrate
    // budget and instead is relying on initial fps configuration assuming that all frames are
    // coming at fixed initial frame rate. Bitrate adjustment is required for this case.
    public final BitrateAdjustmentType bitrateAdjustmentType;

    MediaCodecProperties(
        String codecPrefix, int minSdk, BitrateAdjustmentType bitrateAdjustmentType) {
      this.codecPrefix = codecPrefix;
      this.minSdk = minSdk;
      this.bitrateAdjustmentType = bitrateAdjustmentType;
    }
  }

  // List of supported HW VP8 encoders.
  private static final MediaCodecProperties qcomVp8HwProperties = new MediaCodecProperties(
      "OMX.qcom.", Build.VERSION_CODES.KITKAT, BitrateAdjustmentType.NO_ADJUSTMENT);
  private static final MediaCodecProperties exynosVp8HwProperties = new MediaCodecProperties(
      "OMX.Exynos.", Build.VERSION_CODES.M, BitrateAdjustmentType.DYNAMIC_ADJUSTMENT);
  private static final MediaCodecProperties[] vp8HwList =
      new MediaCodecProperties[] {qcomVp8HwProperties, exynosVp8HwProperties};

  // List of supported HW VP9 encoders.
  private static final MediaCodecProperties qcomVp9HwProperties = new MediaCodecProperties(
      "OMX.qcom.", Build.VERSION_CODES.M, BitrateAdjustmentType.NO_ADJUSTMENT);
  private static final MediaCodecProperties exynosVp9HwProperties = new MediaCodecProperties(
      "OMX.Exynos.", Build.VERSION_CODES.M, BitrateAdjustmentType.NO_ADJUSTMENT);
  private static final MediaCodecProperties[] vp9HwList =
      new MediaCodecProperties[] {qcomVp9HwProperties, exynosVp9HwProperties};

  // List of supported HW H.264 encoders.
  private static final MediaCodecProperties qcomH264HwProperties = new MediaCodecProperties(
      "OMX.qcom.", Build.VERSION_CODES.KITKAT, BitrateAdjustmentType.NO_ADJUSTMENT);
  private static final MediaCodecProperties exynosH264HwProperties = new MediaCodecProperties(
      "OMX.Exynos.", Build.VERSION_CODES.LOLLIPOP, BitrateAdjustmentType.FRAMERATE_ADJUSTMENT);
  private static final MediaCodecProperties[] h264HwList =
      new MediaCodecProperties[] {qcomH264HwProperties, exynosH264HwProperties};

  // List of devices with poor H.264 encoder quality.
  // HW H.264 encoder on below devices has poor bitrate control - actual
  // bitrates deviates a lot from the target value.
  private static final String[] H264_HW_EXCEPTION_MODELS =
      new String[] {"SAMSUNG-SGH-I337", "Nexus 7", "Nexus 4"};

  // Bitrate modes - should be in sync with OMX_VIDEO_CONTROLRATETYPE defined
  // in OMX_Video.h
  private static final int VIDEO_ControlRateConstant = 2;
  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;
  // Allowable color formats supported by codec - in order of preference.
  private static final int[] supportedColorList = {CodecCapabilities.COLOR_FormatYUV420Planar,
      CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
      CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
      COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m};
  private static final int[] supportedSurfaceColorList = {CodecCapabilities.COLOR_FormatSurface};
  private VideoCodecType type;
  private int colorFormat; // Used by native code.

  // Variables used for dynamic bitrate adjustment.
  private BitrateAdjustmentType bitrateAdjustmentType = BitrateAdjustmentType.NO_ADJUSTMENT;
  private double bitrateAccumulator;
  private double bitrateAccumulatorMax;
  private double bitrateObservationTimeMs;
  private int bitrateAdjustmentScaleExp;
  private int targetBitrateBps;
  private int targetFps;

  // SPS and PPS NALs (Config frame) for H.264.
  private ByteBuffer configData = null;

  // MediaCodec error handler - invoked when critical error happens which may prevent
  // further use of media codec API. Now it means that one of media codec instances
  // is hanging and can no longer be used in the next call.
  public static interface MediaCodecVideoEncoderErrorCallback {
    void onMediaCodecVideoEncoderCriticalError(int codecErrors);
  }

  public static void setErrorCallback(MediaCodecVideoEncoderErrorCallback errorCallback) {
    Logging.d(TAG, "Set error callback");
    MediaCodecVideoEncoder.errorCallback = errorCallback;
  }

  // Functions to disable HW encoding - can be called from applications for platforms
  // which have known HW decoding problems.
  public static void disableVp8HwCodec() {
    Logging.w(TAG, "VP8 encoding is disabled by application.");
    hwEncoderDisabledTypes.add(VP8_MIME_TYPE);
  }

  public static void disableVp9HwCodec() {
    Logging.w(TAG, "VP9 encoding is disabled by application.");
    hwEncoderDisabledTypes.add(VP9_MIME_TYPE);
  }

  public static void disableH264HwCodec() {
    Logging.w(TAG, "H.264 encoding is disabled by application.");
    hwEncoderDisabledTypes.add(H264_MIME_TYPE);
  }

  // Functions to query if HW encoding is supported.
  public static boolean isVp8HwSupported() {
    return !hwEncoderDisabledTypes.contains(VP8_MIME_TYPE)
        && (findHwEncoder(VP8_MIME_TYPE, vp8HwList, supportedColorList) != null);
  }

  public static boolean isVp9HwSupported() {
    return !hwEncoderDisabledTypes.contains(VP9_MIME_TYPE)
        && (findHwEncoder(VP9_MIME_TYPE, vp9HwList, supportedColorList) != null);
  }

  public static boolean isH264HwSupported() {
    return !hwEncoderDisabledTypes.contains(H264_MIME_TYPE)
        && (findHwEncoder(H264_MIME_TYPE, h264HwList, supportedColorList) != null);
  }

  public static boolean isVp8HwSupportedUsingTextures() {
    return !hwEncoderDisabledTypes.contains(VP8_MIME_TYPE)
        && (findHwEncoder(VP8_MIME_TYPE, vp8HwList, supportedSurfaceColorList) != null);
  }

  public static boolean isVp9HwSupportedUsingTextures() {
    return !hwEncoderDisabledTypes.contains(VP9_MIME_TYPE)
        && (findHwEncoder(VP9_MIME_TYPE, vp9HwList, supportedSurfaceColorList) != null);
  }

  public static boolean isH264HwSupportedUsingTextures() {
    return !hwEncoderDisabledTypes.contains(H264_MIME_TYPE)
        && (findHwEncoder(H264_MIME_TYPE, h264HwList, supportedSurfaceColorList) != null);
  }

  // Helper struct for findHwEncoder() below.
  private static class EncoderProperties {
    public EncoderProperties(
        String codecName, int colorFormat, BitrateAdjustmentType bitrateAdjustmentType) {
      this.codecName = codecName;
      this.colorFormat = colorFormat;
      this.bitrateAdjustmentType = bitrateAdjustmentType;
    }
    public final String codecName; // OpenMax component name for HW codec.
    public final int colorFormat; // Color format supported by codec.
    public final BitrateAdjustmentType bitrateAdjustmentType; // Bitrate adjustment type
  }

  private static EncoderProperties findHwEncoder(
      String mime, MediaCodecProperties[] supportedHwCodecProperties, int[] colorList) {
    // MediaCodec.setParameters is missing for JB and below, so bitrate
    // can not be adjusted dynamically.
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
      return null;
    }

    // Check if device is in H.264 exception list.
    if (mime.equals(H264_MIME_TYPE)) {
      List<String> exceptionModels = Arrays.asList(H264_HW_EXCEPTION_MODELS);
      if (exceptionModels.contains(Build.MODEL)) {
        Logging.w(TAG, "Model: " + Build.MODEL + " has black listed H.264 encoder.");
        return null;
      }
    }

    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = null;
      try {
        info = MediaCodecList.getCodecInfoAt(i);
      } catch (IllegalArgumentException e) {
        Logging.e(TAG, "Cannot retrieve encoder codec info", e);
      }
      if (info == null || !info.isEncoder()) {
        continue;
      }
      String name = null;
      for (String mimeType : info.getSupportedTypes()) {
        if (mimeType.equals(mime)) {
          name = info.getName();
          break;
        }
      }
      if (name == null) {
        continue; // No HW support in this codec; try the next one.
      }
      Logging.v(TAG, "Found candidate encoder " + name);

      // Check if this is supported HW encoder.
      boolean supportedCodec = false;
      BitrateAdjustmentType bitrateAdjustmentType = BitrateAdjustmentType.NO_ADJUSTMENT;
      for (MediaCodecProperties codecProperties : supportedHwCodecProperties) {
        if (name.startsWith(codecProperties.codecPrefix)) {
          if (Build.VERSION.SDK_INT < codecProperties.minSdk) {
            Logging.w(
                TAG, "Codec " + name + " is disabled due to SDK version " + Build.VERSION.SDK_INT);
            continue;
          }
          if (codecProperties.bitrateAdjustmentType != BitrateAdjustmentType.NO_ADJUSTMENT) {
            bitrateAdjustmentType = codecProperties.bitrateAdjustmentType;
            Logging.w(
                TAG, "Codec " + name + " requires bitrate adjustment: " + bitrateAdjustmentType);
          }
          supportedCodec = true;
          break;
        }
      }
      if (!supportedCodec) {
        continue;
      }

      // Check if HW codec supports known color format.
      CodecCapabilities capabilities;
      try {
        capabilities = info.getCapabilitiesForType(mime);
      } catch (IllegalArgumentException e) {
        Logging.e(TAG, "Cannot retrieve encoder capabilities", e);
        continue;
      }
      for (int colorFormat : capabilities.colorFormats) {
        Logging.v(TAG, "   Color: 0x" + Integer.toHexString(colorFormat));
      }

      for (int supportedColorFormat : colorList) {
        for (int codecColorFormat : capabilities.colorFormats) {
          if (codecColorFormat == supportedColorFormat) {
            // Found supported HW encoder.
            Logging.d(TAG, "Found target encoder for mime " + mime + " : " + name + ". Color: 0x"
                    + Integer.toHexString(codecColorFormat) + ". Bitrate adjustment: "
                    + bitrateAdjustmentType);
            return new EncoderProperties(name, codecColorFormat, bitrateAdjustmentType);
          }
        }
      }
    }
    return null; // No HW encoder.
  }

  private void checkOnMediaCodecThread() {
    if (mediaCodecThread.getId() != Thread.currentThread().getId()) {
      throw new RuntimeException("MediaCodecVideoEncoder previously operated on " + mediaCodecThread
          + " but is now called on " + Thread.currentThread());
    }
  }

  public static void printStackTrace() {
    if (runningInstance != null && runningInstance.mediaCodecThread != null) {
      StackTraceElement[] mediaCodecStackTraces = runningInstance.mediaCodecThread.getStackTrace();
      if (mediaCodecStackTraces.length > 0) {
        Logging.d(TAG, "MediaCodecVideoEncoder stacks trace:");
        for (StackTraceElement stackTrace : mediaCodecStackTraces) {
          Logging.d(TAG, stackTrace.toString());
        }
      }
    }
  }

  static MediaCodec createByCodecName(String codecName) {
    try {
      // In the L-SDK this call can throw IOException so in order to work in
      // both cases catch an exception.
      return MediaCodec.createByCodecName(codecName);
    } catch (Exception e) {
      return null;
    }
  }

  boolean initEncode(VideoCodecType type, int width, int height, int kbps, int fps,
      EglBase14.Context sharedContext) {
    final boolean useSurface = sharedContext != null;
    Logging.d(TAG, "Java initEncode: " + type + " : " + width + " x " + height + ". @ " + kbps
            + " kbps. Fps: " + fps + ". Encode from texture : " + useSurface);

    this.width = width;
    this.height = height;
    if (mediaCodecThread != null) {
      throw new RuntimeException("Forgot to release()?");
    }
    EncoderProperties properties = null;
    String mime = null;
    int keyFrameIntervalSec = 0;
    if (type == VideoCodecType.VIDEO_CODEC_VP8) {
      mime = VP8_MIME_TYPE;
      properties = findHwEncoder(
          VP8_MIME_TYPE, vp8HwList, useSurface ? supportedSurfaceColorList : supportedColorList);
      keyFrameIntervalSec = 100;
    } else if (type == VideoCodecType.VIDEO_CODEC_VP9) {
      mime = VP9_MIME_TYPE;
      properties = findHwEncoder(
          VP9_MIME_TYPE, vp9HwList, useSurface ? supportedSurfaceColorList : supportedColorList);
      keyFrameIntervalSec = 100;
    } else if (type == VideoCodecType.VIDEO_CODEC_H264) {
      mime = H264_MIME_TYPE;
      properties = findHwEncoder(
          H264_MIME_TYPE, h264HwList, useSurface ? supportedSurfaceColorList : supportedColorList);
      keyFrameIntervalSec = 20;
    }
    if (properties == null) {
      throw new RuntimeException("Can not find HW encoder for " + type);
    }
    runningInstance = this; // Encoder is now running and can be queried for stack traces.
    colorFormat = properties.colorFormat;
    bitrateAdjustmentType = properties.bitrateAdjustmentType;
    if (bitrateAdjustmentType == BitrateAdjustmentType.FRAMERATE_ADJUSTMENT) {
      fps = BITRATE_ADJUSTMENT_FPS;
    } else {
      fps = Math.min(fps, MAXIMUM_INITIAL_FPS);
    }
    Logging.d(TAG, "Color format: " + colorFormat + ". Bitrate adjustment: " + bitrateAdjustmentType
            + ". Initial fps: " + fps);
    targetBitrateBps = 1000 * kbps;
    targetFps = fps;
    bitrateAccumulatorMax = targetBitrateBps / 8.0;
    bitrateAccumulator = 0;
    bitrateObservationTimeMs = 0;
    bitrateAdjustmentScaleExp = 0;

    mediaCodecThread = Thread.currentThread();
    try {
      MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
      format.setInteger(MediaFormat.KEY_BIT_RATE, targetBitrateBps);
      format.setInteger("bitrate-mode", VIDEO_ControlRateConstant);
      format.setInteger(MediaFormat.KEY_COLOR_FORMAT, properties.colorFormat);
      format.setInteger(MediaFormat.KEY_FRAME_RATE, targetFps);
      format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, keyFrameIntervalSec);
      Logging.d(TAG, "  Format: " + format);
      mediaCodec = createByCodecName(properties.codecName);
      this.type = type;
      if (mediaCodec == null) {
        Logging.e(TAG, "Can not create media encoder");
        return false;
      }
      mediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);

      if (useSurface) {
        eglBase = new EglBase14(sharedContext, EglBase.CONFIG_RECORDABLE);
        // Create an input surface and keep a reference since we must release the surface when done.
        inputSurface = mediaCodec.createInputSurface();
        eglBase.createSurface(inputSurface);
        drawer = new GlRectDrawer();
      }
      mediaCodec.start();
      outputBuffers = mediaCodec.getOutputBuffers();
      Logging.d(TAG, "Output buffers: " + outputBuffers.length);

    } catch (IllegalStateException e) {
      Logging.e(TAG, "initEncode failed", e);
      return false;
    }
    return true;
  }

  ByteBuffer[] getInputBuffers() {
    ByteBuffer[] inputBuffers = mediaCodec.getInputBuffers();
    Logging.d(TAG, "Input buffers: " + inputBuffers.length);
    return inputBuffers;
  }

  boolean encodeBuffer(
      boolean isKeyframe, int inputBuffer, int size, long presentationTimestampUs) {
    checkOnMediaCodecThread();
    try {
      if (isKeyframe) {
        // Ideally MediaCodec would honor BUFFER_FLAG_SYNC_FRAME so we could
        // indicate this in queueInputBuffer() below and guarantee _this_ frame
        // be encoded as a key frame, but sadly that flag is ignored.  Instead,
        // we request a key frame "soon".
        Logging.d(TAG, "Sync frame request");
        Bundle b = new Bundle();
        b.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
        mediaCodec.setParameters(b);
      }
      mediaCodec.queueInputBuffer(inputBuffer, 0, size, presentationTimestampUs, 0);
      return true;
    } catch (IllegalStateException e) {
      Logging.e(TAG, "encodeBuffer failed", e);
      return false;
    }
  }

  boolean encodeTexture(boolean isKeyframe, int oesTextureId, float[] transformationMatrix,
      long presentationTimestampUs) {
    checkOnMediaCodecThread();
    try {
      if (isKeyframe) {
        Logging.d(TAG, "Sync frame request");
        Bundle b = new Bundle();
        b.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
        mediaCodec.setParameters(b);
      }
      eglBase.makeCurrent();
      // TODO(perkj): glClear() shouldn't be necessary since every pixel is covered anyway,
      // but it's a workaround for bug webrtc:5147.
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      drawer.drawOes(oesTextureId, transformationMatrix, width, height, 0, 0, width, height);
      eglBase.swapBuffers(TimeUnit.MICROSECONDS.toNanos(presentationTimestampUs));
      return true;
    } catch (RuntimeException e) {
      Logging.e(TAG, "encodeTexture failed", e);
      return false;
    }
  }

  void release() {
    Logging.d(TAG, "Java releaseEncoder");
    checkOnMediaCodecThread();

    // Run Mediacodec stop() and release() on separate thread since sometime
    // Mediacodec.stop() may hang.
    final CountDownLatch releaseDone = new CountDownLatch(1);

    Runnable runMediaCodecRelease = new Runnable() {
      @Override
      public void run() {
        try {
          Logging.d(TAG, "Java releaseEncoder on release thread");
          mediaCodec.stop();
          mediaCodec.release();
          Logging.d(TAG, "Java releaseEncoder on release thread done");
        } catch (Exception e) {
          Logging.e(TAG, "Media encoder release failed", e);
        }
        releaseDone.countDown();
      }
    };
    new Thread(runMediaCodecRelease).start();

    if (!ThreadUtils.awaitUninterruptibly(releaseDone, MEDIA_CODEC_RELEASE_TIMEOUT_MS)) {
      Logging.e(TAG, "Media encoder release timeout");
      codecErrors++;
      if (errorCallback != null) {
        Logging.e(TAG, "Invoke codec error callback. Errors: " + codecErrors);
        errorCallback.onMediaCodecVideoEncoderCriticalError(codecErrors);
      }
    }

    mediaCodec = null;
    mediaCodecThread = null;
    if (drawer != null) {
      drawer.release();
      drawer = null;
    }
    if (eglBase != null) {
      eglBase.release();
      eglBase = null;
    }
    if (inputSurface != null) {
      inputSurface.release();
      inputSurface = null;
    }
    runningInstance = null;
    Logging.d(TAG, "Java releaseEncoder done");
  }

  private boolean setRates(int kbps, int frameRate) {
    checkOnMediaCodecThread();

    int codecBitrateBps = 1000 * kbps;
    if (bitrateAdjustmentType == BitrateAdjustmentType.DYNAMIC_ADJUSTMENT) {
      bitrateAccumulatorMax = codecBitrateBps / 8.0;
      if (targetBitrateBps > 0 && codecBitrateBps < targetBitrateBps) {
        // Rescale the accumulator level if the accumulator max decreases
        bitrateAccumulator = bitrateAccumulator * codecBitrateBps / targetBitrateBps;
      }
    }
    targetBitrateBps = codecBitrateBps;
    targetFps = frameRate;

    // Adjust actual encoder bitrate based on bitrate adjustment type.
    if (bitrateAdjustmentType == BitrateAdjustmentType.FRAMERATE_ADJUSTMENT && targetFps > 0) {
      codecBitrateBps = BITRATE_ADJUSTMENT_FPS * targetBitrateBps / targetFps;
      Logging.v(TAG,
          "setRates: " + kbps + " -> " + (codecBitrateBps / 1000) + " kbps. Fps: " + targetFps);
    } else if (bitrateAdjustmentType == BitrateAdjustmentType.DYNAMIC_ADJUSTMENT) {
      Logging.v(TAG, "setRates: " + kbps + " kbps. Fps: " + targetFps + ". ExpScale: "
              + bitrateAdjustmentScaleExp);
      if (bitrateAdjustmentScaleExp != 0) {
        codecBitrateBps = (int) (codecBitrateBps * getBitrateScale(bitrateAdjustmentScaleExp));
      }
    } else {
      Logging.v(TAG, "setRates: " + kbps + " kbps. Fps: " + targetFps);
    }

    try {
      Bundle params = new Bundle();
      params.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, codecBitrateBps);
      mediaCodec.setParameters(params);
      return true;
    } catch (IllegalStateException e) {
      Logging.e(TAG, "setRates failed", e);
      return false;
    }
  }

  // Dequeue an input buffer and return its index, -1 if no input buffer is
  // available, or -2 if the codec is no longer operative.
  int dequeueInputBuffer() {
    checkOnMediaCodecThread();
    try {
      return mediaCodec.dequeueInputBuffer(DEQUEUE_TIMEOUT);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "dequeueIntputBuffer failed", e);
      return -2;
    }
  }

  // Helper struct for dequeueOutputBuffer() below.
  static class OutputBufferInfo {
    public OutputBufferInfo(
        int index, ByteBuffer buffer, boolean isKeyFrame, long presentationTimestampUs) {
      this.index = index;
      this.buffer = buffer;
      this.isKeyFrame = isKeyFrame;
      this.presentationTimestampUs = presentationTimestampUs;
    }

    public final int index;
    public final ByteBuffer buffer;
    public final boolean isKeyFrame;
    public final long presentationTimestampUs;
  }

  // Dequeue and return an output buffer, or null if no output is ready.  Return
  // a fake OutputBufferInfo with index -1 if the codec is no longer operable.
  OutputBufferInfo dequeueOutputBuffer() {
    checkOnMediaCodecThread();
    try {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      int result = mediaCodec.dequeueOutputBuffer(info, DEQUEUE_TIMEOUT);
      // Check if this is config frame and save configuration data.
      if (result >= 0) {
        boolean isConfigFrame = (info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
        if (isConfigFrame) {
          Logging.d(TAG, "Config frame generated. Offset: " + info.offset + ". Size: " + info.size);
          configData = ByteBuffer.allocateDirect(info.size);
          outputBuffers[result].position(info.offset);
          outputBuffers[result].limit(info.offset + info.size);
          configData.put(outputBuffers[result]);
          // Release buffer back.
          mediaCodec.releaseOutputBuffer(result, false);
          // Query next output.
          result = mediaCodec.dequeueOutputBuffer(info, DEQUEUE_TIMEOUT);
        }
      }
      if (result >= 0) {
        // MediaCodec doesn't care about Buffer position/remaining/etc so we can
        // mess with them to get a slice and avoid having to pass extra
        // (BufferInfo-related) parameters back to C++.
        ByteBuffer outputBuffer = outputBuffers[result].duplicate();
        outputBuffer.position(info.offset);
        outputBuffer.limit(info.offset + info.size);
        reportEncodedFrame(info.size);

        // Check key frame flag.
        boolean isKeyFrame = (info.flags & MediaCodec.BUFFER_FLAG_SYNC_FRAME) != 0;
        if (isKeyFrame) {
          Logging.d(TAG, "Sync frame generated");
        }
        if (isKeyFrame && type == VideoCodecType.VIDEO_CODEC_H264) {
          Logging.d(TAG, "Appending config frame of size " + configData.capacity()
                  + " to output buffer with offset " + info.offset + ", size " + info.size);
          // For H.264 key frame append SPS and PPS NALs at the start
          ByteBuffer keyFrameBuffer = ByteBuffer.allocateDirect(configData.capacity() + info.size);
          configData.rewind();
          keyFrameBuffer.put(configData);
          keyFrameBuffer.put(outputBuffer);
          keyFrameBuffer.position(0);
          return new OutputBufferInfo(result, keyFrameBuffer, isKeyFrame, info.presentationTimeUs);
        } else {
          return new OutputBufferInfo(
              result, outputBuffer.slice(), isKeyFrame, info.presentationTimeUs);
        }
      } else if (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
        outputBuffers = mediaCodec.getOutputBuffers();
        return dequeueOutputBuffer();
      } else if (result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        return dequeueOutputBuffer();
      } else if (result == MediaCodec.INFO_TRY_AGAIN_LATER) {
        return null;
      }
      throw new RuntimeException("dequeueOutputBuffer: " + result);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "dequeueOutputBuffer failed", e);
      return new OutputBufferInfo(-1, null, false, -1);
    }
  }

  private double getBitrateScale(int bitrateAdjustmentScaleExp) {
    return Math.pow(BITRATE_CORRECTION_MAX_SCALE,
        (double) bitrateAdjustmentScaleExp / BITRATE_CORRECTION_STEPS);
  }

  private void reportEncodedFrame(int size) {
    if (targetFps == 0 || bitrateAdjustmentType != BitrateAdjustmentType.DYNAMIC_ADJUSTMENT) {
      return;
    }

    // Accumulate the difference between actial and expected frame sizes.
    double expectedBytesPerFrame = targetBitrateBps / (8.0 * targetFps);
    bitrateAccumulator += (size - expectedBytesPerFrame);
    bitrateObservationTimeMs += 1000.0 / targetFps;

    // Put a cap on the accumulator, i.e., don't let it grow beyond some level to avoid
    // using too old data for bitrate adjustment.
    double bitrateAccumulatorCap = BITRATE_CORRECTION_SEC * bitrateAccumulatorMax;
    bitrateAccumulator = Math.min(bitrateAccumulator, bitrateAccumulatorCap);
    bitrateAccumulator = Math.max(bitrateAccumulator, -bitrateAccumulatorCap);

    // Do bitrate adjustment every 3 seconds if actual encoder bitrate deviates too much
    // form the target value.
    if (bitrateObservationTimeMs > 1000 * BITRATE_CORRECTION_SEC) {
      Logging.d(TAG, "Acc: " + (int) bitrateAccumulator + ". Max: " + (int) bitrateAccumulatorMax
              + ". ExpScale: " + bitrateAdjustmentScaleExp);
      boolean bitrateAdjustmentScaleChanged = false;
      if (bitrateAccumulator > bitrateAccumulatorMax) {
        // Encoder generates too high bitrate - need to reduce the scale.
        bitrateAccumulator = bitrateAccumulatorMax;
        bitrateAdjustmentScaleExp--;
        bitrateAdjustmentScaleChanged = true;
      } else if (bitrateAccumulator < -bitrateAccumulatorMax) {
        // Encoder generates too low bitrate - need to increase the scale.
        bitrateAdjustmentScaleExp++;
        bitrateAccumulator = -bitrateAccumulatorMax;
        bitrateAdjustmentScaleChanged = true;
      }
      if (bitrateAdjustmentScaleChanged) {
        bitrateAdjustmentScaleExp = Math.min(bitrateAdjustmentScaleExp, BITRATE_CORRECTION_STEPS);
        bitrateAdjustmentScaleExp = Math.max(bitrateAdjustmentScaleExp, -BITRATE_CORRECTION_STEPS);
        Logging.d(TAG, "Adjusting bitrate scale to " + bitrateAdjustmentScaleExp + ". Value: "
                + getBitrateScale(bitrateAdjustmentScaleExp));
        setRates(targetBitrateBps / 1000, targetFps);
      }
      bitrateObservationTimeMs = 0;
    }
  }

  // Release a dequeued output buffer back to the codec for re-use.  Return
  // false if the codec is no longer operable.
  boolean releaseOutputBuffer(int index) {
    checkOnMediaCodecThread();
    try {
      mediaCodec.releaseOutputBuffer(index, false);
      return true;
    } catch (IllegalStateException e) {
      Logging.e(TAG, "releaseOutputBuffer failed", e);
      return false;
    }
  }
}
