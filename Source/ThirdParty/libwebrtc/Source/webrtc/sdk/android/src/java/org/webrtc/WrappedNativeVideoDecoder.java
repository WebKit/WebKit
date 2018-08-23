/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * Wraps a native webrtc::VideoDecoder.
 */
abstract class WrappedNativeVideoDecoder implements VideoDecoder {
  @Override public abstract long createNativeVideoDecoder();

  @Override
  public VideoCodecStatus initDecode(Settings settings, Callback decodeCallback) {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public VideoCodecStatus release() {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public VideoCodecStatus decode(EncodedImage frame, DecodeInfo info) {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public boolean getPrefersLateDecoding() {
    throw new UnsupportedOperationException("Not implemented.");
  }

  @Override
  public String getImplementationName() {
    throw new UnsupportedOperationException("Not implemented.");
  }
}
