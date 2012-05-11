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
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.DebuggerPresentationModel = function()
{
    this._scriptMapping = new WebInspector.MainScriptMapping();
    this._scriptMapping.addEventListener(WebInspector.MainScriptMapping.Events.UISourceCodeListChanged, this._handleUISourceCodeListChanged, this);

    this._presentationCallFrames = [];

    this._breakpointManager = new WebInspector.BreakpointManager(WebInspector.settings.breakpoints, WebInspector.debuggerModel);

    this._pendingConsoleMessages = {};
    this._consoleMessageLiveLocations = [];

    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.FailedToParseScriptSource, this._failedToParseScriptSource, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.GlobalObjectCleared, this._debuggerReset, this);

    WebInspector.console.addEventListener(WebInspector.ConsoleModel.Events.MessageAdded, this._consoleMessageAdded, this);
    WebInspector.console.addEventListener(WebInspector.ConsoleModel.Events.ConsoleCleared, this._consoleCleared, this);

    new WebInspector.DebuggerPresentationModelResourceBinding(this);
}

WebInspector.DebuggerPresentationModel.Events = {
    UISourceCodeAdded: "source-file-added",
    UISourceCodeReplaced: "source-file-replaced",
    UISourceCodeRemoved: "source-file-removed"
}

WebInspector.DebuggerPresentationModel.prototype = {
    get breakpointManager()
    {
        return this._breakpointManager;
    },

    /**
     * @param {DebuggerAgent.Location} rawLocation
     * @param {function(WebInspector.UILocation):(boolean|undefined)} updateDelegate
     * @return {WebInspector.LiveLocation}
     */
    createLiveLocation: function(rawLocation, updateDelegate)
    {
        var script = WebInspector.debuggerModel.scriptForId(rawLocation.scriptId);
        return script.createLiveLocation(rawLocation, updateDelegate);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _parsedScriptSource: function(event)
    {
        var script = /** @type {WebInspector.Script} */ event.data;
        this._scriptMapping.addScript(script);
        this._addPendingConsoleMessagesToScript(script);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _failedToParseScriptSource: function(event)
    {
        this._parsedScriptSource(event);
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodes: function()
    {
        return this._scriptMapping.uiSourceCodeList();
    },

    /**
     * @param {WebInspector.Event} event
     */
    _handleUISourceCodeListChanged: function(event)
    {
        var removedItems = /** @type {Array.<WebInspector.UISourceCode>} */ event.data["removedItems"];
        var addedItems = /** @type {Array.<WebInspector.UISourceCode>} */ event.data["addedItems"];

        if (!removedItems.length) {
            for (var i = 0; i < addedItems.length; ++i)
                this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.UISourceCodeAdded, addedItems[i]);
        } else if (!addedItems.length) {
            for (var i = 0; i < addedItems.length; ++i)
                this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.UISourceCodeRemoved, removedItems[i]);
        } else {
            var eventData = { uiSourceCodeList: addedItems, oldUISourceCodeList: removedItems };
            this.dispatchEventToListeners(WebInspector.DebuggerPresentationModel.Events.UISourceCodeReplaced, eventData);
        }
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {boolean}
     */
    canEditScriptSource: function(uiSourceCode)
    {
        return WebInspector.debuggerModel.canSetScriptSource() && uiSourceCode.isEditable;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {string} newSource
     * @param {function(?Protocol.Error)} callback
     */
    setScriptSource: function(uiSourceCode, newSource, callback)
    {
        var rawLocation = uiSourceCode.uiLocationToRawLocation(0, 0);
        var script = WebInspector.debuggerModel.scriptForId(rawLocation.scriptId);

        /**
         * @this {WebInspector.DebuggerPresentationModel}
         * @param {?Protocol.Error} error
         */
        function didEditScriptSource(error)
        {
            callback(error);
            if (error)
                return;

            var resource = WebInspector.resourceForURL(script.sourceURL);
            if (resource)
                resource.addRevision(newSource);

            uiSourceCode.contentChanged(newSource);
        }
        WebInspector.debuggerModel.setScriptSource(script.scriptId, newSource, didEditScriptSource.bind(this));
    },

    /**
     * @param {WebInspector.Event} event
     */
    _consoleMessageAdded: function(event)
    {
        var message = /** @type {WebInspector.ConsoleMessage} */ event.data;
        if (!message.url || !message.isErrorOrWarning())
            return;

        var rawLocation = message.location();
        if (rawLocation)
            this._addConsoleMessageToScript(message, rawLocation);
        else
            this._addPendingConsoleMessage(message);
    },

    /**
     * @param {WebInspector.ConsoleMessage} message
     * @param {DebuggerAgent.Location} rawLocation
     */
    _addConsoleMessageToScript: function(message, rawLocation)
    {
        function updateLocation(uiLocation)
        {
            var presentationMessage = new WebInspector.PresentationConsoleMessage(uiLocation.uiSourceCode, uiLocation.lineNumber, message);
            uiLocation.uiSourceCode.consoleMessageAdded(presentationMessage);
        }
        var liveLocation = this.createLiveLocation(rawLocation, updateLocation.bind(this));
        this._consoleMessageLiveLocations.push(liveLocation);
    },

    /**
     * @param {WebInspector.ConsoleMessage} message
     */
    _addPendingConsoleMessage: function(message)
    {
        if (!this._pendingConsoleMessages[message.url])
            this._pendingConsoleMessages[message.url] = [];
        this._pendingConsoleMessages[message.url].push(message);
    },

    /**
     * @param {WebInspector.Script} script
     */
    _addPendingConsoleMessagesToScript: function(script)
    {
        var messages = this._pendingConsoleMessages[script.sourceURL];
        if (!messages)
            return;

        var pendingMessages = [];
        for (var i = 0; i < messages.length; i++) {
            var message = messages[i];
            var rawLocation = message.location();
            if (script.scriptId === rawLocation.scriptId)
                this._addConsoleMessageToScript(messages, rawLocation);
            else
                pendingMessages.push(message);
        }

        if (pendingMessages.length)
            this._pendingConsoleMessages[script.sourceURL] = pendingMessages;
        else
            delete this._pendingConsoleMessages[script.sourceURL];
    },

    _consoleCleared: function()
    {
        this._pendingConsoleMessages = {};
        for (var i = 0; i < this._consoleMessageLiveLocations.length; ++i)
            this._consoleMessageLiveLocations[i].dispose();
        this._consoleMessageLiveLocations = [];
        var uiSourceCodes = this.uiSourceCodes();
        for (var i = 0; i < uiSourceCodes.length; ++i)
            uiSourceCodes[i].consoleMessagesCleared();
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     */
    continueToLine: function(uiSourceCode, lineNumber)
    {
        var rawLocation = uiSourceCode.uiLocationToRawLocation(lineNumber, 0);
        WebInspector.debuggerModel.continueToLocation(rawLocation);
    },

    _debuggerReset: function()
    {
        this._scriptMapping.reset();
        this._pendingConsoleMessages = {};
        this._consoleMessageLiveLocations = [];
    }
}

WebInspector.DebuggerPresentationModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {WebInspector.UISourceCode} uiSourceCode
 * @param {number} lineNumber
 * @param {WebInspector.ConsoleMessage} originalMessage
 */
WebInspector.PresentationConsoleMessage = function(uiSourceCode, lineNumber, originalMessage)
{
    this.uiSourceCode = uiSourceCode;
    this.lineNumber = lineNumber;
    this.originalMessage = originalMessage;
}

/**
 * @constructor
 * @implements {WebInspector.ResourceDomainModelBinding}
 * @param {WebInspector.DebuggerPresentationModel} model
 */
WebInspector.DebuggerPresentationModelResourceBinding = function(model)
{
    this._presentationModel = model;
    WebInspector.Resource.registerDomainModelBinding(WebInspector.resourceTypes.Script, this);
}

WebInspector.DebuggerPresentationModelResourceBinding.prototype = {
    /**
     * @param {WebInspector.Resource} resource
     * @return {boolean}
     */
    canSetContent: function(resource)
    {
        var uiSourceCode = this._uiSourceCodeForResource(resource);
        return !!uiSourceCode && this._presentationModel.canEditScriptSource(uiSourceCode);
    },

    /**
     * @param {WebInspector.Resource} resource
     * @param {string} content
     * @param {boolean} majorChange
     * @param {function(?string)} userCallback
     */
    setContent: function(resource, content, majorChange, userCallback)
    {
        if (!majorChange)
            return;

        var uiSourceCode = this._uiSourceCodeForResource(resource);
        if (!uiSourceCode) {
            userCallback("Resource is not editable");
            return;
        }

        resource.requestContent(this._setContentWithInitialContent.bind(this, uiSourceCode, content, userCallback));
    },

    /**
     * @param {WebInspector.Resource} resource
     * @return {WebInspector.UISourceCode}
     */
    _uiSourceCodeForResource: function(resource)
    {
        var uiSourceCodes = this._presentationModel.uiSourceCodes();
        for (var i = 0; i < uiSourceCodes.length; ++i) {
            if (uiSourceCodes[i].url === resource.url)
                return uiSourceCodes[i];
        }
        return null;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {string} content
     * @param {function(?string)} userCallback
     * @param {?string} oldContent
     * @param {boolean} oldContentEncoded
     * @param {string} mimeType
     */
    _setContentWithInitialContent: function(uiSourceCode, content, userCallback, oldContent, oldContentEncoded, mimeType)
    {
        /**
         * @this {WebInspector.DebuggerPresentationModelResourceBinding}
         * @param {?string} error
         */
        function callback(error)
        {
            if (userCallback)
                userCallback(error);
        }
        this._presentationModel.setScriptSource(uiSourceCode, content, callback.bind(this));
    }
}

WebInspector.DebuggerPresentationModelResourceBinding.prototype.__proto__ = WebInspector.ResourceDomainModelBinding.prototype;

/**
 * @type {?WebInspector.DebuggerPresentationModel}
 */
WebInspector.debuggerPresentationModel = null;
