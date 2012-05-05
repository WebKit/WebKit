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
 * @param {string} id
 * @param {string} url
 * @param {WebInspector.ContentProvider} contentProvider
 */
WebInspector.UISourceCode = function(id, url, contentProvider)
{
    this._id = id;
    this._url = url;
    this._parsedURL = new WebInspector.ParsedURL(url);
    this._contentProvider = contentProvider;
    this.isContentScript = false;
    this.isEditable = false;
    /**
     * @type Array.<function(?string,boolean,string)>
     */
    this._requestContentCallbacks = [];
    this._liveLocations = [];
}

WebInspector.UISourceCode.Events = {
    ContentChanged: "ContentChanged",
    ConsoleMessageAdded: "ConsoleMessageAdded",
    ConsoleMessagesCleared: "ConsoleMessagesCleared"
}

WebInspector.UISourceCode.prototype = {
    /**
     * @return {string}
     */
    get id()
    {
        return this._id;
    },

    /**
     * @return {string}
     */
    get url()
    {
        return this._url;
    },

    /**
     * @return {WebInspector.ParsedURL}
     */
    get parsedURL()
    {
        return this._parsedURL;
    },

    /**
     * @param {function(?string,boolean,string)} callback
     */
    requestContent: function(callback)
    {
        if (this._contentLoaded) {
            callback(this._content, false, this._mimeType);
            return;
        }

        this._requestContentCallbacks.push(callback);
        if (this._requestContentCallbacks.length === 1)
            this._contentProvider.requestContent(this._didRequestContent.bind(this));
    },

    /**
     * @param {string} newContent
     */
    contentChanged: function(newContent)
    {
        console.assert(this._contentLoaded);
        var oldContent = this._content;
        this._content = newContent;
        this.dispatchEventToListeners(WebInspector.UISourceCode.Events.ContentChanged, {oldContent: oldContent, content: newContent});
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
    _didRequestContent: function(content, contentEncoded, mimeType)
    {
        this._contentLoaded = true;
        this._mimeType = mimeType;
        this._content = content;

        for (var i = 0; i < this._requestContentCallbacks.length; ++i)
            this._requestContentCallbacks[i](content, contentEncoded, mimeType);
        this._requestContentCallbacks = [];
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
     * @return {WebInspector.UILocation}
     */
    overrideLocation: function(uiLocation)
    {
        return uiLocation;
    },

    /**
     * @return {Array.<WebInspector.PresentationConsoleMessage>}
     */
    consoleMessages: function() {}
}

WebInspector.UISourceCode.prototype.__proto__ = WebInspector.Object.prototype;
