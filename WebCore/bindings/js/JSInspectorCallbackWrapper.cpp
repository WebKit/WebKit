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
#include "JSInspectorCallbackWrapper.h"

#if ENABLE(INSPECTOR)

#include "JSInspectedObjectWrapper.h"
#include <wtf/StdLibExtras.h>

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSInspectorCallbackWrapper);

typedef HashMap<JSObject*, JSInspectorCallbackWrapper*> WrapperMap;

static WrapperMap& wrappers()
{
    DEFINE_STATIC_LOCAL(WrapperMap, map, ());
    return map;
}

const ClassInfo JSInspectorCallbackWrapper::s_info = { "JSInspectorCallbackWrapper", &JSQuarantinedObjectWrapper::s_info, 0, 0 };

static Structure* leakInspectorCallbackWrapperStructure()
{
    Structure::startIgnoringLeaks();
    Structure* structure = JSInspectorCallbackWrapper::createStructure(jsNull()).releaseRef();
    Structure::stopIgnoringLeaks();
    return structure;
}

JSValue JSInspectorCallbackWrapper::wrap(ExecState* unwrappedExec, JSValue unwrappedValue)
{
    if (!unwrappedValue.isObject())
        return unwrappedValue;

    JSObject* unwrappedObject = asObject(unwrappedValue);

    if (unwrappedObject->inherits(&JSInspectorCallbackWrapper::s_info))
        return unwrappedObject;

    if (JSInspectorCallbackWrapper* wrapper = wrappers().get(unwrappedObject))
        return wrapper;

    JSValue prototype = unwrappedObject->prototype();
    ASSERT(prototype.isNull() || prototype.isObject());

    if (prototype.isNull()) {
        static Structure* structure = leakInspectorCallbackWrapperStructure();
        return new (unwrappedExec) JSInspectorCallbackWrapper(unwrappedExec, unwrappedObject, structure);
    }
    return new (unwrappedExec) JSInspectorCallbackWrapper(unwrappedExec, unwrappedObject, createStructure(wrap(unwrappedExec, prototype)));
}

JSInspectorCallbackWrapper::JSInspectorCallbackWrapper(ExecState* unwrappedExec, JSObject* unwrappedObject, PassRefPtr<Structure> structure)
    : JSQuarantinedObjectWrapper(unwrappedExec, unwrappedObject, structure)
{
    ASSERT(!wrappers().contains(unwrappedObject));
    wrappers().set(unwrappedObject, this);
}

JSInspectorCallbackWrapper::~JSInspectorCallbackWrapper()
{
    wrappers().remove(unwrappedObject());
}

JSValue JSInspectorCallbackWrapper::prepareIncomingValue(ExecState* unwrappedExec, JSValue unwrappedValue) const
{
    if (JSQuarantinedObjectWrapper* wrapper = asWrapper(unwrappedValue)) {
        // The only time a wrapper should be passed into a JSInspectorCallbackWrapper is when a client-side storage callback
        // is called. (The client-side storage API calls the callback with the `this` object set to the callback itself.)
        ASSERT_WITH_MESSAGE(wrapper == this, "A different wrapper was passed into a JSInspectorCallbackWrapper");
        if (wrapper != this)
            return jsUndefined();

        return wrapper->unwrappedObject();
    }

    // Any value being passed to the Inspector from the inspected page should be wrapped in a JSInspectedObjectWrapper.
    return JSInspectedObjectWrapper::wrap(unwrappedExec, unwrappedValue);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
