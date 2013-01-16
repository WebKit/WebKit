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
 * @implements {WebInspector.ScriptSourceMapping}
 * @param {WebInspector.Workspace} workspace
 * @param {WebInspector.SimpleWorkspaceProvider} networkWorkspaceProvider
 */
WebInspector.CompilerScriptMapping = function(workspace, networkWorkspaceProvider)
{
    this._workspace = workspace;
    this._workspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, this._uiSourceCodeAddedToWorkspace, this);
    this._networkWorkspaceProvider = networkWorkspaceProvider;
    /** @type {Object.<string, WebInspector.PositionBasedSourceMap>} */
    this._sourceMapForSourceMapURL = {};
    /** @type {Object.<string, WebInspector.PositionBasedSourceMap>} */
    this._sourceMapForScriptId = {};
    this._scriptForSourceMap = new Map();
    /** @type {Object.<string, WebInspector.PositionBasedSourceMap>} */
    this._sourceMapForURL = {};
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.GlobalObjectCleared, this._debuggerReset, this);
}

WebInspector.CompilerScriptMapping.prototype = {
    /**
     * @param {WebInspector.RawLocation} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var debuggerModelLocation = /** @type {WebInspector.DebuggerModel.Location} */ (rawLocation);
        var sourceMap = this._sourceMapForScriptId[debuggerModelLocation.scriptId];
        var lineNumber = debuggerModelLocation.lineNumber;
        var columnNumber = debuggerModelLocation.columnNumber || 0;
        var entry = sourceMap.findEntry(lineNumber, columnNumber);
        if (entry.length === 2)
            return null;
        var url = entry[2];
        var uri = WebInspector.fileMapping.uriForURL(url);
        var uiSourceCode = this._workspace.uiSourceCodeForURI(uri);
        if (!uiSourceCode)
            return null;
        return new WebInspector.UILocation(uiSourceCode, entry[3], entry[4]);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {WebInspector.DebuggerModel.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        if (!uiSourceCode.url)
            return null;
        var sourceMap = this._sourceMapForURL[uiSourceCode.url];
        if (!sourceMap)
            return null;
        var entry = sourceMap.findEntryReversed(uiSourceCode.url, lineNumber);
        return WebInspector.debuggerModel.createRawLocation(this._scriptForSourceMap.get(sourceMap), entry[0], entry[1]);
    },

    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        var sourceMap = this.loadSourceMapForScript(script);

        if (this._scriptForSourceMap.get(sourceMap)) {
            this._sourceMapForScriptId[script.scriptId] = sourceMap;
            script.pushSourceMapping(this);
            return;
        }

        var sourceURLs = sourceMap.sources();
        for (var i = 0; i < sourceURLs.length; ++i) {
            var sourceURL = sourceURLs[i];
            var uri = WebInspector.fileMapping.uriForURL(sourceURL);
            if (this._sourceMapForURL[sourceURL])
                continue;
            this._sourceMapForURL[sourceURL] = sourceMap;
            if (!WebInspector.fileMapping.hasMappingForURL(sourceURL) && !this._workspace.uiSourceCodeForURI(uri)) {
                var sourceContent = sourceMap.sourceContent(sourceURL);
                var contentProvider;
                if (sourceContent)
                    contentProvider = new WebInspector.StaticContentProvider(WebInspector.resourceTypes.Script, sourceContent);
                else
                    contentProvider = new WebInspector.CompilerSourceMappingContentProvider(sourceURL);
                this._networkWorkspaceProvider.addFileForURL(sourceURL, contentProvider, true);
            }
            var uiSourceCode = this._workspace.uiSourceCodeForURI(uri);
            if (uiSourceCode) {
                this._bindUISourceCode(this._workspace.uiSourceCodeForURI(uri));
                uiSourceCode.isContentScript = script.isContentScript;
            }
        }
        this._sourceMapForScriptId[script.scriptId] = sourceMap;
        this._scriptForSourceMap.put(sourceMap, script);
        script.pushSourceMapping(this);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _bindUISourceCode: function(uiSourceCode)
    {
        uiSourceCode.setSourceMapping(this);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _uiSourceCodeAddedToWorkspace: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ (event.data);
        if (!uiSourceCode.url || !this._sourceMapForURL[uiSourceCode.url])
            return;
        this._bindUISourceCode(uiSourceCode);
    },

    /**
     * @param {WebInspector.Script} script
     * @return {WebInspector.PositionBasedSourceMap}
     */
    loadSourceMapForScript: function(script)
    {
        var sourceMapURL = WebInspector.PositionBasedSourceMap.prototype._canonicalizeURL(script.sourceMapURL, script.sourceURL);
        var sourceMap = this._sourceMapForSourceMapURL[sourceMapURL];
        if (sourceMap)
            return sourceMap;

        try {
            // FIXME: make sendRequest async.
            var response = InspectorFrontendHost.loadResourceSynchronously(sourceMapURL);
            if (response.slice(0, 3) === ")]}")
                response = response.substring(response.indexOf('\n'));
            var payload = /** @type {SourceMapV3} */ (JSON.parse(response));
            sourceMap = new WebInspector.PositionBasedSourceMap(sourceMapURL, payload);
        } catch(e) {
            console.error(e.message);
            return null;
        }
        this._sourceMapForSourceMapURL[sourceMapURL] = sourceMap;
        return sourceMap;
    },

    _debuggerReset: function()
    {
        this._sourceMapForSourceMapURL = {};
        this._sourceMapForScriptId = {};
        this._scriptForSourceMap = new Map();
        this._sourceMapForURL = {};
    }
}
