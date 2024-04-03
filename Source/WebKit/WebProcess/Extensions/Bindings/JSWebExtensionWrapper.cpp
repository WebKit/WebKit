/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSWebExtensionWrapper.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionWrappable.h"
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSWeakObjectMapRefPrivate.h>

namespace WebKit {

static HashMap<JSGlobalContextRef, JSWeakObjectMapRef>& wrapperCache()
{
    static NeverDestroyed<HashMap<JSGlobalContextRef, JSWeakObjectMapRef>> wrappers;
    return wrappers;
}

static void cacheMapDestroyed(JSWeakObjectMapRef map, void* context)
{
    wrapperCache().remove(static_cast<JSGlobalContextRef>(context));
}

static inline JSWeakObjectMapRef wrapperCacheMap(JSContextRef context)
{
    auto globalContext = JSContextGetGlobalContext(context);
    return wrapperCache().ensure(globalContext, [&] {
        return JSWeakObjectMapCreate(globalContext, globalContext, cacheMapDestroyed);
    }).iterator->value;
}

static inline JSValueRef getCachedWrapper(JSContextRef context, JSWeakObjectMapRef wrappers, JSWebExtensionWrappable* object)
{
    ASSERT(context);
    ASSERT(wrappers);
    ASSERT(object);

    if (auto wrapper = JSWeakObjectMapGet(context, wrappers, object)) {
        // Check if the wrapper is still valid. Objects invalidated through finalize
        // will not get removed from the map automatically.
        if (JSObjectGetPrivate(wrapper))
            return wrapper;

        // Remove from the map, since the object is invalid.
        JSWeakObjectMapRemove(context, wrappers, object);
    }

    return nullptr;
}

JSValueRef JSWebExtensionWrapper::wrap(JSContextRef context, JSWebExtensionWrappable* object)
{
    ASSERT(context);

    if (!object)
        return JSValueMakeNull(context);

    auto wrappers = wrapperCacheMap(context);
    if (auto result = getCachedWrapper(context, wrappers, object))
        return result;

    auto objectClass = object->wrapperClass();
    ASSERT(objectClass);

    auto wrapper = JSObjectMake(context, objectClass, object);
    ASSERT(wrapper);

    JSWeakObjectMapSet(context, wrappers, object, wrapper);

    return wrapper;
}

JSWebExtensionWrappable* JSWebExtensionWrapper::unwrap(JSContextRef context, JSValueRef value)
{
    ASSERT(context);
    ASSERT(value);

    if (!context || !value)
        return nullptr;

    return static_cast<JSWebExtensionWrappable*>(JSObjectGetPrivate(JSValueToObject(context, value, nullptr)));
}

static JSWebExtensionWrappable* unwrapObject(JSObjectRef object)
{
    ASSERT(object);

    auto* wrappable = static_cast<JSWebExtensionWrappable*>(JSObjectGetPrivate(object));
    ASSERT(wrappable);
    return wrappable;
}

void JSWebExtensionWrapper::initialize(JSContextRef, JSObjectRef object)
{
    if (auto* wrappable = unwrapObject(object))
        wrappable->ref();
}

void JSWebExtensionWrapper::finalize(JSObjectRef object)
{
    if (auto* wrappable = unwrapObject(object)) {
        JSObjectSetPrivate(object, nullptr);
        wrappable->deref();
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
