/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.Build;
import android.os.SystemClock;
import android.view.Surface;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Queue;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

// Java-side of peerconnection_jni.cc:MediaCodecVideoDecoder.
// This class is an implementation detail of the Java PeerConnection API.
@SuppressWarnings("deprecation")
public class MediaCodecVideoDecoder {
  // This class is constructed, operated, and destroyed by its C++ incarnation,
  // so the class and its methods have non-public visibility.  The API this
  // class exposes aims to mimic the webrtc::VideoDecoder API as closely as
  // possibly to minimize the amount of translation work necessary.

  private static final String TAG = "MediaCodecVideoDecoder";
  private static final long MAX_DECODE_TIME_MS = 200;

  // TODO(magjed): Use MediaFormat constants when part of the public API.
  private static final String FORMAT_KEY_STRIDE = "stride";
  private static final String FORMAT_KEY_SLICE_HEIGHT = "slice-height";
  private static final String FORMAT_KEY_CROP_LEFT = "crop-left";
  private static final String FORMAT_KEY_CROP_RIGHT = "crop-right";
  private static final String FORMAT_KEY_CROP_TOP = "crop-top";
  private static final String FORMAT_KEY_CROP_BOTTOM = "crop-bottom";

  // Tracks webrtc::VideoCodecType.
  public enum VideoCodecType { VIDEO_CODEC_VP8, VIDEO_CODEC_VP9, VIDEO_CODEC_H264 }

  // Timeout for input buffer dequeue.
  private static final int DEQUEUE_INPUT_TIMEOUT = 500000;
  // Timeout for codec releasing.
  private static final int MEDIA_CODEC_RELEASE_TIMEOUT_MS = 5000;
  // Max number of output buffers queued before starting to drop decoded frames.
  private static final int MAX_QUEUED_OUTPUTBUFFERS = 3;
  // Active running decoder instance. Set in initDecode() (called from native code)
  // and reset to null in release() call.
  private static MediaCodecVideoDecoder runningInstance = null;
  private static MediaCodecVideoDecoderErrorCallback errorCallback = null;
  private static int codecErrors = 0;
  // List of disabled codec types - can be set from application.
  private static Set<String> hwDecoderDisabledTypes = new HashSet<String>();

  private Thread mediaCodecThread;
  private MediaCodec mediaCodec;
  private ByteBuffer[] inputBuffers;
  private ByteBuffer[] outputBuffers;
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  private static final String VP9_MIME_TYPE = "video/x-vnd.on2.vp9";
  private static final String H264_MIME_TYPE = "video/avc";
  // List of supported HW VP8 decoders.
  private static final String[] supportedVp8HwCodecPrefixes = {
      "OMX.qcom.", "OMX.Nvidia.", "OMX.Exynos.", "OMX.Intel."};
  // List of supported HW VP9 decoders.
  private static final String[] supportedVp9HwCodecPrefixes = {"OMX.qcom.", "OMX.Exynos."};
  // List of supported HW H.264 decoders.
  private static final String[] supportedH264HwCodecPrefixes = {
      "OMX.qcom.", "OMX.Intel.", "OMX.Exynos."};
  // List of supported HW H.264 high profile decoders.
  private static final String supportedQcomH264HighProfileHwCodecPrefix = "OMX.qcom.";
  private static final String supportedExynosH264HighProfileHwCodecPrefix = "OMX.Exynos.";

  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int COLOR_QCOM_FORMATYVU420PackedSemiPlanar32m4ka = 0x7FA30C01;
  private static final int COLOR_QCOM_FORMATYVU420PackedSemiPlanar16m4ka = 0x7FA30C02;
  private static final int COLOR_QCOM_FORMATYVU420PackedSemiPlanar64x32Tile2m8ka = 0x7FA30C03;
  private static final int COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;
  // Allowable color formats supported by codec - in order of preference.
  private static final List<Integer> supportedColorList = Arrays.asList(
      CodecCapabilities.COLOR_FormatYUV420Planar, CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
      CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
      COLOR_QCOM_FORMATYVU420PackedSemiPlanar32m4ka, COLOR_QCOM_FORMATYVU420PackedSemiPlanar16m4ka,
      COLOR_QCOM_FORMATYVU420PackedSemiPlanar64x32Tile2m8ka,
      COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m);

