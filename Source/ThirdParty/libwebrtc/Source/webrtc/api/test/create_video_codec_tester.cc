/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/create_video_codec_tester.h"

#include <memory>
#include <utility>

#include "api/test/video_codec_tester.h"
#include "modules/video_coding/codecs/test/video_codec_tester_impl.h"

namespace webrtc {
namespace test {

std::unique_ptr<VideoCodecTester> CreateVideoCodecTester() {
  return std::make_unique<VideoCodecTesterImpl>();
}

}  // namespace test
}  // namespace webrtc
