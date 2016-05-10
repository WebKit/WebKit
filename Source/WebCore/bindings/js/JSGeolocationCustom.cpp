/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSGeolocation.h"

#if ENABLE(GEOLOCATION)

#include "CallbackFunction.h"
#include "DOMWindow.h"
#include "Geolocation.h"
#include "JSDOMWindow.h"
#include "JSDictionary.h"
#include "JSPositionCallback.h"
#include "JSPositionErrorCallback.h"
#include "PositionOptions.h"

using namespace JSC;

namespace WebCore {

// JSDictionary helper functions

static void setEnableHighAccuracy(PositionOptions* options, const bool& enableHighAccuracy)
{
    options->setEnableHighAccuracy(enableHighAccuracy);
}

static void setTimeout(PositionOptions* options, const double& timeout)
{
    // If the value is positive infinity, there's nothing to do.
    if (!(std::isinf(timeout) && (timeout > 0))) {
        // Wrap to int32 and force non-negative to match behavior of window.setTimeout.
        options->setTimeout(std::max<int>(0, timeout));
    }
}

static void setMaximumAge(PositionOptions* options, const double& maximumAge)
{
    if (std::isinf(maximumAge) && (maximumAge > 0)) {
        // If the value is positive infinity, clear maximumAge.
        options->clearMaximumAge();
    } else {
        // Wrap to int32 and force non-negative to match behavior of window.setTimeout.
        options->setMaximumAge(std::max<int>(0, maximumAge));
    }
}


static RefPtr<PositionOptions> createPositionOptions(ExecState* exec, JSValue value)
{
    // Create default options.
    auto options = PositionOptions::create();

    // Argument is optional (hence undefined is allowed), and null is allowed.
    if (value.isUndefinedOrNull()) {
        // Use default options.
        return WTFMove(options);
    }

    // Given the above test, this will always yield an object.
    JSObject* object = value.toObject(exec);
    ASSERT(!exec->hadException());

    // Create the dictionary wrapper from the initializer object.
    JSDictionary dictionary(exec, object);

    if (!dictionary.tryGetProperty("enableHighAccuracy", options.ptr(), setEnableHighAccuracy))
        return nullptr;
    if (!dictionary.tryGetProperty("timeout", options.ptr(), setTimeout))
        return nullptr;
    if (!dictionary.tryGetProperty("maximumAge", options.ptr(), setMaximumAge))
        return nullptr;

    return WTFMove(options);
}

JSValue JSGeolocation::getCurrentPosition(ExecState& state)
{
    // Arguments: PositionCallback, (optional)PositionErrorCallback, (optional)PositionOptions

    auto positionCallback = createFunctionOnlyCallback<JSPositionCallback>(&state, globalObject(), state.argument(0));
    if (state.hadException())
        return jsUndefined();
    ASSERT(positionCallback);

    auto positionErrorCallback = createFunctionOnlyCallback<JSPositionErrorCallback>(&state, globalObject(), state.argument(1), CallbackAllowUndefined | CallbackAllowNull);
    if (state.hadException())
        return jsUndefined();

    auto positionOptions = createPositionOptions(&state, state.argument(2));
    if (state.hadException())
        return jsUndefined();
    ASSERT(positionOptions);

    wrapped().getCurrentPosition(WTFMove(positionCallback), WTFMove(positionErrorCallback), WTFMove(positionOptions));
    return jsUndefined();
}

JSValue JSGeolocation::watchPosition(ExecState& state)
{
    // Arguments: PositionCallback, (optional)PositionErrorCallback, (optional)PositionOptions

    auto positionCallback = createFunctionOnlyCallback<JSPositionCallback>(&state, globalObject(), state.argument(0));
    if (state.hadException())
        return jsUndefined();
    ASSERT(positionCallback);

    auto positionErrorCallback = createFunctionOnlyCallback<JSPositionErrorCallback>(&state, globalObject(), state.argument(1), CallbackAllowUndefined | CallbackAllowNull);
    if (state.hadException())
        return jsUndefined();

    auto positionOptions = createPositionOptions(&state, state.argument(2));
    if (state.hadException())
        return jsUndefined();
    ASSERT(positionOptions);

    int watchID = wrapped().watchPosition(WTFMove(positionCallback), WTFMove(positionErrorCallback), WTFMove(positionOptions));
    return jsNumber(watchID);
}

} // namespace WebCore

#endif // ENABLE(GEOLOCATION)
