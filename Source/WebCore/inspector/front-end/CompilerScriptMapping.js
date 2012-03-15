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
 * @extends {WebInspector.ScriptMapping}
 */
WebInspector.CompilerScriptMapping = function()
{
    this._sourceMapByURL = {};
    this._sourceMapForScriptId = {};
    this._scriptForSourceMap = new Map();
    this._sourceMapForUISourceCode = new Map();
    this._uiSourceCodeByURL = {};
}

WebInspector.CompilerScriptMapping.prototype = {
    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var sourceMap = this._sourceMapForScriptId[rawLocation.scriptId];
        var location = sourceMap.compiledLocationToSourceLocation(rawLocation.lineNumber, rawLocation.columnNumber || 0);
        var uiSourceCode = this._uiSourceCodeByURL[location.sourceURL];
        return new WebInspector.UILocation(uiSourceCode, location.lineNumber, location.columnNumber);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {DebuggerAgent.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        var sourceMap = this._sourceMapForUISourceCode.get(uiSourceCode);
        var location = sourceMap.sourceLocationToCompiledLocation(uiSourceCode.url, lineNumber);
        return WebInspector.debuggerModel.createRawLocation(this._scriptForSourceMap.get(sourceMap), location[0], location[1]);
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodeList: function()
    {
        var result = []
        for (var url in this._uiSourceCodeByURL)
            result.push(this._uiSourceCodeByURL[url]);
        return result;
    },

    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        var sourceMap = this.loadSourceMapForScript(script);

        if (this._scriptForSourceMap.get(sourceMap)) {
            this._sourceMapForScriptId[script.scriptId] = sourceMap;
            var data = { removedItems: [], addedItems: [], scriptIds: [script.scriptId] };
            this.dispatchEventToListeners(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, data);
            return;
        }

        var uiSourceCodeList = [];
        var sourceURLs = sourceMap.sources();
        for (var i = 0; i < sourceURLs.length; ++i) {
            var sourceURL = sourceURLs[i];
            if (this._uiSourceCodeByURL[sourceURL])
                continue;
            var contentProvider = new WebInspector.CompilerSourceMappingContentProvider(sourceURL, sourceMap);
            var uiSourceCode = new WebInspector.UISourceCode(sourceURL, sourceURL, contentProvider);
            uiSourceCode.isContentScript = script.isContentScript;
            uiSourceCode.isEditable = false;
            this._uiSourceCodeByURL[sourceURL] = uiSourceCode;
            this._sourceMapForUISourceCode.put(uiSourceCode, sourceMap);
            uiSourceCodeList.push(uiSourceCode);
        }

        this._sourceMapForScriptId[script.scriptId] = sourceMap;
        this._scriptForSourceMap.put(sourceMap, script);
        var data = { removedItems: [], addedItems: uiSourceCodeList, scriptIds: [script.scriptId] };
        this.dispatchEventToListeners(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, data);
    },

    /**
     * @param {WebInspector.Script} script
     * @return {WebInspector.ClosureCompilerSourceMapping}
     */
    loadSourceMapForScript: function(script)
    {
        var sourceMapURL = WebInspector.ClosureCompilerSourceMapping.prototype._canonicalizeURL(script.sourceMapURL, script.sourceURL);
        var sourceMap = this._sourceMapByURL[sourceMapURL];
        if (sourceMap)
            return sourceMap;

        sourceMap = new WebInspector.ClosureCompilerSourceMapping(script.sourceMapURL, script.sourceURL);
        if (!sourceMap.load())
            return null;

        this._sourceMapByURL[sourceMapURL] = sourceMap;
        return sourceMap;
    },

    reset: function()
    {
        var data = { removedItems: this.uiSourceCodeList(), addedItems: [], scriptIds: [] };
        this.dispatchEventToListeners(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, data);

        this._sourceMapByURL = {};
        this._sourceMapForScriptId = {};
        this._scriptForSourceMap = new Map();
        this._sourceMapForUISourceCode = new Map();
        this._uiSourceCodeByURL = {};
    }
}

WebInspector.CompilerScriptMapping.prototype.__proto__ = WebInspector.ScriptMapping.prototype;
