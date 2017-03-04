/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_SIMULCAST_H_
#define WEBRTC_MEDIA_ENGINE_SIMULCAST_H_

#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/config.h"

namespace cricket {
struct StreamParams;

// TODO(sprang): Remove this, as we're moving away from temporal layer mode.
// Config for use with screen cast when temporal layers are enabled.
struct ScreenshareLayerConfig {
 public:
  ScreenshareLayerConfig(int tl0_bitrate, int tl1_bitrate);

  // Bitrates, for temporal layers 0 and 1.
  int tl0_bitrate_kbps;
  int tl1_bitrate_kbps;

  static ScreenshareLayerConfig GetDefault();

  // Parse bitrate from group name on format "(tl0_bitrate)-(tl1_bitrate)",
  // eg. "100-1000" for the default rates.
  static bool FromFieldTrialGroup(const std::string& group,
                                  ScreenshareLayerConfig* config);
};

// TODO(pthatcher): Write unit tests just for these functions,
// independent of WebrtcVideoEngine.

int GetTotalMaxBitrateBps(const std::vector<webrtc::VideoStream>& streams);

// Get the ssrcs of the SIM group from the stream params.
void GetSimulcastSsrcs(const StreamParams& sp, std::vector<uint32_t>* ssrcs);

// Get simulcast settings.
// TODO(sprang): Remove default parameter when it's not longer referenced.
std::vector<webrtc::VideoStream> GetSimulcastConfig(size_t max_streams,
                                                    int width,
                                                    int height,
                                                    int max_bitrate_bps,
                                                    int max_qp,
                                                    int max_framerate,
                                                    bool is_screencast = false);

bool UseSimulcastScreenshare();

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_SIMULCAST_H_
