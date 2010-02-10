/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
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
#include "InspectorBackend.h"

#if ENABLE(INSPECTOR)

#if ENABLE(DATABASE)
#include "Database.h"
#endif

#include "Element.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorDOMAgent.h"
#include "InspectorFrontend.h"
#include "InspectorResource.h"
#include "Pasteboard.h"
#include "ScriptArray.h"
#include "SerializedScriptValue.h"

#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
#include "JavaScriptCallFrame.h"
#include "JavaScriptDebugServer.h"
using namespace JSC;
#endif

#include "markup.h"

#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

InspectorBackend::InspectorBackend(InspectorController* inspectorController)
    : m_inspectorController(inspectorController)
{
}

InspectorBackend::~InspectorBackend()
{
}

void InspectorBackend::saveFrontendSettings(const String& settings)
{
    if (m_inspectorController)
        m_inspectorController->setSetting(InspectorController::FrontendSettingsSettingName, settings);
}

void InspectorBackend::storeLastActivePanel(const String& panelName)
{
    if (m_inspectorController)
        m_inspectorController->storeLastActivePanel(panelName);
}

void InspectorBackend::toggleNodeSearch()
{
    if (m_inspectorController)
        m_inspectorController->toggleSearchForNodeInPage();
}

bool InspectorBackend::searchingForNode()
{
    if (m_inspectorController)
        return m_inspectorController->searchingForNodeInPage();
    return false;
}

bool InspectorBackend::resourceTrackingEnabled() const
{
    if (m_inspectorController)
        return m_inspectorController->resourceTrackingEnabled();
    return false;
}

void InspectorBackend::enableResourceTracking(bool always)
{
    if (m_inspectorController)
        m_inspectorController->enableResourceTracking(always);
}

void InspectorBackend::disableResourceTracking(bool always)
{
    if (m_inspectorController)
        m_inspectorController->disableResourceTracking(always);
}

void InspectorBackend::getResourceContent(long callId, unsigned long identifier)
{
    InspectorFrontend* frontend = inspectorFrontend();
    if (!frontend)
        return;

    RefPtr<InspectorResource> resource = m_inspectorController->resources().get(identifier);
    if (resource)
        frontend->didGetResourceContent(callId, resource->sourceString());
    else
        frontend->didGetResourceContent(callId, "");
}

void InspectorBackend::startTimelineProfiler()
{
    if (m_inspectorController)
        m_inspectorController->startTimelineProfiler();
}

void InspectorBackend::stopTimelineProfiler()
{
    if (m_inspectorController)
        m_inspectorController->stopTimelineProfiler();
}

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
bool InspectorBackend::debuggerEnabled() const
{
    if (m_inspectorController)
        return m_inspectorController->debuggerEnabled();
    return false;
}

void InspectorBackend::enableDebugger(bool always)
{
    if (m_inspectorController)
        m_inspectorController->enableDebuggerFromFrontend(always);
}

void InspectorBackend::disableDebugger(bool always)
{
    if (m_inspectorController)
        m_inspectorController->disableDebugger(always);
}

void InspectorBackend::addBreakpoint(const String& sourceID, unsigned lineNumber, const String& condition)
{
    intptr_t sourceIDValue = sourceID.toIntPtr();
    JavaScriptDebugServer::shared().addBreakpoint(sourceIDValue, lineNumber, condition);
}

void InspectorBackend::updateBreakpoint(const String& sourceID, unsigned lineNumber, const String& condition)
{
    intptr_t sourceIDValue = sourceID.toIntPtr();
    JavaScriptDebugServer::shared().updateBreakpoint(sourceIDValue, lineNumber, condition);
}

void InspectorBackend::removeBreakpoint(const String& sourceID, unsigned lineNumber)
{
    intptr_t sourceIDValue = sourceID.toIntPtr();
    JavaScriptDebugServer::shared().removeBreakpoint(sourceIDValue, lineNumber);
}

void InspectorBackend::pauseInDebugger()
{
    JavaScriptDebugServer::shared().pauseProgram();
}

void InspectorBackend::resumeDebugger()
{
    if (m_inspectorController)
        m_inspectorController->resumeDebugger();
}

