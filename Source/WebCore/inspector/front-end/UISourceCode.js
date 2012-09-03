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
 * @implements {WebInspector.ContentProvider}
 * @param {string} url
 * @param {WebInspector.Resource} resource
 * @param {WebInspector.ContentProvider} contentProvider
 * @param {WebInspector.SourceMapping=} sourceMapping
 */
WebInspector.UISourceCode = function(url, resource, contentProvider, sourceMapping)
{
    this._url = url;
    this._resource = resource;
    this._parsedURL = new WebInspector.ParsedURL(url);
    this._contentProvider = contentProvider;
    this._sourceMapping = sourceMapping;
    this.isContentScript = false;
    /**
     * @type Array.<function(?string,boolean,string)>
     */
    this._requestContentCallbacks = [];
    /**
     * @type Array.<WebInspector.LiveLocation>
     */
    this._liveLocations = [];
    /**
     * @type {Array.<WebInspector.PresentationConsoleMessage>}
     */
    this._consoleMessages = [];
    
    /**
     * @type {Array.<WebInspector.Revision>}
     */
    this.history = [];
    this._restoreRevisionHistory();
    this._formatterMapping = new WebInspector.IdentityFormatterSourceMapping();
}

WebInspector.UISourceCode.Events = {
    ContentChanged: "ContentChanged",
    WorkingCopyChanged: "WorkingCopyChanged",
    TitleChanged: "TitleChanged",
    ConsoleMessageAdded: "ConsoleMessageAdded",
    ConsoleMessageRemoved: "ConsoleMessageRemoved",
    ConsoleMessagesCleared: "ConsoleMessagesCleared"
}

