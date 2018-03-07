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

/** Factory for instantiating the default webrtc::AudioProcessing implementation. */
public class DefaultAudioProcessingFactory implements AudioProcessingFactory {
  public DefaultAudioProcessingFactory() {
    this(null /* postProcessingFactory */);
  }

  /**
   * Allows injecting a PostProcessingFactory. A null PostProcessingFactory creates a
   * webrtc::AudioProcessing with nullptr webrtc::postProcessing.
   */
  public DefaultAudioProcessingFactory(PostProcessingFactory postProcessingFactory) {
    this.postProcessingFactory = postProcessingFactory;
  }

  /**
   * Creates a default webrtc::AudioProcessing module, which takes ownership of objects created by
   * its factories.
   */
  @Override
  public long createNative() {
    long nativePostProcessor = 0;
    if (postProcessingFactory != null) {
      nativePostProcessor = postProcessingFactory.createNative();
      if (nativePostProcessor == 0) {
        throw new NullPointerException(
            "PostProcessingFactory.createNative() may not return 0 (nullptr).");
      }
    }
    return nativeCreateAudioProcessing(nativePostProcessor);
  }

  private PostProcessingFactory postProcessingFactory;

  private static native long nativeCreateAudioProcessing(long nativePostProcessor);
}
