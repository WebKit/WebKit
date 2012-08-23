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
#include "V8Coordinates.h"

#include "Coordinates.h"
#include "V8Binding.h"

namespace WebCore {

v8::Handle<v8::Value> V8Coordinates::altitudeAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Coordinates.altitude._get");
    v8::Handle<v8::Object> holder = info.Holder();
    Coordinates* imp = V8Coordinates::toNative(holder);
    if (!imp->canProvideAltitude())
        return v8::Null(info.GetIsolate());
    return v8::Number::New(imp->altitude());
}

v8::Handle<v8::Value> V8Coordinates::altitudeAccuracyAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Coordinates.altitudeAccuracy._get");
    v8::Handle<v8::Object> holder = info.Holder();
    Coordinates* imp = V8Coordinates::toNative(holder);
    if (!imp->canProvideAltitudeAccuracy())
        return v8::Null(info.GetIsolate());
    return v8::Number::New(imp->altitudeAccuracy());
}

v8::Handle<v8::Value> V8Coordinates::headingAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Coordinates.heading._get");
    v8::Handle<v8::Object> holder = info.Holder();
    Coordinates* imp = V8Coordinates::toNative(holder);
    if (!imp->canProvideHeading())
        return v8::Null(info.GetIsolate());
    return v8::Number::New(imp->heading());
}

v8::Handle<v8::Value> V8Coordinates::speedAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Coordinates.speed._get");
    v8::Handle<v8::Object> holder = info.Holder();
    Coordinates* imp = V8Coordinates::toNative(holder);
    if (!imp->canProvideSpeed())
        return v8::Null(info.GetIsolate());
    return v8::Number::New(imp->speed());
}

} // namespace WebCore
