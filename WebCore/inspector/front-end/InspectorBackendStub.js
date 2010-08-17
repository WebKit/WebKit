/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

WebInspector.InspectorBackendStub = function()
{
    this._registerDelegate("addInspectedNode");
    this._registerDelegate("addScriptToEvaluateOnLoad");
    this._registerDelegate("changeTagName");
    this._registerDelegate("clearConsoleMessages");
    this._registerDelegate("copyNode");
    this._registerDelegate("deleteCookie");
    this._registerDelegate("didEvaluateForTestInFrontend");
    this._registerDelegate("disableMonitoringXHR");
    this._registerDelegate("disableResourceTracking");
    this._registerDelegate("disableSearchingForNode");
    this._registerDelegate("disableTimeline");
    this._registerDelegate("dispatchOnInjectedScript");
    this._registerDelegate("enableMonitoringXHR");
    this._registerDelegate("enableResourceTracking");
    this._registerDelegate("enableSearchingForNode");
    this._registerDelegate("enableTimeline");
    this._registerDelegate("getApplicationCaches");
    this._registerDelegate("getChildNodes");
    this._registerDelegate("getCookies");
    this._registerDelegate("getDatabaseTableNames");
    this._registerDelegate("executeSQL");
    this._registerDelegate("getDOMStorageEntries");
    this._registerDelegate("getEventListenersForNode");
    this._registerDelegate("getOuterHTML");
    this._registerDelegate("getProfile");
    this._registerDelegate("getProfileHeaders");
    this._registerDelegate("removeProfile");
    this._registerDelegate("clearProfiles");
    this._registerDelegate("getResourceContent");
    this._registerDelegate("highlightDOMNode");
    this._registerDelegate("hideDOMNodeHighlight");
    this._registerDelegate("performSearch");
    this._registerDelegate("pushNodeByPathToFrontend");
    this._registerDelegate("releaseWrapperObjectGroup");
    this._registerDelegate("removeAllScriptsToEvaluateOnLoad");
    this._registerDelegate("reloadPage");
    this._registerDelegate("removeAttribute");
    this._registerDelegate("removeDOMStorageItem");
    this._registerDelegate("removeNode");
    this._registerDelegate("saveApplicationSettings");
    this._registerDelegate("saveSessionSettings");
    this._registerDelegate("searchCanceled");
    this._registerDelegate("setAttribute");
    this._registerDelegate("setDOMStorageItem");
    this._registerDelegate("setInjectedScriptSource");
    this._registerDelegate("setOuterHTML");
    this._registerDelegate("setTextNodeValue");
    this._registerDelegate("startProfiling");
    this._registerDelegate("startTimelineProfiler");
    this._registerDelegate("stopProfiling");
    this._registerDelegate("stopTimelineProfiler");
    this._registerDelegate("storeLastActivePanel");
    this._registerDelegate("takeHeapSnapshot");
    this._registerDelegate("getProfilerLogLines");
    this._registerDelegate("openInInspectedWindow");

    this._registerDelegate("getAllStyles");
    this._registerDelegate("getStyles");
    this._registerDelegate("getComputedStyle");
    this._registerDelegate("getInlineStyle");
    this._registerDelegate("getStyleSheet");
    this._registerDelegate("getRuleRanges");
    this._registerDelegate("applyStyleText");
    this._registerDelegate("setStyleText");
    this._registerDelegate("setStyleProperty");
    this._registerDelegate("toggleStyleEnabled");
    this._registerDelegate("setRuleSelector");
    this._registerDelegate("addRule");

    this._registerDelegate("disableDebugger");
    this._registerDelegate("editScriptSource");
    this._registerDelegate("getScriptSource");
    this._registerDelegate("enableDebugger");
    this._registerDelegate("setBreakpoint");
    this._registerDelegate("removeBreakpoint");
    this._registerDelegate("activateBreakpoints");
    this._registerDelegate("deactivateBreakpoints");
    this._registerDelegate("pause");
    this._registerDelegate("resume");
    this._registerDelegate("stepIntoStatement");
    this._registerDelegate("stepOutOfFunction");
    this._registerDelegate("stepOverStatement");
    this._registerDelegate("setPauseOnExceptionsState");
}

WebInspector.InspectorBackendStub.prototype = {
    _registerDelegate: function(methodName)
    {
        this[methodName] = this.sendMessageToBackend.bind(this, methodName);
    },

    sendMessageToBackend: function()
    {
        var message = JSON.stringify(Array.prototype.slice.call(arguments));
        InspectorFrontendHost.sendMessageToBackend(message);
    }
}

InspectorBackend = new WebInspector.InspectorBackendStub();
