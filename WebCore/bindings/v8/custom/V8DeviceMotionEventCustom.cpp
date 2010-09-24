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
#include "V8DeviceMotionEvent.h"

#if ENABLE(DEVICE_ORIENTATION)

#include "DeviceMotionData.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"
#include "V8Proxy.h"

#include <v8.h>

namespace WebCore {

v8::Handle<v8::Value> V8DeviceMotionEvent::xAccelerationAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DeviceMotionEvent.xAcceleration._get");
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->canProvideXAcceleration())
        return v8::Null();
    return v8::Number::New(imp->deviceMotionData()->xAcceleration());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::yAccelerationAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DeviceMotionEvent.yAcceleration._get");
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->canProvideYAcceleration())
        return v8::Null();
    return v8::Number::New(imp->deviceMotionData()->yAcceleration());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::zAccelerationAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DeviceMotionEvent.zAcceleration._get");
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->canProvideZAcceleration())
        return v8::Null();
    return v8::Number::New(imp->deviceMotionData()->zAcceleration());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::xRotationRateAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DeviceMotionEvent.xRotationRate._get");
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->canProvideXRotationRate())
        return v8::Null();
    return v8::Number::New(imp->deviceMotionData()->xRotationRate());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::yRotationRateAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DeviceMotionEvent.yRotationRate._get");
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->canProvideYRotationRate())
        return v8::Null();
    return v8::Number::New(imp->deviceMotionData()->yRotationRate());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::zRotationRateAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DeviceMotionEvent.zRotationRate._get");
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->canProvideZRotationRate())
        return v8::Null();
    return v8::Number::New(imp->deviceMotionData()->zRotationRate());
}


v8::Handle<v8::Value> V8DeviceMotionEvent::intervalAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.DeviceMotionEvent.interval._get");
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->canProvideInterval())
        return v8::Null();
    return v8::Number::New(imp->deviceMotionData()->interval());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::initDeviceMotionEventCallback(const v8::Arguments& args)
{
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(args.Holder());
    STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<>, type, args[0]);
    bool bubbles = args[1]->BooleanValue();
    bool cancelable = args[2]->BooleanValue();
    // If any of the parameters are null or undefined, mark them as not provided.
    // Otherwise, use the standard JavaScript conversion.
    bool xAccelerationProvided = !isUndefinedOrNull(args[3]);
    double xAcceleration = static_cast<double>(args[3]->NumberValue());
    bool yAccelerationProvided = !isUndefinedOrNull(args[4]);
    double yAcceleration = static_cast<double>(args[4]->NumberValue());
    bool zAccelerationProvided = !isUndefinedOrNull(args[5]);
    double zAcceleration = static_cast<double>(args[5]->NumberValue());
    bool xRotationRateProvided = !isUndefinedOrNull(args[6]);
    double xRotationRate = static_cast<double>(args[6]->NumberValue());
    bool yRotationRateProvided = !isUndefinedOrNull(args[7]);
    double yRotationRate = static_cast<double>(args[7]->NumberValue());
    bool zRotationRateProvided = !isUndefinedOrNull(args[8]);
    double zRotationRate = static_cast<double>(args[8]->NumberValue());
    bool intervalProvided = !isUndefinedOrNull(args[9]);
    double interval = static_cast<double>(args[9]->NumberValue());
    RefPtr<DeviceMotionData> deviceMotionData = DeviceMotionData::create(xAccelerationProvided, xAcceleration, yAccelerationProvided, yAcceleration, zAccelerationProvided, zAcceleration, xRotationRateProvided, xRotationRate, yRotationRateProvided, yRotationRate, zRotationRateProvided, zRotationRate, intervalProvided, interval);
    imp->initDeviceMotionEvent(type, bubbles, cancelable, deviceMotionData.get());
    return v8::Handle<v8::Value>();
}

} // namespace WebCore

#endif // ENABLE(DEVICE_ORIENTATION)
