/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "CommandLineAPIHost.h"

#include "Database.h"
#include "Document.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "JSCommandLineAPIHost.h"
#include "JSDOMGlobalObject.h"
#include "JSEventListener.h"
#include "Pasteboard.h"
#include "Storage.h"
#include <bindings/ScriptValue.h>
#include <inspector/agents/InspectorAgent.h>
#include <inspector/agents/InspectorConsoleAgent.h>
#include <runtime/JSCInlines.h>
#include <runtime/JSLock.h>
#include <wtf/JSONValues.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace JSC;
using namespace Inspector;

Ref<CommandLineAPIHost> CommandLineAPIHost::create()
{
    return adoptRef(*new CommandLineAPIHost);
}

CommandLineAPIHost::CommandLineAPIHost()
    : m_inspectedObject(std::make_unique<InspectableObject>())
{
}

CommandLineAPIHost::~CommandLineAPIHost() = default;

void CommandLineAPIHost::disconnect()
{
    m_inspectorAgent = nullptr;
    m_consoleAgent = nullptr;
    m_domAgent = nullptr;
    m_domStorageAgent = nullptr;
    m_databaseAgent = nullptr;
}

void CommandLineAPIHost::inspect(JSC::ExecState& state, JSC::JSValue valueToInspect, JSC::JSValue hintsValue)
{
    if (!m_inspectorAgent)
        return;

    RefPtr<JSON::Object> hintsObject;
    if (!Inspector::toInspectorValue(state, hintsValue)->asObject(hintsObject))
        return;

    auto remoteObject = BindingTraits<Inspector::Protocol::Runtime::RemoteObject>::runtimeCast(Inspector::toInspectorValue(state, valueToInspect));
    m_inspectorAgent->inspect(WTFMove(remoteObject), WTFMove(hintsObject));
}

static Vector<CommandLineAPIHost::ListenerEntry> listenerEntriesFromListenerInfo(ExecState& state, Document& document, const EventListenerInfo& listenerInfo)
{
    VM& vm = state.vm();

    Vector<CommandLineAPIHost::ListenerEntry> entries;
    for (auto& eventListener : listenerInfo.eventListenerVector) {
        auto jsListener = JSEventListener::cast(&eventListener->callback());
        if (!jsListener) {
            ASSERT_NOT_REACHED();
            continue;
        }

        // Hide listeners from other contexts.
        if (&jsListener->isolatedWorld() != &currentWorld(&state))
            continue;

        auto function = jsListener->jsFunction(document);
        if (!function)
            continue;

        entries.append({ JSC::Strong<JSC::JSObject>(vm, function), eventListener->useCapture(), eventListener->isPassive(), eventListener->isOnce() });
    }

    return entries;
}

auto CommandLineAPIHost::getEventListeners(JSC::ExecState& state, Node* node) -> EventListenersRecord
{
    if (!m_domAgent)
        return { };

    if (!node)
        return { };

    Vector<EventListenerInfo> listenerInfoArray;
    m_domAgent->getEventListeners(node, listenerInfoArray, false);

    EventListenersRecord result;

    for (auto& listenerInfo : listenerInfoArray) {
        auto entries = listenerEntriesFromListenerInfo(state, node->document(), listenerInfo);
        if (entries.isEmpty())
            continue;
        result.append({ listenerInfo.eventType, WTFMove(entries) });
    }

    return result;
}

void CommandLineAPIHost::clearConsoleMessages()
{
    if (m_consoleAgent) {
        ErrorString unused;
        m_consoleAgent->clearMessages(unused);
    }
}

void CommandLineAPIHost::copyText(const String& text)
{
    Pasteboard::createForCopyAndPaste()->writePlainText(text, Pasteboard::CannotSmartReplace);
}

JSC::JSValue CommandLineAPIHost::InspectableObject::get(JSC::ExecState&)
{
    return { };
}

void CommandLineAPIHost::addInspectedObject(std::unique_ptr<CommandLineAPIHost::InspectableObject> object)
{
    m_inspectedObject = WTFMove(object);
}

JSC::JSValue CommandLineAPIHost::inspectedObject(JSC::ExecState& state)
{
    if (!m_inspectedObject)
        return jsUndefined();

    JSC::JSLockHolder lock(&state);
    auto scriptValue = m_inspectedObject->get(state);
    return scriptValue ? scriptValue : jsUndefined();
}

String CommandLineAPIHost::databaseId(Database& database)
{
    if (m_databaseAgent)
        return m_databaseAgent->databaseId(database);
    return { };
}

String CommandLineAPIHost::storageId(Storage& storage)
{
    if (m_domStorageAgent)
        return m_domStorageAgent->storageId(storage);
    return { };
}

JSValue CommandLineAPIHost::wrapper(ExecState* exec, JSDOMGlobalObject* globalObject)
{
    JSValue value = m_wrappers.getWrapper(globalObject);
    if (value)
        return value;

    JSObject* prototype = JSCommandLineAPIHost::createPrototype(exec->vm(), *globalObject);
    Structure* structure = JSCommandLineAPIHost::createStructure(exec->vm(), globalObject, prototype);
    JSCommandLineAPIHost* commandLineAPIHost = JSCommandLineAPIHost::create(structure, globalObject, makeRef(*this));
    m_wrappers.addWrapper(globalObject, commandLineAPIHost);

    return commandLineAPIHost;
}

void CommandLineAPIHost::clearAllWrappers()
{
    m_wrappers.clearAllWrappers();
    m_inspectedObject = std::make_unique<InspectableObject>();
}

} // namespace WebCore
