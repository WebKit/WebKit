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

// RawSourceCode represents JavaScript resource or HTML resource with inlined scripts
// as it came from network.

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {string} id
 * @param {WebInspector.Script} script
 * @param {WebInspector.Resource} resource
 * @param {WebInspector.NetworkRequest} request
 * @param {WebInspector.SourceMapping} sourceMapping
 */
WebInspector.RawSourceCode = function(id, script, resource, request, sourceMapping)
{
    this.id = id;
    this.url = script.sourceURL;
    this.isContentScript = script.isContentScript;
    this._scripts = [script];
    this._resource = resource;
    this._pendingRequest = request;
    this._sourceMapping = sourceMapping;

    this._uiSourceCode = null;
    if (this._pendingRequest)
        this._pendingRequest.addEventListener(WebInspector.NetworkRequest.Events.FinishedLoading, this._finishedLoading, this);
    else
        this._uiSourceCode = this._createUISourceCode(this.url);
}

WebInspector.RawSourceCode.Events = {
    UISourceCodeChanged: "us-source-code-changed"
}

WebInspector.RawSourceCode.prototype = {
    /**
     * @param {WebInspector.Script} script
     */
    addScript: function(script)
    {
        this._scripts.push(script);
    },

    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @return {WebInspector.UILocation}
     */
    rawLocationToUILocation: function(rawLocation)
    {
        var uiSourceCode = this._uiSourceCode || this._temporaryUISourceCode;
        if (!uiSourceCode) {
            this._temporaryUISourceCode = this._createUISourceCode("tmp-" + this.url);
            uiSourceCode = this._temporaryUISourceCode;
            this.dispatchEventToListeners(WebInspector.RawSourceCode.Events.UISourceCodeChanged, { uiSourceCode: uiSourceCode });
        }
        return new WebInspector.UILocation(uiSourceCode, rawLocation.lineNumber, rawLocation.columnNumber || 0);
    },

    /**
     * @param {string} id
     * @return {WebInspector.UISourceCode}
     */
    _createUISourceCode: function(id)
    {
        var isStandaloneScript = this._scripts.length === 1 && !this._scripts[0].isInlineScript();

        var contentProvider;
        if (this._resource)
            contentProvider = this._resource;
        else if (isStandaloneScript)
            contentProvider = this._scripts[0];
        else
            contentProvider = new WebInspector.ConcatenatedScriptsContentProvider(this._scripts);

        var uiSourceCode = new WebInspector.JavaScriptSource(id, this.url, contentProvider, this._sourceMapping);
        uiSourceCode.isContentScript = this.isContentScript;
        uiSourceCode.isEditable = isStandaloneScript;
        return uiSourceCode;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {DebuggerAgent.Location}
     */
    uiLocationToRawLocation: function(uiSourceCode, lineNumber, columnNumber)
    {
        if (this.url)
            return WebInspector.debuggerModel.createRawLocationByURL(this.url, lineNumber, columnNumber);
        return WebInspector.debuggerModel.createRawLocation(this._scripts[0], lineNumber, columnNumber);
    },

    /**
     * @return {WebInspector.UISourceCode|null}
     */
    uiSourceCode: function()
    {
        return this._uiSourceCode || this._temporaryUISourceCode;
    },

    _finishedLoading: function(event)
    {
        this._resource = WebInspector.resourceForURL(this._pendingRequest.url);
        delete this._pendingRequest;
        var oldUISourceCode = this._uiSourceCode || this._temporaryUISourceCode;
        this._uiSourceCode = this._createUISourceCode(this.url);
        this.dispatchEventToListeners(WebInspector.RawSourceCode.Events.UISourceCodeChanged, { uiSourceCode: this._uiSourceCode, oldUISourceCode: oldUISourceCode });
    }
}

WebInspector.RawSourceCode.prototype.__proto__ = WebInspector.Object.prototype;
