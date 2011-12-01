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
 * DevTools.js is responsible for configuring Web Inspector for the Chromium
 * port as well as additional features specific to the Chromium port.
 */

{(function () {
    Preferences.useLowerCaseMenuTitlesOnWindows = true;
    Preferences.sharedWorkersDebugNote = "Shared workers can be inspected in the Task Manager";
    Preferences.localizeUI = false;
    Preferences.applicationTitle = "Developer Tools - %s";
    Preferences.exposeDisableCache = true;
    Preferences.exposeWorkersInspection = true;
})();}

WebInspector.platformExtensionAPI = function(tabId)
{
    function getTabId()
    {
        return tabId;
    }
    webInspector.inspectedWindow.__proto__.__defineGetter__("tabId", getTabId);
    chrome = window.chrome || {};
    chrome.experimental = chrome.experimental || {};
    chrome.experimental.devtools = chrome.experimental.devtools || {};

    var properties = Object.getOwnPropertyNames(webInspector);
    for (var i = 0; i < properties.length; ++i) {
        var descriptor = Object.getOwnPropertyDescriptor(webInspector, properties[i]);
        Object.defineProperty(chrome.experimental.devtools, properties[i], descriptor);
    }
}

WebInspector.buildPlatformExtensionAPI = function()
{
    return "(" + WebInspector.platformExtensionAPI + ")(" + WebInspector._inspectedTabId + ");";
}

WebInspector.setInspectedTabId = function(tabId)
{
    WebInspector._inspectedTabId = tabId;
}
