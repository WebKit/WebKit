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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "InjectedScriptHost.h"

#if ENABLE(INSPECTOR)

#include "Element.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
#include "InjectedScript.h"
#include "InjectedScriptSource.h"
#include "InspectorAgent.h"
#include "InspectorClient.h"
#include "InspectorConsoleAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorFrontend.h"
#include "InspectorValues.h"
#include "Pasteboard.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "ScriptDebugServer.h"
#endif

#if ENABLE(DATABASE)
#include "Database.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#endif

#include "markup.h"

#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

InjectedScriptHost::InjectedScriptHost(InspectorAgent* inspectorAgent)
    : m_inspectorAgent(inspectorAgent)
    , m_nextInjectedScriptId(1)
    , m_lastWorkerId(1 << 31) // Distinguish ids of fake workers from real ones, to minimize the chances they overlap.
{
}

InjectedScriptHost::~InjectedScriptHost()
{
}

void InjectedScriptHost::inspectImpl(PassRefPtr<InspectorValue> objectId, PassRefPtr<InspectorValue> hints)
{
    if (InspectorFrontend* fe = frontend())
        fe->inspector()->inspect(objectId->asObject(), hints->asObject());
}

void InjectedScriptHost::clearConsoleMessages()
{
    if (m_inspectorAgent) {
        ErrorString error;
        m_inspectorAgent->consoleAgent()->clearConsoleMessages(&error);
    }
}

void InjectedScriptHost::addInspectedNode(Node* node)
{
    m_inspectedNodes.prepend(node);
    while (m_inspectedNodes.size() > 5)
        m_inspectedNodes.removeLast();
}

void InjectedScriptHost::clearInspectedNodes()
{
    m_inspectedNodes.clear();
}

void InjectedScriptHost::copyText(const String& text)
{
    Pasteboard::generalPasteboard()->writePlainText(text);
}

Node* InjectedScriptHost::inspectedNode(unsigned long num)
{
    if (num < m_inspectedNodes.size())
        return m_inspectedNodes[num].get();
    return 0;
}

#if ENABLE(DATABASE)
long InjectedScriptHost::databaseIdImpl(Database* database)
{
    if (m_inspectorAgent && m_inspectorAgent->databaseAgent())
        return m_inspectorAgent->databaseAgent()->databaseId(database);
    return 0;
}
#endif

#if ENABLE(DOM_STORAGE)
long InjectedScriptHost::storageIdImpl(Storage* storage)
{
    if (m_inspectorAgent && m_inspectorAgent->domStorageAgent())
        return m_inspectorAgent->domStorageAgent()->storageId(storage);
    return 0;
}
#endif

InjectedScript InjectedScriptHost::injectedScriptForId(long id)
{
    return m_idToInjectedScript.get(id);
}

InjectedScript InjectedScriptHost::injectedScriptForObjectId(InspectorObject* objectId)
{
    long injectedScriptId = 0;
    bool success = objectId->getNumber("injectedScriptId", &injectedScriptId);
    if (success)
        return injectedScriptForId(injectedScriptId);
    return InjectedScript();
}

InjectedScript InjectedScriptHost::injectedScriptForMainFrame()
{
    return injectedScriptFor(mainWorldScriptState(m_inspectorAgent->inspectedPage()->mainFrame()));
}

void InjectedScriptHost::discardInjectedScripts()
{
    IdToInjectedScriptMap::iterator end = m_idToInjectedScript.end();
    for (IdToInjectedScriptMap::iterator it = m_idToInjectedScript.begin(); it != end; ++it)
        discardInjectedScript(it->second.scriptState());
    m_idToInjectedScript.clear();
}

void InjectedScriptHost::releaseWrapperObjectGroup(long injectedScriptId, const String& objectGroup)
{
    if (injectedScriptId) {
         InjectedScript injectedScript = m_idToInjectedScript.get(injectedScriptId);
         if (!injectedScript.hasNoValue())
             injectedScript.releaseWrapperObjectGroup(objectGroup);
    } else {
         // Iterate over all injected scripts if injectedScriptId is not specified.
         for (IdToInjectedScriptMap::iterator it = m_idToInjectedScript.begin(); it != m_idToInjectedScript.end(); ++it)
              it->second.releaseWrapperObjectGroup(objectGroup);
    }
}

InspectorFrontend* InjectedScriptHost::frontend()
{
    if (!m_inspectorAgent)
        return 0;
    return m_inspectorAgent->frontend();
}

String InjectedScriptHost::injectedScriptSource()
{
    return String(reinterpret_cast<char*>(InjectedScriptSource_js), sizeof(InjectedScriptSource_js));
}

pair<long, ScriptObject> InjectedScriptHost::injectScript(const String& source, ScriptState* scriptState)
{
    long id = m_nextInjectedScriptId++;
    return std::make_pair(id, createInjectedScript(source, scriptState, id));
}

#if ENABLE(WORKERS)
long InjectedScriptHost::nextWorkerId()
{
    return ++m_lastWorkerId;
}

void InjectedScriptHost::didCreateWorker(long id, const String& url, bool isSharedWorker)
{
    if (m_inspectorAgent)
        m_inspectorAgent->didCreateWorker(id, url, isSharedWorker);
}

void InjectedScriptHost::didDestroyWorker(long id)
{
    if (m_inspectorAgent)
        m_inspectorAgent->didDestroyWorker(id);
}
#endif // ENABLE(WORKERS)

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
