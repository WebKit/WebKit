/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef InspectorBackend_h
#define InspectorBackend_h

#include "Console.h"
#include "InspectorController.h"
#include "PlatformString.h"

#include <wtf/RefCounted.h>

namespace WebCore {

class CachedResource;
class Database;
class InspectorClient;
class InspectorDOMAgent;
class JavaScriptCallFrame;
class Node;
class Storage;

class InspectorBackend : public RefCounted<InspectorBackend>
{
public:
    static PassRefPtr<InspectorBackend> create(InspectorController* inspectorController, InspectorClient* client)
    {
        return adoptRef(new InspectorBackend(inspectorController, client));
    }

    ~InspectorBackend();

    InspectorController* inspectorController() { return m_inspectorController; }
    
    void disconnectController() { m_inspectorController = 0; }

    void hideDOMNodeHighlight();

    String localizedStringsURL();
    String hiddenPanels();

    void windowUnloading();

    bool isWindowVisible();

    void addResourceSourceToFrame(long identifier, Node* frame);
    bool addSourceToFrame(const String& mimeType, const String& source, Node* frame);

    void clearMessages(bool clearUI);

    void toggleNodeSearch();

    void attach();
    void detach();

    void setAttachedWindowHeight(unsigned height);

    void storeLastActivePanel(const String& panelName);

    bool searchingForNode();

    void loaded();

    void enableResourceTracking(bool always);
    void disableResourceTracking(bool always);
    bool resourceTrackingEnabled() const;

    void moveWindowBy(float x, float y) const;
    void closeWindow();

    const String& platform() const;

    void enableTimeline(bool always);
    void disableTimeline(bool always);
    bool timelineEnabled() const;

#if ENABLE(JAVASCRIPT_DEBUGGER)
    const ProfilesArray& profiles() const;

    void startProfiling();
    void stopProfiling();

    void enableProfiler(bool always);
    void disableProfiler(bool always);
    bool profilerEnabled();

    void enableDebugger(bool always);
    void disableDebugger(bool always);
    bool debuggerEnabled() const;

    JavaScriptCallFrame* currentCallFrame() const;

    void addBreakpoint(const String& sourceID, unsigned lineNumber, const String& condition);
    void updateBreakpoint(const String& sourceID, unsigned lineNumber, const String& condition);
    void removeBreakpoint(const String& sourceID, unsigned lineNumber);

    bool pauseOnExceptions();
    void setPauseOnExceptions(bool pause);

    void pauseInDebugger();
    void resumeDebugger();

    void stepOverStatementInDebugger();
    void stepIntoStatementInDebugger();
    void stepOutOfFunctionInDebugger();
#endif

    void dispatchOnInjectedScript(long callId, const String& methodName, const String& arguments, bool async);
    void getChildNodes(long callId, long nodeId);
    void setAttribute(long callId, long elementId, const String& name, const String& value);
    void removeAttribute(long callId, long elementId, const String& name);
    void setTextNodeValue(long callId, long nodeId, const String& value);
    void getEventListenersForNode(long callId, long nodeId);
    void copyNode(long nodeId);
    void removeNode(long callId, long nodeId);

    void getCookies(long callId, const String& domain);
    void deleteCookie(const String& cookieName, const String& domain);

    // Generic code called from custom implementations.
    void highlight(long nodeId);
    Node* nodeForId(long nodeId);
    ScriptValue wrapObject(const ScriptValue& object, const String& objectGroup);
    ScriptValue unwrapObject(const String& objectId);
    void releaseWrapperObjectGroup(const String& objectGroup);
    long pushNodePathToFrontend(Node* node, bool selectInUI);
    void addNodesToSearchResult(const String& nodeIds);
#if ENABLE(DATABASE)
    Database* databaseForId(long databaseId);
    void selectDatabase(Database* database);
    void getDatabaseTableNames(long callId, long databaseId);
#endif
#if ENABLE(DOM_STORAGE)
    void selectDOMStorage(Storage* storage);
    void getDOMStorageEntries(long callId, long storageId);
    void setDOMStorageItem(long callId, long storageId, const String& key, const String& value);
    void removeDOMStorageItem(long callId, long storageId, const String& key);
#endif
    void reportDidDispatchOnInjectedScript(long callId, const String& result, bool isException);
    void didEvaluateForTestInFrontend(long callId, const String& jsonResult);

private:
    InspectorBackend(InspectorController* inspectorController, InspectorClient* client);
    InspectorDOMAgent* inspectorDOMAgent();
    InspectorFrontend* inspectorFrontend();

    InspectorController* m_inspectorController;
    InspectorClient* m_client;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    ProfilesArray m_emptyProfiles;
#endif
};

} // namespace WebCore

#endif // !defined(InspectorBackend_h)
