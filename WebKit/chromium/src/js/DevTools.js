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
 * FIXME: change field naming style to use trailing underscore.
 * @fileoverview Tools is a main class that wires all components of the
 * DevTools frontend together. It is also responsible for overriding existing
 * WebInspector functionality while it is getting upstreamed into WebCore.
 */

/**
 * Dispatches raw message from the host.
 * @param {string} remoteName
 * @prama {string} methodName
 * @param {string} param1, param2, param3 Arguments to dispatch.
 */
devtools$$dispatch = function(remoteName, methodName, param1, param2, param3)
{
    remoteName = "Remote" + remoteName.substring(0, remoteName.length - 8);
    var agent = window[remoteName];
    if (!agent) {
        debugPrint("No remote agent '" + remoteName + "' found.");
        return;
    }
    var method = agent[methodName];
    if (!method) {
        debugPrint("No method '" + remoteName + "." + methodName + "' found.");
        return;
    }
    method.call(this, param1, param2, param3);
};


devtools.ToolsAgent = function()
{
    RemoteToolsAgent.didDispatchOn = WebInspector.Callback.processCallback;
    RemoteToolsAgent.dispatchOnClient = this.dispatchOnClient_.bind(this);
    this.debuggerAgent_ = new devtools.DebuggerAgent();
    this.profilerAgent_ = new devtools.ProfilerAgent();
};


/**
 * Resets tools agent to its initial state.
 */
devtools.ToolsAgent.prototype.reset = function()
{
    this.debuggerAgent_.reset();
};


/**
 * @param {string} script Script exression to be evaluated in the context of the
 *     inspected page.
 * @param {function(Object|string, boolean):undefined} opt_callback Function to
 *     call with the result.
 */
devtools.ToolsAgent.prototype.evaluateJavaScript = function(script, opt_callback)
{
    InspectorBackend.evaluate(script, opt_callback || function() {});
};


/**
 * @return {devtools.DebuggerAgent} Debugger agent instance.
 */
devtools.ToolsAgent.prototype.getDebuggerAgent = function()
{
    return this.debuggerAgent_;
};


/**
 * @return {devtools.ProfilerAgent} Profiler agent instance.
 */
devtools.ToolsAgent.prototype.getProfilerAgent = function()
{
    return this.profilerAgent_;
};


(function () {
var orig = WebInspector.reset;
WebInspector.reset = function()
{
    devtools.tools.reset();
    orig.call(this);
};
})();


/**
 * @param {string} message Serialized call to be dispatched on WebInspector.
 * @private
 */
devtools.ToolsAgent.prototype.dispatchOnClient_ = function(message)
{
    var args = JSON.parse(message);
    var methodName = args[0];
    var parameters = args.slice(1);
    WebInspector[methodName].apply(WebInspector, parameters);
};


/**
 * Evaluates js expression.
 * @param {string} expr
 */
devtools.ToolsAgent.prototype.evaluate = function(expr)
{
    RemoteToolsAgent.evaluate(expr);
};


/**
 * Prints string  to the inspector console or shows alert if the console doesn't
 * exist.
 * @param {string} text
 */
function debugPrint(text) {
    WebInspector.log(text);
}


/**
 * Global instance of the tools agent.
 * @type {devtools.ToolsAgent}
 */
devtools.tools = null;


var context = {};  // Used by WebCore's inspector routines.

///////////////////////////////////////////////////////////////////////////////
// Here and below are overrides to existing WebInspector methods only.
// TODO(pfeldman): Patch WebCore and upstream changes.
var oldLoaded = WebInspector.loaded;
WebInspector.loaded = function()
{
    devtools.tools = new devtools.ToolsAgent();
    devtools.tools.reset();

    Preferences.ignoreWhitespace = false;
    Preferences.samplingCPUProfiler = true;
    Preferences.heapProfilerPresent = true;
    Preferences.debuggerAlwaysEnabled = true;
    Preferences.profilerAlwaysEnabled = true;
    Preferences.canEditScriptSource = true;

    oldLoaded.call(this);

    InspectorFrontendHost.loaded();
};

devtools.domContentLoaded = function()
{
    var queryParams = window.location.search;
    if (queryParams) {
        var params = queryParams.substring(1).split("&");
        var paramsObject = {};
        for (var i = 0; i < params.length; ++i) {
            var pair = params[i].split("=");
            paramsObject[pair[0]] = pair[1];
        }
        WebInspector.setAttachedWindow(paramsObject.docked === "true");
        if (paramsObject.toolbar_color && paramsObject.text_color)
            WebInspector.setToolbarColors(paramsObject.toolbar_color, paramsObject.text_color);
    }
}
document.addEventListener("DOMContentLoaded", devtools.domContentLoaded, false);


