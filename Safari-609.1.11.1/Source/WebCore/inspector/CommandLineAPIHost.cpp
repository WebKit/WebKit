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
#include "EventTarget.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "JSCommandLineAPIHost.h"
#include "JSDOMGlobalObject.h"
#include "JSEventListener.h"
#include "Pasteboard.h"
#include "Storage.h"
#include "WebConsoleAgent.h"
#include <JavaScriptCore/InspectorAgent.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ScriptValue.h>
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
    : m_inspectedObject(makeUnique<InspectableObject>())
{
}

CommandLineAPIHost::~CommandLineAPIHost() = default;

void CommandLineAPIHost::disconnect()
{

    m_instrumentingAgents = nullptr;
}

void CommandLineAPIHost::inspect(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue valueToInspect, JSC::JSValue hintsValue)
{
    if (!m_instrumentingAgents)
        return;

    auto* inspectorAgent = m_instrumentingAgents->inspectorAgent();
    if (!inspectorAgent)
        return;

    RefPtr<JSON::Object> hintsObject;
    if (!Inspector::toInspectorValue(&lexicalGlobalObject, hintsValue)->asObject(hintsObject))
        return;

    auto remoteObject = BindingTraits<Inspector::Protocol::Runtime::RemoteObject>::runtimeCast(Inspector::toInspectorValue(&lexicalGlobalObject, valueToInspect));
    inspectorAgent->inspect(WTFMove(remoteObject), WTFMove(hintsObject));
}

CommandLineAPIHost::EventListenersRecord CommandLineAPIHost::getEventListeners(JSGlobalObject& lexicalGlobalObject, EventTarget& target)
{
    auto* scriptExecutionContext = target.scriptExecutionContext();
    if (!scriptExecutionContext)
        return { };

    EventListenersRecord result;

    VM& vm = lexicalGlobalObject.vm();

    for (auto& eventType : target.eventTypes()) {
        Vector<CommandLineAPIHost::ListenerEntry> entries;

        for (auto& eventListener : target.eventListeners(eventType)) {
            if (!is<JSEventListener>(eventListener->callback()))
                continue;

            auto& jsListener = downcast<JSEventListener>(eventListener->callback());

            // Hide listeners from other contexts.
            if (&jsListener.isolatedWorld() != &currentWorld(lexicalGlobalObject))
                continue;

            auto* function = jsListener.jsFunction(*scriptExecutionContext);
            if (!function)
                continue;

            entries.append({ Strong<JSObject>(vm, function), eventListener->useCapture(), eventListener->isPassive(), eventListener->isOnce() });
        }

        if (!entries.isEmpty())
            result.append({ eventType, WTFMove(entries) });
    }

    return result;
}

void CommandLineAPIHost::clearConsoleMessages()
{
    if (!m_instrumentingAgents)
        return;

    auto* consoleAgent = m_instrumentingAgents->webConsoleAgent();
    if (!consoleAgent)
        return;

    ErrorString ignored;
    consoleAgent->clearMessages(ignored);
}

void CommandLineAPIHost::copyText(const String& text)
{
    Pasteboard::createForCopyAndPaste()->writePlainText(text, Pasteboard::CannotSmartReplace);
}

JSC::JSValue CommandLineAPIHost::InspectableObject::get(JSC::JSGlobalObject&)
{
    return { };
}

void CommandLineAPIHost::addInspectedObject(std::unique_ptr<CommandLineAPIHost::InspectableObject> object)
{
    m_inspectedObject = WTFMove(object);
}

JSC::JSValue CommandLineAPIHost::inspectedObject(JSC::JSGlobalObject& lexicalGlobalObject)
{
    if (!m_inspectedObject)
        return jsUndefined();

    JSC::JSLockHolder lock(&lexicalGlobalObject);
    auto scriptValue = m_inspectedObject->get(lexicalGlobalObject);
    return scriptValue ? scriptValue : jsUndefined();
}

String CommandLineAPIHost::databaseId(Database& database)
{
    if (m_instrumentingAgents) {
        if (auto* databaseAgent = m_instrumentingAgents->inspectorDatabaseAgent())
            return databaseAgent->databaseId(database);
    }
    return { };
}

String CommandLineAPIHost::storageId(Storage& storage)
{
    return InspectorDOMStorageAgent::storageId(storage);
}

JSValue CommandLineAPIHost::wrapper(JSGlobalObject* exec, JSDOMGlobalObject* globalObject)
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
    m_inspectedObject = makeUnique<InspectableObject>();
}

} // namespace WebCore
