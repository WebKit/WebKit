/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * @constructor
 * @implements {WebInspector.SourceMapping}
 * @param {WebInspector.Workspace} workspace
 */
WebInspector.ResourceScriptMapping = function(workspace)
{
    this._workspace = workspace;
    this._workspace.addEventListener(WebInspector.Workspace.Events.ProjectWillReset, this._reset, this);
    this._workspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, this._uiSourceCodeAddedToWorkspace, this);

    /** @type {Object.<string, WebInspector.UISourceCode>} */
    this._uiSourceCodeForScriptId = {};
    this._scriptIdForUISourceCode = new Map();
    this._temporaryUISourceCodes = new Map();
    /** @type {Object.<string, number>} */
    this._nextDynamicScriptIndexForURL = {};
    this._scripts = [];
}

WebInspector.ResourceScriptMapping.prototype = {
    /**
     * @param {WebInspector.RawLocation} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var debuggerModelLocation = /** @type {WebInspector.DebuggerModel.Location} */ rawLocation;
        var script = WebInspector.debuggerModel.scriptForId(debuggerModelLocation.scriptId);
        var uiSourceCode = this._uiSourceCodeForScriptId[debuggerModelLocation.scriptId];
        return new WebInspector.UILocation(uiSourceCode, debuggerModelLocation.lineNumber, debuggerModelLocation.columnNumber || 0);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {WebInspector.DebuggerModel.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        var scriptId = this._scriptIdForUISourceCode.get(uiSourceCode);
        var script = WebInspector.debuggerModel.scriptForId(scriptId);
        return WebInspector.debuggerModel.createRawLocation(script, lineNumber, columnNumber);
    },

    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        console.assert(!this._uiSourceCodeForScriptId[script.scriptId]);

        var isDynamicScript = false;
        if (!script.isAnonymousScript()) {
            this._scripts.push(script);
            var uiSourceCode = this._workspace.uiSourceCodeForURL(script.sourceURL);
            isDynamicScript = !!uiSourceCode && uiSourceCode.contentType() === WebInspector.resourceTypes.Document && !script.isInlineScript();
            if (uiSourceCode && !isDynamicScript && !this._temporaryUISourceCodes.get(uiSourceCode))
                this._bindUISourceCodeToScripts(uiSourceCode, [script]);
        }
        if (!this._uiSourceCodeForScriptId[script.scriptId])
            this._addOrReplaceTemporaryUISourceCode(script, isDynamicScript);

        console.assert(this._uiSourceCodeForScriptId[script.scriptId]);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {Array.<WebInspector.Script>} scripts
     */
    _bindUISourceCodeToScripts: function(uiSourceCode, scripts)
    {
        console.assert(scripts.length);

        for (var i = 0; i < scripts.length; ++i) {
            this._uiSourceCodeForScriptId[scripts[i].scriptId] = uiSourceCode;
            scripts[i].setSourceMapping(this);
        }
        uiSourceCode.isContentScript = scripts[0].isContentScript;
        uiSourceCode.setSourceMapping(this);
        this._scriptIdForUISourceCode.put(uiSourceCode, scripts[0].scriptId);
    },

    /**
     * @param {string} sourceURL
     * @param {boolean} isInlineScript
     * @return {Array.<WebInspector.Script>}
     */
    _scriptsForSourceURL: function(sourceURL, isInlineScript)
    {
        function filter(script)
        {
            return script.sourceURL === sourceURL && script.isInlineScript() === isInlineScript;
        }

        return this._scripts.filter(filter.bind(this));
    },

    /**
     * @param {WebInspector.Script} script
     * @param {boolean} isDynamicScript
     */
    _addOrReplaceTemporaryUISourceCode: function(script, isDynamicScript)
    {
        var scripts = script.isInlineScript() ? this._scriptsForSourceURL(script.sourceURL, true) : [script];

        var oldUISourceCode;
        for (var i = 0; i < scripts.length; ++i) {
            oldUISourceCode = this._uiSourceCodeForScriptId[scripts[i].scriptId];
            if (oldUISourceCode)
                break;
        }
        console.assert(!oldUISourceCode || this._temporaryUISourceCodes.get(oldUISourceCode));

        var contentProvider = script.isInlineScript() ? new WebInspector.ConcatenatedScriptsContentProvider(scripts) : script;
        var url = script.sourceURL;
        if (isDynamicScript) {
            var nextIndex = this._nextDynamicScriptIndexForURL[script.sourceURL] || 1;
            url += " (" + nextIndex + ")";
            this._nextDynamicScriptIndexForURL[script.sourceURL] = nextIndex + 1;
        }
        var uiSourceCode = new WebInspector.JavaScriptSource(url, null, contentProvider, !script.isInlineScript());
        this._temporaryUISourceCodes.put(uiSourceCode, uiSourceCode);
        this._bindUISourceCodeToScripts(uiSourceCode, scripts);

        if (!script.sourceURL)
            return uiSourceCode;

        if (oldUISourceCode)
            this._uiSourceCodeReplaced(oldUISourceCode, uiSourceCode);
        else
            this._workspace.project().addUISourceCode(uiSourceCode);
        return uiSourceCode;
    },

    _uiSourceCodeAddedToWorkspace: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data;
        console.assert(!this._scriptIdForUISourceCode.get(uiSourceCode) || this._temporaryUISourceCodes.get(uiSourceCode));
        if (!uiSourceCode.url || this._temporaryUISourceCodes.get(uiSourceCode))
            return;
        this._addUISourceCode(uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _addUISourceCode: function(uiSourceCode)
    {
        var isInlineScript;
        switch (uiSourceCode.contentType()) {
        case WebInspector.resourceTypes.Document:
            isInlineScript = true;
            break;
        case WebInspector.resourceTypes.Script:
            isInlineScript = false;
            break;
        default:
            return;
        }

        var scripts = this._scriptsForSourceURL(uiSourceCode.url, isInlineScript);
        if (!scripts.length)
            return;

        var oldUISourceCode = this._uiSourceCodeForScriptId[scripts[0].scriptId];
        this._bindUISourceCodeToScripts(uiSourceCode, scripts);

        if (oldUISourceCode) {
            console.assert(this._temporaryUISourceCodes.get(oldUISourceCode));
            this._uiSourceCodeReplaced(oldUISourceCode, uiSourceCode);
        }

        console.assert(this._scriptIdForUISourceCode.get(uiSourceCode) && !this._temporaryUISourceCodes.get(uiSourceCode));
    },

    /**
     * @param {WebInspector.UISourceCode} oldUISourceCode
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _uiSourceCodeReplaced: function(oldUISourceCode, uiSourceCode)
    {
        this._temporaryUISourceCodes.remove(oldUISourceCode);
        this._scriptIdForUISourceCode.remove(oldUISourceCode);
        this._workspace.project().replaceUISourceCode(oldUISourceCode, uiSourceCode);
    },

    _reset: function()
    {
        this._uiSourceCodeForScriptId = {};
        this._scriptIdForUISourceCode.clear();
        this._temporaryUISourceCodes.clear();
        this._nextDynamicScriptIndexForURL = {};
        this._scripts = [];
    },
}
