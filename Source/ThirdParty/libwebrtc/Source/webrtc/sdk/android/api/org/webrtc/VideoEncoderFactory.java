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

/** Factory for creating VideoEncoders. */
interface VideoEncoderFactory {
  /** Creates an encoder for the given video codec. */
  public VideoEncoder createEncoder(VideoCodecInfo info);

  /** Enumerates the list of supported video codecs. */
  public VideoCodecInfo[] getSupportedCodecs();
}
