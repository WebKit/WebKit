/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"

namespace webrtc {
namespace test {
#if defined(WEBRTC_WIN)
#define MAYBE_PsnrIsCollected DISABLED_PsnrIsCollected
#else
#define MAYBE_PsnrIsCollected PsnrIsCollected
#endif
TEST(PeerScenarioQualityTest, MAYBE_PsnrIsCollected) {
  VideoQualityAnalyzer analyzer;
  {
    PeerScenario s(*test_info_);
    auto caller = s.CreateClient(PeerScenarioClient::Config());
    auto callee = s.CreateClient(PeerScenarioClient::Config());
    PeerScenarioClient::VideoSendTrackConfig video_conf;
    video_conf.generator.squares_video->framerate = 20;
    auto video = caller->CreateVideo("VIDEO", video_conf);
    auto link_builder = s.net()->NodeBuilder().delay_ms(100).capacity_kbps(600);
    s.AttachVideoQualityAnalyzer(&analyzer, video.track, callee);
    s.SimpleConnection(caller, callee, {link_builder.Build().node},
                       {link_builder.Build().node});
    s.ProcessMessages(TimeDelta::Seconds(2));
    // Exit scope to ensure that there's no pending tasks reporting to analyzer.
  }

  // We expect ca 40 frames to be produced, but to avoid flakiness on slow
  // machines we only test for 10.
  EXPECT_GT(analyzer.stats().render.count, 10);
  EXPECT_GT(analyzer.stats().psnr_with_freeze.Mean(), 20);
}

}  // namespace test
}  // namespace webrtc
