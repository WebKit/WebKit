/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/pubsubstateclient.h"

namespace buzz {

std::string PublishedNickKeySerializer::GetKey(
    const std::string& publisher_nick, const std::string& published_nick) {
  return published_nick;
}

std::string PublisherAndPublishedNicksKeySerializer::GetKey(
    const std::string& publisher_nick, const std::string& published_nick) {
  return publisher_nick + ":" + published_nick;
}

}  // namespace buzz
