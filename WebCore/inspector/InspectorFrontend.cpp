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

#include "config.h"
#include "InspectorFrontend.h"

#if ENABLE(INSPECTOR)

#include "ConsoleMessage.h"
#include "Frame.h"
#include "InspectorController.h"
#include "Node.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"
#include "ScriptObjectQuarantine.h"
#include "ScriptState.h"
#include "ScriptString.h"
#include <wtf/OwnPtr.h>

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include <parser/SourceCode.h>
#include <runtime/JSValue.h>
#include <runtime/UString.h>
#endif

namespace WebCore {

InspectorFrontend::InspectorFrontend(InspectorController* inspectorController, ScriptState* scriptState, ScriptObject webInspector)
    : m_inspectorController(inspectorController)
    , m_scriptState(scriptState)
    , m_webInspector(webInspector)
{
}

InspectorFrontend::~InspectorFrontend() 
{
    m_webInspector = ScriptObject();
}

ScriptArray InspectorFrontend::newScriptArray()
{
    return ScriptArray::createNew(m_scriptState);
}

ScriptObject InspectorFrontend::newScriptObject()
{
    return ScriptObject::createNew(m_scriptState);
}

void InspectorFrontend::didCommitLoad()
{
    callSimpleFunction("didCommitLoad");
}

void InspectorFrontend::addMessageToConsole(const ScriptObject& messageObj, const Vector<ScriptString>& frames, const Vector<ScriptValue> wrappedArguments, const String& message)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addMessageToConsole"));
    function->appendArgument(messageObj);
    if (!frames.isEmpty()) {
        for (unsigned i = 0; i < frames.size(); ++i)
            function->appendArgument(frames[i]);
    } else if (!wrappedArguments.isEmpty()) {
        for (unsigned i = 0; i < wrappedArguments.size(); ++i)
            function->appendArgument(m_inspectorController->wrapObject(wrappedArguments[i], "console"));
    } else
        function->appendArgument(message);
    function->call();
}

void InspectorFrontend::clearConsoleMessages()
{
    callSimpleFunction("clearConsoleMessages");
}

bool InspectorFrontend::addResource(long long identifier, const ScriptObject& resourceObj)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addResource"));
    function->appendArgument(identifier);
    function->appendArgument(resourceObj);
    bool hadException = false;
    function->call(hadException);
    return !hadException;
}

bool InspectorFrontend::updateResource(long long identifier, const ScriptObject& resourceObj)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("updateResource"));
    function->appendArgument(identifier);
    function->appendArgument(resourceObj);
    bool hadException = false;
    function->call(hadException);
    return !hadException;
}

void InspectorFrontend::removeResource(long long identifier)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("removeResource"));
    function->appendArgument(identifier);
    function->call();
}

void InspectorFrontend::updateFocusedNode(long long nodeId)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("updateFocusedNode"));
    function->appendArgument(nodeId);
    function->call();
}

void InspectorFrontend::setAttachedWindow(bool attached)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("setAttachedWindow"));
    function->appendArgument(attached);
    function->call();
}

void InspectorFrontend::showPanel(int panel)
{
    const char* showFunctionName;
    switch (panel) {
        case InspectorController::ConsolePanel:
            showFunctionName = "showConsole";
            break;
        case InspectorController::ElementsPanel:
            showFunctionName = "showElementsPanel";
            break;
        case InspectorController::ProfilesPanel:
            showFunctionName = "showProfilesPanel";
            break;
        case InspectorController::ResourcesPanel:
            showFunctionName = "showResourcesPanel";
            break;
        case InspectorController::ScriptsPanel:
            showFunctionName = "showScriptsPanel";
            break;
        case InspectorController::StoragePanel:
            showFunctionName = "showStoragePanel";
            break;
        default:
            ASSERT_NOT_REACHED();
            showFunctionName = 0;
    }

    if (showFunctionName)
        callSimpleFunction(showFunctionName);
}

