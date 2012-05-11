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
 * @param {WebInspector.UISourceCode} uiSourceCode
 * @param {number} lineNumber
 * @param {number} columnNumber
 */
WebInspector.UILocation = function(uiSourceCode, lineNumber, columnNumber)
{
    this.uiSourceCode = uiSourceCode;
    this.lineNumber = lineNumber;
    this.columnNumber = columnNumber;
}

WebInspector.UILocation.prototype = {
    /**
     * @return {DebuggerAgent.Location}
     */
    uiLocationToRawLocation: function()
    {
        return this.uiSourceCode.uiLocationToRawLocation(this.lineNumber, this.columnNumber);
    }
}

/**
 * @interface
 */
WebInspector.LiveLocation = function()
{
}

WebInspector.LiveLocation.prototype = {
    update: function(rawLocation) { },

    dispose: function() { }
}

/**
 * @interface
 */
WebInspector.SourceMapping = function()
{
}

WebInspector.SourceMapping.prototype = {
    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation) { },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {DebuggerAgent.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber) { }
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @implements {WebInspector.SourceMapping}
 */
WebInspector.ScriptMapping = function()
{
}

WebInspector.ScriptMapping.Events = {
    UISourceCodeListChanged: "ui-source-code-list-changed"
}

WebInspector.ScriptMapping.prototype = {
    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation) {},

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {DebuggerAgent.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber) {},

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodeList: function() {}
}

WebInspector.ScriptMapping.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.MainScriptMapping = function()
{
    this._mappings = [];

    this._resourceMapping = new WebInspector.ResourceScriptMapping();
    this._mappings.push(this._resourceMapping);
    this._compilerMapping = new WebInspector.CompilerScriptMapping();
    this._mappings.push(this._compilerMapping);
    this._snippetsMapping = new WebInspector.SnippetsScriptMapping();
    this._mappings.push(this._snippetsMapping);

    for (var i = 0; i < this._mappings.length; ++i)
        this._mappings[i].addEventListener(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, this._handleUISourceCodeListChanged, this);

    this._mappingForScriptId = {};
}

WebInspector.MainScriptMapping.Events = {
    UISourceCodeListChanged: "ui-source-code-list-changed"
}

WebInspector.MainScriptMapping.prototype = {
    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodeList: function()
    {
        var result = [];
        for (var i = 0; i < this._mappings.length; ++i) {
            var uiSourceCodeList = this._mappings[i].uiSourceCodeList();
            for (var j = 0; j < uiSourceCodeList.length; ++j)
                result.push(uiSourceCodeList[j]);
        }
        return result;
    },

    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        var mapping = this._mappingForScript(script);
        this._mappingForScriptId[script.scriptId] = mapping;
        mapping.addScript(script);
    },

    /**
     * @param {WebInspector.Script} script
     * @return {WebInspector.ScriptMapping}
     */
    _mappingForScript: function(script)
    {
        if (WebInspector.experimentsSettings.snippetsSupport.isEnabled()) {
            if (WebInspector.snippetsModel.snippetIdForSourceURL(script.sourceURL))
                return this._snippetsMapping;
        }

        if (WebInspector.settings.sourceMapsEnabled.get() && script.sourceMapURL) {
            if (this._compilerMapping.loadSourceMapForScript(script))
                return this._compilerMapping;
        }

        return this._resourceMapping;
    },

    /**
     * @param {WebInspector.Event} event
     */
    _handleUISourceCodeListChanged: function(event)
    {
        this.dispatchEventToListeners(WebInspector.MainScriptMapping.Events.UISourceCodeListChanged, event.data);
    },

    reset: function()
    {
        for (var i = 0; i < this._mappings.length; ++i)
            this._mappings[i].reset();
        this._mappingForScriptId = {};
    }
}

WebInspector.MainScriptMapping.prototype.__proto__ = WebInspector.Object.prototype;
