/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @implements {WebInspector.ContentProvider}
 * @param {?WebInspector.NetworkRequest} request
 * @param {string} url
 * @param {string} documentURL
 * @param {NetworkAgent.FrameId} frameId
 * @param {NetworkAgent.LoaderId} loaderId
 * @param {WebInspector.ResourceType} type
 * @param {string} mimeType
 * @param {boolean=} isHidden
 */
WebInspector.Resource = function(request, url, documentURL, frameId, loaderId, type, mimeType, isHidden)
{
    this._request = request;
    if (this._request)
        this._request.setResource(this);
    this.url = url;
    this._documentURL = documentURL;
    this._frameId = frameId;
    this._loaderId = loaderId;
    this._type = type || WebInspector.resourceTypes.Other;
    this._mimeType = mimeType;
    this.history = [];
    this._isHidden = isHidden;

    /** @type {?string} */ this._content;
    /** @type {boolean} */ this._contentEncoded;
    this._pendingContentCallbacks = [];
}

WebInspector.Resource._domainModelBindings = [];

/**
 * @param {WebInspector.ResourceType} type
 * @param {WebInspector.ResourceDomainModelBinding} binding
 */
WebInspector.Resource.registerDomainModelBinding = function(type, binding)
{
    WebInspector.Resource._domainModelBindings[type.name()] = binding;
}

WebInspector.Resource._resourceRevisionRegistry = function()
{
    if (!WebInspector.Resource._resourceRevisionRegistryObject) {
        if (window.localStorage) {
            var resourceHistory = window.localStorage["resource-history"];
            try {
                WebInspector.Resource._resourceRevisionRegistryObject = resourceHistory ? JSON.parse(resourceHistory) : {};
            } catch (e) {
                WebInspector.Resource._resourceRevisionRegistryObject = {};
            }
        } else
            WebInspector.Resource._resourceRevisionRegistryObject = {};
    }
    return WebInspector.Resource._resourceRevisionRegistryObject;
}

WebInspector.Resource.restoreRevisions = function()
{
    var registry = WebInspector.Resource._resourceRevisionRegistry();
    var filteredRegistry = {};
    for (var url in registry) {
        var historyItems = registry[url];
        var resource = WebInspector.resourceForURL(url);

        var filteredHistoryItems = [];
        for (var i = 0; historyItems && i < historyItems.length; ++i) {
            var historyItem = historyItems[i];
            if (resource && historyItem.loaderId === resource.loaderId) {
                resource.addRevision(window.localStorage[historyItem.key], new Date(historyItem.timestamp), true);
                filteredHistoryItems.push(historyItem);
                filteredRegistry[url] = filteredHistoryItems;
            } else
                delete window.localStorage[historyItem.key];
        }
    }
    WebInspector.Resource._resourceRevisionRegistryObject = filteredRegistry;

    function persist()
    {
        window.localStorage["resource-history"] = JSON.stringify(filteredRegistry);
    }

    // Schedule async storage.
    setTimeout(persist, 0);
}

/**
 * @param {WebInspector.ResourceRevision} revision
 */
WebInspector.Resource.persistRevision = function(revision)
{
    if (!window.localStorage)
        return;

    if (revision.resource.url.startsWith("inspector://"))
        return;

    var resource = revision.resource;
    var url = resource.url;
    var loaderId = resource.loaderId;
    var timestamp = revision.timestamp.getTime();
    var key = "resource-history|" + url + "|" + loaderId + "|" + timestamp;
    var content = revision._content;

    var registry = WebInspector.Resource._resourceRevisionRegistry();

    var historyItems = registry[resource.url];
    if (!historyItems) {
        historyItems = [];
        registry[resource.url] = historyItems;
    }
    historyItems.push({url: url, loaderId: loaderId, timestamp: timestamp, key: key});

    function persist()
    {
        window.localStorage[key] = content;
        window.localStorage["resource-history"] = JSON.stringify(registry);
    }

    // Schedule async storage.
    setTimeout(persist, 0);
}

WebInspector.Resource.Events = {
    RevisionAdded: "revision-added",
    MessageAdded: "message-added",
    MessagesCleared: "messages-cleared",
}