WebInspector.UISourceCode.prototype = {
    /**
     * @return {string}
     */
    get url()
    {
        return this._url;
    },

    /**
     * @param {string} url
     */
    urlChanged: function(url)
    {
        this._url = url;
        this._parsedURL = new WebInspector.ParsedURL(this._url);
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.TitleChanged, null);
    },

    /**
     * @return {WebInspector.Resource}
     */
    resource: function()
    {
        return this._resource;
    },

    /**
     * @return {WebInspector.ParsedURL}
     */
    get parsedURL()
    {
        return this._parsedURL;
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
        return this._contentProvider.contentType();
    },

    /**
     * @param {function(?string,boolean,string)} callback
     */
    requestContent: function(callback)
    {
        if (this._content || this._contentLoaded) {
            callback(this._content, false, this._mimeType);
            return;
        }
        this._requestContentCallbacks.push(callback);
        if (this._requestContentCallbacks.length === 1)
            this._contentProvider.requestContent(this._fireContentAvailable.bind(this));
    },

    /**
     * @param {function(?string,boolean,string)} callback
     */
    requestOriginalContent: function(callback)
    {
        this._contentProvider.requestContent(callback);
    },

    /**
     * @param {string} newContent
     */
    _setContent: function(newContent)
    {
        this.setWorkingCopy(newContent);
        this.commitWorkingCopy(function() {});
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
        var revision = new WebInspector.Revision(this, content, timestamp || new Date());
        this.history.push(revision);

        this.contentChanged(revision.content || "", this.canonicalMimeType());
        if (!restoringHistory)
            revision._persist();
        WebInspector.workspace.dispatchEventToListeners(WebInspector.Workspace.Events.UISourceCodeContentCommitted, { uiSourceCode: this, content: content });
    },

    _restoreRevisionHistory: function()
    {
        if (!window.localStorage)
            return;

        var registry = WebInspector.Revision._revisionHistoryRegistry();
        var historyItems = registry[this.url];
        for (var i = 0; historyItems && i < historyItems.length; ++i)
            this.addRevision(window.localStorage[historyItems[i].key], new Date(historyItems[i].timestamp), true);
    },

    _clearRevisionHistory: function()
    {
        if (!window.localStorage)
            return;

        var registry = WebInspector.Revision._revisionHistoryRegistry();
        var historyItems = registry[this.url];
        for (var i = 0; historyItems && i < historyItems.length; ++i)
            delete window.localStorage[historyItems[i].key];
        delete registry[this.url];
        window.localStorage["revision-history"] = JSON.stringify(registry);
    },
   
    revertToOriginal: function()
    {
        /**
         * @param {?string} content
         * @param {boolean} contentEncoded
         * @param {string} mimeType
         */
      function callback(content, contentEncoded, mimeType)
        {
            this._setContent();
        }

        this.requestOriginalContent(callback.bind(this));
    },

    revertAndClearHistory: function(callback)
    {
        function revert(content)
        {
            this._setContent(content);
            this._clearRevisionHistory();
            this.history = [];
            callback();
        }

        this.requestOriginalContent(revert.bind(this));
    },

    /**
     * @param {string} newContent
     * @param {string} mimeType
     */
    contentChanged: function(newContent, mimeType)
    {
        this._content = newContent;
        this._mimeType = mimeType;
        this._contentLoaded = true;
        delete this._workingCopy;
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ContentChanged, {content: newContent});
    },

    /**
     * @return {boolean}
     */
    isEditable: function()
    {
        return false;
    },

    /**
     * @return {string}
     */
    workingCopy: function()
    {
        if (this.isDirty())
            return this._workingCopy;
        return this._content;
    },

    /**
     * @param {string} newWorkingCopy
     */
    setWorkingCopy: function(newWorkingCopy)
    {
        var oldWorkingCopy = this._workingCopy;
        if (this._content === newWorkingCopy)
            delete this._workingCopy;
        else
            this._workingCopy = newWorkingCopy;
        this.workingCopyChanged();
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.WorkingCopyChanged, {oldWorkingCopy: oldWorkingCopy, workingCopy: newWorkingCopy});
    },

    workingCopyChanged: function()
    {  
        // Overridden.
    },

    /**
     * @param {function(?string)} callback
     */
    commitWorkingCopy: function(callback)
    {
        if (!this.isDirty()) {
            callback(null);
            return;
        }

        var newContent = this._workingCopy;
        this.workingCopyCommitted(callback);
        this.addRevision(newContent);
    },

    /**
     * @param {function(?string)} callback
     */
    workingCopyCommitted: function(callback)
    {  
        // Overridden.
    },

    /**
     * @return {boolean}
     */
    isDirty: function()
    {
        return typeof this._workingCopy !== "undefined" && this._workingCopy !== this._content;
    },

    /**
     * @return {string}
     */
    mimeType: function()
    {
        return this._mimeType;
    },

    /**
     * @return {string}
     */
    canonicalMimeType: function()
    {
        return this.contentType().canonicalMimeType() || this._mimeType;
    },

    /**
     * @return {?string}
     */
    content: function()
    {
        return this._content;
    },

    /**
     * @param {string} query
     * @param {boolean} caseSensitive
     * @param {boolean} isRegex
     * @param {function(Array.<WebInspector.ContentProvider.SearchMatch>)} callback
     */
    searchInContent: function(query, caseSensitive, isRegex, callback)
    {
        this._contentProvider.searchInContent(query, caseSensitive, isRegex, callback);
    },

    /**
     * @param {?string} content
     * @param {boolean} contentEncoded
     * @param {string} mimeType
     */
    _fireContentAvailable: function(content, contentEncoded, mimeType)
    {
        this._contentLoaded = true;
        this._mimeType = mimeType;
        this._content = content;

        var callbacks = this._requestContentCallbacks.slice();
        this._requestContentCallbacks = [];
        for (var i = 0; i < callbacks.length; ++i)
            callbacks[i](content, contentEncoded, mimeType);

        if (this._formatOnLoad) {
            delete this._formatOnLoad;
            this.setFormatted(true);
        }
    },

    /**
     * @return {boolean}
     */
    contentLoaded: function()
    {
        return this._contentLoaded;
    },

    /**
     * @param {number} lineNumber
     * @param {number} columnNumber
     * @return {WebInspector.RawLocation}
     */
    uiLocationToRawLocation: function(lineNumber, columnNumber)
    {
        var location = this._formatterMapping.formattedToOriginal(lineNumber, columnNumber);
        return this._sourceMapping.uiLocationToRawLocation(this, location[0], location[1]);
    },

    /**
     * @param {WebInspector.LiveLocation} liveLocation
     */
    addLiveLocation: function(liveLocation)
    {
        this._liveLocations.push(liveLocation);
    },

    /**
     * @param {WebInspector.LiveLocation} liveLocation
     */
    removeLiveLocation: function(liveLocation)
    {
        this._liveLocations.remove(liveLocation);
    },

    updateLiveLocations: function()
    {
        var locationsCopy = this._liveLocations.slice();
        for (var i = 0; i < locationsCopy.length; ++i)
            locationsCopy[i].update();
    },

    /**
     * @param {WebInspector.UILocation} uiLocation
     */
    overrideLocation: function(uiLocation)
    {
        var location = this._formatterMapping.originalToFormatted(uiLocation.lineNumber, uiLocation.columnNumber);
        uiLocation.lineNumber = location[0];
        uiLocation.columnNumber = location[1];
        return uiLocation;
    },

    /**
     * @return {Array.<WebInspector.PresentationConsoleMessage>}
     */
    consoleMessages: function()
    {
        return this._consoleMessages;
    },

    /**
     * @param {WebInspector.PresentationConsoleMessage} message
     */
    consoleMessageAdded: function(message)
    {
        this._consoleMessages.push(message);
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ConsoleMessageAdded, message);
    },

    /**
     * @param {WebInspector.PresentationConsoleMessage} message
     */
    consoleMessageRemoved: function(message)
    {
        this._consoleMessages.remove(message);
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ConsoleMessageRemoved, message);
    },

    consoleMessagesCleared: function()
    {
        this._consoleMessages = [];
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ConsoleMessagesCleared);
    },

    /**
     * @return {boolean}
     */
    togglingFormatter: function()
    {
        return this._togglingFormatter;
    },

    /**
     * @return {boolean}
     */
    formatted: function()
    {
        return this._formatted;
    },

    /**
     * @param {boolean} formatted
     * @param {function()=} callback
     */
    setFormatted: function(formatted, callback)
    {
        callback = callback || function() {};
        if (!this.contentLoaded()) {
            this._formatOnLoad = formatted;
            callback();
            return;
        }

        if (this._formatted === formatted) {
            callback();
            return;
        }

        this._formatted = formatted;

        // Re-request content
        this._contentLoaded = false;
        this._content = false;
        WebInspector.UISourceCode.prototype.requestContent.call(this, didGetContent.bind(this));
  
        /**
         * @this {WebInspector.UISourceCode}
         * @param {?string} content
         * @param {boolean} contentEncoded
         * @param {string} mimeType
         */
        function didGetContent(content, contentEncoded, mimeType)
        {
            var formatter;
            if (!formatted)
                formatter = new WebInspector.IdentityFormatter();
            else
                formatter = WebInspector.Formatter.createFormatter(this.contentType());
            formatter.formatContent(mimeType, content || "", formattedChanged.bind(this));
  
            /**
             * @this {WebInspector.UISourceCode}
             * @param {string} content
             * @param {WebInspector.FormatterSourceMapping} formatterMapping
             */
            function formattedChanged(content, formatterMapping)
            {
                this._togglingFormatter = true;
                this.contentChanged(content, mimeType);
                delete this._togglingFormatter;
                this._formatterMapping = formatterMapping;
                this.updateLiveLocations();
                this.formattedChanged();
                callback();
            }
        }
    },

    /**
     * @return {WebInspector.Formatter} formatter
     */
    createFormatter: function()
    {
        // overridden by subclasses.
        return null;
    },

    formattedChanged: function()
    {
    }
}

