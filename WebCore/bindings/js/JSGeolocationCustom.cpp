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
#include "JSPositionOptions.h"

using namespace JSC;

namespace WebCore {

JSValuePtr JSGeolocation::getCurrentPosition(ExecState* exec, const ArgList& args)
{
    // Arguments: PositionCallback, (optional)PositionErrorCallback, (optional)PositionOptions
    RefPtr<PositionCallback> positionCallback;
    JSObject* object = args.at(exec, 0)->getObject();
    if (exec->hadException())
        return jsUndefined();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    if (Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame())
        positionCallback = JSCustomPositionCallback::create(object, frame);
    
    RefPtr<PositionErrorCallback> positionErrorCallback;
    if (!args.at(exec, 1)->isUndefinedOrNull()) {
        JSObject* object = args.at(exec, 1)->getObject();
        if (!object) {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return jsUndefined();
        }

        if (Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame())
            positionErrorCallback = JSCustomPositionErrorCallback::create(object, frame);
    }
    
    RefPtr<PositionOptions> positionOptions;
    if (!args.at(exec, 2)->isUndefinedOrNull())
        positionOptions = toPositionOptions(args.at(exec, 2));
    
    m_impl->getCurrentPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions.get());
    
    return jsUndefined();
}

JSValuePtr JSGeolocation::watchPosition(ExecState* exec, const ArgList& args)
{
    // Arguments: PositionCallback, (optional)PositionErrorCallback, (optional)PositionOptions
    RefPtr<PositionCallback> positionCallback;
    JSObject* object = args.at(exec, 0)->getObject();
    if (exec->hadException())
        return jsUndefined();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }
    
    if (Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame())
        positionCallback = JSCustomPositionCallback::create(object, frame);
    
    RefPtr<PositionErrorCallback> positionErrorCallback;
    if (!args.at(exec, 1)->isUndefinedOrNull()) {
        JSObject* object = args.at(exec, 1)->getObject();
        if (!object) {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return jsUndefined();
        }
        
        if (Frame* frame = toJSDOMWindow(exec->lexicalGlobalObject())->impl()->frame())
            positionErrorCallback = JSCustomPositionErrorCallback::create(object, frame);
    }
    
    RefPtr<PositionOptions> positionOptions;
    if (!args.at(exec, 2)->isUndefinedOrNull())
        positionOptions = toPositionOptions(args.at(exec, 2));
    
    int watchID = m_impl->watchPosition(positionCallback.release(), positionErrorCallback.release(), positionOptions.get());
    return jsNumber(exec, watchID);
}

} // namespace WebCore
