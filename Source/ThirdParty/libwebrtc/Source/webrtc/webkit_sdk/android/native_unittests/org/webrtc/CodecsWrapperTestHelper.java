/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.HashMap;
import java.util.Map;

public class CodecsWrapperTestHelper {
  @CalledByNative
  public static VideoCodecInfo createTestVideoCodecInfo() {
    Map<String, String> params = new HashMap<String, String>();
    params.put(
        VideoCodecInfo.H264_FMTP_PROFILE_LEVEL_ID, VideoCodecInfo.H264_CONSTRAINED_BASELINE_3_1);

    VideoCodecInfo codec_info = new VideoCodecInfo("H264", params);
    return codec_info;
  }

  @CalledByNative
  public static VideoEncoder createFakeVideoEncoder() {
    return new FakeVideoEncoder();
  }
}
