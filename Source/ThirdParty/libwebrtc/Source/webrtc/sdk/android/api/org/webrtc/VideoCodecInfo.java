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

import java.util.Map;

/**
 * Represent a video codec as encoded in SDP.
 */
public class VideoCodecInfo {
  public final int payload;
  public final String name;
  public final Map<String, String> params;

  public VideoCodecInfo(int payload, String name, Map<String, String> params) {
    this.payload = payload;
    this.name = name;
    this.params = params;
  }
}
