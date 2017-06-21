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

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.os.Build;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Factory for android hardware video encoders. */
@SuppressWarnings("deprecation") // API 16 requires the use of deprecated methods.
public class HardwareVideoEncoderFactory implements VideoEncoderFactory {
  private static final String TAG = "HardwareVideoEncoderFactory";

  // Prefixes for supported hardware encoder component names.
  private static final String QCOM_PREFIX = "OMX.qcom.";
  private static final String EXYNOS_PREFIX = "OMX.Exynos.";
  private static final String INTEL_PREFIX = "OMX.Intel.";

  // Forced key frame interval - used to reduce color distortions on Qualcomm platforms.
  private static final int QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_L_MS = 15000;
  private static final int QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_M_MS = 20000;
  private static final int QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_N_MS = 15000;

  // List of devices with poor H.264 encoder quality.
  // HW H.264 encoder on below devices has poor bitrate control - actual
  // bitrates deviates a lot from the target value.
  private static final List<String> H264_HW_EXCEPTION_MODELS =
      Arrays.asList("SAMSUNG-SGH-I337", "Nexus 7", "Nexus 4");

  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;

  // Supported color formats, in order of preference.
  private static final int[] SUPPORTED_COLOR_FORMATS = {
      MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar,
      MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
      MediaCodecInfo.CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
      COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m};

  // Keys for H264 VideoCodecInfo properties.
  private static final String H264_FMTP_PROFILE_LEVEL_ID = "profile-level-id";
  private static final String H264_FMTP_LEVEL_ASYMMETRY_ALLOWED = "level-asymmetry-allowed";
  private static final String H264_FMTP_PACKETIZATION_MODE = "packetization-mode";

  // Supported H264 profile ids and levels.
  private static final String H264_PROFILE_CONSTRAINED_BASELINE = "4200";
  private static final String H264_PROFILE_CONSTRAINED_HIGH = "640c";
  private static final String H264_LEVEL_3_1 = "1f"; // 31 in hex.
  private static final String H264_CONSTRAINED_BASELINE_3_1 =
      H264_PROFILE_CONSTRAINED_BASELINE + H264_LEVEL_3_1;
  private static final String H264_CONSTRAINED_HIGH_3_1 =
      H264_PROFILE_CONSTRAINED_HIGH + H264_LEVEL_3_1;

  private final boolean enableIntelVp8Encoder;
  private final boolean enableH264HighProfile;

  public HardwareVideoEncoderFactory(boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    this.enableIntelVp8Encoder = enableIntelVp8Encoder;
    this.enableH264HighProfile = enableH264HighProfile;
  }

  @Override
  public VideoEncoder createEncoder(VideoCodecInfo input) {
    VideoCodecType type = VideoCodecType.valueOf(input.name);
    MediaCodecInfo info = findCodecForType(type);

    if (info == null) {
      return null; // No support for this type.
    }

    String codecName = info.getName();
    String mime = type.mimeType();
    int colorFormat = selectColorFormat(SUPPORTED_COLOR_FORMATS, info.getCapabilitiesForType(mime));

    return new HardwareVideoEncoder(codecName, type, colorFormat, getKeyFrameIntervalSec(type),
        getForcedKeyFrameIntervalMs(type, codecName), createBitrateAdjuster(type, codecName));
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
          supportedCodecInfos.add(new VideoCodecInfo(0, name, getCodecProperties(type, true)));
        }

