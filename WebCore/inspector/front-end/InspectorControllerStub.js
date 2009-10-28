/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

if (!window.InspectorController) {

WebInspector.InspectorControllerStub = function()
{
    this._searchingForNode = false;
    this._windowVisible = true;
    this._attachedWindowHeight = 0;
    this._debuggerEnabled = true;
    this._profilerEnabled = true;
    this._resourceTrackingEnabled = false;
    this._timelineEnabled = false;
    this._settings = {};
}

WebInspector.InspectorControllerStub.prototype = {
    wrapCallback: function(func)
    {
        return func;
    },

    isWindowVisible: function()
    {
        return this._windowVisible;
    },

    platform: function()
    {
        return "mac-leopard";
    },


    closeWindow: function()
    {
        this._windowVisible = false;
    },

    attach: function()
    {
    },

    detach: function()
    {
    },

    storeLastActivePanel: function(panel)
    {
    },

    clearMessages: function()
    {
    },

    searchingForNode: function()
    {
        return this._searchingForNode;
    },

    search: function(sourceRow, query)
    {
    },

    toggleNodeSearch: function()
    {
        this._searchingForNode = !this._searchingForNode;
    },

    setAttachedWindowHeight: function(height)
    {
    },

    moveByUnrestricted: function(x, y)
    {
    },


    addResourceSourceToFrame: function(identifier, element)
    {
    },

    addSourceToFrame: function(mimeType, source, element)
    {
        return false;
    },

    getResourceDocumentNode: function(identifier)
    {
        return undefined;
    },

    highlightDOMNode: function(node)
    {
    },

    hideDOMNodeHighlight: function()
    {
    },

    inspectedWindow: function()
    {
        return window;
    },

    loaded: function()
    {
    },

    localizedStringsURL: function()
    {
        return undefined;
    },

    windowUnloading: function()
    {
        return false;
    },

    hiddenPanels: function()
    {
        return "";
    },

    debuggerEnabled: function()
    {
        return this._debuggerEnabled;
    },

    enableResourceTracking: function()
    {
        this._resourceTrackingEnabled = true;
        WebInspector.resourceTrackingWasEnabled();
    },

    disableResourceTracking: function()
    {
        this._resourceTrackingEnabled = false;
        WebInspector.resourceTrackingWasDisabled();
    },

    resourceTrackingEnabled: function()
    {
        return this._resourceTrackingEnabled;
    },

    enableDebugger: function()
    {
        this._debuggerEnabled = true;
    },

    disableDebugger: function()
    {
        this._debuggerEnabled = false;
    },

    addBreakpoint: function(sourceID, line, condition)
    {
    },

    removeBreakpoint: function(sourceID, line)
    {
    },

    updateBreakpoint: function(sourceID, line, condition)
    {
    },

    pauseInDebugger: function()
    {
    },

    pauseOnExceptions: function()
    {
        return false;
    },

    setPauseOnExceptions: function(value)
    {
    },

    resumeDebugger: function()
    {
    },

    profilerEnabled: function()
    {
        return true;
    },

    enableProfiler: function()
    {
        this._profilerEnabled = true;
    },

    disableProfiler: function()
    {
        this._profilerEnabled = false;
    },

    startProfiling: function()
    {
    },

    stopProfiling: function()
    {
    },

    getProfileHeaders: function(callId)
    {
        WebInspector.didGetProfileHeaders(callId, []);
    },

    getProfile: function(callId, uid)
    {
        if (WebInspector.__fullProfiles && (uid in WebInspector.__fullProfiles))
        {
            WebInspector.didGetProfile(callId, WebInspector.__fullProfiles[uid]);
        }
    },

    takeHeapSnapshot: function()
    {
    },

    databaseTableNames: function(database)
    {
        return [];
    },

    stepIntoStatementInDebugger: function()
    {
    },

    stepOutOfFunctionInDebugger: function()
    {
    },

    stepOverStatementInDebugger: function()
    {
    },

    setSetting: function(setting, value)
    {
        this._settings[setting] = value;
    },

    dispatchOnInjectedScript: function()
    {
    },

    releaseWrapperObjectGroup: function()
    {
    },

    setting: function(setting)
    {
        return this._settings[setting];
    }
}

window.InspectorController = new WebInspector.InspectorControllerStub();

}
