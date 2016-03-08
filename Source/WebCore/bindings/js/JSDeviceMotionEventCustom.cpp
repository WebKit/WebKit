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

#if ENABLE(DEVICE_ORIENTATION)

#include "JSDeviceMotionEvent.h"

#include "DeviceMotionData.h"
#include "DeviceMotionEvent.h"
#include <runtime/IdentifierInlines.h>
#include <runtime/JSCJSValueInlines.h>
#include <runtime/ObjectConstructor.h>
#include <runtime/StructureInlines.h>

using namespace JSC;

namespace WebCore {

static RefPtr<DeviceMotionData::Acceleration> readAccelerationArgument(JSValue value, ExecState& state)
{
    if (value.isUndefinedOrNull())
        return nullptr;

    // Given the above test, this will always yield an object.
    JSObject* object = value.toObject(&state);
    ASSERT(!state.hadException());

    JSValue xValue = object->get(&state, Identifier::fromString(&state, "x"));
    if (state.hadException())
        return nullptr;
    bool canProvideX = !xValue.isUndefinedOrNull();
    double x = xValue.toNumber(&state);
    if (state.hadException())
        return nullptr;

    JSValue yValue = object->get(&state, Identifier::fromString(&state, "y"));
    if (state.hadException())
        return nullptr;
    bool canProvideY = !yValue.isUndefinedOrNull();
    double y = yValue.toNumber(&state);
    if (state.hadException())
        return nullptr;

    JSValue zValue = object->get(&state, Identifier::fromString(&state, "z"));
    if (state.hadException())
        return nullptr;
    bool canProvideZ = !zValue.isUndefinedOrNull();
    double z = zValue.toNumber(&state);
    if (state.hadException())
        return nullptr;

    if (!canProvideX && !canProvideY && !canProvideZ)
        return nullptr;

    return DeviceMotionData::Acceleration::create(canProvideX, x, canProvideY, y, canProvideZ, z);
}

static RefPtr<DeviceMotionData::RotationRate> readRotationRateArgument(JSValue value, ExecState& state)
{
    if (value.isUndefinedOrNull())
        return nullptr;

    // Given the above test, this will always yield an object.
    JSObject* object = value.toObject(&state);
    ASSERT(!state.hadException());

    JSValue alphaValue = object->get(&state, Identifier::fromString(&state, "alpha"));
    if (state.hadException())
        return nullptr;
    bool canProvideAlpha = !alphaValue.isUndefinedOrNull();
    double alpha = alphaValue.toNumber(&state);
    if (state.hadException())
        return nullptr;

    JSValue betaValue = object->get(&state, Identifier::fromString(&state, "beta"));
    if (state.hadException())
        return nullptr;
    bool canProvideBeta = !betaValue.isUndefinedOrNull();
    double beta = betaValue.toNumber(&state);
    if (state.hadException())
        return nullptr;

    JSValue gammaValue = object->get(&state, Identifier::fromString(&state, "gamma"));
    if (state.hadException())
        return nullptr;
    bool canProvideGamma = !gammaValue.isUndefinedOrNull();
    double gamma = gammaValue.toNumber(&state);
    if (state.hadException())
        return nullptr;

    if (!canProvideAlpha && !canProvideBeta && !canProvideGamma)
        return nullptr;

    return DeviceMotionData::RotationRate::create(canProvideAlpha, alpha, canProvideBeta, beta, canProvideGamma, gamma);
}

static JSObject* createAccelerationObject(const DeviceMotionData::Acceleration* acceleration, ExecState& state)
{
    JSObject* object = constructEmptyObject(&state);
    object->putDirect(state.vm(), Identifier::fromString(&state, "x"), acceleration->canProvideX() ? jsNumber(acceleration->x()) : jsNull());
    object->putDirect(state.vm(), Identifier::fromString(&state, "y"), acceleration->canProvideY() ? jsNumber(acceleration->y()) : jsNull());
    object->putDirect(state.vm(), Identifier::fromString(&state, "z"), acceleration->canProvideZ() ? jsNumber(acceleration->z()) : jsNull());
    return object;
}

static JSObject* createRotationRateObject(const DeviceMotionData::RotationRate* rotationRate, ExecState& state)
{
    JSObject* object = constructEmptyObject(&state);
    object->putDirect(state.vm(), Identifier::fromString(&state, "alpha"), rotationRate->canProvideAlpha() ? jsNumber(rotationRate->alpha()) : jsNull());
    object->putDirect(state.vm(), Identifier::fromString(&state, "beta"),  rotationRate->canProvideBeta()  ? jsNumber(rotationRate->beta())  : jsNull());
    object->putDirect(state.vm(), Identifier::fromString(&state, "gamma"), rotationRate->canProvideGamma() ? jsNumber(rotationRate->gamma()) : jsNull());
    return object;
}

JSValue JSDeviceMotionEvent::acceleration(ExecState& state) const
{
    DeviceMotionEvent& imp = wrapped();
    if (!imp.deviceMotionData()->acceleration())
        return jsNull();
    return createAccelerationObject(imp.deviceMotionData()->acceleration(), state);
}

JSValue JSDeviceMotionEvent::accelerationIncludingGravity(ExecState& state) const
{
    DeviceMotionEvent& imp = wrapped();
    if (!imp.deviceMotionData()->accelerationIncludingGravity())
        return jsNull();
    return createAccelerationObject(imp.deviceMotionData()->accelerationIncludingGravity(), state);
}

JSValue JSDeviceMotionEvent::rotationRate(ExecState& state) const
{
    DeviceMotionEvent& imp = wrapped();
    if (!imp.deviceMotionData()->rotationRate())
        return jsNull();
    return createRotationRateObject(imp.deviceMotionData()->rotationRate(), state);
}

JSValue JSDeviceMotionEvent::interval(ExecState&) const
{
    DeviceMotionEvent& imp = wrapped();
    if (!imp.deviceMotionData()->canProvideInterval())
        return jsNull();
    return jsNumber(imp.deviceMotionData()->interval());
}

JSValue JSDeviceMotionEvent::initDeviceMotionEvent(ExecState& state)
{
    const String type = state.argument(0).toString(&state)->value(&state);
    bool bubbles = state.argument(1).toBoolean(&state);
    bool cancelable = state.argument(2).toBoolean(&state);

    // If any of the parameters are null or undefined, mark them as not provided.
    // Otherwise, use the standard JavaScript conversion.
    RefPtr<DeviceMotionData::Acceleration> acceleration = readAccelerationArgument(state.argument(3), state);
    if (state.hadException())
        return jsUndefined();

    RefPtr<DeviceMotionData::Acceleration> accelerationIncludingGravity = readAccelerationArgument(state.argument(4), state);
    if (state.hadException())
        return jsUndefined();

    RefPtr<DeviceMotionData::RotationRate> rotationRate = readRotationRateArgument(state.argument(5), state);
    if (state.hadException())
        return jsUndefined();

    bool intervalProvided = !state.argument(6).isUndefinedOrNull();
    double interval = state.argument(6).toNumber(&state);
    auto deviceMotionData = DeviceMotionData::create(WTFMove(acceleration), WTFMove(accelerationIncludingGravity), WTFMove(rotationRate), intervalProvided, interval);
    wrapped().initDeviceMotionEvent(type, bubbles, cancelable, deviceMotionData.ptr());
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(DEVICE_ORIENTATION)