void InspectorBackend::stepOverStatementInDebugger()
{
    JavaScriptDebugServer::shared().stepOverStatement();
}

void InspectorBackend::stepIntoStatementInDebugger()
{
    JavaScriptDebugServer::shared().stepIntoStatement();
}

void InspectorBackend::stepOutOfFunctionInDebugger()
{
    JavaScriptDebugServer::shared().stepOutOfFunction();
}

long InspectorBackend::pauseOnExceptionsState()
{
    return JavaScriptDebugServer::shared().pauseOnExceptionsState();
}

void InspectorBackend::setPauseOnExceptionsState(long pauseState)
{
    JavaScriptDebugServer::shared().setPauseOnExceptionsState(static_cast<JavaScriptDebugServer::PauseOnExceptionsState>(pauseState));
}

JavaScriptCallFrame* InspectorBackend::currentCallFrame() const
{
    return JavaScriptDebugServer::shared().currentCallFrame();
}
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
bool InspectorBackend::profilerEnabled()
{
    if (m_inspectorController)
        return m_inspectorController->profilerEnabled();
    return false;
}

void InspectorBackend::enableProfiler(bool always)
{
    if (m_inspectorController)
        m_inspectorController->enableProfiler(always);
}

void InspectorBackend::disableProfiler(bool always)
{
    if (m_inspectorController)
        m_inspectorController->disableProfiler(always);
}

void InspectorBackend::startProfiling()
{
    if (m_inspectorController)
        m_inspectorController->startUserInitiatedProfiling();
}

void InspectorBackend::stopProfiling()
{
    if (m_inspectorController)
        m_inspectorController->stopUserInitiatedProfiling();
}

void InspectorBackend::getProfileHeaders(long callId)
{
    if (m_inspectorController)
        m_inspectorController->getProfileHeaders(callId);
}

void InspectorBackend::getProfile(long callId, unsigned uid)
{
    if (m_inspectorController)
        m_inspectorController->getProfile(callId, uid);
}
#endif

void InspectorBackend::setInjectedScriptSource(const String& source)
{
    if (m_inspectorController)
        m_inspectorController->injectedScriptHost()->setInjectedScriptSource(source);
}

void InspectorBackend::dispatchOnInjectedScript(long callId, long injectedScriptId, const String& methodName, const String& arguments, bool async)
{
    InspectorFrontend* frontend = inspectorFrontend();
    if (!frontend)
        return;

    // FIXME: explicitly pass injectedScriptId along with node id to the frontend.
    bool injectedScriptIdIsNodeId = injectedScriptId <= 0;

    InjectedScript injectedScript;
    if (injectedScriptIdIsNodeId)
        injectedScript = m_inspectorController->injectedScriptForNodeId(-injectedScriptId);
    else
        injectedScript = m_inspectorController->injectedScriptHost()->injectedScriptForId(injectedScriptId);

    if (injectedScript.hasNoValue())
        return;

    RefPtr<SerializedScriptValue> result;
    bool hadException = false;
    injectedScript.dispatch(callId, methodName, arguments, async, &result, &hadException);
    if (async)
        return;  // InjectedScript will return result asynchronously by means of ::reportDidDispatchOnInjectedScript.
    frontend->didDispatchOnInjectedScript(callId, result.get(), hadException);
}

void InspectorBackend::getChildNodes(long callId, long nodeId)
{
    if (InspectorDOMAgent* domAgent = inspectorDOMAgent())
        domAgent->getChildNodes(callId, nodeId);
}

void InspectorBackend::setAttribute(long callId, long elementId, const String& name, const String& value)
{
    if (InspectorDOMAgent* domAgent = inspectorDOMAgent())
        domAgent->setAttribute(callId, elementId, name, value);
}

void InspectorBackend::removeAttribute(long callId, long elementId, const String& name)
{
    if (InspectorDOMAgent* domAgent = inspectorDOMAgent())
        domAgent->removeAttribute(callId, elementId, name);
}

void InspectorBackend::setTextNodeValue(long callId, long nodeId, const String& value)
{
    if (InspectorDOMAgent* domAgent = inspectorDOMAgent())
        domAgent->setTextNodeValue(callId, nodeId, value);
}

