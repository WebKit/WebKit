/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_WEBRTCCOMMON_H_
#define WEBRTC_MEDIA_ENGINE_WEBRTCCOMMON_H_

#include "webrtc/common_types.h"

namespace cricket {

// Tracing helpers, for easy logging when WebRTC calls fail.
// Example: "LOG_RTCERR1(StartSend, channel);" produces the trace
//          "StartSend(1) failed, err=XXXX"
// The method GetLastEngineError must be defined in the calling scope.
#define LOG_RTCERR0(func) \
    LOG_RTCERR0_EX(func, GetLastEngineError())
#define LOG_RTCERR1(func, a1) \
    LOG_RTCERR1_EX(func, a1, GetLastEngineError())
#define LOG_RTCERR2(func, a1, a2) \
    LOG_RTCERR2_EX(func, a1, a2, GetLastEngineError())
#define LOG_RTCERR3(func, a1, a2, a3) \
    LOG_RTCERR3_EX(func, a1, a2, a3, GetLastEngineError())
#define LOG_RTCERR4(func, a1, a2, a3, a4) \
    LOG_RTCERR4_EX(func, a1, a2, a3, a4, GetLastEngineError())
#define LOG_RTCERR5(func, a1, a2, a3, a4, a5) \
    LOG_RTCERR5_EX(func, a1, a2, a3, a4, a5, GetLastEngineError())
#define LOG_RTCERR6(func, a1, a2, a3, a4, a5, a6) \
    LOG_RTCERR6_EX(func, a1, a2, a3, a4, a5, a6, GetLastEngineError())
#define LOG_RTCERR0_EX(func, err) LOG(LS_WARNING) \
    << "" << #func << "() failed, err=" << err
#define LOG_RTCERR1_EX(func, a1, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ") failed, err=" << err
#define LOG_RTCERR2_EX(func, a1, a2, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ") failed, err=" \
    << err
#define LOG_RTCERR3_EX(func, a1, a2, a3, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ", " << a3 \
    << ") failed, err=" << err
#define LOG_RTCERR4_EX(func, a1, a2, a3, a4, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ", " << a3 \
    << ", " << a4 << ") failed, err=" << err
#define LOG_RTCERR5_EX(func, a1, a2, a3, a4, a5, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ", " << a3 \
    << ", " << a4 << ", " << a5 << ") failed, err=" << err
#define LOG_RTCERR6_EX(func, a1, a2, a3, a4, a5, a6, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ", " << a3 \
    << ", " << a4 << ", " << a5 << ", " << a6 << ") failed, err=" << err

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCCOMMON_H_
