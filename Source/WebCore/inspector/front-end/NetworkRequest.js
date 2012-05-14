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
 * @implements {WebInspector.ContentProvider}
 * @param {NetworkAgent.RequestId} requestId
 * @param {string} url
 * @param {string} documentURL
 * @param {NetworkAgent.FrameId} frameId
 * @param {NetworkAgent.LoaderId} loaderId
 */
WebInspector.NetworkRequest = function(requestId, url, documentURL, frameId, loaderId)
{
    this._requestId = requestId;
    this.url = url;
    this._documentURL = documentURL;
    this._frameId = frameId;
    this._loaderId = loaderId;
    this._startTime = -1;
    this._endTime = -1;

    this.statusCode = 0;
    this.statusText = "";
    this.requestMethod = "";
    this.requestTime = 0;
    this.receiveHeadersEnd = 0;

    this._type = WebInspector.resourceTypes.Other;
    this._content = undefined;
    this._contentEncoded = false;
    this._pendingContentCallbacks = [];
    this._frames = [];
}

WebInspector.NetworkRequest.Events = {
    FinishedLoading: "FinishedLoading",
    TimingChanged: "TimingChanged",
    RequestHeadersChanged: "RequestHeadersChanged",
    ResponseHeadersChanged: "ResponseHeadersChanged",
}

