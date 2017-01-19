/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_

#include <set>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/random.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/system_wrappers/include/static_instance.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// TODO(tommi, holmer): Look into whether we can eliminate locking in this
// class or the class itself completely once voice engine doesn't rely on it.
// At the moment voe_auto_test requires locking, but it's not clear if that's
// an issue with the test code or if it reflects real world usage or if that's
// the best design performance wise.
// If we do decide to keep the class, we should at least get rid of using
// StaticInstance.
class SSRCDatabase {
 public:
  static SSRCDatabase* GetSSRCDatabase();
  static void ReturnSSRCDatabase();

  uint32_t CreateSSRC();
  void RegisterSSRC(uint32_t ssrc);
  void ReturnSSRC(uint32_t ssrc);

 protected:
  SSRCDatabase();
  ~SSRCDatabase();

  static SSRCDatabase* CreateInstance() { return new SSRCDatabase(); }

  // Friend function to allow the SSRC destructor to be accessed from the
  // template class.
  friend SSRCDatabase* GetStaticInstance<SSRCDatabase>(
      CountOperation count_operation);

 private:
  rtc::CriticalSection crit_;
  Random random_ GUARDED_BY(crit_);
  std::set<uint32_t> ssrcs_ GUARDED_BY(crit_);
  // TODO(tommi): Use a thread checker to ensure the object is created and
  // deleted on the same thread.  At the moment this isn't possible due to
  // voe::ChannelOwner in voice engine.  To reproduce, run:
  // voe_auto_test --automated --gtest_filter=*MixManyChannelsForStressOpus
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_
