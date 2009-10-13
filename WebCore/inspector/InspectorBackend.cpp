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
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorDOMAgent.h"
#include "InspectorFrontend.h"
#include "InspectorResource.h"
#include "Pasteboard.h"
#include "ScriptArray.h"
#include "ScriptFunctionCall.h"

#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "JavaScriptCallFrame.h"
#include "JavaScriptDebugServer.h"
using namespace JSC;
#endif

#include "markup.h"

#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

InspectorBackend::InspectorBackend(InspectorController* inspectorController, InspectorClient* client)
    : m_inspectorController(inspectorController)
    , m_client(client)
{
}

InspectorBackend::~InspectorBackend()
{
}

void InspectorBackend::hideDOMNodeHighlight()
{
    if (m_inspectorController)
        m_inspectorController->hideHighlight();
}

String InspectorBackend::localizedStringsURL()
{
    return m_client->localizedStringsURL();
}

String InspectorBackend::hiddenPanels()
{
    return m_client->hiddenPanels();
}

void InspectorBackend::windowUnloading()
{
    if (m_inspectorController)
        m_inspectorController->close();
}

bool InspectorBackend::isWindowVisible()
{
    if (m_inspectorController)
        return m_inspectorController->windowVisible();
    return false;
}

void InspectorBackend::addResourceSourceToFrame(long identifier, Node* frame)
{
    if (!m_inspectorController)
        return;
    RefPtr<InspectorResource> resource = m_inspectorController->resources().get(identifier);
    if (resource) {
        String sourceString = resource->sourceString();
        if (!sourceString.isEmpty())
            addSourceToFrame(resource->mimeType(), sourceString, frame);
    }
}

