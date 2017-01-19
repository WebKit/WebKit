/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_EXAMPLES_PEERCONNECTION_SERVER_UTILS_H_
#define WEBRTC_EXAMPLES_PEERCONNECTION_SERVER_UTILS_H_
#pragma once

#include <assert.h>
#include <string>

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

std::string int2str(int i);
std::string size_t2str(size_t i);

#endif  // WEBRTC_EXAMPLES_PEERCONNECTION_SERVER_UTILS_H_