WebInspector.UISourceCode.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @interface
 */
WebInspector.UISourceCodeProvider = function()
{
}

WebInspector.UISourceCodeProvider.Events = {
    UISourceCodeAdded: "UISourceCodeAdded",
    UISourceCodeReplaced: "UISourceCodeReplaced",
    UISourceCodeRemoved: "UISourceCodeRemoved"
}

WebInspector.UISourceCodeProvider.prototype = {
    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodes: function() {},

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
     * @return {WebInspector.RawLocation}
     */
    uiLocationToRawLocation: function()
    {
        return this.uiSourceCode.uiLocationToRawLocation(this.lineNumber, this.columnNumber);
    },

    /**
     * @return {?string}
     */
    url: function()
    {
        return this.uiSourceCode.contentURL();
    }
}

/**
 * @interface
 */
WebInspector.RawLocation = function()
{
}

/**
 * @constructor
 * @param {WebInspector.RawLocation} rawLocation
 * @param {function(WebInspector.UILocation):(boolean|undefined)} updateDelegate
 */
WebInspector.LiveLocation = function(rawLocation, updateDelegate)
{
    this._rawLocation = rawLocation;
    this._updateDelegate = updateDelegate;
    this._uiSourceCodes = [];
}

WebInspector.LiveLocation.prototype = {
    update: function()
    {
        var uiLocation = this.uiLocation();
        if (uiLocation) {
            var uiSourceCode = uiLocation.uiSourceCode;
            if (this._uiSourceCodes.indexOf(uiSourceCode) === -1) {
                uiSourceCode.addLiveLocation(this);
                this._uiSourceCodes.push(uiSourceCode);
            }
            var oneTime = this._updateDelegate(uiLocation);
            if (oneTime)
                this.dispose();
        }
    },

    /**
     * @return {WebInspector.RawLocation}
     */
    rawLocation: function()
    {
        return this._rawLocation;
    },

    /**
     * @return {WebInspector.UILocation}
     */
    uiLocation: function()
    {
        // Should be overridden by subclasses.
    },

    dispose: function()
    {
        for (var i = 0; i < this._uiSourceCodes.length; ++i)
            this._uiSourceCodes[i].removeLiveLocation(this);
        this._uiSourceCodes = [];
    }
}