WebInspector.Resource.prototype = {
    /**
     * @return {?WebInspector.NetworkRequest}
     */
    get request()
    {
        return this._request;
    },

    /**
     * @return {string}
     */
    get url()
    {
        return this._url;
    },

    set url(x)
    {
        this._url = x;
        this._parsedURL = new WebInspector.ParsedURL(x);
    },

    get parsedURL()
    {
        return this._parsedURL;
    },

    /**
     * @return {string}
     */
    get documentURL()
    {
        return this._documentURL;
    },

    /**
     * @return {NetworkAgent.FrameId}
     */
    get frameId()
    {
        return this._frameId;
    },

    /**
     * @return {NetworkAgent.LoaderId}
     */
    get loaderId()
    {
        return this._loaderId;
    },

    /**
     * @return {string}
     */
    get displayName()
    {
        return this._parsedURL.displayName;
    },

    /**
     * @return {WebInspector.ResourceType}
     */
    get type()
    {
        return this._request ? this._request.type : this._type;
    },

    /**
     * @return {string}
     */
    get mimeType()
    {
        return this._request ? this._request.mimeType : this._mimeType;
    },

    /**
     * @return {Array.<WebInspector.ConsoleMessage>}
     */
    get messages()
    {
        return this._messages || [];
    },

    /**
     * @param {WebInspector.ConsoleMessage} msg
     */
    addMessage: function(msg)
    {
        if (!msg.isErrorOrWarning() || !msg.message)
            return;

        if (!this._messages)
            this._messages = [];
        this._messages.push(msg);
        this.dispatchEventToListeners(WebInspector.Resource.Events.MessageAdded, msg);
    },

    /**
     * @return {number}
     */
    get errors()
    {
        return this._errors || 0;
    },

    set errors(x)
    {
        this._errors = x;
    },

    /**
     * @return {number}
     */
    get warnings()
    {
        return this._warnings || 0;
    },

    set warnings(x)
    {
        this._warnings = x;
    },

    clearErrorsAndWarnings: function()
    {
        this._messages = [];
        this._warnings = 0;
        this._errors = 0;
        this.dispatchEventToListeners(WebInspector.Resource.Events.MessagesCleared);
    },

    /**
     * @return {?string}
     */
    get content()
    {
        return this._content;
    },

    /**
     * @return {boolean}
     */
    get contentEncoded()
    {
        return this._contentEncoded;
    },

    /**
     * @return {boolean}
     */
    isEditable: function()
    {
        if (this._actualResource)
            return false;
        var binding = WebInspector.Resource._domainModelBindings[this.type.name()];
        return binding && binding.canSetContent(this);
    },

    /**
     * @param {string} newContent
     * @param {boolean} majorChange
     * @param {function(?string)} callback
     */
    setContent: function(newContent, majorChange, callback)
    {
        if (!this.isEditable()) {
            if (callback)
                callback("Resource is not editable");
            return;
        }
        var binding = WebInspector.Resource._domainModelBindings[this.type.name()];
        binding.setContent(this, newContent, majorChange, callback);
    },

    /**
     * @param {string} content
     * @param {Date=} timestamp
     * @param {boolean=} restoringHistory
     */
    addRevision: function(content, timestamp, restoringHistory)
    {
        if (this.history.length) {
            var lastRevision = this.history[this.history.length - 1];
            if (lastRevision._content === content)
                return;
        }
        var revision = new WebInspector.ResourceRevision(this, content, timestamp || new Date());
        this.history.push(revision);

        this.dispatchEventToListeners(WebInspector.Resource.Events.RevisionAdded, revision);
        if (!restoringHistory)
            revision._persistRevision();
        WebInspector.resourceTreeModel.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.ResourceContentCommitted, { resource: this, content: content });
    },

    /**
     * @return {?string}
     */
    contentURL: function()
    {
        return this._url;
    },

    /**
     * @return {WebInspector.ResourceType}
     */
    contentType: function()
    {
        return this.type;
    },

    /**
     * @param {function(?string, boolean, string)} callback
     */
    requestContent: function(callback)
    {
        if (typeof this._content !== "undefined") {
            callback(this._content, !!this._contentEncoded, this.canonicalMimeType());
            return;
        }

        this._pendingContentCallbacks.push(callback);
        this._innerRequestContent();
    },

    canonicalMimeType: function()
    {
        return this.type.canonicalMimeType() || this.mimeType;
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(Array.<WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInContent: function(query, caseSensitive, isRegex, callback)
    {
        /**
         * @param {?Protocol.Error} error
         * @param {Array.<PageAgent.SearchMatch>} searchMatches
         */
        function callbackWrapper(error, searchMatches)
        {
            callback(searchMatches || []);
        }

        if (this.frameId)
            PageAgent.searchInResource(this.frameId, this.url, query, caseSensitive, isRegex, callbackWrapper);
        else
            callback([]);
    },

    /**
     * @param {Element} image
     */
    populateImageSource: function(image)
    {
        function onResourceContent()
        {
            image.src = this._contentURL();
        }

        this.requestContent(onResourceContent.bind(this));
    },

    /**
     * @return {string}
     */
    _contentURL: function()
    {
        const maxDataUrlSize = 1024 * 1024;
        // If resource content is not available or won't fit a data URL, fall back to using original URL.
        if (this._content == null || this._content.length > maxDataUrlSize)
            return this.url;

        return "data:" + this.mimeType + (this._contentEncoded ? ";base64," : ",") + this._content;
    },

    _innerRequestContent: function()
    {
        if (this._contentRequested)
            return;
        this._contentRequested = true;

        /**
         * @param {?Protocol.Error} error
         * @param {string} content
         * @param {boolean} contentEncoded
         */
        function callback(error, content, contentEncoded)
        {
            this._content = error ? null : content;
            this._contentEncoded = contentEncoded;
            var callbacks = this._pendingContentCallbacks.slice();
            for (var i = 0; i < callbacks.length; ++i)
                callbacks[i](this._content, this._contentEncoded, this.canonicalMimeType());
            this._pendingContentCallbacks.length = 0;
            delete this._contentRequested;
        }
        PageAgent.getResourceContent(this.frameId, this.url, callback.bind(this));
    },

    revertToOriginal: function()
    {
        function revert(content)
        {
            this.setContent(content, true, function() {});
        }
        this.requestContent(revert.bind(this));
    },

    /**
     * @return {boolean}
     */
    isHidden: function()
    {
        return !!this._isHidden; 
    }
}

WebInspector.Resource.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @implements {WebInspector.ContentProvider}
 * @param {WebInspector.Resource} resource
 * @param {?string|undefined} content
 * @param {Date} timestamp
 */
WebInspector.ResourceRevision = function(resource, content, timestamp)
{
    this._resource = resource;
    this._content = content;
    this._timestamp = timestamp;
}

WebInspector.ResourceRevision.prototype = {
    /**
     * @return {WebInspector.Resource}
     */
    get resource()
    {
        return this._resource;
    },

    /**
     * @return {Date}
     */
    get timestamp()
    {
        return this._timestamp;
    },

    /**
     * @return {?string}
     */
    get content()
    {
        return this._content || null;
    },

    revertToThis: function()
    {
        function revert(content)
        {
            if (this._resource._content !== content)
                this._resource.setContent(content, true, function() {});
        }
        this.requestContent(revert.bind(this));
    },

    /**
     * @return {?string}
     */
    contentURL: function()
    {
        return this._resource.url;
    },

    /**
     * @return {WebInspector.ResourceType}
     */
    contentType: function()
    {
        return this._resource.contentType();
    },

    /**
     * @param {function(?string, boolean, string)} callback
     */
    requestContent: function(callback)
    {
        callback(this._content || "", false, this.resource.mimeType);
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(Array.<WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInContent: function(query, caseSensitive, isRegex, callback)
    {
        callback([]);
    },

    _persistRevision: function()
    {
        WebInspector.Resource.persistRevision(this);
    }
}

/**
 * @interface
 */
WebInspector.ResourceDomainModelBinding = function() { }

WebInspector.ResourceDomainModelBinding.prototype = {
    /**
     * @param {WebInspector.Resource} resource
     * @return {boolean}
     */
    canSetContent: function(resource) { return true; },

    /**
     * @param {WebInspector.Resource} resource
     * @param {string} content
     * @param {boolean} majorChange
     * @param {function(?string)} callback
     */
    setContent: function(resource, content, majorChange, callback) { }
}
