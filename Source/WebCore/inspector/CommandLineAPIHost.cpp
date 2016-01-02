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
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "JSCommandLineAPIHost.h"
#include "JSDOMGlobalObject.h"
#include "Pasteboard.h"
#include "Storage.h"
#include <bindings/ScriptValue.h>
#include <inspector/InspectorValues.h>
#include <inspector/agents/InspectorAgent.h>
#include <inspector/agents/InspectorConsoleAgent.h>
#include <runtime/JSCInlines.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

using namespace JSC;
using namespace Inspector;

namespace WebCore {

Ref<CommandLineAPIHost> CommandLineAPIHost::create()
{
    return adoptRef(*new CommandLineAPIHost);
}

CommandLineAPIHost::CommandLineAPIHost()
    : m_inspectedObject(std::make_unique<InspectableObject>())
{
}

CommandLineAPIHost::~CommandLineAPIHost()
{
}

void CommandLineAPIHost::disconnect()
{
    m_inspectorAgent = nullptr;
    m_consoleAgent = nullptr;
    m_domAgent = nullptr;
    m_domStorageAgent = nullptr;
    m_databaseAgent = nullptr;
}

void CommandLineAPIHost::inspectImpl(RefPtr<InspectorValue>&& object, RefPtr<InspectorValue>&& hints)
{
    if (!m_inspectorAgent)
        return;

    RefPtr<InspectorObject> hintsObject;
    if (!hints->asObject(hintsObject))
        return;

    auto remoteObject = BindingTraits<Inspector::Protocol::Runtime::RemoteObject>::runtimeCast(WTFMove(object));
    m_inspectorAgent->inspect(WTFMove(remoteObject), WTFMove(hintsObject));
}

void CommandLineAPIHost::getEventListenersImpl(Node* node, Vector<EventListenerInfo>& listenersArray)
{
    if (m_domAgent)
        m_domAgent->getEventListeners(node, listenersArray, false);
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

Deprecated::ScriptValue CommandLineAPIHost::InspectableObject::get(JSC::ExecState*)
{
    return Deprecated::ScriptValue();
}

void CommandLineAPIHost::addInspectedObject(std::unique_ptr<CommandLineAPIHost::InspectableObject> object)
{
    m_inspectedObject = WTFMove(object);
}

CommandLineAPIHost::InspectableObject* CommandLineAPIHost::inspectedObject()
{
    return m_inspectedObject.get();
}

String CommandLineAPIHost::databaseIdImpl(Database* database)
{
    if (m_databaseAgent)
        return m_databaseAgent->databaseId(database);
    return String();
}

String CommandLineAPIHost::storageIdImpl(Storage* storage)
{
    if (m_domStorageAgent)
        return m_domStorageAgent->storageId(storage);
    return String();
}

JSValue CommandLineAPIHost::wrapper(ExecState* exec, JSDOMGlobalObject* globalObject)
{
    JSValue value = m_wrappers.getWrapper(globalObject);
    if (value)
        return value;

    JSObject* prototype = JSCommandLineAPIHost::createPrototype(exec->vm(), globalObject);
    Structure* structure = JSCommandLineAPIHost::createStructure(exec->vm(), globalObject, prototype);
    JSCommandLineAPIHost* commandLineAPIHost = JSCommandLineAPIHost::create(structure, globalObject, Ref<CommandLineAPIHost>(*this));
    m_wrappers.addWrapper(globalObject, commandLineAPIHost);

    return commandLineAPIHost;
}

void CommandLineAPIHost::clearAllWrappers()
{
    m_wrappers.clearAllWrappers();
    m_inspectedObject = std::make_unique<InspectableObject>();
}

} // namespace WebCore