        supportedCodecInfos.add(new VideoCodecInfo(0, name, getCodecProperties(type, false)));
      }
    }
    return supportedCodecInfos.toArray(new VideoCodecInfo[supportedCodecInfos.size()]);
  }

  private MediaCodecInfo findCodecForType(VideoCodecType type) {
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

      if (isSupportedCodec(info, type)) {
        return info;
      }
    }
    return null; // No support for this type.
  }

  // Returns true if the given MediaCodecInfo indicates a supported encoder for the given type.
  private boolean isSupportedCodec(MediaCodecInfo info, VideoCodecType type) {
    if (!codecSupportsType(info, type)) {
      return false;
    }
    // Check for a supported color format.
    if (selectColorFormat(SUPPORTED_COLOR_FORMATS, info.getCapabilitiesForType(type.mimeType()))
        == null) {
      return false;
    }
    return isHardwareSupportedInCurrentSdk(info, type);
  }

  private Integer selectColorFormat(int[] supportedColorFormats, CodecCapabilities capabilities) {
    for (int supportedColorFormat : supportedColorFormats) {
      for (int codecColorFormat : capabilities.colorFormats) {
        if (codecColorFormat == supportedColorFormat) {
          return codecColorFormat;
        }
      }
    }
    return null;
  }

  private boolean codecSupportsType(MediaCodecInfo info, VideoCodecType type) {
    for (String mimeType : info.getSupportedTypes()) {
      if (type.mimeType().equals(mimeType)) {
        return true;
      }
    }
    return false;
  }

  // Returns true if the given MediaCodecInfo indicates a hardware module that is supported on the
  // current SDK.
  private boolean isHardwareSupportedInCurrentSdk(MediaCodecInfo info, VideoCodecType type) {
    switch (type) {
      case VP8:
        return isHardwareSupportedInCurrentSdkVp8(info);
      case VP9:
        return isHardwareSupportedInCurrentSdkVp9(info);
      case H264:
        return isHardwareSupportedInCurrentSdkH264(info);
    }
    return false;
  }

  private boolean isHardwareSupportedInCurrentSdkVp8(MediaCodecInfo info) {
    String name = info.getName();
    // QCOM Vp8 encoder is supported in KITKAT or later.
    return (name.startsWith(QCOM_PREFIX) && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
        // Exynos VP8 encoder is supported in M or later.
        || (name.startsWith(EXYNOS_PREFIX) && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
        // Intel Vp8 encoder is supported in LOLLIPOP or later, with the intel encoder enabled.
        || (name.startsWith(INTEL_PREFIX) && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
               && enableIntelVp8Encoder);
  }

  private boolean isHardwareSupportedInCurrentSdkVp9(MediaCodecInfo info) {
    String name = info.getName();
    return (name.startsWith(QCOM_PREFIX) || name.startsWith(EXYNOS_PREFIX))
        // Both QCOM and Exynos VP9 encoders are supported in N or later.
        && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
  }

  private boolean isHardwareSupportedInCurrentSdkH264(MediaCodecInfo info) {
    // First, H264 hardware might perform poorly on this model.
    if (H264_HW_EXCEPTION_MODELS.contains(Build.MODEL)) {
      return false;
    }
    String name = info.getName();
    // QCOM H264 encoder is supported in KITKAT or later.
    return (name.startsWith(QCOM_PREFIX) && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
        // Exynos H264 encoder is supported in LOLLIPOP or later.
        || (name.startsWith(EXYNOS_PREFIX)
               && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP);
  }

  private int getKeyFrameIntervalSec(VideoCodecType type) {
    switch (type) {
      case VP8: // Fallthrough intended.
      case VP9:
        return 100;
      case H264:
        return 20;
    }
    throw new IllegalArgumentException("Unsupported VideoCodecType " + type);
  }

  private int getForcedKeyFrameIntervalMs(VideoCodecType type, String codecName) {
    if (type == VideoCodecType.VP8 && codecName.startsWith(QCOM_PREFIX)) {
      if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP
          || Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP_MR1) {
        return QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_L_MS;
      } else if (Build.VERSION.SDK_INT == Build.VERSION_CODES.M) {
        return QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_M_MS;
      } else if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
        return QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_N_MS;
      }
    }
    // Other codecs don't need key frame forcing.
    return 0;
  }

  private BitrateAdjuster createBitrateAdjuster(VideoCodecType type, String codecName) {
    if (codecName.startsWith(EXYNOS_PREFIX)) {
      if (type == VideoCodecType.VP8) {
        // Exynos VP8 encoders need dynamic bitrate adjustment.
        return new DynamicBitrateAdjuster();
      } else {
        // Exynos VP9 and H264 encoders need framerate-based bitrate adjustment.
        return new FramerateBitrateAdjuster();
      }
    }
    // Other codecs don't need bitrate adjustment.
    return new BaseBitrateAdjuster();
  }

  private boolean isH264HighProfileSupported(MediaCodecInfo info) {
    return enableH264HighProfile && info.getName().startsWith(QCOM_PREFIX);
  }

  private Map<String, String> getCodecProperties(VideoCodecType type, boolean highProfile) {
    switch (type) {
      case VP8:
      case VP9:
        return new HashMap<String, String>();
      case H264:
        Map<String, String> properties = new HashMap<>();
        properties.put(H264_FMTP_LEVEL_ASYMMETRY_ALLOWED, "1");
        properties.put(H264_FMTP_PACKETIZATION_MODE, "1");
        properties.put(H264_FMTP_PROFILE_LEVEL_ID,
            highProfile ? H264_CONSTRAINED_HIGH_3_1 : H264_CONSTRAINED_BASELINE_3_1);
        return properties;
      default:
        throw new IllegalArgumentException("Unsupported codec: " + type);
    }
  }
}
