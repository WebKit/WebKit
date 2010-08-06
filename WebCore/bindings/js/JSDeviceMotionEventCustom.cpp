/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSDeviceMotionEvent.h"

#if ENABLE(DEVICE_ORIENTATION)

#include "DeviceMotionData.h"
#include "DeviceMotionEvent.h"

using namespace JSC;

namespace WebCore {

JSValue JSDeviceMotionEvent::xAcceleration(ExecState* exec) const
{
    DeviceMotionEvent* imp = static_cast<DeviceMotionEvent*>(impl());
    if (!imp->deviceMotionData()->canProvideXAcceleration())
        return jsNull();
    return jsNumber(exec, imp->deviceMotionData()->xAcceleration());
}

JSValue JSDeviceMotionEvent::yAcceleration(ExecState* exec) const
{
    DeviceMotionEvent* imp = static_cast<DeviceMotionEvent*>(impl());
    if (!imp->deviceMotionData()->canProvideYAcceleration())
        return jsNull();
    return jsNumber(exec, imp->deviceMotionData()->yAcceleration());
}

JSValue JSDeviceMotionEvent::zAcceleration(ExecState* exec) const
{
    DeviceMotionEvent* imp = static_cast<DeviceMotionEvent*>(impl());
    if (!imp->deviceMotionData()->canProvideZAcceleration())
        return jsNull();
    return jsNumber(exec, imp->deviceMotionData()->zAcceleration());
}

JSValue JSDeviceMotionEvent::xRotationRate(ExecState* exec) const
{
    DeviceMotionEvent* imp = static_cast<DeviceMotionEvent*>(impl());
    if (!imp->deviceMotionData()->canProvideXRotationRate())
        return jsNull();
    return jsNumber(exec, imp->deviceMotionData()->xRotationRate());
}

JSValue JSDeviceMotionEvent::yRotationRate(ExecState* exec) const
{
    DeviceMotionEvent* imp = static_cast<DeviceMotionEvent*>(impl());
    if (!imp->deviceMotionData()->canProvideYRotationRate())
        return jsNull();
    return jsNumber(exec, imp->deviceMotionData()->yRotationRate());
}

JSValue JSDeviceMotionEvent::zRotationRate(ExecState* exec) const
{
    DeviceMotionEvent* imp = static_cast<DeviceMotionEvent*>(impl());
    if (!imp->deviceMotionData()->canProvideZRotationRate())
        return jsNull();
    return jsNumber(exec, imp->deviceMotionData()->zRotationRate());
}
    
JSValue JSDeviceMotionEvent::interval(ExecState* exec) const
{
    DeviceMotionEvent* imp = static_cast<DeviceMotionEvent*>(impl());
    if (!imp->deviceMotionData()->canProvideInterval())
        return jsNull();
    return jsNumber(exec, imp->deviceMotionData()->interval());
}
    
JSValue JSDeviceMotionEvent::initDeviceMotionEvent(ExecState* exec)
{
    const String& type = ustringToString(exec->argument(0).toString(exec));
    bool bubbles = exec->argument(1).toBoolean(exec);
    bool cancelable = exec->argument(2).toBoolean(exec);
    // If any of the parameters are null or undefined, mark them as not provided.
    // Otherwise, use the standard JavaScript conversion.
    bool xAccelerationProvided = !exec->argument(3).isUndefinedOrNull();
    double xAcceleration = exec->argument(3).toNumber(exec);
    bool yAccelerationProvided = !exec->argument(4).isUndefinedOrNull();
    double yAcceleration = exec->argument(4).toNumber(exec);
    bool zAccelerationProvided = !exec->argument(5).isUndefinedOrNull();
    double zAcceleration = exec->argument(5).toNumber(exec);
    bool xRotationRateProvided = !exec->argument(6).isUndefinedOrNull();
    double xRotationRate = exec->argument(6).toNumber(exec);
    bool yRotationRateProvided = !exec->argument(7).isUndefinedOrNull();
    double yRotationRate = exec->argument(7).toNumber(exec);
    bool zRotationRateProvided = !exec->argument(8).isUndefinedOrNull();
    double zRotationRate = exec->argument(8).toNumber(exec);
    bool intervalProvided = !exec->argument(9).isUndefinedOrNull();
    double interval = exec->argument(9).toNumber(exec);
    RefPtr<DeviceMotionData> deviceMotionData = DeviceMotionData::create(xAccelerationProvided, xAcceleration,
                                                                         yAccelerationProvided, yAcceleration,
                                                                         zAccelerationProvided, zAcceleration,
                                                                         xRotationRateProvided, xRotationRate,
                                                                         yRotationRateProvided, yRotationRate,
                                                                         zRotationRateProvided, zRotationRate,
                                                                         intervalProvided, interval);
    DeviceMotionEvent* imp = static_cast<DeviceMotionEvent*>(impl());
    imp->initDeviceMotionEvent(type, bubbles, cancelable, deviceMotionData.get());
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(DEVICE_ORIENTATION)
