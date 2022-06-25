/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GCController.h"

#include "JSBasics.h"

// Static Functions

static JSValueRef collectCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto& controller = *static_cast<GCController*>(JSObjectGetPrivate(thisObject));
    controller.collect();
    return JSValueMakeUndefined(context);
}

static JSValueRef collectOnAlternateThreadCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto& controller = *static_cast<GCController*>(JSObjectGetPrivate(thisObject));
    bool waitUntilDone = argumentCount > 0 && JSValueToBoolean(context, arguments[0]);
    controller.collectOnAlternateThread(waitUntilDone);
    return JSValueMakeUndefined(context);
}

static JSValueRef getJSObjectCountCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto& controller = *static_cast<GCController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, controller.getJSObjectCount());
}

// Object Creation

void GCController::makeWindowObject(JSContextRef context)
{
    WTR::setGlobalObjectProperty(context, "GCController", JSObjectMake(context, createJSClass().get(), this));
}

JSRetainPtr<JSClassRef> GCController::createJSClass()
{
    static constexpr JSStaticFunction functions[] = {
        { "collect", collectCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "collectOnAlternateThread", collectOnAlternateThreadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "getJSObjectCount", getJSObjectCountCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };
    static const JSClassDefinition definition = {
        0, kJSClassAttributeNone, "GCController", 0, 0, functions,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    return adopt(JSClassCreate(&definition));
}
