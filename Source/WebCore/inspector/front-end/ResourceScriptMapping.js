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
 * @implements {WebInspector.SourceMapping}
 * @implements {WebInspector.UISourceCodeProvider}
 */
WebInspector.ResourceScriptMapping = function()
{
    this._rawSourceCodes = [];
    this._rawSourceCodeForScriptId = {};
    this._rawSourceCodeForURL = {};
    this._rawSourceCodeForDocumentURL = {};
    this._rawSourceCodeForUISourceCode = new Map();
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
    uiSourceCodes: function()
    {
        var result = [];
        for (var i = 0; i < this._rawSourceCodes.length; ++i) {
            var uiSourceCode = this._rawSourceCodes[i].uiSourceCode();
            if (uiSourceCode)
                result.push(uiSourceCode);
        }
        return result;
    },

    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        var resource = null;
        var request = null;
        if (!script.isAnonymousScript()) {
            // First lookup the resource that has already been loaded.
            resource = WebInspector.resourceForURL(script.sourceURL);
            // Ignore resource in case it has not yet finished loading.
            if (resource && resource.request && !resource.request.finished)
                resource = null;
            // Only bind inline and standalone scripts.
            if (resource && !this._shouldBindScriptToContentProvider(script, resource))
                resource = null;
            if (!resource) {
                // When there is no resource, lookup in-flight requests.
                request = WebInspector.networkManager.inflightRequestForURL(script.sourceURL);
                // Only bind inline and standalone scripts.
                if (request && !this._shouldBindScriptToContentProvider(script, request))
                  request = null;
            }
        }
        console.assert(!resource || !request);

        var isInlineScript = script.isInlineScript() && (request || resource);
        // If either of these exists, we bind script to the resource.
        if (isInlineScript) {
            var rawSourceCode = this._rawSourceCodeForDocumentURL[script.sourceURL];
            if (rawSourceCode) {
                rawSourceCode.addScript(script);
                this._bindScriptToRawSourceCode(script, rawSourceCode);
                return;
            }
        }

        var rawSourceCode = new WebInspector.RawSourceCode(script.scriptId, script, resource, request, this);
        this._rawSourceCodes.push(rawSourceCode);
        this._bindScriptToRawSourceCode(script, rawSourceCode);
        if (isInlineScript)
            this._rawSourceCodeForDocumentURL[script.sourceURL] = rawSourceCode;

        if (rawSourceCode.uiSourceCode())
            this._uiSourceCodeAdded(rawSourceCode, rawSourceCode.uiSourceCode());
        rawSourceCode.addEventListener(WebInspector.RawSourceCode.Events.UISourceCodeChanged, this._handleUISourceCodeChanged, this);
    },

    /**
     * @param {WebInspector.Script} script
     * @param {WebInspector.ContentProvider} contentProvider
     * @return {boolean}
     */
    _shouldBindScriptToContentProvider: function(script, contentProvider)
    {
        if (script.isInlineScript())
            return contentProvider.contentType() === WebInspector.resourceTypes.Document;
        return contentProvider.contentType() === WebInspector.resourceTypes.Script;
    },

    /**
     * @param {WebInspector.Event} event
     */
    _handleUISourceCodeChanged: function(event)
    {
        var rawSourceCode = /** @type {WebInspector.RawSourceCode} */ event.target;
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data.uiSourceCode;
        var oldUISourceCode = /** @type {WebInspector.UISourceCode} */ event.data.oldUISourceCode;
        if (!oldUISourceCode)
            this._uiSourceCodeAdded(rawSourceCode, uiSourceCode);
        else
            this._uiSourceCodeReplaced(rawSourceCode, oldUISourceCode, uiSourceCode);
    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     * @paran {WebInspector.UISourceCode} uiSourceCode
     */
    _uiSourceCodeAdded: function(rawSourceCode, uiSourceCode)
    {
        this._rawSourceCodeForUISourceCode.put(uiSourceCode, rawSourceCode);
        this.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, uiSourceCode);
    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     * @param {WebInspector.UISourceCode} oldUISourceCode
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _uiSourceCodeReplaced: function(rawSourceCode, oldUISourceCode, uiSourceCode)
    {
        this._rawSourceCodeForUISourceCode.remove(oldUISourceCode);
        this._rawSourceCodeForUISourceCode.put(uiSourceCode, rawSourceCode);

        for (var i = 0; i < rawSourceCode._scripts.length; ++i)
            rawSourceCode._scripts[i].setSourceMapping(this);

        var data = { oldUISourceCode: oldUISourceCode, uiSourceCode: uiSourceCode };
        this.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, data);
    },

    /**
     * @param {WebInspector.RawSourceCode} rawSourceCode
     * @paran {WebInspector.UISourceCode} uiSourceCode
     */
    _uiSourceCodeRemoved: function(rawSourceCode, uiSourceCode)
    {
        this._rawSourceCodeForUISourceCode.remove(uiSourceCode);
        this.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeRemoved, uiSourceCode);
    },

    /**
     * @param {WebInspector.Script} script
     * @param {WebInspector.RawSourceCode} rawSourceCode
     */
    _bindScriptToRawSourceCode: function(script, rawSourceCode)
    {
        this._rawSourceCodeForScriptId[script.scriptId] = rawSourceCode;
        this._rawSourceCodeForURL[script.sourceURL] = rawSourceCode;
        script.setSourceMapping(this);
    },

    reset: function()
    {
        for (var i = 0; i < this._rawSourceCodes.length; ++i) {
            var rawSourceCode = this._rawSourceCodes[i];
            this._uiSourceCodeRemoved(rawSourceCode, rawSourceCode.uiSourceCode());
            rawSourceCode.removeAllListeners();
        }
        this._rawSourceCodes = [];
        this._rawSourceCodeForScriptId = {};
        this._rawSourceCodeForURL = {};
        this._rawSourceCodeForDocumentURL = {};
        this._rawSourceCodeForUISourceCode.clear();
    }
}

WebInspector.ResourceScriptMapping.prototype.__proto__ = WebInspector.Object.prototype;
