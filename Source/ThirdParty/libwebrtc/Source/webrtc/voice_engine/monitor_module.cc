/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/timeutils.h"
#include "webrtc/voice_engine/monitor_module.h"

namespace webrtc  {

namespace voe  {

MonitorModule::MonitorModule() :
    _observerPtr(NULL),
    _lastProcessTime(rtc::TimeMillis())
{
}

MonitorModule::~MonitorModule()
{
}

int32_t
MonitorModule::RegisterObserver(MonitorObserver& observer)
{
    rtc::CritScope lock(&_callbackCritSect);
    if (_observerPtr)
    {
        return -1;
    }
    _observerPtr = &observer;
    return 0;
}

int32_t
MonitorModule::DeRegisterObserver()
{
    rtc::CritScope lock(&_callbackCritSect);
    if (!_observerPtr)
    {
        return 0;
    }
    _observerPtr = NULL;
    return 0;
}

int64_t
MonitorModule::TimeUntilNextProcess()
{
    int64_t now = rtc::TimeMillis();
    const int64_t kAverageProcessUpdateTimeMs = 1000;
    return kAverageProcessUpdateTimeMs - (now - _lastProcessTime);
}

void
MonitorModule::Process()
{
    _lastProcessTime = rtc::TimeMillis();
    rtc::CritScope lock(&_callbackCritSect);
    if (_observerPtr)
    {
        _observerPtr->OnPeriodicProcess();
    }
}

}  // namespace voe

}  // namespace webrtc