void InspectorBackend::getEventListenersForNode(long callId, long nodeId)
{
    if (InspectorDOMAgent* domAgent = inspectorDOMAgent())
        domAgent->getEventListenersForNode(callId, nodeId);
}

void InspectorBackend::copyNode(long nodeId)
{
    Node* node = nodeForId(nodeId);
    if (!node)
        return;
    String markup = createMarkup(node);
    Pasteboard::generalPasteboard()->writePlainText(markup);
}
    
void InspectorBackend::removeNode(long callId, long nodeId)
{
    InspectorFrontend* frontend = inspectorFrontend();
    if (!frontend)
        return;

    Node* node = nodeForId(nodeId);
    if (!node) {
        // Use -1 to denote an error condition.
        frontend->didRemoveNode(callId, -1);
        return;
    }

    Node* parentNode = node->parentNode();
    if (!parentNode) {
        frontend->didRemoveNode(callId, -1);
        return;
    }

    ExceptionCode code;
    parentNode->removeChild(node, code);
    if (code) {
        frontend->didRemoveNode(callId, -1);
        return;
    }

    frontend->didRemoveNode(callId, nodeId);
}

void InspectorBackend::highlightDOMNode(long nodeId)
{
    if (Node* node = nodeForId(nodeId))
        m_inspectorController->highlight(node);
}

void InspectorBackend::hideDOMNodeHighlight()
{
    if (m_inspectorController)
        m_inspectorController->hideHighlight();
}

void InspectorBackend::getCookies(long callId)
{
    if (!m_inspectorController)
        return;
    m_inspectorController->getCookies(callId);
}

void InspectorBackend::deleteCookie(const String& cookieName, const String& domain)
{
    if (!m_inspectorController)
        return;
    m_inspectorController->deleteCookie(cookieName, domain);
}

void InspectorBackend::releaseWrapperObjectGroup(long injectedScriptId, const String& objectGroup)
{
    if (!m_inspectorController)
        return;
    m_inspectorController->injectedScriptHost()->releaseWrapperObjectGroup(injectedScriptId, objectGroup);
}

void InspectorBackend::didEvaluateForTestInFrontend(long callId, const String& jsonResult)
{
    if (m_inspectorController)
        m_inspectorController->didEvaluateForTestInFrontend(callId, jsonResult);
}

#if ENABLE(DATABASE)
void InspectorBackend::getDatabaseTableNames(long callId, long databaseId)
{
    if (InspectorFrontend* frontend = inspectorFrontend()) {
        ScriptArray result = frontend->newScriptArray();
        Database* database = m_inspectorController->databaseForId(databaseId);
        if (database) {
            Vector<String> tableNames = database->tableNames();
            unsigned length = tableNames.size();
            for (unsigned i = 0; i < length; ++i)
                result.set(i, tableNames[i]);
        }
        frontend->didGetDatabaseTableNames(callId, result);
    }
}
#endif

#if ENABLE(DOM_STORAGE)
void InspectorBackend::getDOMStorageEntries(long callId, long storageId)
{
    if (m_inspectorController)
        m_inspectorController->getDOMStorageEntries(callId, storageId);
}

void InspectorBackend::setDOMStorageItem(long callId, long storageId, const String& key, const String& value)
{
    if (m_inspectorController)
        m_inspectorController->setDOMStorageItem(callId, storageId, key, value);
}

void InspectorBackend::removeDOMStorageItem(long callId, long storageId, const String& key)
{
    if (m_inspectorController)
        m_inspectorController->removeDOMStorageItem(callId, storageId, key);
}
#endif

InspectorDOMAgent* InspectorBackend::inspectorDOMAgent()
{
    if (!m_inspectorController)
        return 0;
    return m_inspectorController->domAgent();
}

InspectorFrontend* InspectorBackend::inspectorFrontend()
{
    if (!m_inspectorController)
        return 0;
    return m_inspectorController->m_frontend.get();
}

Node* InspectorBackend::nodeForId(long nodeId)
{
    if (InspectorDOMAgent* domAgent = inspectorDOMAgent())
        return domAgent->nodeForId(nodeId);
    return 0;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