void InspectorFrontend::populateInterface()
{
    callSimpleFunction("populateInterface");
}

void InspectorFrontend::reset()
{
    callSimpleFunction("reset");
}

void InspectorFrontend::resourceTrackingWasEnabled()
{
    callSimpleFunction("resourceTrackingWasEnabled");
}

void InspectorFrontend::resourceTrackingWasDisabled()
{
    callSimpleFunction("resourceTrackingWasDisabled");
}

void InspectorFrontend::timelineWasEnabled()
{
    callSimpleFunction("timelineWasEnabled");
}

void InspectorFrontend::timelineWasDisabled()
{
    callSimpleFunction("timelineWasDisabled");
}

void InspectorFrontend::addItemToTimeline(const ScriptObject& itemObj)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addItemToTimeline"));
    function->appendArgument(itemObj);
    function->call();
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorFrontend::attachDebuggerWhenShown()
{
    callSimpleFunction("attachDebuggerWhenShown");
}

void InspectorFrontend::debuggerWasEnabled()
{
    callSimpleFunction("debuggerWasEnabled");
}

void InspectorFrontend::debuggerWasDisabled()
{
    callSimpleFunction("debuggerWasDisabled");
}

void InspectorFrontend::profilerWasEnabled()
{
    callSimpleFunction("profilerWasEnabled");
}

void InspectorFrontend::profilerWasDisabled()
{
    callSimpleFunction("profilerWasDisabled");
}

void InspectorFrontend::parsedScriptSource(const JSC::SourceCode& source)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("parsedScriptSource"));
    function->appendArgument(JSC::UString(JSC::UString::from(source.provider()->asID())));
    function->appendArgument(source.provider()->url());
    function->appendArgument(JSC::UString(source.data(), source.length()));
    function->appendArgument(source.firstLine());
    function->call();
}

void InspectorFrontend::failedToParseScriptSource(const JSC::SourceCode& source, int errorLine, const JSC::UString& errorMessage)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("failedToParseScriptSource"));
    function->appendArgument(source.provider()->url());
    function->appendArgument(JSC::UString(source.data(), source.length()));
    function->appendArgument(source.firstLine());
    function->appendArgument(errorLine);
    function->appendArgument(errorMessage);
    function->call();
}

void InspectorFrontend::addProfile(const JSC::JSValue& profile)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addProfile"));
    function->appendArgument(profile);
    function->call();
}

void InspectorFrontend::setRecordingProfile(bool isProfiling)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("setRecordingProfile"));
    function->appendArgument(isProfiling);
    function->call();
}

void InspectorFrontend::pausedScript(const ScriptValue& callFrames)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("pausedScript"));
    function->appendArgument(callFrames);
    function->call();
}

void InspectorFrontend::resumedScript()
{
    callSimpleFunction("resumedScript");
}
#endif

void InspectorFrontend::setDocument(const ScriptObject& root)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("setDocument"));
    function->appendArgument(root);
    function->call();
}

void InspectorFrontend::setDetachedRoot(const ScriptObject& root)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("setDetachedRoot"));
    function->appendArgument(root);
    function->call();
}

void InspectorFrontend::setChildNodes(int parentId, const ScriptArray& nodes)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("setChildNodes"));
    function->appendArgument(parentId);
    function->appendArgument(nodes);
    function->call();
}

void InspectorFrontend::childNodeCountUpdated(int id, int newValue)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("childNodeCountUpdated"));
    function->appendArgument(id);
    function->appendArgument(newValue);
    function->call();
}

void InspectorFrontend::childNodeInserted(int parentId, int prevId, const ScriptObject& node)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("childNodeInserted"));
    function->appendArgument(parentId);
    function->appendArgument(prevId);
    function->appendArgument(node);
    function->call();
}

void InspectorFrontend::childNodeRemoved(int parentId, int id)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("childNodeRemoved"));
    function->appendArgument(parentId);
    function->appendArgument(id);
    function->call();
}

