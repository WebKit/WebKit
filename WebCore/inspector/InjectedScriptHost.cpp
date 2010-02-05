/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorDOMAgent.h"
#include "InspectorFrontend.h"
#include "InspectorResource.h"
#include "Pasteboard.h"

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
#include "JavaScriptCallFrame.h"
#include "JavaScriptDebugServer.h"
using namespace JSC;
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

InjectedScriptHost::InjectedScriptHost(InspectorController* inspectorController)
    : m_inspectorController(inspectorController)
    , m_nextInjectedScriptId(1)
{
}

InjectedScriptHost::~InjectedScriptHost()
{
}

void InjectedScriptHost::clearConsoleMessages()
{
    if (m_inspectorController)
        m_inspectorController->clearConsoleMessages();
}

void InjectedScriptHost::copyText(const String& text)
{
    Pasteboard::generalPasteboard()->writePlainText(text);
}

Node* InjectedScriptHost::nodeForId(long nodeId)
{
    if (InspectorDOMAgent* domAgent = inspectorDOMAgent())
        return domAgent->nodeForId(nodeId);
    return 0;
}

long InjectedScriptHost::pushNodePathToFrontend(Node* node, bool withChildren, bool selectInUI)
{
    InspectorFrontend* frontend = inspectorFrontend();
    InspectorDOMAgent* domAgent = inspectorDOMAgent();
    if (!domAgent || !frontend)
        return 0;
    long id = domAgent->pushNodePathToFrontend(node);
    if (withChildren)
        domAgent->pushChildNodesToFrontend(id);
    if (selectInUI)
        frontend->updateFocusedNode(id);
    return id;
}

void InjectedScriptHost::addNodesToSearchResult(const String& nodeIds)
{
    if (InspectorFrontend* frontend = inspectorFrontend())
        frontend->addNodesToSearchResult(nodeIds);
}

long InjectedScriptHost::pushNodeByPathToFrontend(const String& path)
{
    InspectorDOMAgent* domAgent = inspectorDOMAgent();
    if (!domAgent)
        return 0;

    Node* node = domAgent->nodeForPath(path);
    if (!node)
        return 0;

    return domAgent->pushNodePathToFrontend(node);
}

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
JavaScriptCallFrame* InjectedScriptHost::currentCallFrame() const
{
    return JavaScriptDebugServer::shared().currentCallFrame();
}
#endif

#if ENABLE(DATABASE)
Database* InjectedScriptHost::databaseForId(long databaseId)
{
    if (m_inspectorController)
        return m_inspectorController->databaseForId(databaseId);
    return 0;
}

void InjectedScriptHost::selectDatabase(Database* database)
{
    if (m_inspectorController)
        m_inspectorController->selectDatabase(database);
}
#endif

#if ENABLE(DOM_STORAGE)
void InjectedScriptHost::selectDOMStorage(Storage* storage)
{
    if (m_inspectorController)
        m_inspectorController->selectDOMStorage(storage);
}
#endif

void InjectedScriptHost::reportDidDispatchOnInjectedScript(long callId, SerializedScriptValue* result, bool isException)
{
    if (InspectorFrontend* frontend = inspectorFrontend())
        frontend->didDispatchOnInjectedScript(callId, result, isException);
}

InjectedScript InjectedScriptHost::injectedScriptForId(long id)
{
    return m_idToInjectedScript.get(id);
}

void InjectedScriptHost::discardInjectedScripts()
{
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

InspectorDOMAgent* InjectedScriptHost::inspectorDOMAgent()
{
    if (!m_inspectorController)
        return 0;
    return m_inspectorController->domAgent();
}

InspectorFrontend* InjectedScriptHost::inspectorFrontend()
{
    if (!m_inspectorController)
        return 0;
    return m_inspectorController->m_frontend.get();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
