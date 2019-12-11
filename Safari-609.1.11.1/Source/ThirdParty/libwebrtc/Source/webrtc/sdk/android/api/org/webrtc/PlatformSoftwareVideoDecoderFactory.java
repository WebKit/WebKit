/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.media.MediaCodecInfo;
import android.support.annotation.Nullable;
import java.util.Arrays;

/** Factory for Android platform software VideoDecoders. */
public class PlatformSoftwareVideoDecoderFactory extends MediaCodecVideoDecoderFactory {
  /**
   * Default allowed predicate.
   */
  private static final Predicate<MediaCodecInfo> defaultAllowedPredicate =
      new Predicate<MediaCodecInfo>() {
        private String[] prefixWhitelist =
            Arrays.copyOf(MediaCodecUtils.SOFTWARE_IMPLEMENTATION_PREFIXES,
                MediaCodecUtils.SOFTWARE_IMPLEMENTATION_PREFIXES.length);

        @Override
        public boolean test(MediaCodecInfo arg) {
          final String name = arg.getName();
          for (String prefix : prefixWhitelist) {
            if (name.startsWith(prefix)) {
              return true;
            }
          }
          return false;
        }
      };

  /**
   * Creates a PlatformSoftwareVideoDecoderFactory that supports surface texture rendering.
   *
   * @param sharedContext The textures generated will be accessible from this context. May be null,
   *                      this disables texture support.
   */
  public PlatformSoftwareVideoDecoderFactory(@Nullable EglBase.Context sharedContext) {
    super(sharedContext, defaultAllowedPredicate);
  }
}