void InspectorFrontend::attributesUpdated(int id, const ScriptArray& attributes)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("attributesUpdated"));
    function->appendArgument(id);
    function->appendArgument(attributes);
    function->call();
}

void InspectorFrontend::didGetChildNodes(int callId)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("didGetChildNodes"));
    function->appendArgument(callId);
    function->call();
}

void InspectorFrontend::didApplyDomChange(int callId, bool success)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("didApplyDomChange"));
    function->appendArgument(callId);
    function->appendArgument(success);
    function->call();
}

void InspectorFrontend::didGetEventListenersForNode(int callId, int nodeId, ScriptArray& listenersArray)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("didGetEventListenersForNode"));
    function->appendArgument(callId);
    function->appendArgument(nodeId);
    function->appendArgument(listenersArray);
    function->call();
}

void InspectorFrontend::didGetCookies(int callId, const ScriptArray& cookies, const String& cookiesString)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("didGetCookies"));
    function->appendArgument(callId);
    function->appendArgument(cookies);
    function->appendArgument(cookiesString);
    function->call();
}

void InspectorFrontend::didDispatchOnInjectedScript(int callId, const String& result, bool isException)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("didDispatchOnInjectedScript"));
    function->appendArgument(callId);
    function->appendArgument(result);
    function->appendArgument(isException);
    function->call();
}

#if ENABLE(DATABASE)
bool InspectorFrontend::addDatabase(const ScriptObject& dbObject)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addDatabase"));
    function->appendArgument(dbObject);
    bool hadException = false;
    function->call(hadException);
    return !hadException;
}

void InspectorFrontend::selectDatabase(int databaseId)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("selectDatabase"));
    function->appendArgument(databaseId);
    function->call();
}
void InspectorFrontend::didGetDatabaseTableNames(int callId, const ScriptArray& tableNames)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("didGetDatabaseTableNames"));
    function->appendArgument(callId);
    function->appendArgument(tableNames);
    function->call();
}
#endif

#if ENABLE(DOM_STORAGE)
bool InspectorFrontend::addDOMStorage(const ScriptObject& domStorageObj)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addDOMStorage"));
    function->appendArgument(domStorageObj);
    bool hadException = false;
    function->call(hadException);
    return !hadException;
}

void InspectorFrontend::selectDOMStorage(int storageId)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("selectDOMStorage"));
    function->appendArgument(storageId);
    function->call();
}

void InspectorFrontend::didGetDOMStorageEntries(int callId, const ScriptArray& entries)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("didGetDOMStorageEntries"));
    function->appendArgument(callId);
    function->appendArgument(entries);
    function->call();
}

void InspectorFrontend::didSetDOMStorageItem(int callId, bool success)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("didSetDOMStorageItem"));
    function->appendArgument(callId);
    function->appendArgument(success);
    function->call();
}

void InspectorFrontend::didRemoveDOMStorageItem(int callId, bool success)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("didRemoveDOMStorageItem"));
    function->appendArgument(callId);
    function->appendArgument(success);
    function->call();
}

void InspectorFrontend::updateDOMStorage(int storageId)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("updateDOMStorage"));
    function->appendArgument(storageId);
    function->call();
}
#endif

void InspectorFrontend::addNodesToSearchResult(const String& nodeIds)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addNodesToSearchResult"));
    function->appendArgument(nodeIds);
    function->call();
}

void InspectorFrontend::evaluateForTestInFrontend(int callId, const String& script)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("evaluateForTestInFrontend"));
    function->appendArgument(callId);
    function->appendArgument(script);
    function->call();
}

PassOwnPtr<ScriptFunctionCall> InspectorFrontend::newFunctionCall(const String& functionName)
{
    ScriptFunctionCall* function = new ScriptFunctionCall(m_scriptState, m_webInspector, "dispatch");
    function->appendArgument(functionName);
    return function;
}

void InspectorFrontend::callSimpleFunction(const String& functionName)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "dispatch");
    function.appendArgument(functionName);
    function.call();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