  private int colorFormat;
  private int width;
  private int height;
  private int stride;
  private int sliceHeight;
  private boolean hasDecodedFirstFrame;
  private final Queue<TimeStamps> decodeStartTimeMs = new ArrayDeque<TimeStamps>();
  private boolean useSurface;

  // The below variables are only used when decoding to a Surface.
  private TextureListener textureListener;
  private int droppedFrames;
  private Surface surface = null;
  private final Queue<DecodedOutputBuffer> dequeuedSurfaceOutputBuffers =
      new ArrayDeque<DecodedOutputBuffer>();

  // MediaCodec error handler - invoked when critical error happens which may prevent
  // further use of media codec API. Now it means that one of media codec instances
  // is hanging and can no longer be used in the next call.
  public static interface MediaCodecVideoDecoderErrorCallback {
    void onMediaCodecVideoDecoderCriticalError(int codecErrors);
  }

  public static void setErrorCallback(MediaCodecVideoDecoderErrorCallback errorCallback) {
    Logging.d(TAG, "Set error callback");
    MediaCodecVideoDecoder.errorCallback = errorCallback;
  }

  // Functions to disable HW decoding - can be called from applications for platforms
  // which have known HW decoding problems.
  public static void disableVp8HwCodec() {
    Logging.w(TAG, "VP8 decoding is disabled by application.");
    hwDecoderDisabledTypes.add(VP8_MIME_TYPE);
  }

  public static void disableVp9HwCodec() {
    Logging.w(TAG, "VP9 decoding is disabled by application.");
    hwDecoderDisabledTypes.add(VP9_MIME_TYPE);
  }

  public static void disableH264HwCodec() {
    Logging.w(TAG, "H.264 decoding is disabled by application.");
    hwDecoderDisabledTypes.add(H264_MIME_TYPE);
  }

  // Functions to query if HW decoding is supported.
  public static boolean isVp8HwSupported() {
    return !hwDecoderDisabledTypes.contains(VP8_MIME_TYPE)
        && (findDecoder(VP8_MIME_TYPE, supportedVp8HwCodecPrefixes) != null);
  }

  public static boolean isVp9HwSupported() {
    return !hwDecoderDisabledTypes.contains(VP9_MIME_TYPE)
        && (findDecoder(VP9_MIME_TYPE, supportedVp9HwCodecPrefixes) != null);
  }

  public static boolean isH264HwSupported() {
    return !hwDecoderDisabledTypes.contains(H264_MIME_TYPE)
        && (findDecoder(H264_MIME_TYPE, supportedH264HwCodecPrefixes) != null);
  }

