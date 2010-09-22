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

var context = {};  // Used by WebCore's inspector routines.

(function () {
    Preferences.ignoreWhitespace = false;
    Preferences.samplingCPUProfiler = true;
    Preferences.heapProfilerPresent = true;
    Preferences.debuggerAlwaysEnabled = true;
    Preferences.profilerAlwaysEnabled = true;
    Preferences.canEditScriptSource = true;
    Preferences.onlineDetectionEnabled = false;
    Preferences.nativeInstrumentationEnabled = true;
})();

var devtools = devtools || {};

devtools.domContentLoaded = function()
{
    WebInspector.setAttachedWindow(WebInspector.queryParamsObject.docked === "true");
    if (WebInspector.queryParamsObject.toolbar_color && WebInspector.queryParamsObject.text_color)
        WebInspector.setToolbarColors(WebInspector.queryParamsObject.toolbar_color, WebInspector.queryParamsObject.text_color);
}
document.addEventListener("DOMContentLoaded", devtools.domContentLoaded, false);


// FIXME: This needs to be upstreamed.
(function InterceptProfilesPanelEvents()
{
    var oldShow = WebInspector.ProfilesPanel.prototype.show;
    WebInspector.ProfilesPanel.prototype.show = function()
    {
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

