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

#include <v8.h>

namespace WebCore {

namespace {

static v8::Handle<v8::Value> createAccelerationObject(const DeviceMotionData::Acceleration* acceleration, v8::Isolate* isolate)
{
    v8::Local<v8::Object> object = v8::Object::New();
    object->Set(v8::String::NewSymbol("x"), acceleration->canProvideX() ? v8::Number::New(acceleration->x()) : v8Null(isolate));
    object->Set(v8::String::NewSymbol("y"), acceleration->canProvideY() ? v8::Number::New(acceleration->y()) : v8Null(isolate));
    object->Set(v8::String::NewSymbol("z"), acceleration->canProvideZ() ? v8::Number::New(acceleration->z()) : v8Null(isolate));
    return object;
}

static v8::Handle<v8::Value> createRotationRateObject(const DeviceMotionData::RotationRate* rotationRate, v8::Isolate* isolate)
{
    v8::Local<v8::Object> object = v8::Object::New();
    object->Set(v8::String::NewSymbol("alpha"), rotationRate->canProvideAlpha() ? v8::Number::New(rotationRate->alpha()) : v8Null(isolate));
    object->Set(v8::String::NewSymbol("beta"),  rotationRate->canProvideBeta()  ? v8::Number::New(rotationRate->beta())  : v8Null(isolate));
    object->Set(v8::String::NewSymbol("gamma"), rotationRate->canProvideGamma() ? v8::Number::New(rotationRate->gamma()) : v8Null(isolate));
    return object;
}

RefPtr<DeviceMotionData::Acceleration> readAccelerationArgument(v8::Local<v8::Value> value)
{
    if (isUndefinedOrNull(value))
        return 0;

    // Given the test above, this will always yield an object.
    v8::Local<v8::Object> object = value->ToObject();

    v8::Local<v8::Value> xValue = object->Get(v8::String::NewSymbol("x"));
    if (xValue.IsEmpty())
        return 0;
    bool canProvideX = !isUndefinedOrNull(xValue);
    double x = xValue->NumberValue();

    v8::Local<v8::Value> yValue = object->Get(v8::String::NewSymbol("y"));
    if (yValue.IsEmpty())
        return 0;
    bool canProvideY = !isUndefinedOrNull(yValue);
    double y = yValue->NumberValue();

    v8::Local<v8::Value> zValue = object->Get(v8::String::NewSymbol("z"));
    if (zValue.IsEmpty())
        return 0;
    bool canProvideZ = !isUndefinedOrNull(zValue);
    double z = zValue->NumberValue();

    if (!canProvideX && !canProvideY && !canProvideZ)
        return 0;

    return DeviceMotionData::Acceleration::create(canProvideX, x, canProvideY, y, canProvideZ, z);
}

RefPtr<DeviceMotionData::RotationRate> readRotationRateArgument(v8::Local<v8::Value> value)
{
    if (isUndefinedOrNull(value))
        return 0;

    // Given the test above, this will always yield an object.
    v8::Local<v8::Object> object = value->ToObject();

    v8::Local<v8::Value> alphaValue = object->Get(v8::String::NewSymbol("alpha"));
    if (alphaValue.IsEmpty())
        return 0;
    bool canProvideAlpha = !isUndefinedOrNull(alphaValue);
    double alpha = alphaValue->NumberValue();

    v8::Local<v8::Value> betaValue = object->Get(v8::String::NewSymbol("beta"));
    if (betaValue.IsEmpty())
        return 0;
    bool canProvideBeta = !isUndefinedOrNull(betaValue);
    double beta = betaValue->NumberValue();

    v8::Local<v8::Value> gammaValue = object->Get(v8::String::NewSymbol("gamma"));
    if (gammaValue.IsEmpty())
        return 0;
    bool canProvideGamma = !isUndefinedOrNull(gammaValue);
    double gamma = gammaValue->NumberValue();

    if (!canProvideAlpha && !canProvideBeta && !canProvideGamma)
        return 0;

    return DeviceMotionData::RotationRate::create(canProvideAlpha, alpha, canProvideBeta, beta, canProvideGamma, gamma);
}

} // namespace

v8::Handle<v8::Value> V8DeviceMotionEvent::accelerationAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->acceleration())
        return v8Null(info.GetIsolate());
    return createAccelerationObject(imp->deviceMotionData()->acceleration(), info.GetIsolate());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::accelerationIncludingGravityAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->accelerationIncludingGravity())
        return v8Null(info.GetIsolate());
    return createAccelerationObject(imp->deviceMotionData()->accelerationIncludingGravity(), info.GetIsolate());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::rotationRateAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->rotationRate())
        return v8Null(info.GetIsolate());
    return createRotationRateObject(imp->deviceMotionData()->rotationRate(), info.GetIsolate());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::intervalAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    v8::Handle<v8::Object> holder = info.Holder();
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(holder);
    if (!imp->deviceMotionData()->canProvideInterval())
        return v8Null(info.GetIsolate());
    return v8::Number::New(imp->deviceMotionData()->interval());
}

v8::Handle<v8::Value> V8DeviceMotionEvent::initDeviceMotionEventMethodCustom(const v8::Arguments& args)
{
    DeviceMotionEvent* imp = V8DeviceMotionEvent::toNative(args.Holder());
    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<>, type, args[0]);
    bool bubbles = args[1]->BooleanValue();
    bool cancelable = args[2]->BooleanValue();
    RefPtr<DeviceMotionData::Acceleration> acceleration = readAccelerationArgument(args[3]);
    RefPtr<DeviceMotionData::Acceleration> accelerationIncludingGravity = readAccelerationArgument(args[4]);
    RefPtr<DeviceMotionData::RotationRate> rotationRate = readRotationRateArgument(args[5]);
    bool intervalProvided = !isUndefinedOrNull(args[6]);
    double interval = args[6]->NumberValue();
    RefPtr<DeviceMotionData> deviceMotionData = DeviceMotionData::create(acceleration, accelerationIncludingGravity, rotationRate, intervalProvided, interval);
    imp->initDeviceMotionEvent(type, bubbles, cancelable, deviceMotionData.get());
    return v8Undefined();
}

} // namespace WebCore

#endif // ENABLE(DEVICE_ORIENTATION)