/**
 * @constructor
 * @implements {WebInspector.ContentProvider}
 * @param {WebInspector.UISourceCode} uiSourceCode
 * @param {?string|undefined} content
 * @param {Date} timestamp
 */
WebInspector.Revision = function(uiSourceCode, content, timestamp)
{
    this._uiSourceCode = uiSourceCode;
    this._content = content;
    this._timestamp = timestamp;
}

WebInspector.Revision._revisionHistoryRegistry = function()
{
    if (!WebInspector.Revision._revisionHistoryRegistryObject) {
        if (window.localStorage) {
            var revisionHistory = window.localStorage["revision-history"];
            try {
                WebInspector.Revision._revisionHistoryRegistryObject = revisionHistory ? JSON.parse(revisionHistory) : {};
            } catch (e) {
                WebInspector.Revision._revisionHistoryRegistryObject = {};
            }
        } else
            WebInspector.Revision._revisionHistoryRegistryObject = {};
    }
    return WebInspector.Revision._revisionHistoryRegistryObject;
}

WebInspector.Revision.filterOutStaleRevisions = function()
{
    if (!window.localStorage)
        return;

    var registry = WebInspector.Revision._revisionHistoryRegistry();
    var filteredRegistry = {};
    for (var url in registry) {
        var historyItems = registry[url];
        var filteredHistoryItems = [];
        for (var i = 0; historyItems && i < historyItems.length; ++i) {
            var historyItem = historyItems[i];
            if (historyItem.loaderId === WebInspector.resourceTreeModel.mainFrame.loaderId) {
                filteredHistoryItems.push(historyItem);
                filteredRegistry[url] = filteredHistoryItems;
            } else
                delete window.localStorage[historyItem.key];
        }
    }
    WebInspector.Revision._revisionHistoryRegistryObject = filteredRegistry;

    function persist()
    {
        window.localStorage["revision-history"] = JSON.stringify(filteredRegistry);
    }

    // Schedule async storage.
    setTimeout(persist, 0);
}

WebInspector.Revision.prototype = {
    /**
     * @return {WebInspector.UISourceCode}
     */
    get uiSourceCode()
    {
        return this._uiSourceCode;
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
            if (this._uiSourceCode._content !== content)
                this._uiSourceCode._setContent(content);
        }
        this.requestContent(revert.bind(this));
    },

    /**
     * @return {?string}
     */
    contentURL: function()
    {
        return this._uiSourceCode.url;
    },

    /**
     * @return {WebInspector.ResourceType}
     */
    contentType: function()
    {
        return this._uiSourceCode.contentType();
    },

    /**
     * @param {function(?string, boolean, string)} callback
     */
    requestContent: function(callback)
    {
        callback(this._content || "", false, this.uiSourceCode.canonicalMimeType());
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

    _persist: function()
    {
        if (!window.localStorage)
            return;

        var url = this.contentURL();
        if (url.startsWith("inspector://"))
            return;

        var loaderId = WebInspector.resourceTreeModel.mainFrame.loaderId;
        var timestamp = this.timestamp.getTime();
        var key = "revision-history|" + url + "|" + loaderId + "|" + timestamp;

        var registry = WebInspector.Revision._revisionHistoryRegistry();

        var historyItems = registry[url];
        if (!historyItems) {
            historyItems = [];
            registry[url] = historyItems;
        }
        historyItems.push({url: url, loaderId: loaderId, timestamp: timestamp, key: key});

        function persist()
        {
            window.localStorage[key] = this._content;
            window.localStorage["revision-history"] = JSON.stringify(registry);
        }

        // Schedule async storage.
        setTimeout(persist.bind(this), 0);
    }
}
