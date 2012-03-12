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
 * @extends {WebInspector.Object}
 */
WebInspector.ScriptMapping = function()
{
}

WebInspector.ScriptMapping.Events = {
    UISourceCodeListChanged: "us-source-code-list-changed"
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
 * @extends {WebInspector.ScriptMapping}
 */
WebInspector.ResourceScriptMapping = function()
{
    this._rawSourceCodes = [];
    this._rawSourceCodeForScriptId = {};
    this._rawSourceCodeForURL = {};
    this._rawSourceCodeForDocumentURL = {};
    this._rawSourceCodeForUISourceCode = new Map();
    this._formatter = new WebInspector.ScriptFormatter();
    this._formatSource = false;
}

WebInspector.ResourceScriptMapping.prototype = {
    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var rawSourceCode = this._rawSourceCodeForScriptId[rawLocation.scriptId];
        return rawSourceCode.rawLocationToUILocation(rawLocation);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {DebuggerAgent.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        var rawSourceCode = this._rawSourceCodeForUISourceCode.get(uiSourceCode);
        return rawSourceCode.uiLocationToRawLocation(uiSourceCode, lineNumber, columnNumber);
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodeList: function()
    {
        var result = [];
        for (var i = 0; i < this._rawSourceCodes.length; ++i) {
            var uiSourceCodeList = this._rawSourceCodes[i].uiSourceCodeList();
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
        var resource = null;
        var isInlineScript = false;
        if (script.isInlineScript()) {
            resource = WebInspector.networkManager.inflightResourceForURL(script.sourceURL) || WebInspector.resourceForURL(script.sourceURL);
            if (resource && resource.type === WebInspector.Resource.Type.Document) {
                isInlineScript = true;
                var rawSourceCode = this._rawSourceCodeForDocumentURL[script.sourceURL];
                if (rawSourceCode) {
                    rawSourceCode.addScript(script);
                    this._bindScriptToRawSourceCode(script, rawSourceCode);
                    return;
                }
            }
        }

        var compilerSourceMapping = null;
        if (WebInspector.settings.sourceMapsEnabled.get() && script.sourceMapURL)
            compilerSourceMapping = new WebInspector.ClosureCompilerSourceMapping(script.sourceMapURL, script.sourceURL);

        var rawSourceCode = new WebInspector.RawSourceCode(script.scriptId, script, resource, this._formatter, this._formatSource, compilerSourceMapping);
        this._rawSourceCodes.push(rawSourceCode);
        this._bindScriptToRawSourceCode(script, rawSourceCode);

        if (isInlineScript)
            this._rawSourceCodeForDocumentURL[script.sourceURL] = rawSourceCode;

        if (rawSourceCode.uiSourceCodeList().length)
            this._uiSourceCodeListChanged(rawSourceCode, [], rawSourceCode.uiSourceCodeList());
        rawSourceCode.addEventListener(WebInspector.RawSourceCode.Events.UISourceCodeListChanged, this._handleUISourceCodeListChanged, this);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _handleUISourceCodeListChanged: function(event)
    {
        var rawSourceCode = /** @type {WebInspector.RawSourceCode} */ event.target;
        var oldUISourceCodeList = /** @type {Array.<WebInspector.UISourceCode>} */ event.data["oldUISourceCodeList"];
        this._uiSourceCodeListChanged(rawSourceCode, oldUISourceCodeList, rawSourceCode.uiSourceCodeList());
    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     * @param {Array.<WebInspector.UISourceCode>} removedItems
     * @param {Array.<WebInspector.UISourceCode>} addedItems
     */
    _uiSourceCodeListChanged: function(rawSourceCode, removedItems, addedItems)
    {
        for (var i = 0; i < removedItems.length; ++i)
            this._rawSourceCodeForUISourceCode.remove(removedItems[i]);
        for (var i = 0; i < addedItems.length; ++i)
            this._rawSourceCodeForUISourceCode.put(addedItems[i], rawSourceCode);

        var scriptIds = [];
        for (var i = 0; i < rawSourceCode._scripts.length; ++i)
            scriptIds.push(rawSourceCode._scripts[i].scriptId);
        var data = { removedItems: removedItems, addedItems: addedItems, scriptIds: scriptIds };
        this.dispatchEventToListeners(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, data);
    },

    /**
     * @param {WebInspector.Script} script
     * @param {WebInspector.RawSourceCode} rawSourceCode
     */
    _bindScriptToRawSourceCode: function(script, rawSourceCode)
    {
        this._rawSourceCodeForScriptId[script.scriptId] = rawSourceCode;
        this._rawSourceCodeForURL[script.sourceURL] = rawSourceCode;
    },

    /**
     * @param {boolean} formatSource
     */
    setFormatSource: function(formatSource)
    {
        if (this._formatSource === formatSource)
            return;

        this._formatSource = formatSource;
        for (var i = 0; i < this._rawSourceCodes.length; ++i)
            this._rawSourceCodes[i].setFormatted(this._formatSource);
    },

    /**
     * @param {DebuggerAgent.Location} rawLocation
     */
    forceUpdateSourceMapping: function(rawLocation)
    {
        var rawSourceCode = this._rawSourceCodeForScriptId[rawLocation.scriptId];
        rawSourceCode.forceUpdateSourceMapping();
    },

    reset: function()
    {
        for (var i = 0; i < this._rawSourceCodes.length; ++i) {
            var rawSourceCode = this._rawSourceCodes[i];
            this._uiSourceCodeListChanged(rawSourceCode, rawSourceCode.uiSourceCodeList(), []);
            rawSourceCode.removeAllListeners();
        }
        this._rawSourceCodes = [];
        this._rawSourceCodeForScriptId = {};
        this._rawSourceCodeForURL = {};
        this._rawSourceCodeForDocumentURL = {};
        this._rawSourceCodeForUISourceCode.clear();
    }
}

WebInspector.ResourceScriptMapping.prototype.__proto__ = WebInspector.ScriptMapping.prototype;

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.MainScriptMapping = function()
{
    this._mappings = [];
    this._resourceMapping = new WebInspector.ResourceScriptMapping();
    this._resourceMapping.addEventListener(WebInspector.ScriptMapping.Events.UISourceCodeListChanged, this._handleUISourceCodeListChanged, this);
    this._mappings.push(this._resourceMapping);

    this._mappingForScriptId = {};
    this._mappingForUISourceCode = new Map();
    this._liveLocationsForScriptId = {};
}

WebInspector.MainScriptMapping.Events = {
    UISourceCodeListChanged: "us-source-code-list-changed"
}

WebInspector.MainScriptMapping.prototype = {
    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        return this._mappingForScriptId[rawLocation.scriptId].rawLocationToUILocation(rawLocation);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {DebuggerAgent.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        return this._mappingForUISourceCode.get(uiSourceCode).uiLocationToRawLocation(uiSourceCode, lineNumber, columnNumber);
    },

    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @param {function(WebInspector.UILocation)} updateDelegate
     * @return {WebInspector.LiveLocation}
     */
    createLiveLocation: function(rawLocation, updateDelegate)
    {
        return new WebInspector.LiveLocation(this, rawLocation, updateDelegate);
    },

    _registerLiveLocation: function(scriptId, liveLocation)
    {
        this._liveLocationsForScriptId[scriptId].push(liveLocation)
        liveLocation._update();
    },

    _unregisterLiveLocation: function(scriptId, liveLocation)
    {
        this._liveLocationsForScriptId[scriptId].remove(liveLocation);
    },

    _updateLiveLocations: function(scriptIds)
    {
        for (var i = 0; i < scriptIds.length; ++i) {
            var liveLocations = this._liveLocationsForScriptId[scriptIds[i]];
            for (var j = 0; j < liveLocations.length; ++j)
                liveLocations[j]._update();
        }
    },

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
        this._liveLocationsForScriptId[script.scriptId] = [];

        var mapping = this._resourceMapping;

        this._mappingForScriptId[script.scriptId] = mapping;
        mapping.addScript(script);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _handleUISourceCodeListChanged: function(event)
    {
        var scriptMapping = /** @type {WebInspector.ScriptMapping} */ event.target;
        var removedItems = /** @type {Array.<WebInspector.UISourceCode>} */ event.data["removedItems"];
        var addedItems = /** @type {Array.<WebInspector.UISourceCode>} */ event.data["addedItems"];
        var scriptIds = /** @type {Array.<number>} */ event.data["scriptIds"];

        for (var i = 0; i < removedItems.length; ++i)
            this._mappingForUISourceCode.remove(removedItems[i]);
        for (var i = 0; i < addedItems.length; ++i)
            this._mappingForUISourceCode.put(addedItems[i], scriptMapping);
        this._updateLiveLocations(scriptIds);
        this.dispatchEventToListeners(WebInspector.MainScriptMapping.Events.UISourceCodeListChanged, event.data);
    },

    /**
     * @param {boolean} formatSource
     */
    setFormatSource: function(formatSource)
    {
        this._resourceMapping.setFormatSource(formatSource);
    },

    /**
     * @param {DebuggerAgent.Location} rawLocation
     */
    forceUpdateSourceMapping: function(rawLocation)
    {
        if (this._mappingForScriptId[rawLocation.scriptId] === this._resourceMapping)
            this._resourceMapping.forceUpdateSourceMapping(rawLocation);
    },

    reset: function()
    {
        for (var i = 0; i < this._mappings.length; ++i)
            this._mappings[i].reset();
        this._mappingForScriptId = {};
        this._mappingForUISourceCode = new Map();
        this._liveLocationsForScriptId = {};
    }
}

WebInspector.MainScriptMapping.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {WebInspector.MainScriptMapping} scriptMapping
 * @param {DebuggerAgent.Location} rawLocation
 * @param {function(WebInspector.UILocation)} updateDelegate
 */
WebInspector.LiveLocation = function(scriptMapping, rawLocation, updateDelegate)
{
    this._scriptMapping = scriptMapping;
    this._rawLocation = rawLocation;
    this._updateDelegate = updateDelegate;
}

WebInspector.LiveLocation.prototype = {
    init: function()
    {
        this._scriptMapping._registerLiveLocation(this._rawLocation.scriptId, this);
    },

    dispose: function()
    {
        this._scriptMapping._unregisterLiveLocation(this._rawLocation.scriptId, this);
    },

    _update: function()
    {
        var uiLocation = this._scriptMapping.rawLocationToUILocation(this._rawLocation);
        if (uiLocation)
            this._updateDelegate(uiLocation);
    }
}
