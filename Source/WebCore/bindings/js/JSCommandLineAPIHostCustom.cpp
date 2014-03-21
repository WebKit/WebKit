/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INSPECTOR)

#include "JSCommandLineAPIHost.h"

#include "CommandLineAPIHost.h"
#include "InspectorDOMAgent.h"
#include "JSEventListener.h"
#include "JSNode.h"
#include "JSStorage.h"
#include "Storage.h"
#include <bindings/ScriptValue.h>
#include <inspector/InspectorValues.h>
#include <parser/SourceCode.h>
#include <runtime/IdentifierInlines.h>
#include <runtime/JSArray.h>
#include <runtime/JSFunction.h>
#include <runtime/JSLock.h>
#include <runtime/ObjectConstructor.h>

#if ENABLE(SQL_DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif

using namespace JSC;

namespace WebCore {

JSValue JSCommandLineAPIHost::inspectedObject(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    CommandLineAPIHost::InspectableObject* object = impl().inspectedObject(exec->uncheckedArgument(0).toInt32(exec));
    if (!object)
        return jsUndefined();

    JSLockHolder lock(exec);
    Deprecated::ScriptValue scriptValue = object->get(exec);
    if (scriptValue.hasNoValue())
        return jsUndefined();

    return scriptValue.jsValue();
}

static JSArray* getJSListenerFunctions(ExecState* exec, Document* document, const EventListenerInfo& listenerInfo)
{
    JSArray* result = constructEmptyArray(exec, nullptr);
    size_t handlersCount = listenerInfo.eventListenerVector.size();
    for (size_t i = 0, outputIndex = 0; i < handlersCount; ++i) {
        const JSEventListener* jsListener = JSEventListener::cast(listenerInfo.eventListenerVector[i].listener.get());
        if (!jsListener) {
            ASSERT_NOT_REACHED();
            continue;
        }

        // Hide listeners from other contexts.
        if (&jsListener->isolatedWorld() != &currentWorld(exec))
            continue;

        JSObject* function = jsListener->jsFunction(document);
        if (!function)
            continue;

        JSObject* listenerEntry = constructEmptyObject(exec);
        listenerEntry->putDirect(exec->vm(), Identifier(exec, "listener"), function);
        listenerEntry->putDirect(exec->vm(), Identifier(exec, "useCapture"), jsBoolean(listenerInfo.eventListenerVector[i].useCapture));
        result->putDirectIndex(exec, outputIndex++, JSValue(listenerEntry));
    }
    return result;
}

JSValue JSCommandLineAPIHost::getEventListeners(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    if (!value.isObject() || value.isNull())
        return jsUndefined();

    Node* node = toNode(value);
    if (!node)
        return jsUndefined();

    Vector<EventListenerInfo> listenersArray;
    impl().getEventListenersImpl(node, listenersArray);

    JSObject* result = constructEmptyObject(exec);
    for (size_t i = 0; i < listenersArray.size(); ++i) {
        JSArray* listeners = getJSListenerFunctions(exec, &node->document(), listenersArray[i]);
        if (!listeners->length())
            continue;
        AtomicString eventType = listenersArray[i].eventType;
        result->putDirect(exec->vm(), Identifier(exec, eventType.impl()), JSValue(listeners));
    }

    return result;
}

JSValue JSCommandLineAPIHost::inspect(ExecState* exec)
{
    if (exec->argumentCount() >= 2) {
        Deprecated::ScriptValue object(exec->vm(), exec->uncheckedArgument(0));
        Deprecated::ScriptValue hints(exec->vm(), exec->uncheckedArgument(1));
        impl().inspectImpl(object.toInspectorValue(exec), hints.toInspectorValue(exec));
    }

    return jsUndefined();
}

JSValue JSCommandLineAPIHost::databaseId(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

#if ENABLE(SQL_DATABASE)
    Database* database = toDatabase(exec->uncheckedArgument(0));
    if (database)
        return jsStringWithCache(exec, impl().databaseIdImpl(database));
#endif

    return jsUndefined();
}

JSValue JSCommandLineAPIHost::storageId(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    Storage* storage = toStorage(exec->uncheckedArgument(0));
    if (storage)
        return jsStringWithCache(exec, impl().storageIdImpl(storage));

    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
