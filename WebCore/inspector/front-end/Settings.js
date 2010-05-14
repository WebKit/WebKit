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


var Preferences = {
    maxInlineTextChildLength: 80,
    minConsoleHeight: 75,
    minSidebarWidth: 100,
    minElementsSidebarWidth: 200,
    minScriptsSidebarWidth: 200,
    styleRulesExpandedState: {},
    showMissingLocalizedStrings: false,
    samplingCPUProfiler: false,
    showColorNicknames: true,
    debuggerAlwaysEnabled: false,
    profilerAlwaysEnabled: false,
    auditsPanelEnabled: false
}

WebInspector.populateFrontendSettings = function(settingsString)
{
    WebInspector.settings._load(settingsString);
}

WebInspector.Settings = function()
{
}

WebInspector.Settings.prototype = {
    _load: function(settingsString)
    {
        try {
            this._store = JSON.parse(settingsString);
        } catch (e) {
            // May fail;
            this._store = {};
        }

        this._installSetting("eventListenersFilter", "event-listeners-filter", "all");
        this._installSetting("colorFormat", "color-format", "hex");
        this._installSetting("resourcesLargeRows", "resources-large-rows", true);
        this._installSetting("watchExpressions", "watch-expressions", []);
        this._installSetting("lastViewedScriptFile", "last-viewed-script-file");
        this._installSetting("showInheritedComputedStyleProperties", "show-inherited-computed-style-properties", false);
        this._installSetting("showUserAgentStyles", "show-user-agent-styles", true);
        this._installSetting("resourceViewTab", "resource-view-tab", "content");
        this._installSetting("consoleHistory", "console-history", []);
        this.dispatchEventToListeners("loaded");
    },

    _installSetting: function(name, propertyName, defaultValue)
    {
        this.__defineGetter__(name, this._get.bind(this, propertyName));
        this.__defineSetter__(name, this._set.bind(this, propertyName));
        if (!(propertyName in this._store)) {
            this._store[propertyName] = defaultValue;
        }
    },

    _get: function(propertyName)
    {
        return this._store[propertyName];
    },

    _set: function(propertyName, newValue)
    {
        this._store[propertyName] = newValue;
        try {
            InspectorBackend.saveFrontendSettings(JSON.stringify(this._store));
        } catch (e) {
            // May fail;
        }
    }
}

WebInspector.Settings.prototype.__proto__ = WebInspector.Object.prototype;
