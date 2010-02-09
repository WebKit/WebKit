/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @fileoverview DevTools' implementation of the InspectorController API.
 */

if (!this.devtools)
    devtools = {};

devtools.InspectorBackendImpl = function()
{
    WebInspector.InspectorBackendStub.call(this);
    this.installInspectorControllerDelegate_("clearMessages");
    this.installInspectorControllerDelegate_("copyNode");
    this.installInspectorControllerDelegate_("deleteCookie");
    this.installInspectorControllerDelegate_("didEvaluateForTestInFrontend");
    this.installInspectorControllerDelegate_("disableResourceTracking");
    this.installInspectorControllerDelegate_("disableTimeline");
    this.installInspectorControllerDelegate_("enableResourceTracking");
    this.installInspectorControllerDelegate_("enableTimeline");
    this.installInspectorControllerDelegate_("getChildNodes");
    this.installInspectorControllerDelegate_("getCookies");
    this.installInspectorControllerDelegate_("getDatabaseTableNames");
    this.installInspectorControllerDelegate_("getDOMStorageEntries");
    this.installInspectorControllerDelegate_("getEventListenersForNode");
    this.installInspectorControllerDelegate_("getResourceContent");
    this.installInspectorControllerDelegate_("highlightDOMNode");
    this.installInspectorControllerDelegate_("hideDOMNodeHighlight");
    this.installInspectorControllerDelegate_("releaseWrapperObjectGroup");
    this.installInspectorControllerDelegate_("removeAttribute");
    this.installInspectorControllerDelegate_("removeDOMStorageItem");
    this.installInspectorControllerDelegate_("removeNode");
    this.installInspectorControllerDelegate_("saveFrontendSettings");
    this.installInspectorControllerDelegate_("setAttribute");
    this.installInspectorControllerDelegate_("setDOMStorageItem");
    this.installInspectorControllerDelegate_("setInjectedScriptSource");
    this.installInspectorControllerDelegate_("setTextNodeValue");
    this.installInspectorControllerDelegate_("startTimelineProfiler");
    this.installInspectorControllerDelegate_("stopTimelineProfiler");
    this.installInspectorControllerDelegate_("storeLastActivePanel");
};
devtools.InspectorBackendImpl.prototype.__proto__ = WebInspector.InspectorBackendStub.prototype;


/**
 * {@inheritDoc}.
 */
devtools.InspectorBackendImpl.prototype.toggleNodeSearch = function()
{
    WebInspector.InspectorBackendStub.prototype.toggleNodeSearch.call(this);
    this.callInspectorController_.call(this, "toggleNodeSearch");
    if (!this.searchingForNode()) {
        // This is called from ElementsPanel treeOutline's focusNodeChanged().
        DevToolsHost.activateWindow();
    }
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.debuggerEnabled = function()
{
    return true;
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.profilerEnabled = function()
{
    return true;
};


devtools.InspectorBackendImpl.prototype.addBreakpoint = function(sourceID, line, condition)
{
    devtools.tools.getDebuggerAgent().addBreakpoint(sourceID, line, condition);
};


devtools.InspectorBackendImpl.prototype.removeBreakpoint = function(sourceID, line)
{
    devtools.tools.getDebuggerAgent().removeBreakpoint(sourceID, line);
};

devtools.InspectorBackendImpl.prototype.updateBreakpoint = function(sourceID, line, condition)
{
    devtools.tools.getDebuggerAgent().updateBreakpoint(sourceID, line, condition);
};

devtools.InspectorBackendImpl.prototype.pauseInDebugger = function()
{
    devtools.tools.getDebuggerAgent().pauseExecution();
};


devtools.InspectorBackendImpl.prototype.resumeDebugger = function()
{
    devtools.tools.getDebuggerAgent().resumeExecution();
};


devtools.InspectorBackendImpl.prototype.stepIntoStatementInDebugger = function()
{
    devtools.tools.getDebuggerAgent().stepIntoStatement();
};


devtools.InspectorBackendImpl.prototype.stepOutOfFunctionInDebugger = function()
{
    devtools.tools.getDebuggerAgent().stepOutOfFunction();
};


devtools.InspectorBackendImpl.prototype.stepOverStatementInDebugger = function()
{
    devtools.tools.getDebuggerAgent().stepOverStatement();
};

/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.setPauseOnExceptionsState = function(state)
{
    this._setPauseOnExceptionsState = state;
    // TODO(yurys): support all three states. See http://crbug.com/32877
    var enabled = (state !== WebInspector.ScriptsPanel.PauseOnExceptionsState.DontPauseOnExceptions);
    return devtools.tools.getDebuggerAgent().setPauseOnExceptions(enabled);
};

/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.pauseOnExceptionsState = function()
{
    return (this._setPauseOnExceptionsState || WebInspector.ScriptsPanel.PauseOnExceptionsState.DontPauseOnExceptions);
};

/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.pauseOnExceptions = function()
{
    return devtools.tools.getDebuggerAgent().pauseOnExceptions();
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.setPauseOnExceptions = function(value)
{
    return devtools.tools.getDebuggerAgent().setPauseOnExceptions(value);
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.startProfiling = function()
{
    devtools.tools.getProfilerAgent().startProfiling(devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_CPU);
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.stopProfiling = function()
{
    devtools.tools.getProfilerAgent().stopProfiling( devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_CPU);
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.getProfileHeaders = function(callId)
{
    WebInspector.didGetProfileHeaders(callId, []);
};


/**
 * Emulate WebKit InspectorController behavior. It stores profiles on renderer side,
 * and is able to retrieve them by uid using "getProfile".
 */
devtools.InspectorBackendImpl.prototype.addFullProfile = function(profile)
{
    WebInspector.__fullProfiles = WebInspector.__fullProfiles || {};
    WebInspector.__fullProfiles[profile.uid] = profile;
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.getProfile = function(callId, uid)
{
    if (WebInspector.__fullProfiles && (uid in WebInspector.__fullProfiles))
        WebInspector.didGetProfile(callId, WebInspector.__fullProfiles[uid]);
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.takeHeapSnapshot = function()
{
    devtools.tools.getProfilerAgent().startProfiling(devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_HEAP_SNAPSHOT
        | devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_HEAP_STATS
        | devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_JS_CONSTRUCTORS);
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.dispatchOnInjectedScript = function(callId, injectedScriptId, methodName, argsString, async)
{
    // Encode injectedScriptId into callId
    if (typeof injectedScriptId !== "number")
        injectedScriptId = 0;
    RemoteToolsAgent.dispatchOnInjectedScript(callId, injectedScriptId, methodName, argsString, async);
};


/**
 * Installs delegating handler into the inspector controller.
 * @param {string} methodName Method to install delegating handler for.
 */
devtools.InspectorBackendImpl.prototype.installInspectorControllerDelegate_ = function(methodName)
{
    this[methodName] = this.callInspectorController_.bind(this, methodName);
};


/**
 * Bound function with the installInjectedScriptDelegate_ actual
 * implementation.
 */
devtools.InspectorBackendImpl.prototype.callInspectorController_ = function(methodName, var_arg)
{
    var args = Array.prototype.slice.call(arguments, 1);
    RemoteToolsAgent.dispatchOnInspectorController(WebInspector.Callback.wrap(function(){}), methodName, JSON.stringify(args));
};


InspectorBackend = new devtools.InspectorBackendImpl();
