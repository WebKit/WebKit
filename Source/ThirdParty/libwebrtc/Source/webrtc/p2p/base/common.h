/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_COMMON_H_
#define WEBRTC_P2P_BASE_COMMON_H_

#include "webrtc/base/logging.h"

// Common log description format for jingle messages
#define LOG_J(sev, obj) LOG(sev) << "Jingle:" << obj->ToString() << ": "
#define LOG_JV(sev, obj) LOG_V(sev) << "Jingle:" << obj->ToString() << ": "

#endif  // WEBRTC_P2P_BASE_COMMON_H_
