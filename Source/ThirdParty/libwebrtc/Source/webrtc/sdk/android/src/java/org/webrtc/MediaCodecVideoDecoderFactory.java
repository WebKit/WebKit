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
import static org.webrtc.MediaCodecUtils.QCOM_PREFIX;

import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.os.Build;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import javax.annotation.Nullable;

/** Factory for decoders backed by Android MediaCodec API. */
@SuppressWarnings("deprecation") // API level 16 requires use of deprecated methods.
class MediaCodecVideoDecoderFactory implements VideoDecoderFactory {
  private static final String TAG = "MediaCodecVideoDecoderFactory";

  private final @Nullable EglBase.Context sharedContext;
  private final String[] prefixWhitelist;
  private final String[] prefixBlacklist;

  /**
   * MediaCodecVideoDecoderFactory will support codecs whitelisted excluding those blacklisted.
   *
   * @param sharedContext The textures generated will be accessible from this context. May be null,
   *                      this disables texture support.
   * @param prefixWhitelist List of codec prefixes to be whitelisted.
   * @param prefixBlacklist List of codec prefixes to be blacklisted.
   */
  public MediaCodecVideoDecoderFactory(
      @Nullable EglBase.Context sharedContext, String[] prefixWhitelist, String[] prefixBlacklist) {
    this.sharedContext = sharedContext;
    this.prefixWhitelist = Arrays.copyOf(prefixWhitelist, prefixWhitelist.length);
    this.prefixBlacklist = Arrays.copyOf(prefixBlacklist, prefixBlacklist.length);
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
    return new AndroidVideoDecoder(new MediaCodecWrapperFactoryImpl(), info.getName(), type,
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
        Logging.e(TAG, "Cannot retrieve decoder codec info", e);
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
    String name = info.getName();
    if (!MediaCodecUtils.codecSupportsType(info, type)) {
      return false;
    }
    // Check for a supported color format.
    if (MediaCodecUtils.selectColorFormat(
            MediaCodecUtils.DECODER_COLOR_FORMATS, info.getCapabilitiesForType(type.mimeType()))
        == null) {
      return false;
    }
    return isWhitelisted(name) && !isBlacklisted(name);
  }

  private boolean isWhitelisted(String name) {
    for (String prefix : prefixWhitelist) {
      if (name.startsWith(prefix)) {
        return true;
      }
    }
    return false;
  }

  private boolean isBlacklisted(String name) {
    for (String prefix : prefixBlacklist) {
      if (name.startsWith(prefix)) {
        return true;
      }
    }
    return false;
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
