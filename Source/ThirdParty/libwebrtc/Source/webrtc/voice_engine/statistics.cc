/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdio.h>

#include "webrtc/voice_engine/statistics.h"

#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {

namespace voe {

Statistics::Statistics(uint32_t instanceId) :
    _instanceId(instanceId),
    _lastError(0),
    _isInitialized(false)
{
}

Statistics::~Statistics()
{
}

int32_t Statistics::SetInitialized()
{
    _isInitialized = true;
    return 0;
}

int32_t Statistics::SetUnInitialized()
{
    _isInitialized = false;
    return 0;
}

bool Statistics::Initialized() const
{
    return _isInitialized;
}

int32_t Statistics::SetLastError(int32_t error) const
{
    rtc::CritScope cs(&lock_);
    _lastError = error;
    return 0;
}

int32_t Statistics::SetLastError(int32_t error,
                                 TraceLevel level) const
{
    WEBRTC_TRACE(level, kTraceVoice, VoEId(_instanceId,-1),
                 "error code is set to %d",
                 error);
    rtc::CritScope cs(&lock_);
    _lastError = error;
    return 0;
}

int32_t Statistics::SetLastError(
    int32_t error,
    TraceLevel level, const char* msg) const
{
    char traceMessage[KTraceMaxMessageSize];
    assert(strlen(msg) < KTraceMaxMessageSize);
    sprintf(traceMessage, "%s (error=%d)", msg, error);

    WEBRTC_TRACE(level, kTraceVoice, VoEId(_instanceId,-1), "%s",
                 traceMessage);

    rtc::CritScope cs(&lock_);
    _lastError = error;
    return 0;
}

int32_t Statistics::LastError() const
{
    int32_t ret;
    {
        rtc::CritScope cs(&lock_);
        ret = _lastError;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "LastError() => %d", ret);
    return ret;
}

}  // namespace voe

}  // namespace webrtc