if (!window.v8ScriptDebugServerEnabled) {

(function()
{
    var oldShow = WebInspector.ScriptsPanel.prototype.show;
    WebInspector.ScriptsPanel.prototype.show =  function()
    {
        devtools.tools.getDebuggerAgent().initUI();
        this.enableToggleButton.visible = false;
        oldShow.call(this);
    };
})();


(function () {
var orig = InjectedScriptAccess.prototype.getProperties;
InjectedScriptAccess.prototype.getProperties = function(objectProxy, ignoreHasOwnProperty, abbreviate, callback)
{
    if (objectProxy.isScope)
        devtools.tools.getDebuggerAgent().resolveScope(objectProxy.objectId, callback);
    else if (objectProxy.isV8Ref)
        devtools.tools.getDebuggerAgent().resolveChildren(objectProxy.objectId, callback, false);
    else
        orig.apply(this, arguments);
};
})();


(function()
{
InjectedScriptAccess.prototype.evaluateInCallFrame = function(callFrameId, code, objectGroup, callback)
{
    //TODO(pfeldman): remove once 49084 is rolled.
    if (!callback)
        callback = objectGroup;
    devtools.tools.getDebuggerAgent().evaluateInCallFrame(callFrameId, code, callback);
};
})();


(function()
{
var orig = InjectedScriptAccess.prototype.getCompletions;
InjectedScriptAccess.prototype.getCompletions = function(expressionString, includeInspectorCommandLineAPI, callFrameId, reportCompletions)
{
    if (typeof callFrameId === "number")
        devtools.tools.getDebuggerAgent().resolveCompletionsOnFrame(expressionString, callFrameId, reportCompletions);
    else
        return orig.apply(this, arguments);
};
})();

// Highlight extension content scripts in the scripts list.
(function () {
    var original = WebInspector.ScriptsPanel.prototype._addScriptToFilesMenu;
    WebInspector.ScriptsPanel.prototype._addScriptToFilesMenu = function(script)
    {
        var result = original.apply(this, arguments);
        var debuggerAgent = devtools.tools.getDebuggerAgent();
        var type = debuggerAgent.getScriptContextType(script.sourceID);
        var option = script.filesSelectOption;
        if (type === "injected" && option)
            option.addStyleClass("injected");
        return result;
    };
})();

}


(function InterceptProfilesPanelEvents()
{
    var oldShow = WebInspector.ProfilesPanel.prototype.show;
    WebInspector.ProfilesPanel.prototype.show = function()
    {
        devtools.tools.getProfilerAgent().initializeProfiling();
        this.enableToggleButton.visible = false;
        oldShow.call(this);
        // Show is called on every show event of a panel, so
        // we only need to intercept it once.
        WebInspector.ProfilesPanel.prototype.show = oldShow;
    };
})();


/*
 * @override
 * TODO(mnaganov): Restore l10n when it will be agreed that it is needed.
 */
WebInspector.UIString = function(string)
{
    return String.vsprintf(string, Array.prototype.slice.call(arguments, 1));
};


/** Pending WebKit upstream by apavlov). Fixes iframe vs drag problem. */
(function()
{
    var originalDragStart = WebInspector.elementDragStart;
    WebInspector.elementDragStart = function(element)
    {
        if (element) {
            var glassPane = document.createElement("div");
            glassPane.style.cssText = "position:absolute;width:100%;height:100%;opacity:0;z-index:1";
            glassPane.id = "glass-pane-for-drag";
            element.parentElement.appendChild(glassPane);
        }

        originalDragStart.apply(this, arguments);
    };

    var originalDragEnd = WebInspector.elementDragEnd;
    WebInspector.elementDragEnd = function()
    {
        originalDragEnd.apply(this, arguments);

        var glassPane = document.getElementById("glass-pane-for-drag");
        if (glassPane)
            glassPane.parentElement.removeChild(glassPane);
    };
})();



///////////////////////////////////////////
// Chromium layout test harness support. //
///////////////////////////////////////////

WebInspector.runAfterPendingDispatchesQueue = [];

WebInspector.TestController.prototype.runAfterPendingDispatches = function(callback)
{
    WebInspector.runAfterPendingDispatchesQueue.push(callback);
};

WebInspector.queuesAreEmpty = function()
{
    var copy = this.runAfterPendingDispatchesQueue.slice();
    this.runAfterPendingDispatchesQueue = [];
    for (var i = 0; i < copy.length; ++i)
        copy[i].call(this);
};


/////////////////////////////
// Chromium theme support. //
/////////////////////////////

WebInspector.setToolbarColors = function(backgroundColor, color)
{
    if (!WebInspector._themeStyleElement) {
        WebInspector._themeStyleElement = document.createElement("style");
        document.head.appendChild(WebInspector._themeStyleElement);
    }
    WebInspector._themeStyleElement.textContent =
        "#toolbar {\
             background-image: none !important;\
             background-color: " + backgroundColor + " !important;\
         }\
         \
         .toolbar-label {\
             color: " + color + " !important;\
             text-shadow: none;\
         }";
}

WebInspector.resetToolbarColors = function()
{
    if (WebInspector._themeStyleElement)
        WebInspector._themeStyleElement.textContent = "";

}
