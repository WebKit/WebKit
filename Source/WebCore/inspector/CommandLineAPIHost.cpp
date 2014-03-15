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

#if ENABLE(INSPECTOR)

#include "Element.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorWebFrontendDispatchers.h"
#include "Pasteboard.h"
#include "Storage.h"
#include "markup.h"
#include <bindings/ScriptValue.h>
#include <inspector/InspectorValues.h>
#include <inspector/agents/InspectorAgent.h>
#include <inspector/agents/InspectorConsoleAgent.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(SQL_DATABASE)
#include "Database.h"
#endif

using namespace Inspector;

namespace WebCore {

PassRefPtr<CommandLineAPIHost> CommandLineAPIHost::create()
{
    return adoptRef(new CommandLineAPIHost);
}

CommandLineAPIHost::CommandLineAPIHost()
    : m_inspectorAgent(nullptr)
    , m_consoleAgent(nullptr)
    , m_domAgent(nullptr)
    , m_domStorageAgent(nullptr)
#if ENABLE(SQL_DATABASE)
    , m_databaseAgent(nullptr)
#endif
{
    m_defaultInspectableObject = std::make_unique<InspectableObject>();
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
#if ENABLE(SQL_DATABASE)
    m_databaseAgent = nullptr;
#endif
}

void CommandLineAPIHost::inspectImpl(PassRefPtr<InspectorValue> object, PassRefPtr<InspectorValue> hints)
{
    if (m_inspectorAgent) {
        RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject> remoteObject = Inspector::TypeBuilder::Runtime::RemoteObject::runtimeCast(object);
        m_inspectorAgent->inspect(remoteObject, hints->asObject());
    }
}

void CommandLineAPIHost::getEventListenersImpl(Node* node, Vector<EventListenerInfo>& listenersArray)
{
    if (m_domAgent)
        m_domAgent->getEventListeners(node, listenersArray, false);
}

void CommandLineAPIHost::clearConsoleMessages()
{
    if (m_consoleAgent) {
        ErrorString error;
        m_consoleAgent->clearMessages(&error);
    }
}

void CommandLineAPIHost::copyText(const String& text)
{
    Pasteboard::createForCopyAndPaste()->writePlainText(text, Pasteboard::CannotSmartReplace);
}

Deprecated::ScriptValue CommandLineAPIHost::InspectableObject::get(JSC::ExecState*)
{
    return Deprecated::ScriptValue();
};

void CommandLineAPIHost::addInspectedObject(std::unique_ptr<CommandLineAPIHost::InspectableObject> object)
{
    m_inspectedObjects.insert(0, std::move(object));
    while (m_inspectedObjects.size() > 5)
        m_inspectedObjects.removeLast();
}

void CommandLineAPIHost::clearInspectedObjects()
{
    m_inspectedObjects.clear();
}

CommandLineAPIHost::InspectableObject* CommandLineAPIHost::inspectedObject(unsigned index)
{
    if (index >= m_inspectedObjects.size())
        return m_defaultInspectableObject.get();

    return m_inspectedObjects[index].get();
}

#if ENABLE(SQL_DATABASE)
String CommandLineAPIHost::databaseIdImpl(Database* database)
{
    if (m_databaseAgent)
        return m_databaseAgent->databaseId(database);
    return String();
}
#endif

String CommandLineAPIHost::storageIdImpl(Storage* storage)
{
    if (m_domStorageAgent)
        return m_domStorageAgent->storageId(storage);
    return String();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
