/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef InspectorFrontend_h
#define InspectorFrontend_h

#include "ScriptArray.h"
#include "ScriptObject.h"
#include "ScriptState.h"
#include <wtf/PassOwnPtr.h>

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
namespace JSC {
    class JSValue;
    class SourceCode;
    class UString;
}
#endif

namespace WebCore {
    class ConsoleMessage;
    class Database;
    class Frame;
    class InspectorController;
    class InspectorResource;
    class Node;
    class ScriptString;
    class SerializedScriptValue;
    class Storage;

    class InspectorFrontend : public Noncopyable {
    public:
        InspectorFrontend(InspectorController* inspectorController, ScriptObject webInspector);
        ~InspectorFrontend();

        ScriptArray newScriptArray();
        ScriptObject newScriptObject();

        void didCommitLoad();

        void populateFrontendSettings(const String& settings);

        void updateConsoleMessageExpiredCount(unsigned count);
        void addConsoleMessage(const ScriptObject& messageObj, const Vector<ScriptString>& frames, ScriptState*, const Vector<ScriptValue> arguments, const String& message);
        void updateConsoleMessageRepeatCount(unsigned count);
        void clearConsoleMessages();

        bool updateResource(unsigned long identifier, const ScriptObject& resourceObj);
        void removeResource(unsigned long identifier);
        void didGetResourceContent(int callId, const String& content);

        void updateFocusedNode(long nodeId);
        void setAttachedWindow(bool attached);
        void showPanel(int panel);
        void populateInterface();
        void reset();

        void resourceTrackingWasEnabled();
        void resourceTrackingWasDisabled();

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
        void attachDebuggerWhenShown();
        void debuggerWasEnabled();
        void debuggerWasDisabled();
        void parsedScriptSource(const JSC::SourceCode&);
        void failedToParseScriptSource(const JSC::SourceCode&, int errorLine, const JSC::UString& errorMessage);
        void pausedScript(SerializedScriptValue* callFrames);
        void resumedScript();
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER)
        void profilerWasEnabled();
        void profilerWasDisabled();
        void addProfileHeader(const ScriptValue& profile);
        void setRecordingProfile(bool isProfiling);
        void didGetProfileHeaders(int callId, const ScriptArray& headers);
        void didGetProfile(int callId, const ScriptValue& profile);
#endif

#if ENABLE(DATABASE)
        bool addDatabase(const ScriptObject& dbObj);
        void selectDatabase(int databaseId);
        void didGetDatabaseTableNames(int callId, const ScriptArray& tableNames);
#endif
        
#if ENABLE(DOM_STORAGE)
        bool addDOMStorage(const ScriptObject& domStorageObj);
        void selectDOMStorage(int storageId);
        void didGetDOMStorageEntries(int callId, const ScriptArray& entries);
        void didSetDOMStorageItem(int callId, bool success);
        void didRemoveDOMStorageItem(int callId, bool success);
        void updateDOMStorage(int storageId);
#endif

        void setDocument(const ScriptObject& root);
        void setDetachedRoot(const ScriptObject& root);
        void setChildNodes(int parentId, const ScriptArray& nodes);
        void childNodeCountUpdated(int id, int newValue);
        void childNodeInserted(int parentId, int prevId, const ScriptObject& node);
        void childNodeRemoved(int parentId, int id);
        void attributesUpdated(int id, const ScriptArray& attributes);
        void didGetChildNodes(int callId);
        void didApplyDomChange(int callId, bool success);
        void didGetEventListenersForNode(int callId, int nodeId, ScriptArray& listenersArray);
        void didRemoveNode(int callId, int nodeId);

        void timelineProfilerWasStarted();
        void timelineProfilerWasStopped();
        void addRecordToTimeline(const ScriptObject&);

        void didGetCookies(int callId, const ScriptArray& cookies, const String& cookiesString);
        void didDispatchOnInjectedScript(int callId, SerializedScriptValue* result, bool isException);

        void addNodesToSearchResult(const String& nodeIds);

        void contextMenuItemSelected(int itemId);
        void contextMenuCleared();

        ScriptState* scriptState() const { return m_webInspector.scriptState(); }

        void evaluateForTestInFrontend(int callId, const String& script);
    private:
        void callSimpleFunction(const String& functionName);
        InspectorController* m_inspectorController;
        ScriptObject m_webInspector;
    };

} // namespace WebCore

#endif // !defined(InspectorFrontend_h)
