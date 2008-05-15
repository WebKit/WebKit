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
#include "JavaScriptProfile.h"

#include "JavaScriptProfileNode.h"
#include <profiler/Profile.h>
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <kjs/object.h>
#include <kjs/value.h>

using namespace KJS;

namespace WebCore {

// Cache

typedef HashMap<Profile*, JSValue*> ProfileMap;

static ProfileMap& profileCache()
{ 
    static ProfileMap staticProfiles;
    return staticProfiles;
}

// Static Values

static JSClassRef profileClass();

static JSValueRef getTitleCallback(JSContextRef ctx, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    if (!JSValueIsObjectOfClass(ctx, thisObject, profileClass()))
        return JSValueMakeUndefined(ctx);

    Profile* profile = static_cast<Profile*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeString(ctx, toRef(profile->title().rep()));
}

static JSValueRef getHeadCallback(JSContextRef ctx, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    if (!JSValueIsObjectOfClass(ctx, thisObject, profileClass()))
        return JSValueMakeUndefined(ctx);

    Profile* profile = static_cast<Profile*>(JSObjectGetPrivate(thisObject));
    return toRef(toJS(toJS(ctx), profile->callTree()));
}

static void finalize(JSObjectRef object)
{
    Profile* profile = static_cast<Profile*>(JSObjectGetPrivate(object));
    profileCache().remove(profile);
    profile->deref();
}

JSClassRef profileClass()
{
    static JSStaticValue staticValues[] = {
        { "title", getTitleCallback, 0, kJSPropertyAttributeNone },
        { "head", getHeadCallback, 0, kJSPropertyAttributeNone },
        { 0, 0, 0, 0 }
    };

    static JSClassDefinition classDefinition = {
        0, kJSClassAttributeNone, "Profile", 0, staticValues, 0,
        0, finalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static JSClassRef profileClass = JSClassCreate(&classDefinition);
    return profileClass;
}

JSValue* toJS(ExecState* exec, Profile* profile)
{
    if (!profile)
        return jsNull();

    JSValue* profileWrapper = profileCache().get(profile);
    if (profileWrapper)
        return profileWrapper;

    profile->ref();
    profileWrapper = toJS(JSObjectMake(toRef(exec), profileClass(), static_cast<void*>(profile)));
    profileCache().set(profile, profileWrapper);
    return profileWrapper;
}


} // namespace WebCore