WebInspector.NetworkRequest.prototype = {
    /**
     * @return {NetworkAgent.RequestId}
     */
    get requestId()
    {
        return this._requestId;
    },

    set requestId(requestId)
    {
        this._requestId = requestId;
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
        if (this._url === x)
            return;

        this._url = x;
        this._parsedURL = new WebInspector.ParsedURL(x);
        delete this._parsedQueryParameters;
    },

    /**
     * @return {string}
     */
    get documentURL()
    {
        return this._documentURL;
    },

    get parsedURL()
    {
        return this._parsedURL;
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
     * @return {number}
     */
    get startTime()
    {
        return this._startTime || -1;
    },

    set startTime(x)
    {
        this._startTime = x;
    },

    /**
     * @return {number}
     */
    get responseReceivedTime()
    {
        return this._responseReceivedTime || -1;
    },

    set responseReceivedTime(x)
    {
        this._responseReceivedTime = x;
    },

    /**
     * @return {number}
     */
    get endTime()
    {
        return this._endTime || -1;
    },

    set endTime(x)
    {
        if (this.timing && this.timing.requestTime) {
            // Check against accurate responseReceivedTime.
            this._endTime = Math.max(x, this.responseReceivedTime);
        } else {
            // Prefer endTime since it might be from the network stack.
            this._endTime = x;
            if (this._responseReceivedTime > x)
                this._responseReceivedTime = x;
        }
    },

    /**
     * @return {number}
     */
    get duration()
    {
        if (this._endTime === -1 || this._startTime === -1)
            return -1;
        return this._endTime - this._startTime;
    },

    /**
     * @return {number}
     */
    get latency()
    {
        if (this._responseReceivedTime === -1 || this._startTime === -1)
            return -1;
        return this._responseReceivedTime - this._startTime;
    },

    /**
     * @return {number}
     */
    get receiveDuration()
    {
        if (this._endTime === -1 || this._responseReceivedTime === -1)
            return -1;
        return this._endTime - this._responseReceivedTime;
    },

    /**
     * @return {number}
     */
    get resourceSize()
    {
        return this._resourceSize || 0;
    },

    set resourceSize(x)
    {
        this._resourceSize = x;
    },

    /**
     * @return {number}
     */
    get transferSize()
    {
        if (this.cached)
            return 0;
        if (this.statusCode === 304) // Not modified
            return this.responseHeadersSize;
        if (this._transferSize !== undefined)
            return this._transferSize;
        // If we did not receive actual transfer size from network
        // stack, we prefer using Content-Length over resourceSize as
        // resourceSize may differ from actual transfer size if platform's
        // network stack performed decoding (e.g. gzip decompression).
        // The Content-Length, though, is expected to come from raw
        // response headers and will reflect actual transfer length.
        // This won't work for chunked content encoding, so fall back to
        // resourceSize when we don't have Content-Length. This still won't
        // work for chunks with non-trivial encodings. We need a way to
        // get actual transfer size from the network stack.
        var bodySize = Number(this.responseHeaderValue("Content-Length") || this.resourceSize);
        return this.responseHeadersSize + bodySize;
    },

    /**
     * @param {number} x
     */
    increaseTransferSize: function(x)
    {
        this._transferSize = (this._transferSize || 0) + x;
    },

    /**
     * @return {boolean}
     */
    get finished()
    {
        return this._finished;
    },

    set finished(x)
    {
        if (this._finished === x)
            return;

        this._finished = x;

        if (x) {
            this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.FinishedLoading, this);
            if (this._pendingContentCallbacks.length)
                this._innerRequestContent();
        }
    },

    /**
     * @return {boolean}
     */
    get failed()
    {
        return this._failed;
    },

    set failed(x)
    {
        this._failed = x;
    },

    /**
     * @return {boolean}
     */
    get canceled()
    {
        return this._canceled;
    },

    set canceled(x)
    {
        this._canceled = x;
    },

    /**
     * @return {boolean}
     */
    get cached()
    {
        return this._cached;
    },

    set cached(x)
    {
        this._cached = x;
        if (x)
            delete this._timing;
    },

    /**
     * @return {NetworkAgent.ResourceTiming|undefined}
     */
    get timing()
    {
        return this._timing;
    },

    set timing(x)
    {
        if (x && !this._cached) {
            // Take startTime and responseReceivedTime from timing data for better accuracy.
            // Timing's requestTime is a baseline in seconds, rest of the numbers there are ticks in millis.
            this._startTime = x.requestTime;
            this._responseReceivedTime = x.requestTime + x.receiveHeadersEnd / 1000.0;

            this._timing = x;
            this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.TimingChanged, this);
        }
    },

    /**
     * @return {string}
     */
    get mimeType()
    {
        return this._mimeType;
    },

    set mimeType(x)
    {
        this._mimeType = x;
    },

    /**
     * @return {string}
     */
    get displayName()
    {
        return this._parsedURL.displayName;
    },

    /**
     * @return {string}
     */
    get folder()
    {
        var path = this._parsedURL.path;
        var indexOfQuery = path.indexOf("?");
        if (indexOfQuery !== -1)
            path = path.substring(0, indexOfQuery);
        var lastSlashIndex = path.lastIndexOf("/");
        return lastSlashIndex !== -1 ? path.substring(0, lastSlashIndex) : "";
    },

    /**
     * @return {WebInspector.ResourceType}
     */
    get type()
    {
        return this._type;
    },

    set type(x)
    {
        this._type = x;
    },

    /**
     * @return {WebInspector.Resource|undefined}
     */
    get redirectSource()
    {
        if (this.redirects && this.redirects.length > 0)
            return this.redirects[this.redirects.length - 1];
        return this._redirectSource;
    },

    set redirectSource(x)
    {
        this._redirectSource = x;
    },

    /**
     * @return {Array.<Object>}
     */
    get requestHeaders()
    {
        return this._requestHeaders || [];
    },

    set requestHeaders(x)
    {
        this._requestHeaders = x;
        delete this._sortedRequestHeaders;
        delete this._requestCookies;

        this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.RequestHeadersChanged);
    },

    /**
     * @return {string}
     */
    get requestHeadersText()
    {
        if (this._requestHeadersText === undefined) {
            this._requestHeadersText = this.requestMethod + " " + this.url + " HTTP/1.1\r\n";
            for (var i = 0; i < this.requestHeaders; ++i)
                this._requestHeadersText += this.requestHeaders[i].name + ": " + this.requestHeaders[i].value + "\r\n";
        }
        return this._requestHeadersText;
    },

    set requestHeadersText(x)
    {
        this._requestHeadersText = x;

        this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.RequestHeadersChanged);
    },

    /**
     * @return {number}
     */
    get requestHeadersSize()
    {
        return this.requestHeadersText.length;
    },

    /**
     * @return {Array.<Object>}
     */
    get sortedRequestHeaders()
    {
        if (this._sortedRequestHeaders !== undefined)
            return this._sortedRequestHeaders;

        this._sortedRequestHeaders = [];
        this._sortedRequestHeaders = this.requestHeaders.slice();
        this._sortedRequestHeaders.sort(function(a,b) { return a.name.toLowerCase().localeCompare(b.name.toLowerCase()) });
        return this._sortedRequestHeaders;
    },

    /**
     * @param {string} headerName
     * @return {string|undefined}
     */
    requestHeaderValue: function(headerName)
    {
        return this._headerValue(this.requestHeaders, headerName);
    },

    /**
     * @return {Array.<WebInspector.Cookie>}
     */
    get requestCookies()
    {
        if (!this._requestCookies)
            this._requestCookies = WebInspector.CookieParser.parseCookie(this.requestHeaderValue("Cookie"));
        return this._requestCookies;
    },

    /**
     * @return {string|undefined}
     */
    get requestFormData()
    {
        return this._requestFormData;
    },

    set requestFormData(x)
    {
        this._requestFormData = x;
        delete this._parsedFormParameters;
    },

    /**
     * @return {string|undefined}
     */
    get requestHttpVersion()
    {
        var firstLine = this.requestHeadersText.split(/\r\n/)[0];
        var match = firstLine.match(/(HTTP\/\d+\.\d+)$/);
        return match ? match[1] : undefined;
    },

    /**
     * @return {Array.<Object>}
     */
    get responseHeaders()
    {
        return this._responseHeaders || [];
    },

    set responseHeaders(x)
    {
        this._responseHeaders = x;
        delete this._sortedResponseHeaders;
        delete this._responseCookies;

        this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.ResponseHeadersChanged);
    },

    /**
     * @return {string}
     */
    get responseHeadersText()
    {
        if (this._responseHeadersText === undefined) {
            this._responseHeadersText = "HTTP/1.1 " + this.statusCode + " " + this.statusText + "\r\n";
            for (var i = 0; i < this.requestHeaders; ++i)
                this._responseHeadersText += this.responseHeaders[i].name + ": " + this.responseHeaders[i].value + "\r\n";
        }
        return this._responseHeadersText;
    },

    set responseHeadersText(x)
    {
        this._responseHeadersText = x;

        this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.ResponseHeadersChanged);
    },

    /**
     * @return {number}
     */
    get responseHeadersSize()
    {
        return this.responseHeadersText.length;
    },

    /**
     * @return {Array.<Object>}
     */
    get sortedResponseHeaders()
    {
        if (this._sortedResponseHeaders !== undefined)
            return this._sortedResponseHeaders;
        
        this._sortedResponseHeaders = [];
        this._sortedResponseHeaders = this.responseHeaders.slice();
        this._sortedResponseHeaders.sort(function(a,b) { return a.name.toLowerCase().localeCompare(b.name.toLowerCase()) });
        return this._sortedResponseHeaders;
    },

    /**
     * @param {string} headerName
     * @return {string|undefined}
     */
    responseHeaderValue: function(headerName)
    {
        return this._headerValue(this.responseHeaders, headerName);
    },

    /**
     * @return {Array.<WebInspector.Cookie>}
     */
    get responseCookies()
    {
        if (!this._responseCookies)
            this._responseCookies = WebInspector.CookieParser.parseSetCookie(this.responseHeaderValue("Set-Cookie"));
        return this._responseCookies;
    },

    /**
     * @return {?Array.<Object>}
     */
    get queryParameters()
    {
        if (this._parsedQueryParameters)
            return this._parsedQueryParameters;
        var queryString = this.url.split("?", 2)[1];
        if (!queryString)
            return null;
        queryString = queryString.split("#", 2)[0];
        this._parsedQueryParameters = this._parseParameters(queryString);
        return this._parsedQueryParameters;
    },

    /**
     * @return {?Array.<Object>}
     */
    get formParameters()
    {
        if (this._parsedFormParameters)
            return this._parsedFormParameters;
        if (!this.requestFormData)
            return null;
        var requestContentType = this.requestContentType();
        if (!requestContentType || !requestContentType.match(/^application\/x-www-form-urlencoded\s*(;.*)?$/i))
            return null;
        this._parsedFormParameters = this._parseParameters(this.requestFormData);
        return this._parsedFormParameters;
    },

    /**
     * @return {string|undefined}
     */
    get responseHttpVersion()
    {
        var match = this.responseHeadersText.match(/^(HTTP\/\d+\.\d+)/);
        return match ? match[1] : undefined;
    },

    /**
     * @param {string} queryString
     * @return {Array.<Object>}
     */
    _parseParameters: function(queryString)
    {
        function parseNameValue(pair)
        {
            var parameter = {};
            var splitPair = pair.split("=", 2);

            parameter.name = splitPair[0];
            if (splitPair.length === 1)
                parameter.value = "";
            else
                parameter.value = splitPair[1];
            return parameter;
        }
        return queryString.split("&").map(parseNameValue);
    },

    /**
     * @param {Object} headers
     * @param {string} headerName
     * @return {string|undefined}
     */
    _headerValue: function(headers, headerName)
    {
        headerName = headerName.toLowerCase();
        
        var values = [];
        for (var i = 0; i < headers.length; ++i) {
            if (headers[i].name.toLowerCase() === headerName)
                values.push(headers[i].value);
        }
        // Set-Cookie values should be separated by '\n', not comma, otherwise cookies could not be parsed.
        if (headerName === "set-cookie")
            return values.join("\n");
        return values.join(", ");
    },

    /**
     * @return {?string|undefined}
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
     * @return {?string}
     */
    contentURL: function()
    {
        return this._url;
    },

    /**
     * @param {function(?string, boolean, string)} callback
     */
    requestContent: function(callback)
    {
        // We do not support content retrieval for WebSockets at the moment.
        // Since WebSockets are potentially long-living, fail requests immediately
        // to prevent caller blocking until resource is marked as finished.
        if (this.type === WebInspector.resourceTypes.WebSocket) {
            callback(null, false, this._mimeType);
            return;
        }
        if (typeof this._content !== "undefined") {
            callback(this.content || null, this._contentEncoded, this._mimeType);
            return;
        }
        this._pendingContentCallbacks.push(callback);
        if (this.finished)
            this._innerRequestContent();
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

    /**
     * @return {boolean}
     */
    isHttpFamily: function()
    {
        return !!this.url.match(/^https?:/i);
    },

    /**
     * @return {string|undefined}
     */
    requestContentType: function()
    {
        return this.requestHeaderValue("Content-Type");
    },

    /**
     * @return {boolean}
     */
    isPingRequest: function()
    {
        return "text/ping" === this.requestContentType();
    },

    /**
     * @return {boolean}
     */
    hasErrorStatusCode: function()
    {
        return this.statusCode >= 400;
    },

    /**
     * @param {Element} image
     */
    populateImageSource: function(image)
    {
        /**
         * @param {?string} content
         * @param {boolean} contentEncoded
         * @param {string} mimeType
         */
        function onResourceContent(content, contentEncoded, mimeType)
        {
            const maxDataUrlSize = 1024 * 1024;
            // If resource content is not available or won't fit a data URL, fall back to using original URL.
            if (this._content == null || this._content.length > maxDataUrlSize)
                return this.url;
            image.src = "data:" + this.mimeType + (this._contentEncoded ? ";base64," : ",") + this._content;
        }

        this.requestContent(onResourceContent.bind(this));
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
        function onResourceContent(error, content, contentEncoded)
        {
            this._content = error ? null : content;
            this._contentEncoded = contentEncoded;
            var callbacks = this._pendingContentCallbacks.slice();
            for (var i = 0; i < callbacks.length; ++i)
                callbacks[i](this._content, this._contentEncoded, this._mimeType);
            this._pendingContentCallbacks.length = 0;
            delete this._contentRequested;
        }
        NetworkAgent.getResponseBody(this._requestId, onResourceContent.bind(this));
    },

    /**
     * @param {WebInspector.Resource} resource
     */
    setResource: function(resource)
    {
        this._resource = resource;
    },

    /**
     * @return {WebInspector.Resource}
     */
    resource: function()
    {
        return this._resource;
    },

    /**
     * @return {Object}
     */
    frames: function()
    {
        return this._frames;
    },

    /**
     * @param {number} position
     * @return {Object}
     */
    frame: function(position)
    {
        return this._frames[position];
    },

    /**
     * @param {string} errorMessage
     * @param {number} time
     */
    addFrameError: function(errorMessage, time)
    {
        var errorObject = {};
        errorObject.errorMessage = errorMessage;
        errorObject.time = time;
        this._pushFrame(errorObject);
    },

    /**
     * @param {Object} response
     * @param {number} time
     * @param {boolean} sent
     */
    addFrame: function(response, time, sent)
    {
        response.time = time;
        if (sent)
            response.sent = true;
        this._pushFrame(response);
    },

    _pushFrame: function(object)
    {
        if (this._frames.length >= 100) {
            this._frames.splice(0, 10);
        }
        this._frames.push(object);
    }
}

WebInspector.NetworkRequest.prototype.__proto__ = WebInspector.Object.prototype;
