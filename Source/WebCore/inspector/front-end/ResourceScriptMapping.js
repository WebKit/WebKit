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
    this._temporaryUISourceCodeForScriptId = {};
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
        var uiSourceCode = this._workspaceUISourceCodeForScript(script);
        if (!uiSourceCode)
            uiSourceCode = this._getOrCreateTemporaryUISourceCode(script);
        else if (uiSourceCode.scriptFile() && uiSourceCode.scriptFile().hasDivergedFromVM()) {
            var temporaryUISourceCode = this._getOrCreateTemporaryUISourceCode(script);
            temporaryUISourceCode.divergedVersion = uiSourceCode;
            uiSourceCode = temporaryUISourceCode;
        }
        console.assert(!!uiSourceCode);
        return new WebInspector.UILocation(uiSourceCode, debuggerModelLocation.lineNumber, debuggerModelLocation.columnNumber || 0);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _hasMergedToVM: function(uiSourceCode)
    {
        var scripts = this._scriptsForUISourceCode(uiSourceCode);
        if (!scripts.length)
            return;
        for (var i = 0; i < scripts.length; ++i)
            scripts[i].setSourceMapping(this);
        this._deleteTemporaryUISourceCodeForScripts(scripts);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _hasDivergedFromVM: function(uiSourceCode)
    {
        var scripts = this._scriptsForUISourceCode(uiSourceCode);
        if (!scripts.length)
            return;
        for (var i = 0; i < scripts.length; ++i)
            scripts[i].setSourceMapping(this);
    },

    /**
     * @param {WebInspector.Script} script
     * @return {WebInspector.UISourceCode}
     */
    _workspaceUISourceCodeForScript: function(script)
    {
        if (script.isAnonymousScript() || this._isDynamicScript(script))
            return null;
        return this._workspace.uiSourceCodeForURL(script.sourceURL);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {WebInspector.DebuggerModel.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        var scripts = this._scriptsForUISourceCode(uiSourceCode);
        console.assert(scripts.length);
        return WebInspector.debuggerModel.createRawLocation(scripts[0], lineNumber, columnNumber);
    },

    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        if (!script.isAnonymousScript())
            this._scripts.push(script);
        script.setSourceMapping(this);
        var uiSourceCode = this._workspaceUISourceCodeForScript(script);
        if (uiSourceCode) {
            this._bindUISourceCodeToScripts(uiSourceCode, [script]);
            return;
        }

        var scripts = script.isInlineScript() ? this._scriptsForSourceURL(script.sourceURL, true) : [script];
        if (this._deleteTemporaryUISourceCodeForScripts(scripts))
            uiSourceCode = this._getOrCreateTemporaryUISourceCode(script);
    },

    /**
     * @param {Array.<WebInspector.Script>} scripts
     * @return {boolean}
     */
    _deleteTemporaryUISourceCodeForScripts: function(scripts)
    {
        var temporaryUISourceCode;
        for (var i = 0; i < scripts.length; ++i) {
            var script = scripts[i];
            temporaryUISourceCode = temporaryUISourceCode || this._temporaryUISourceCodeForScriptId[script.scriptId];
            delete this._temporaryUISourceCodeForScriptId[script.scriptId];
        }
        if (temporaryUISourceCode)
            this._workspace.project().removeTemporaryUISourceCode(temporaryUISourceCode);
        return !!temporaryUISourceCode;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {Array.<WebInspector.Script>} scripts
     */
    _bindUISourceCodeToScripts: function(uiSourceCode, scripts)
    {
        console.assert(scripts.length);
        for (var i = 0; i < scripts.length; ++i)
            scripts[i].setSourceMapping(this);
        uiSourceCode.isContentScript = scripts[0].isContentScript;
        uiSourceCode.setSourceMapping(this);
        if (!uiSourceCode.isTemporary) {
            var scriptFile = new WebInspector.ResourceScriptFile(this, uiSourceCode);
            uiSourceCode.setScriptFile(scriptFile);
        }
    },

    /**
     * @param {WebInspector.Script} script
     * @return {boolean}
     */
    _isDynamicScript: function(script)
    {
        if (script.isAnonymousScript() || script.isInlineScript())
            return false;
        var inlineScriptsWithTheSameURL = this._scriptsForSourceURL(script.sourceURL, true);
        return !!inlineScriptsWithTheSameURL.length;
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
     * @return {WebInspector.UISourceCode}
     */
    _getOrCreateTemporaryUISourceCode: function(script)
    {
        var temporaryUISourceCode = this._temporaryUISourceCodeForScriptId[script.scriptId];
        if (temporaryUISourceCode)
            return temporaryUISourceCode;

        var scripts = script.isInlineScript() ? this._scriptsForSourceURL(script.sourceURL, true) : [script];
        var contentProvider = script.isInlineScript() ? new WebInspector.ConcatenatedScriptsContentProvider(scripts) : script;
        var isDynamicScript = this._isDynamicScript(script);
        var url = isDynamicScript ? "" : script.sourceURL;
        temporaryUISourceCode = new WebInspector.UISourceCode(url, contentProvider, false);
        for (var i = 0; i < scripts.length; ++i)
            this._temporaryUISourceCodeForScriptId[scripts[i].scriptId] = temporaryUISourceCode;
        this._bindUISourceCodeToScripts(temporaryUISourceCode, scripts);
        this._workspace.project().addTemporaryUISourceCode(temporaryUISourceCode);
        return temporaryUISourceCode;
    },

    _uiSourceCodeAddedToWorkspace: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data;
        console.assert(!!uiSourceCode.url);

        var scripts = this._scriptsForUISourceCode(uiSourceCode);
        if (!scripts.length)
            return;

        this._deleteTemporaryUISourceCodeForScripts(scripts);
        this._bindUISourceCodeToScripts(uiSourceCode, scripts);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {Array.<WebInspector.Script>}
     */
    _scriptsForUISourceCode: function(uiSourceCode)
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
            return [];
        }

        return this._scriptsForSourceURL(uiSourceCode.url, isInlineScript);
    },

    _reset: function()
    {
        this._temporaryUISourceCodeForScriptId = {};
        this._scripts = [];
    },
}

