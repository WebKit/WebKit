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

namespace WebCore {
    class ConsoleMessage;
    class Database;
    class Frame;
    class InspectorResource;
    class Node;
    class ScriptString;
    class SerializedScriptValue;
    class Storage;
    class InspectorWorkerResource;

    class InspectorFrontend : public Noncopyable {
    public:
        InspectorFrontend(ScriptObject webInspector);
        ~InspectorFrontend();
        
        void close();
        void inspectedPageDestroyed();

        ScriptArray newScriptArray();
        ScriptObject newScriptObject();

        void didCommitLoad();

        void populateFrontendSettings(const String& settings);

        void updateConsoleMessageExpiredCount(unsigned count);
        void addConsoleMessage(const ScriptObject& messageObj, const Vector<ScriptString>& frames, const Vector<RefPtr<SerializedScriptValue> >& arguments, const String& message);
        void updateConsoleMessageRepeatCount(unsigned count);
        void clearConsoleMessages();

        bool updateResource(unsigned long identifier, const ScriptObject& resourceObj);
        void removeResource(unsigned long identifier);
        void didGetResourceContent(long callId, const String& content);

        void updateFocusedNode(long nodeId);
        void setAttachedWindow(bool attached);
        void showPanel(int panel);
        void populateInterface();
        void reset();
        
        void bringToFront();
        void inspectedURLChanged(const String&);

        void resourceTrackingWasEnabled();
        void resourceTrackingWasDisabled();

        void searchingForNodeWasEnabled();
        void searchingForNodeWasDisabled();

        void updatePauseOnExceptionsState(long state);

#if ENABLE(JAVASCRIPT_DEBUGGER)
        void attachDebuggerWhenShown();
        void debuggerWasEnabled();
        void debuggerWasDisabled();

        void parsedScriptSource(const String& sourceID, const String& url, const String& data, int firstLine);
        void restoredBreakpoint(const String& sourceID, const String& url, int line, bool enabled, const String& condition);
        void failedToParseScriptSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage);
        void pausedScript(SerializedScriptValue* callFrames);
        void resumedScript();

        void profilerWasEnabled();
        void profilerWasDisabled();
        void addProfileHeader(const ScriptValue& profile);
        void setRecordingProfile(bool isProfiling);
        void didGetProfileHeaders(long callId, const ScriptArray& headers);
        void didGetProfile(long callId, const ScriptValue& profile);
#endif

#if ENABLE(DATABASE)
        bool addDatabase(const ScriptObject& dbObj);
        void selectDatabase(int databaseId);
        void didGetDatabaseTableNames(long callId, const ScriptArray& tableNames);
#endif
        
#if ENABLE(DOM_STORAGE)
        bool addDOMStorage(const ScriptObject& domStorageObj);
        void selectDOMStorage(long storageId);
        void didGetDOMStorageEntries(long callId, const ScriptArray& entries);
        void didSetDOMStorageItem(long callId, bool success);
        void didRemoveDOMStorageItem(long callId, bool success);
        void updateDOMStorage(long storageId);
#endif

        void setDocument(const ScriptObject& root);
        void setDetachedRoot(const ScriptObject& root);
        void setChildNodes(long parentId, const ScriptArray& nodes);
        void childNodeCountUpdated(long id, int newValue);
        void childNodeInserted(long parentId, long prevId, const ScriptObject& node);
        void childNodeRemoved(long parentId, long id);
        void attributesUpdated(long id, const ScriptArray& attributes);
        void didGetChildNodes(long callId);
        void didApplyDomChange(long callId, bool success);
        void didGetEventListenersForNode(long callId, long nodeId, const ScriptArray& listenersArray);
        void didRemoveNode(long callId, long nodeId);
        void didChangeTagName(long callId, long nodeId);

        void didGetStyles(long callId, const ScriptValue& styles);
        void didGetAllStyles(long callId, const ScriptArray& styles);
        void didGetInlineStyle(long callId, const ScriptValue& style);
        void didGetComputedStyle(long callId, const ScriptValue& style);
        void didApplyStyleText(long callId, bool success, const ScriptValue& style, const ScriptArray& changedProperties);
        void didSetStyleText(long callId, bool success);
        void didSetStyleProperty(long callId, bool success);
        void didToggleStyleEnabled(long callId, const ScriptValue& style);
        void didSetRuleSelector(long callId, const ScriptValue& rule, bool selectorAffectsNode);
        void didAddRule(long callId, const ScriptValue& rule, bool selectorAffectsNode);

        void timelineProfilerWasStarted();
        void timelineProfilerWasStopped();
        void addRecordToTimeline(const ScriptObject&);

#if ENABLE(WORKERS)
        void didCreateWorker(const InspectorWorkerResource&);
        void didDestroyWorker(const InspectorWorkerResource&);
#endif // ENABLE(WORKER)

        void didGetCookies(long callId, const ScriptArray& cookies, const String& cookiesString);
        void didDispatchOnInjectedScript(long callId, SerializedScriptValue* result, bool isException);

        void addNodesToSearchResult(const String& nodeIds);

        void contextMenuItemSelected(int itemId);
        void contextMenuCleared();

        ScriptState* scriptState() const { return m_webInspector.scriptState(); }

        void evaluateForTestInFrontend(long callId, const String& script);
    private:
        void callSimpleFunction(const String& functionName);
        ScriptObject m_webInspector;
    };

} // namespace WebCore

#endif // !defined(InspectorFrontend_h)