  public static boolean isH264HighProfileHwSupported() {
    if (hwDecoderDisabledTypes.contains(H264_MIME_TYPE)) {
      return false;
    }
    // Support H.264 HP decoding on QCOM chips for Android L and above.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
        && findDecoder(H264_MIME_TYPE, new String[] {supportedQcomH264HighProfileHwCodecPrefix})
            != null) {
      return true;
    }
    // Support H.264 HP decoding on Exynos chips for Android M and above.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
        && findDecoder(H264_MIME_TYPE, new String[] {supportedExynosH264HighProfileHwCodecPrefix})
            != null) {
      return true;
    }
    return false;
  }

  public static void printStackTrace() {
    if (runningInstance != null && runningInstance.mediaCodecThread != null) {
      StackTraceElement[] mediaCodecStackTraces = runningInstance.mediaCodecThread.getStackTrace();
      if (mediaCodecStackTraces.length > 0) {
        Logging.d(TAG, "MediaCodecVideoDecoder stacks trace:");
        for (StackTraceElement stackTrace : mediaCodecStackTraces) {
          Logging.d(TAG, stackTrace.toString());
        }
      }
    }
  }

  // Helper struct for findDecoder() below.
  private static class DecoderProperties {
    public DecoderProperties(String codecName, int colorFormat) {
      this.codecName = codecName;
      this.colorFormat = colorFormat;
    }
    public final String codecName; // OpenMax component name for VP8 codec.
    public final int colorFormat; // Color format supported by codec.
  }

  private static DecoderProperties findDecoder(String mime, String[] supportedCodecPrefixes) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
      return null; // MediaCodec.setParameters is missing.
    }
    Logging.d(TAG, "Trying to find HW decoder for mime " + mime);
    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = null;
      try {
        info = MediaCodecList.getCodecInfoAt(i);
      } catch (IllegalArgumentException e) {
        Logging.e(TAG, "Cannot retrieve decoder codec info", e);
      }
      if (info == null || info.isEncoder()) {
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
      Logging.d(TAG, "Found candidate decoder " + name);

      // Check if this is supported decoder.
      boolean supportedCodec = false;
      for (String codecPrefix : supportedCodecPrefixes) {
        if (name.startsWith(codecPrefix)) {
          supportedCodec = true;
          break;
        }
      }
      if (!supportedCodec) {
        continue;
      }

      // Check if codec supports either yuv420 or nv12.
      CodecCapabilities capabilities;
      try {
        capabilities = info.getCapabilitiesForType(mime);
      } catch (IllegalArgumentException e) {
        Logging.e(TAG, "Cannot retrieve decoder capabilities", e);
        continue;
      }
      for (int colorFormat : capabilities.colorFormats) {
        Logging.v(TAG, "   Color: 0x" + Integer.toHexString(colorFormat));
      }
      for (int supportedColorFormat : supportedColorList) {
        for (int codecColorFormat : capabilities.colorFormats) {
          if (codecColorFormat == supportedColorFormat) {
            // Found supported HW decoder.
            Logging.d(TAG, "Found target decoder " + name + ". Color: 0x"
                    + Integer.toHexString(codecColorFormat));
            return new DecoderProperties(name, codecColorFormat);
          }
        }
      }
    }
    Logging.d(TAG, "No HW decoder found for mime " + mime);
    return null; // No HW decoder.
  }

  private void checkOnMediaCodecThread() throws IllegalStateException {
    if (mediaCodecThread.getId() != Thread.currentThread().getId()) {
      throw new IllegalStateException("MediaCodecVideoDecoder previously operated on "
          + mediaCodecThread + " but is now called on " + Thread.currentThread());
    }
  }

  // Pass null in |surfaceTextureHelper| to configure the codec for ByteBuffer output.
  private boolean initDecode(
      VideoCodecType type, int width, int height, SurfaceTextureHelper surfaceTextureHelper) {
    if (mediaCodecThread != null) {
      throw new RuntimeException("initDecode: Forgot to release()?");
    }

    String mime = null;
    useSurface = (surfaceTextureHelper != null);
    String[] supportedCodecPrefixes = null;
    if (type == VideoCodecType.VIDEO_CODEC_VP8) {
      mime = VP8_MIME_TYPE;
      supportedCodecPrefixes = supportedVp8HwCodecPrefixes;
    } else if (type == VideoCodecType.VIDEO_CODEC_VP9) {
      mime = VP9_MIME_TYPE;
      supportedCodecPrefixes = supportedVp9HwCodecPrefixes;
    } else if (type == VideoCodecType.VIDEO_CODEC_H264) {
      mime = H264_MIME_TYPE;
      supportedCodecPrefixes = supportedH264HwCodecPrefixes;
    } else {
      throw new RuntimeException("initDecode: Non-supported codec " + type);
    }
    DecoderProperties properties = findDecoder(mime, supportedCodecPrefixes);
    if (properties == null) {
      throw new RuntimeException("Cannot find HW decoder for " + type);
    }

    Logging.d(TAG, "Java initDecode: " + type + " : " + width + " x " + height + ". Color: 0x"
            + Integer.toHexString(properties.colorFormat) + ". Use Surface: " + useSurface);

    runningInstance = this; // Decoder is now running and can be queried for stack traces.
    mediaCodecThread = Thread.currentThread();
    try {
      this.width = width;
      this.height = height;
      stride = width;
      sliceHeight = height;

      if (useSurface) {
        textureListener = new TextureListener(surfaceTextureHelper);
        surface = new Surface(surfaceTextureHelper.getSurfaceTexture());
      }

      MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
      if (!useSurface) {
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, properties.colorFormat);
      }
      Logging.d(TAG, "  Format: " + format);
      mediaCodec = MediaCodecVideoEncoder.createByCodecName(properties.codecName);
      if (mediaCodec == null) {
        Logging.e(TAG, "Can not create media decoder");
        return false;
      }
      mediaCodec.configure(format, surface, null, 0);
      mediaCodec.start();

      colorFormat = properties.colorFormat;
      outputBuffers = mediaCodec.getOutputBuffers();
      inputBuffers = mediaCodec.getInputBuffers();
      decodeStartTimeMs.clear();
      hasDecodedFirstFrame = false;
      dequeuedSurfaceOutputBuffers.clear();
      droppedFrames = 0;
      Logging.d(TAG,
          "Input buffers: " + inputBuffers.length + ". Output buffers: " + outputBuffers.length);
      return true;
    } catch (IllegalStateException e) {
      Logging.e(TAG, "initDecode failed", e);
      return false;
    }
  }

  // Resets the decoder so it can start decoding frames with new resolution.
  // Flushes MediaCodec and clears decoder output buffers.
  private void reset(int width, int height) {
    if (mediaCodecThread == null || mediaCodec == null) {
      throw new RuntimeException("Incorrect reset call for non-initialized decoder.");
    }
    Logging.d(TAG, "Java reset: " + width + " x " + height);

    mediaCodec.flush();

    this.width = width;
    this.height = height;
    decodeStartTimeMs.clear();
    dequeuedSurfaceOutputBuffers.clear();
    hasDecodedFirstFrame = false;
    droppedFrames = 0;
  }

  private void release() {
    Logging.d(TAG, "Java releaseDecoder. Total number of dropped frames: " + droppedFrames);
    checkOnMediaCodecThread();

    // Run Mediacodec stop() and release() on separate thread since sometime
    // Mediacodec.stop() may hang.
    final CountDownLatch releaseDone = new CountDownLatch(1);

    Runnable runMediaCodecRelease = new Runnable() {
      @Override
      public void run() {
        try {
          Logging.d(TAG, "Java releaseDecoder on release thread");
          mediaCodec.stop();
          mediaCodec.release();
          Logging.d(TAG, "Java releaseDecoder on release thread done");
        } catch (Exception e) {
          Logging.e(TAG, "Media decoder release failed", e);
        }
        releaseDone.countDown();
      }
    };
    new Thread(runMediaCodecRelease).start();

    if (!ThreadUtils.awaitUninterruptibly(releaseDone, MEDIA_CODEC_RELEASE_TIMEOUT_MS)) {
      Logging.e(TAG, "Media decoder release timeout");
      codecErrors++;
      if (errorCallback != null) {
        Logging.e(TAG, "Invoke codec error callback. Errors: " + codecErrors);
        errorCallback.onMediaCodecVideoDecoderCriticalError(codecErrors);
      }
    }

    mediaCodec = null;
    mediaCodecThread = null;
    runningInstance = null;
    if (useSurface) {
      surface.release();
      surface = null;
      textureListener.release();
    }
    Logging.d(TAG, "Java releaseDecoder done");
  }

  // Dequeue an input buffer and return its index, -1 if no input buffer is
  // available, or -2 if the codec is no longer operative.
  private int dequeueInputBuffer() {
    checkOnMediaCodecThread();
    try {
      return mediaCodec.dequeueInputBuffer(DEQUEUE_INPUT_TIMEOUT);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "dequeueIntputBuffer failed", e);
      return -2;
    }
  }

  private boolean queueInputBuffer(int inputBufferIndex, int size, long presentationTimeStamUs,
      long timeStampMs, long ntpTimeStamp) {
    checkOnMediaCodecThread();
    try {
      inputBuffers[inputBufferIndex].position(0);
      inputBuffers[inputBufferIndex].limit(size);
      decodeStartTimeMs.add(
          new TimeStamps(SystemClock.elapsedRealtime(), timeStampMs, ntpTimeStamp));
      mediaCodec.queueInputBuffer(inputBufferIndex, 0, size, presentationTimeStamUs, 0);
      return true;
    } catch (IllegalStateException e) {
      Logging.e(TAG, "decode failed", e);
      return false;
    }
  }

  private static class TimeStamps {
    public TimeStamps(long decodeStartTimeMs, long timeStampMs, long ntpTimeStampMs) {
      this.decodeStartTimeMs = decodeStartTimeMs;
      this.timeStampMs = timeStampMs;
      this.ntpTimeStampMs = ntpTimeStampMs;
    }
    // Time when this frame was queued for decoding.
    private final long decodeStartTimeMs;
    // Only used for bookkeeping in Java. Stores C++ inputImage._timeStamp value for input frame.
    private final long timeStampMs;
    // Only used for bookkeeping in Java. Stores C++ inputImage.ntp_time_ms_ value for input frame.
    private final long ntpTimeStampMs;
  }

  // Helper struct for dequeueOutputBuffer() below.
  private static class DecodedOutputBuffer {
    public DecodedOutputBuffer(int index, int offset, int size, long presentationTimeStampMs,
        long timeStampMs, long ntpTimeStampMs, long decodeTime, long endDecodeTime) {
      this.index = index;
      this.offset = offset;
      this.size = size;
      this.presentationTimeStampMs = presentationTimeStampMs;
      this.timeStampMs = timeStampMs;
      this.ntpTimeStampMs = ntpTimeStampMs;
      this.decodeTimeMs = decodeTime;
      this.endDecodeTimeMs = endDecodeTime;
    }

    private final int index;
    private final int offset;
    private final int size;
    // Presentation timestamp returned in dequeueOutputBuffer call.
    private final long presentationTimeStampMs;
    // C++ inputImage._timeStamp value for output frame.
    private final long timeStampMs;
    // C++ inputImage.ntp_time_ms_ value for output frame.
    private final long ntpTimeStampMs;
    // Number of ms it took to decode this frame.
    private final long decodeTimeMs;
    // System time when this frame decoding finished.
    private final long endDecodeTimeMs;
  }

  // Helper struct for dequeueTextureBuffer() below.
  private static class DecodedTextureBuffer {
    private final int textureID;
    private final float[] transformMatrix;
    // Presentation timestamp returned in dequeueOutputBuffer call.
    private final long presentationTimeStampMs;
    // C++ inputImage._timeStamp value for output frame.
    private final long timeStampMs;
    // C++ inputImage.ntp_time_ms_ value for output frame.
    private final long ntpTimeStampMs;
    // Number of ms it took to decode this frame.
    private final long decodeTimeMs;
    // Interval from when the frame finished decoding until this buffer has been created.
    // Since there is only one texture, this interval depend on the time from when
    // a frame is decoded and provided to C++ and until that frame is returned to the MediaCodec
    // so that the texture can be updated with the next decoded frame.
    private final long frameDelayMs;

    // A DecodedTextureBuffer with zero |textureID| has special meaning and represents a frame
    // that was dropped.
    public DecodedTextureBuffer(int textureID, float[] transformMatrix,
        long presentationTimeStampMs, long timeStampMs, long ntpTimeStampMs, long decodeTimeMs,
        long frameDelay) {
      this.textureID = textureID;
      this.transformMatrix = transformMatrix;
      this.presentationTimeStampMs = presentationTimeStampMs;
      this.timeStampMs = timeStampMs;
      this.ntpTimeStampMs = ntpTimeStampMs;
      this.decodeTimeMs = decodeTimeMs;
      this.frameDelayMs = frameDelay;
    }
  }

  // Poll based texture listener.
  private static class TextureListener
      implements SurfaceTextureHelper.OnTextureFrameAvailableListener {
    private final SurfaceTextureHelper surfaceTextureHelper;
    // |newFrameLock| is used to synchronize arrival of new frames with wait()/notifyAll().
    private final Object newFrameLock = new Object();
    // |bufferToRender| is non-null when waiting for transition between addBufferToRender() to
    // onTextureFrameAvailable().
    private DecodedOutputBuffer bufferToRender;
    private DecodedTextureBuffer renderedBuffer;

    public TextureListener(SurfaceTextureHelper surfaceTextureHelper) {
      this.surfaceTextureHelper = surfaceTextureHelper;
      surfaceTextureHelper.startListening(this);
    }

    public void addBufferToRender(DecodedOutputBuffer buffer) {
      if (bufferToRender != null) {
        Logging.e(TAG, "Unexpected addBufferToRender() called while waiting for a texture.");
        throw new IllegalStateException("Waiting for a texture.");
      }
      bufferToRender = buffer;
    }

    public boolean isWaitingForTexture() {
      synchronized (newFrameLock) {
        return bufferToRender != null;
      }
    }

    // Callback from |surfaceTextureHelper|. May be called on an arbitrary thread.
    @Override
    public void onTextureFrameAvailable(
        int oesTextureId, float[] transformMatrix, long timestampNs) {
      synchronized (newFrameLock) {
        if (renderedBuffer != null) {
          Logging.e(
              TAG, "Unexpected onTextureFrameAvailable() called while already holding a texture.");
          throw new IllegalStateException("Already holding a texture.");
        }
        // |timestampNs| is always zero on some Android versions.
        renderedBuffer = new DecodedTextureBuffer(oesTextureId, transformMatrix,
            bufferToRender.presentationTimeStampMs, bufferToRender.timeStampMs,
            bufferToRender.ntpTimeStampMs, bufferToRender.decodeTimeMs,
            SystemClock.elapsedRealtime() - bufferToRender.endDecodeTimeMs);
        bufferToRender = null;
        newFrameLock.notifyAll();
      }
    }

    // Dequeues and returns a DecodedTextureBuffer if available, or null otherwise.
    @SuppressWarnings("WaitNotInLoop")
    public DecodedTextureBuffer dequeueTextureBuffer(int timeoutMs) {
      synchronized (newFrameLock) {
        if (renderedBuffer == null && timeoutMs > 0 && isWaitingForTexture()) {
          try {
            newFrameLock.wait(timeoutMs);
          } catch (InterruptedException e) {
            // Restore the interrupted status by reinterrupting the thread.
            Thread.currentThread().interrupt();
          }
        }
        DecodedTextureBuffer returnedBuffer = renderedBuffer;
        renderedBuffer = null;
        return returnedBuffer;
      }
    }

    public void release() {
      // SurfaceTextureHelper.stopListening() will block until any onTextureFrameAvailable() in
      // progress is done. Therefore, the call must be outside any synchronized
      // statement that is also used in the onTextureFrameAvailable() above to avoid deadlocks.
      surfaceTextureHelper.stopListening();
      synchronized (newFrameLock) {
        if (renderedBuffer != null) {
          surfaceTextureHelper.returnTextureFrame();
          renderedBuffer = null;
        }
      }
    }
  }

  // Returns null if no decoded buffer is available, and otherwise a DecodedByteBuffer.
  // Throws IllegalStateException if call is made on the wrong thread, if color format changes to an
  // unsupported format, or if |mediaCodec| is not in the Executing state. Throws CodecException
  // upon codec error.
  private DecodedOutputBuffer dequeueOutputBuffer(int dequeueTimeoutMs) {
    checkOnMediaCodecThread();
    if (decodeStartTimeMs.isEmpty()) {
      return null;
    }
    // Drain the decoder until receiving a decoded buffer or hitting
    // MediaCodec.INFO_TRY_AGAIN_LATER.
    final MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
    while (true) {
      final int result =
          mediaCodec.dequeueOutputBuffer(info, TimeUnit.MILLISECONDS.toMicros(dequeueTimeoutMs));
      switch (result) {
        case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
          outputBuffers = mediaCodec.getOutputBuffers();
          Logging.d(TAG, "Decoder output buffers changed: " + outputBuffers.length);
          if (hasDecodedFirstFrame) {
            throw new RuntimeException("Unexpected output buffer change event.");
          }
          break;
        case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
          MediaFormat format = mediaCodec.getOutputFormat();
          Logging.d(TAG, "Decoder format changed: " + format.toString());
          final int newWidth;
          final int newHeight;
          if (format.containsKey(FORMAT_KEY_CROP_LEFT) && format.containsKey(FORMAT_KEY_CROP_RIGHT)
              && format.containsKey(FORMAT_KEY_CROP_BOTTOM)
              && format.containsKey(FORMAT_KEY_CROP_TOP)) {
            newWidth = 1 + format.getInteger(FORMAT_KEY_CROP_RIGHT)
                - format.getInteger(FORMAT_KEY_CROP_LEFT);
            newHeight = 1 + format.getInteger(FORMAT_KEY_CROP_BOTTOM)
                - format.getInteger(FORMAT_KEY_CROP_TOP);
          } else {
            newWidth = format.getInteger(MediaFormat.KEY_WIDTH);
            newHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
          }
          if (hasDecodedFirstFrame && (newWidth != width || newHeight != height)) {
            throw new RuntimeException("Unexpected size change. Configured " + width + "*" + height
                + ". New " + newWidth + "*" + newHeight);
          }
          width = newWidth;
          height = newHeight;

          if (!useSurface && format.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
            colorFormat = format.getInteger(MediaFormat.KEY_COLOR_FORMAT);
            Logging.d(TAG, "Color: 0x" + Integer.toHexString(colorFormat));
            if (!supportedColorList.contains(colorFormat)) {
              throw new IllegalStateException("Non supported color format: " + colorFormat);
            }
          }
          if (format.containsKey(FORMAT_KEY_STRIDE)) {
            stride = format.getInteger(FORMAT_KEY_STRIDE);
          }
          if (format.containsKey(FORMAT_KEY_SLICE_HEIGHT)) {
            sliceHeight = format.getInteger(FORMAT_KEY_SLICE_HEIGHT);
          }
          Logging.d(TAG, "Frame stride and slice height: " + stride + " x " + sliceHeight);
          stride = Math.max(width, stride);
          sliceHeight = Math.max(height, sliceHeight);
          break;
        case MediaCodec.INFO_TRY_AGAIN_LATER:
          return null;
        default:
          hasDecodedFirstFrame = true;
          TimeStamps timeStamps = decodeStartTimeMs.remove();
          long decodeTimeMs = SystemClock.elapsedRealtime() - timeStamps.decodeStartTimeMs;
          if (decodeTimeMs > MAX_DECODE_TIME_MS) {
            Logging.e(TAG, "Very high decode time: " + decodeTimeMs + "ms"
                    + ". Q size: " + decodeStartTimeMs.size()
                    + ". Might be caused by resuming H264 decoding after a pause.");
            decodeTimeMs = MAX_DECODE_TIME_MS;
          }
          return new DecodedOutputBuffer(result, info.offset, info.size,
              TimeUnit.MICROSECONDS.toMillis(info.presentationTimeUs), timeStamps.timeStampMs,
              timeStamps.ntpTimeStampMs, decodeTimeMs, SystemClock.elapsedRealtime());
      }
    }
  }

  // Returns null if no decoded buffer is available, and otherwise a DecodedTextureBuffer.
  // Throws IllegalStateException if call is made on the wrong thread, if color format changes to an
  // unsupported format, or if |mediaCodec| is not in the Executing state. Throws CodecException
  // upon codec error. If |dequeueTimeoutMs| > 0, the oldest decoded frame will be dropped if
  // a frame can't be returned.
  private DecodedTextureBuffer dequeueTextureBuffer(int dequeueTimeoutMs) {
    checkOnMediaCodecThread();
    if (!useSurface) {
      throw new IllegalStateException("dequeueTexture() called for byte buffer decoding.");
    }
    DecodedOutputBuffer outputBuffer = dequeueOutputBuffer(dequeueTimeoutMs);
    if (outputBuffer != null) {
      dequeuedSurfaceOutputBuffers.add(outputBuffer);
    }

    MaybeRenderDecodedTextureBuffer();
    // Check if there is texture ready now by waiting max |dequeueTimeoutMs|.
    DecodedTextureBuffer renderedBuffer = textureListener.dequeueTextureBuffer(dequeueTimeoutMs);
    if (renderedBuffer != null) {
      MaybeRenderDecodedTextureBuffer();
      return renderedBuffer;
    }

    if ((dequeuedSurfaceOutputBuffers.size()
                >= Math.min(MAX_QUEUED_OUTPUTBUFFERS, outputBuffers.length)
            || (dequeueTimeoutMs > 0 && !dequeuedSurfaceOutputBuffers.isEmpty()))) {
      ++droppedFrames;
      // Drop the oldest frame still in dequeuedSurfaceOutputBuffers.
      // The oldest frame is owned by |textureListener| and can't be dropped since
      // mediaCodec.releaseOutputBuffer has already been called.
      final DecodedOutputBuffer droppedFrame = dequeuedSurfaceOutputBuffers.remove();
      if (dequeueTimeoutMs > 0) {
        // TODO(perkj): Re-add the below log when VideoRenderGUI has been removed or fixed to
        // return the one and only texture even if it does not render.
        Logging.w(TAG, "Draining decoder. Dropping frame with TS: "
                + droppedFrame.presentationTimeStampMs + ". Total number of dropped frames: "
                + droppedFrames);
      } else {
        Logging.w(TAG, "Too many output buffers " + dequeuedSurfaceOutputBuffers.size()
                + ". Dropping frame with TS: " + droppedFrame.presentationTimeStampMs
                + ". Total number of dropped frames: " + droppedFrames);
      }

      mediaCodec.releaseOutputBuffer(droppedFrame.index, false /* render */);
      return new DecodedTextureBuffer(0, null, droppedFrame.presentationTimeStampMs,
          droppedFrame.timeStampMs, droppedFrame.ntpTimeStampMs, droppedFrame.decodeTimeMs,
          SystemClock.elapsedRealtime() - droppedFrame.endDecodeTimeMs);
    }
    return null;
  }

  private void MaybeRenderDecodedTextureBuffer() {
    if (dequeuedSurfaceOutputBuffers.isEmpty() || textureListener.isWaitingForTexture()) {
      return;
    }
    // Get the first frame in the queue and render to the decoder output surface.
    final DecodedOutputBuffer buffer = dequeuedSurfaceOutputBuffers.remove();
    textureListener.addBufferToRender(buffer);
    mediaCodec.releaseOutputBuffer(buffer.index, true /* render */);
  }

  // Release a dequeued output byte buffer back to the codec for re-use. Should only be called for
  // non-surface decoding.
  // Throws IllegalStateException if the call is made on the wrong thread, if codec is configured
  // for surface decoding, or if |mediaCodec| is not in the Executing state. Throws
  // MediaCodec.CodecException upon codec error.
  private void returnDecodedOutputBuffer(int index)
      throws IllegalStateException, MediaCodec.CodecException {
    checkOnMediaCodecThread();
    if (useSurface) {
      throw new IllegalStateException("returnDecodedOutputBuffer() called for surface decoding.");
    }
    mediaCodec.releaseOutputBuffer(index, false /* render */);
  }
}
