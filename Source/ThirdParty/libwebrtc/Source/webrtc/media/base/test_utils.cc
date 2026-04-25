/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/test_utils.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "media/base/stream_params.h"

namespace webrtc {

StreamParams CreateSimStreamParams(const std::string& cname,
                                   const std::vector<uint32_t>& ssrcs) {
  StreamParams sp;
  SsrcGroup sg(kSimSsrcGroupSemantics, ssrcs);
  sp.ssrcs = ssrcs;
  sp.ssrc_groups.push_back(sg);
  sp.cname = cname;
  return sp;
}

// There should be an rtx_ssrc per ssrc.
StreamParams CreateSimWithRtxStreamParams(
    const std::string& cname,
    const std::vector<uint32_t>& ssrcs,
    const std::vector<uint32_t>& rtx_ssrcs) {
  StreamParams sp = CreateSimStreamParams(cname, ssrcs);
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    sp.AddFidSsrc(ssrcs[i], rtx_ssrcs[i]);
  }
  return sp;
}

// There should be one fec ssrc per ssrc.
StreamParams CreatePrimaryWithFecFrStreamParams(const std::string& cname,
                                                uint32_t primary_ssrc,
                                                uint32_t flexfec_ssrc) {
  StreamParams sp;
  sp.ssrcs = {primary_ssrc};
  sp.cname = cname;
  sp.AddFecFrSsrc(primary_ssrc, flexfec_ssrc);
  return sp;
}

}  // namespace webrtc
