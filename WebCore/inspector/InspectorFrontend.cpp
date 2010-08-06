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

#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorWorkerResource.h"
#include "ScriptFunctionCall.h"
#include "ScriptState.h"

namespace WebCore {

InspectorFrontend::InspectorFrontend(ScriptObject webInspector, InspectorClient* inspectorClient)
    : m_webInspector(webInspector)
    , m_inspectorClient(inspectorClient)
{
}

InspectorFrontend::~InspectorFrontend() 
{
    m_webInspector = ScriptObject();
}

void InspectorFrontend::close()
{
    ScriptFunctionCall function(m_webInspector, "close");
    function.call();
}

void InspectorFrontend::inspectedPageDestroyed()
{
    ScriptFunctionCall function(m_webInspector, "inspectedPageDestroyed");
    function.call();
}

void InspectorFrontend::didCommitLoad()
{
    callSimpleFunction("didCommitLoad");
}

void InspectorFrontend::populateApplicationSettings(const String& settings)
{
    ScriptFunctionCall function(m_webInspector, "dispatch");
    function.appendArgument("populateApplicationSettings");
    function.appendArgument(settings);
    function.call();
}

void InspectorFrontend::populateSessionSettings(const String& settings)
{
    ScriptFunctionCall function(m_webInspector, "dispatch");
    function.appendArgument("populateSessionSettings");
    function.appendArgument(settings);
    function.call();
}

void InspectorFrontend::updateFocusedNode(long nodeId)
{
    ScriptFunctionCall function(m_webInspector, "dispatch"); 
    function.appendArgument("updateFocusedNode");
    function.appendArgument(nodeId);
    function.call();
}

void InspectorFrontend::showPanel(int panel)
{
    const char* showFunctionName;
    switch (panel) {
        case InspectorController::AuditsPanel:
            showFunctionName = "showAuditsPanel";
            break;
        case InspectorController::ConsolePanel:
            showFunctionName = "showConsolePanel";
            break;
        case InspectorController::ElementsPanel:
            showFunctionName = "showElementsPanel";
            break;
        case InspectorController::ResourcesPanel:
            showFunctionName = "showResourcesPanel";
            break;
        case InspectorController::TimelinePanel:
            showFunctionName = "showTimelinePanel";
            break;
        case InspectorController::ProfilesPanel:
            showFunctionName = "showProfilesPanel";
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

void InspectorFrontend::resetProfilesPanel()
{
    callSimpleFunction("resetProfilesPanel");
}

void InspectorFrontend::bringToFront()
{
    callSimpleFunction("bringToFront");
}

void InspectorFrontend::inspectedURLChanged(const String& url)
{
    ScriptFunctionCall function(m_webInspector, "dispatch");
    function.appendArgument("inspectedURLChanged");
    function.appendArgument(url);
    function.call();
}

void InspectorFrontend::resourceTrackingWasEnabled()
{
    callSimpleFunction("resourceTrackingWasEnabled");
}

void InspectorFrontend::resourceTrackingWasDisabled()
{
    callSimpleFunction("resourceTrackingWasDisabled");
}

void InspectorFrontend::searchingForNodeWasEnabled()
{
    callSimpleFunction("searchingForNodeWasEnabled");
}

void InspectorFrontend::searchingForNodeWasDisabled()
{
    callSimpleFunction("searchingForNodeWasDisabled");
}

void InspectorFrontend::monitoringXHRWasEnabled()
{
    callSimpleFunction("monitoringXHRWasEnabled");
}

void InspectorFrontend::monitoringXHRWasDisabled()
{
    callSimpleFunction("monitoringXHRWasDisabled");
}

void InspectorFrontend::updatePauseOnExceptionsState(long state)
{
    ScriptFunctionCall function(m_webInspector, "dispatch"); 
    function.appendArgument("updatePauseOnExceptionsState");
    function.appendArgument(state);
    function.call();
}

void InspectorFrontend::timelineProfilerWasStarted()
{
    callSimpleFunction("timelineProfilerWasStarted");
}

void InspectorFrontend::timelineProfilerWasStopped()
{
    callSimpleFunction("timelineProfilerWasStopped");
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

void InspectorFrontend::setRecordingProfile(bool isProfiling)
{
    ScriptFunctionCall function(m_webInspector, "dispatch"); 
    function.appendArgument("setRecordingProfile");
    function.appendArgument(isProfiling);
    function.call();
}
#endif

void InspectorFrontend::didPushNodeByPathToFrontend(long callId, long nodeId)
{
    ScriptFunctionCall function(m_webInspector, "dispatch"); 
    function.appendArgument("didPushNodeByPathToFrontend");
    function.appendArgument(callId);
    function.appendArgument(nodeId);
    function.call();
}

#if ENABLE(WORKERS)
void InspectorFrontend::didCreateWorker(const InspectorWorkerResource& worker)
{
    ScriptFunctionCall function(m_webInspector, "dispatch");
    function.appendArgument("didCreateWorker");
    function.appendArgument(worker.id());
    function.appendArgument(worker.url());
    function.appendArgument(worker.isSharedWorker());
    function.call();
}

void InspectorFrontend::didDestroyWorker(const InspectorWorkerResource& worker)
{
    ScriptFunctionCall function(m_webInspector, "dispatch"); 
    function.appendArgument("didDestroyWorker");
    function.appendArgument(worker.id());
    function.call();
}
#endif // ENABLE(WORKERS)

void InspectorFrontend::contextMenuItemSelected(int itemId)
{
    ScriptFunctionCall function(m_webInspector, "dispatch");
    function.appendArgument("contextMenuItemSelected");
    function.appendArgument(itemId);
    function.call();
}

void InspectorFrontend::contextMenuCleared()
{
    callSimpleFunction("contextMenuCleared");
}

void InspectorFrontend::evaluateForTestInFrontend(long callId, const String& script)
{
    ScriptFunctionCall function(m_webInspector, "dispatch"); 
    function.appendArgument("evaluateForTestInFrontend");
    function.appendArgument(callId);
    function.appendArgument(script);
    function.call();
}

void InspectorFrontend::callSimpleFunction(const String& functionName)
{
    ScriptFunctionCall function(m_webInspector, "dispatch");
    function.appendArgument(functionName);
    function.call();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
