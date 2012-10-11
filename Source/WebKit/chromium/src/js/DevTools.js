/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
    Preferences.showDockToRight = true;
    Preferences.exposeFileSystemInspection = true;
    Preferences.experimentsEnabled = false;
})();}

function buildPlatformExtensionAPI(extensionInfo)
{
    return "var extensionInfo = " + JSON.stringify(extensionInfo) + ";" +
       "var tabId = " + WebInspector._inspectedTabId + ";" +
       platformExtensionAPI.toString();
}

WebInspector.setInspectedTabId = function(tabId)
{
    WebInspector._inspectedTabId = tabId;
}

WebInspector.clipboardAccessDeniedMessage = function()
{
    return "You need to install a Chrome extension that grants clipboard access to Developer Tools.";
}
