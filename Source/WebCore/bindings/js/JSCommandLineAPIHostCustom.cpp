/*
 * Copyright (C) 2007-2008, 2016 Apple Inc. All rights reserved.
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
#include "JSCommandLineAPIHost.h"

#include "CommandLineAPIHost.h"
#include "Database.h"
#include "InspectorDOMAgent.h"
#include "JSDatabase.h"
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

using namespace JSC;

namespace WebCore {

JSValue JSCommandLineAPIHost::inspectedObject(ExecState& state)
{
    CommandLineAPIHost::InspectableObject* object = wrapped().inspectedObject();
    if (!object)
        return jsUndefined();

    JSLockHolder lock(&state);
    auto scriptValue = object->get(state);
    return scriptValue ? scriptValue : jsUndefined();
}

static JSArray* getJSListenerFunctions(ExecState& state, Document* document, const EventListenerInfo& listenerInfo)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSArray* result = constructEmptyArray(&state, nullptr);
    RETURN_IF_EXCEPTION(scope, nullptr);
    size_t handlersCount = listenerInfo.eventListenerVector.size();
    for (size_t i = 0, outputIndex = 0; i < handlersCount; ++i) {
        const JSEventListener* jsListener = JSEventListener::cast(&listenerInfo.eventListenerVector[i]->callback());
        if (!jsListener) {
            ASSERT_NOT_REACHED();
            continue;
        }

        // Hide listeners from other contexts.
        if (&jsListener->isolatedWorld() != &currentWorld(&state))
            continue;

        JSObject* function = jsListener->jsFunction(document);
        if (!function)
            continue;

        JSObject* listenerEntry = constructEmptyObject(&state);
        listenerEntry->putDirect(vm, Identifier::fromString(&state, "listener"), function);
        listenerEntry->putDirect(vm, Identifier::fromString(&state, "useCapture"), jsBoolean(listenerInfo.eventListenerVector[i]->useCapture()));
        result->putDirectIndex(&state, outputIndex++, JSValue(listenerEntry));
        RETURN_IF_EXCEPTION(scope, nullptr);
    }
    return result;
}

JSValue JSCommandLineAPIHost::getEventListeners(ExecState& state)
{
    if (state.argumentCount() < 1)
        return jsUndefined();

    VM& vm = state.vm();
    JSValue value = state.uncheckedArgument(0);
    if (!value.isObject() || value.isNull())
        return jsUndefined();

    Node* node = JSNode::toWrapped(vm, value);
    if (!node)
        return jsUndefined();

    Vector<EventListenerInfo> listenersArray;
    wrapped().getEventListenersImpl(node, listenersArray);

    JSObject* result = constructEmptyObject(&state);
    for (size_t i = 0; i < listenersArray.size(); ++i) {
        JSArray* listeners = getJSListenerFunctions(state, &node->document(), listenersArray[i]);
        if (!listeners->length())
            continue;
        AtomicString eventType = listenersArray[i].eventType;
        result->putDirect(state.vm(), Identifier::fromString(&state, eventType.impl()), JSValue(listeners));
    }

    return result;
}

JSValue JSCommandLineAPIHost::inspect(ExecState& state)
{
    if (state.argumentCount() < 2)
        return jsUndefined();
    wrapped().inspectImpl(Inspector::toInspectorValue(state, state.uncheckedArgument(0)),
        Inspector::toInspectorValue(state, state.uncheckedArgument(1)));
    return jsUndefined();
}

JSValue JSCommandLineAPIHost::databaseId(ExecState& state)
{
    if (state.argumentCount() < 1)
        return jsUndefined();

    VM& vm = state.vm();
    Database* database = JSDatabase::toWrapped(vm, state.uncheckedArgument(0));
    if (database)
        return jsStringWithCache(&state, wrapped().databaseIdImpl(database));

    return jsUndefined();
}

JSValue JSCommandLineAPIHost::storageId(ExecState& state)
{
    if (state.argumentCount() < 1)
        return jsUndefined();

    VM& vm = state.vm();
    Storage* storage = JSStorage::toWrapped(vm, state.uncheckedArgument(0));
    if (storage)
        return jsStringWithCache(&state, wrapped().storageIdImpl(storage));

    return jsUndefined();
}

} // namespace WebCore