bool InspectorBackend::addSourceToFrame(const String& mimeType, const String& source, Node* frameNode)
{
    ASSERT_ARG(frameNode, frameNode);

    if (!frameNode)
        return false;

    if (!frameNode->attached()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT(frameNode->isElementNode());
    if (!frameNode->isElementNode())
        return false;

    Element* element = static_cast<Element*>(frameNode);
    ASSERT(element->isFrameOwnerElement());
    if (!element->isFrameOwnerElement())
        return false;

    HTMLFrameOwnerElement* frameOwner = static_cast<HTMLFrameOwnerElement*>(element);
    ASSERT(frameOwner->contentFrame());
    if (!frameOwner->contentFrame())
        return false;

    FrameLoader* loader = frameOwner->contentFrame()->loader();

    loader->setResponseMIMEType(mimeType);
    loader->begin();
    loader->write(source);
    loader->end();

    return true;
}

void InspectorBackend::clearMessages(bool clearUI)
{
    if (m_inspectorController)
        m_inspectorController->clearConsoleMessages(clearUI);
}

void InspectorBackend::toggleNodeSearch()
{
    if (m_inspectorController)
        m_inspectorController->toggleSearchForNodeInPage();
}

void InspectorBackend::attach()
{
    if (m_inspectorController)
        m_inspectorController->attachWindow();
}

void InspectorBackend::detach()
{
    if (m_inspectorController)
        m_inspectorController->detachWindow();
}

void InspectorBackend::setAttachedWindowHeight(unsigned height)
{
    if (m_inspectorController)
        m_inspectorController->setAttachedWindowHeight(height);
}

void InspectorBackend::storeLastActivePanel(const String& panelName)
{
    if (m_inspectorController)
        m_inspectorController->storeLastActivePanel(panelName);
}

bool InspectorBackend::searchingForNode()
{
    if (m_inspectorController)
        return m_inspectorController->searchingForNodeInPage();
    return false;
}

void InspectorBackend::loaded()
{
    if (m_inspectorController)
        m_inspectorController->scriptObjectReady();
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

bool InspectorBackend::resourceTrackingEnabled() const
{
    if (m_inspectorController)
        return m_inspectorController->resourceTrackingEnabled();
    return false;
}

void InspectorBackend::moveWindowBy(float x, float y) const
{
    if (m_inspectorController)
        m_inspectorController->moveWindowBy(x, y);
}

void InspectorBackend::closeWindow()
{
    if (m_inspectorController)
        m_inspectorController->closeWindow();
}

const String& InspectorBackend::platform() const
{
#if PLATFORM(MAC)
#ifdef BUILDING_ON_TIGER
    DEFINE_STATIC_LOCAL(const String, platform, ("mac-tiger"));
#else
    DEFINE_STATIC_LOCAL(const String, platform, ("mac-leopard"));
#endif
#elif PLATFORM(WIN_OS)
    DEFINE_STATIC_LOCAL(const String, platform, ("windows"));
#elif PLATFORM(QT)
    DEFINE_STATIC_LOCAL(const String, platform, ("qt"));
#elif PLATFORM(GTK)
    DEFINE_STATIC_LOCAL(const String, platform, ("gtk"));
#elif PLATFORM(WX)
    DEFINE_STATIC_LOCAL(const String, platform, ("wx"));
#else
    DEFINE_STATIC_LOCAL(const String, platform, ("unknown"));
#endif

    return platform;
}

void InspectorBackend::enableTimeline(bool always)
{
    if (m_inspectorController)
        m_inspectorController->enableTimeline(always);
}

void InspectorBackend::disableTimeline(bool always)
{
    if (m_inspectorController)
        m_inspectorController->disableTimeline(always);
}

bool InspectorBackend::timelineEnabled() const
{
    if (m_inspectorController)
        return m_inspectorController->timelineEnabled();
    return false;
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
const ProfilesArray& InspectorBackend::profiles() const
{
    if (m_inspectorController)
        return m_inspectorController->profiles();
    return m_emptyProfiles;
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

bool InspectorBackend::profilerEnabled()
{
    if (m_inspectorController)
        return m_inspectorController->profilerEnabled();
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

bool InspectorBackend::debuggerEnabled() const
{
    if (m_inspectorController)
        return m_inspectorController->debuggerEnabled();
    return false;
}

JavaScriptCallFrame* InspectorBackend::currentCallFrame() const
{
    return JavaScriptDebugServer::shared().currentCallFrame();
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

bool InspectorBackend::pauseOnExceptions()
{
    return JavaScriptDebugServer::shared().pauseOnExceptions();
}

void InspectorBackend::setPauseOnExceptions(bool pause)
{
    JavaScriptDebugServer::shared().setPauseOnExceptions(pause);
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

#endif

void InspectorBackend::dispatchOnInjectedScript(long callId, const String& methodName, const String& arguments, bool async)
{
    InspectorFrontend* frontend = inspectorFrontend();
    if (!frontend)
        return;

    ScriptFunctionCall function(m_inspectorController->m_scriptState, m_inspectorController->m_injectedScriptObj, "dispatch");
    function.appendArgument(methodName);
    function.appendArgument(arguments);
    if (async)
        function.appendArgument(static_cast<int>(callId));
    bool hadException = false;
    ScriptValue result = function.call(hadException);
    if (async)
        return;  // InjectedScript will return result asynchronously by means of ::reportDidDispatchOnInjectedScript.
    if (hadException)
        frontend->didDispatchOnInjectedScript(callId, "", true);
    else
        frontend->didDispatchOnInjectedScript(callId, result.toString(m_inspectorController->m_scriptState), false);
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

void InspectorBackend::getCookies(long callId, const String& domain)
{
    if (!m_inspectorController)
        return;
    m_inspectorController->getCookies(callId, domain);
}

void InspectorBackend::deleteCookie(const String& cookieName, const String& domain)
{
    if (!m_inspectorController)
        return;
    m_inspectorController->deleteCookie(cookieName, domain);
}

void InspectorBackend::highlight(long nodeId)
{
    if (Node* node = nodeForId(nodeId))
        m_inspectorController->highlight(node);
}

Node* InspectorBackend::nodeForId(long nodeId)
{
    if (InspectorDOMAgent* domAgent = inspectorDOMAgent())
        return domAgent->nodeForId(nodeId);
    return 0;
}

ScriptValue InspectorBackend::wrapObject(const ScriptValue& object, const String& objectGroup)
{
    if (m_inspectorController)
        return m_inspectorController->wrapObject(object, objectGroup);
    return ScriptValue();
}

ScriptValue InspectorBackend::unwrapObject(const String& objectId)
{
    if (m_inspectorController)
        return m_inspectorController->unwrapObject(objectId);
    return ScriptValue();
}

void InspectorBackend::releaseWrapperObjectGroup(const String& objectGroup)
{
    if (m_inspectorController)
        m_inspectorController->releaseWrapperObjectGroup(objectGroup);
}

long InspectorBackend::pushNodePathToFrontend(Node* node, bool selectInUI)
{
    InspectorFrontend* frontend = inspectorFrontend();
    InspectorDOMAgent* domAgent = inspectorDOMAgent();
    if (!domAgent || !frontend)
        return 0;
    long id = domAgent->pushNodePathToFrontend(node);
    if (selectInUI)
        frontend->updateFocusedNode(id);
    return id;
}

void InspectorBackend::addNodesToSearchResult(const String& nodeIds)
{
    if (InspectorFrontend* frontend = inspectorFrontend())
        frontend->addNodesToSearchResult(nodeIds);
}

#if ENABLE(DATABASE)
Database* InspectorBackend::databaseForId(long databaseId)
{
    if (m_inspectorController)
        return m_inspectorController->databaseForId(databaseId);
    return 0;
}

void InspectorBackend::selectDatabase(Database* database)
{
    if (m_inspectorController)
        m_inspectorController->selectDatabase(database);
}

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
void InspectorBackend::selectDOMStorage(Storage* storage)
{
    if (m_inspectorController)
        m_inspectorController->selectDOMStorage(storage);
}

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

void InspectorBackend::didEvaluateForTestInFrontend(long callId, const String& jsonResult)
{
    if (m_inspectorController)
        m_inspectorController->didEvaluateForTestInFrontend(callId, jsonResult);
}

void InspectorBackend::reportDidDispatchOnInjectedScript(long callId, const String& result, bool isException)
{
    if (InspectorFrontend* frontend = inspectorFrontend())
        frontend->didDispatchOnInjectedScript(callId, result, isException);
}

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

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
