/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.webrtc.MediaCodecUtils.EXYNOS_PREFIX;
import static org.webrtc.MediaCodecUtils.INTEL_PREFIX;
import static org.webrtc.MediaCodecUtils.NVIDIA_PREFIX;
import static org.webrtc.MediaCodecUtils.QCOM_PREFIX;

import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.os.Build;
import java.util.ArrayList;
import java.util.List;
import javax.annotation.Nullable;

/** Factory for Android hardware VideoDecoders. */
@SuppressWarnings("deprecation") // API level 16 requires use of deprecated methods.
public class HardwareVideoDecoderFactory implements VideoDecoderFactory {
  private static final String TAG = "HardwareVideoDecoderFactory";

  private final EglBase.Context sharedContext;

  /** Creates a HardwareVideoDecoderFactory that does not use surface textures. */
  @Deprecated // Not removed yet to avoid breaking callers.
  public HardwareVideoDecoderFactory() {
    this(null);
  }

  /**
   * Creates a HardwareVideoDecoderFactory that supports surface texture rendering using the given
   * shared context.  The context may be null.  If it is null, then surface support is disabled.
   */
  public HardwareVideoDecoderFactory(EglBase.Context sharedContext) {
    this.sharedContext = sharedContext;
  }

  @Nullable
  @Override
  public VideoDecoder createDecoder(VideoCodecInfo codecType) {
    VideoCodecType type = VideoCodecType.valueOf(codecType.getName());
    MediaCodecInfo info = findCodecForType(type);

    if (info == null) {
      return null;
    }

    CodecCapabilities capabilities = info.getCapabilitiesForType(type.mimeType());
    return new HardwareVideoDecoder(new MediaCodecWrapperFactoryImpl(), info.getName(), type,
        MediaCodecUtils.selectColorFormat(MediaCodecUtils.DECODER_COLOR_FORMATS, capabilities),
        sharedContext);
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    List<VideoCodecInfo> supportedCodecInfos = new ArrayList<VideoCodecInfo>();
    // Generate a list of supported codecs in order of preference:
    // VP8, VP9, H264 (high profile), and H264 (baseline profile).
    for (VideoCodecType type :
        new VideoCodecType[] {VideoCodecType.VP8, VideoCodecType.VP9, VideoCodecType.H264}) {
      MediaCodecInfo codec = findCodecForType(type);
      if (codec != null) {
        String name = type.name();
        if (type == VideoCodecType.H264 && isH264HighProfileSupported(codec)) {
          supportedCodecInfos.add(new VideoCodecInfo(
              name, MediaCodecUtils.getCodecProperties(type, /* highProfile= */ true)));
        }

        supportedCodecInfos.add(new VideoCodecInfo(
            name, MediaCodecUtils.getCodecProperties(type, /* highProfile= */ false)));
      }
    }

    return supportedCodecInfos.toArray(new VideoCodecInfo[supportedCodecInfos.size()]);
  }

  private @Nullable MediaCodecInfo findCodecForType(VideoCodecType type) {
    // HW decoding is not supported on builds before KITKAT.
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
      return null;
    }

    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = null;
      try {
        info = MediaCodecList.getCodecInfoAt(i);
      } catch (IllegalArgumentException e) {
        Logging.e(TAG, "Cannot retrieve encoder codec info", e);
      }

      if (info == null || info.isEncoder()) {
        continue;
      }

      if (isSupportedCodec(info, type)) {
        return info;
      }
    }
    return null; // No support for this type.
  }

  // Returns true if the given MediaCodecInfo indicates a supported encoder for the given type.
  private boolean isSupportedCodec(MediaCodecInfo info, VideoCodecType type) {
    if (!MediaCodecUtils.codecSupportsType(info, type)) {
      return false;
    }
    // Check for a supported color format.
    if (MediaCodecUtils.selectColorFormat(
            MediaCodecUtils.DECODER_COLOR_FORMATS, info.getCapabilitiesForType(type.mimeType()))
        == null) {
      return false;
    }
    return isHardwareSupported(info, type);
  }

  private boolean isHardwareSupported(MediaCodecInfo info, VideoCodecType type) {
    String name = info.getName();
    switch (type) {
      case VP8:
        // QCOM, Intel, Exynos, and Nvidia all supported for VP8.
        return name.startsWith(QCOM_PREFIX) || name.startsWith(INTEL_PREFIX)
            || name.startsWith(EXYNOS_PREFIX) || name.startsWith(NVIDIA_PREFIX);
      case VP9:
        // QCOM and Exynos supported for VP9.
        return name.startsWith(QCOM_PREFIX) || name.startsWith(EXYNOS_PREFIX);
      case H264:
        // QCOM, Intel, and Exynos supported for H264.
        return name.startsWith(QCOM_PREFIX) || name.startsWith(INTEL_PREFIX)
            || name.startsWith(EXYNOS_PREFIX);
      default:
        return false;
    }
  }

  private boolean isH264HighProfileSupported(MediaCodecInfo info) {
    String name = info.getName();
    // Support H.264 HP decoding on QCOM chips for Android L and above.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && name.startsWith(QCOM_PREFIX)) {
      return true;
    }
    // Support H.264 HP decoding on Exynos chips for Android M and above.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && name.startsWith(EXYNOS_PREFIX)) {
      return true;
    }
    return false;
  }
}
