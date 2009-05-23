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

#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "Geolocation.h"
#include "GeolocationService.h"
#include "JSCustomPositionCallback.h"
#include "JSCustomPositionErrorCallback.h"
#include "JSDOMWindow.h"
#include "PositionOptions.h"

using namespace JSC;

namespace WebCore {

static PassRefPtr<PositionOptions> createPositionOptions(ExecState* exec, JSValue value)
{
    if (!value.isObject())
        return 0;

    JSObject* object = asObject(value);

    JSValue enableHighAccuracyValue = object->get(exec, Identifier(exec, "enableHighAccuracy"));
    if (exec->hadException())
        return 0;
    bool enableHighAccuracy = enableHighAccuracyValue.toBoolean(exec);
    if (exec->hadException())
        return 0;

    JSValue timeoutValue = object->get(exec, Identifier(exec, "timeout"));
    if (exec->hadException())
        return 0;
    unsigned timeout = timeoutValue.toUInt32(exec);
    if (exec->hadException())
        return 0;

    JSValue maximumAgeValue = object->get(exec, Identifier(exec, "maximumAge"));
    if (exec->hadException())
        return 0;
    unsigned maximumAge = maximumAgeValue.toUInt32(exec);
    if (exec->hadException())
        return 0;

    return PositionOptions::create(enableHighAccuracy, timeout, maximumAge);
}

JSValue JSGeolocation::getCurrentPosition(ExecState* exec, const ArgList& args)
{
    // Arguments: PositionCallback, (optional)PositionErrorCallback, (optional)PositionOptions
    RefPtr<PositionCallback> positionCallback;
    JSObject* object = args.at(0).getObject();
    if (exec->hadException())
        return jsUndefined();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    if (Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame())
        positionCallback = JSCustomPositionCallback::create(object, frame);
    
    RefPtr<PositionErrorCallback> positionErrorCallback;
    if (!args.at(1).isUndefinedOrNull()) {
        JSObject* object = args.at(1).getObject();
        if (!object) {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return jsUndefined();
        }

        if (Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame())
            positionErrorCallback = JSCustomPositionErrorCallback::create(object, frame);
    }
    
    RefPtr<PositionOptions> positionOptions;
    if (!args.at(2).isUndefinedOrNull()) {
        positionOptions = createPositionOptions(exec, args.at(2));
        if (exec->hadException())
            return jsUndefined();
    }

    m_impl->getCurrentPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions.release());
    
    return jsUndefined();
}

JSValue JSGeolocation::watchPosition(ExecState* exec, const ArgList& args)
{
    // Arguments: PositionCallback, (optional)PositionErrorCallback, (optional)PositionOptions
    RefPtr<PositionCallback> positionCallback;
    JSObject* object = args.at(0).getObject();
    if (exec->hadException())
        return jsUndefined();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }
    
    if (Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame())
        positionCallback = JSCustomPositionCallback::create(object, frame);
    
    RefPtr<PositionErrorCallback> positionErrorCallback;
    if (!args.at(1).isUndefinedOrNull()) {
        JSObject* object = args.at(1).getObject();
        if (!object) {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return jsUndefined();
        }
        
        if (Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame())
            positionErrorCallback = JSCustomPositionErrorCallback::create(object, frame);
    }
    
    RefPtr<PositionOptions> positionOptions;
    if (!args.at(2).isUndefinedOrNull()) {
        positionOptions = createPositionOptions(exec, args.at(2));
        if (exec->hadException())
            return jsUndefined();
    }

    int watchID = m_impl->watchPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions.release());
    return jsNumber(exec, watchID);
}

} // namespace WebCore
