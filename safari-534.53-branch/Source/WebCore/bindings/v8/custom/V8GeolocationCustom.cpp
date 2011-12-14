/*
 * Copyright 2009, The Android Open Source Project
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
#include "V8Geolocation.h"

#if ENABLE(GEOLOCATION)

#include "Frame.h"
#include "Geolocation.h"
#include "V8Binding.h"
#include "V8CustomPositionCallback.h"
#include "V8CustomPositionErrorCallback.h"
#include "V8Utilities.h"

using namespace std;
using namespace WTF;

namespace WebCore {

static PassRefPtr<PositionOptions> createPositionOptions(v8::Local<v8::Value> value, bool& succeeded)
{
    succeeded = true;

    // Create default options.
    RefPtr<PositionOptions> options = PositionOptions::create();

    // Argument is optional (hence undefined is allowed), and null is allowed.
    if (isUndefinedOrNull(value)) {
        // Use default options.
        return options.release();
    }

    // Given the above test, this will always yield an object.
    v8::Local<v8::Object> object = value->ToObject();

    // For all three properties, we apply the following ...
    // - If the getter or the property's valueOf method throws an exception, we
    //   quit so as not to risk overwriting the exception.
    // - If the value is absent or undefined, we don't override the default.
    v8::Local<v8::Value> enableHighAccuracyValue = object->Get(v8::String::New("enableHighAccuracy"));
    if (enableHighAccuracyValue.IsEmpty()) {
        succeeded = false;
        return 0;
    }
    if (!enableHighAccuracyValue->IsUndefined()) {
        v8::Local<v8::Boolean> enableHighAccuracyBoolean = enableHighAccuracyValue->ToBoolean();
        if (enableHighAccuracyBoolean.IsEmpty()) {
            succeeded = false;
            return 0;
        }
        options->setEnableHighAccuracy(enableHighAccuracyBoolean->Value());
    }

    v8::Local<v8::Value> timeoutValue = object->Get(v8::String::New("timeout"));
    if (timeoutValue.IsEmpty()) {
        succeeded = false;
        return 0;
    }
    if (!timeoutValue->IsUndefined()) {
        v8::Local<v8::Number> timeoutNumber = timeoutValue->ToNumber();
        if (timeoutNumber.IsEmpty()) {
            succeeded = false;
            return 0;
        }
        double timeoutDouble = timeoutNumber->Value();
        // If the value is positive infinity, there's nothing to do.
        if (!(isinf(timeoutDouble) && timeoutDouble > 0)) {
            v8::Local<v8::Int32> timeoutInt32 = timeoutValue->ToInt32();
            if (timeoutInt32.IsEmpty()) {
                succeeded = false;
                return 0;
            }
            // Wrap to int32 and force non-negative to match behavior of window.setTimeout.
            options->setTimeout(max(0, timeoutInt32->Value()));
        }
    }

    v8::Local<v8::Value> maximumAgeValue = object->Get(v8::String::New("maximumAge"));
    if (maximumAgeValue.IsEmpty()) {
        succeeded = false;
        return 0;
    }
    if (!maximumAgeValue->IsUndefined()) {
        v8::Local<v8::Number> maximumAgeNumber = maximumAgeValue->ToNumber();
        if (maximumAgeNumber.IsEmpty()) {
            succeeded = false;
            return 0;
        }
        double maximumAgeDouble = maximumAgeNumber->Value();
        if (isinf(maximumAgeDouble) && maximumAgeDouble > 0) {
            // If the value is positive infinity, clear maximumAge.
            options->clearMaximumAge();
        } else {
            v8::Local<v8::Int32> maximumAgeInt32 = maximumAgeValue->ToInt32();
            if (maximumAgeInt32.IsEmpty()) {
                succeeded = false;
                return 0;
            }
            // Wrap to int32 and force non-negative to match behavior of window.setTimeout.
            options->setMaximumAge(max(0, maximumAgeInt32->Value()));
        }
    }

    return options.release();
}

v8::Handle<v8::Value> V8Geolocation::getCurrentPositionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Geolocation.getCurrentPosition()");

    bool succeeded = false;

    RefPtr<PositionCallback> positionCallback = createFunctionOnlyCallback<V8CustomPositionCallback>(args[0], succeeded);
    if (!succeeded)
        return v8::Undefined();
    ASSERT(positionCallback);

    // Argument is optional (hence undefined is allowed), and null is allowed.
    RefPtr<PositionErrorCallback> positionErrorCallback = createFunctionOnlyCallback<V8CustomPositionErrorCallback>(args[1], succeeded, CallbackAllowUndefined | CallbackAllowNull);
    if (!succeeded)
        return v8::Undefined();

    RefPtr<PositionOptions> positionOptions = createPositionOptions(args[2], succeeded);
    if (!succeeded)
        return v8::Undefined();
    ASSERT(positionOptions);

    Geolocation* geolocation = V8Geolocation::toNative(args.Holder());
    geolocation->getCurrentPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions.release());
    return v8::Undefined();
}

v8::Handle<v8::Value> V8Geolocation::watchPositionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Geolocation.watchPosition()");

    bool succeeded = false;

    RefPtr<PositionCallback> positionCallback = createFunctionOnlyCallback<V8CustomPositionCallback>(args[0], succeeded);
    if (!succeeded)
        return v8::Undefined();
    ASSERT(positionCallback);

    // Argument is optional (hence undefined is allowed), and null is allowed.
    RefPtr<PositionErrorCallback> positionErrorCallback = createFunctionOnlyCallback<V8CustomPositionErrorCallback>(args[1], succeeded, CallbackAllowUndefined | CallbackAllowNull);
    if (!succeeded)
        return v8::Undefined();

    RefPtr<PositionOptions> positionOptions = createPositionOptions(args[2], succeeded);
    if (!succeeded)
        return v8::Undefined();
    ASSERT(positionOptions);

    Geolocation* geolocation = V8Geolocation::toNative(args.Holder());
    int watchId = geolocation->watchPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions.release());
    return v8::Number::New(watchId);
}

} // namespace WebCore

#endif // ENABLE(GEOLOCATION)