/**
 * @interface
 */
WebInspector.ScriptFile = function()
{
}

WebInspector.ScriptFile.Events = {
    WillMergeToVM: "WillMergeToVM",
    DidMergeToVM: "DidMergeToVM",
    WillDivergeFromVM: "WillDivergeFromVM",
    DidDivergeFromVM: "DidDivergeFromVM",
}

WebInspector.ScriptFile.prototype = {
    /**
     * @return {boolean}
     */
    hasDivergedFromVM: function() { return false; },

    /**
     * @return {boolean}
     */
    isDivergingFromVM: function() { return false; },

    /**
     * @param {string} eventType
     * @param {function(WebInspector.Event)} listener
     * @param {Object=} thisObject
     */
    addEventListener: function(eventType, listener, thisObject) { },

    /**
     * @param {string} eventType
     * @param {function(WebInspector.Event)} listener
     * @param {Object=} thisObject
     */
    removeEventListener: function(eventType, listener, thisObject) { }
}

/**
 * @constructor
 * @implements {WebInspector.ScriptFile}
 * @extends {WebInspector.Object}
 * @param {WebInspector.ResourceScriptMapping} resourceScriptMapping
 * @param {WebInspector.UISourceCode} uiSourceCode
 */
WebInspector.ResourceScriptFile = function(resourceScriptMapping, uiSourceCode)
{
    WebInspector.ScriptFile.call(this);
    this._resourceScriptMapping = resourceScriptMapping;
    this._uiSourceCode = uiSourceCode;
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted, this._workingCopyCommitted, this);
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged, this._workingCopyChanged, this);
}

WebInspector.ResourceScriptFile.prototype = {
    _workingCopyCommitted: function(event)
    {
        /**
         * @param {?string} error
         */
        function innerCallback(error)
        {
            if (error) {
                this._hasDivergedFromVM = true;
                WebInspector.showErrorMessage(error);
                return;
            }

            this.dispatchEventToListeners(WebInspector.ScriptFile.Events.WillMergeToVM, this);
            delete this._hasDivergedFromVM;
            this._resourceScriptMapping._hasMergedToVM(this._uiSourceCode);
            this.dispatchEventToListeners(WebInspector.ScriptFile.Events.DidMergeToVM, this);
        }

        var rawLocation = /** @type {WebInspector.DebuggerModel.Location} */ this._uiSourceCode.uiLocationToRawLocation(0, 0);
        if (!rawLocation)
            return;
        var script = WebInspector.debuggerModel.scriptForId(rawLocation.scriptId);
        WebInspector.debuggerModel.setScriptSource(script.scriptId, this._uiSourceCode.workingCopy(), innerCallback.bind(this));
    },

    _workingCopyChanged: function(event)
    {
        var wasDirty = /** @type {boolean} */ event.data.wasDirty;
        if (!wasDirty && this._uiSourceCode.isDirty() && !this._hasDivergedFromVM) {
            this._isDivergingFromVM = true;
            this.dispatchEventToListeners(WebInspector.ScriptFile.Events.WillDivergeFromVM, this._uiSourceCode);
            this._resourceScriptMapping._hasDivergedFromVM(this._uiSourceCode);
            this.dispatchEventToListeners(WebInspector.ScriptFile.Events.DidDivergeFromVM, this._uiSourceCode);
            delete this._isDivergingFromVM;
        } else if (wasDirty && !this._uiSourceCode.isDirty() && !this._hasDivergedFromVM) {
            this.dispatchEventToListeners(WebInspector.ScriptFile.Events.WillMergeToVM, this._uiSourceCode);
            this._resourceScriptMapping._hasMergedToVM(this._uiSourceCode);
            this.dispatchEventToListeners(WebInspector.ScriptFile.Events.DidMergeToVM, this._uiSourceCode);
        }
    },

    /**
     * @return {boolean}
     */
    hasDivergedFromVM: function()
    {
        return this._uiSourceCode.isDirty() || this._hasDivergedFromVM;
    },

    /**
     * @return {boolean}
     */
    isDivergingFromVM: function()
    {
        return this._isDivergingFromVM;
    },

    __proto__: WebInspector.Object.prototype
}
