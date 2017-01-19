/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_MONITOR_MODULE_H
#define WEBRTC_VOICE_ENGINE_MONITOR_MODULE_H

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

class MonitorObserver
{
public:
    virtual void OnPeriodicProcess() = 0;
protected:
    virtual ~MonitorObserver() {}
};


namespace webrtc {
namespace voe {

class MonitorModule : public Module
{
public:
    int32_t RegisterObserver(MonitorObserver& observer);

    int32_t DeRegisterObserver();

    MonitorModule();

    virtual ~MonitorModule();
public:	// module
 int64_t TimeUntilNextProcess() override;

 void Process() override;

private:
    rtc::CriticalSection _callbackCritSect;
    MonitorObserver* _observerPtr GUARDED_BY(_callbackCritSect);
    int64_t _lastProcessTime;
};

}  // namespace voe

}  // namespace webrtc

#endif // VOICE_ENGINE_MONITOR_MODULE
