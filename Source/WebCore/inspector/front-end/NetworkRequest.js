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
 *
 * @param {NetworkAgent.RequestId} requestId
 * @param {string} url
 * @param {NetworkAgent.FrameId} frameId
 * @param {NetworkAgent.LoaderId} loaderId
 */
WebInspector.NetworkRequest = function(requestId, url, frameId, loaderId)
{
    this._requestId = requestId;
    this._url = url;
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

    delete this._parsedQueryParameters;

    var parsedURL = url.asParsedURL();
    this.domain = parsedURL ? parsedURL.host : "";
    this.path = parsedURL ? parsedURL.path : "";
    this.urlFragment = parsedURL ? parsedURL.fragment : "";
    this.lastPathComponent = parsedURL ? parsedURL.lastPathComponent : "";
}

WebInspector.NetworkRequest.Events = {
    FinishedLoading: "FinishedLoading",
    TimingChanged: "TimingChanged",
    RequestHeadersChanged: "RequestHeadersChanged",
    ResponseHeadersChanged: "ResponseHeadersChanged",
}

WebInspector.NetworkRequest.prototype = {
    /**
     * @type {NetworkAgent.RequestId}
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
     * @type {string}
     */
    get url()
    {
        return this._url;
    },

    /**
     * @type {NetworkAgent.FrameId}
     */
    get frameId()
    {
        return this._frameId;
    },

    /**
     * @type {NetworkAgent.LoaderId}
     */
    get loaderId()
    {
        return this._loaderId;
    },

    /**
     * @type {number}
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
     * @type {number}
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
     * @type {number}
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
     * @type {number}
     */
    get duration()
    {
        if (this._endTime === -1 || this._startTime === -1)
            return -1;
        return this._endTime - this._startTime;
    },

    /**
     * @type {number}
     */
    get latency()
    {
        if (this._responseReceivedTime === -1 || this._startTime === -1)
            return -1;
        return this._responseReceivedTime - this._startTime;
    },

    /**
     * @type {number}
     */
    get receiveDuration()
    {
        if (this._endTime === -1 || this._responseReceivedTime === -1)
            return -1;
        return this._endTime - this._responseReceivedTime;
    },

    /**
     * @type {number}
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
     * @type {number}
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
        var bodySize = Number(this.responseHeaders["Content-Length"] || this.resourceSize);
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
     * @type {boolean}
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
     * @type {boolean}
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
     * @type {boolean}
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
     * @type {boolean}
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
     * @type {NetworkAgent.ResourceTiming|undefined}
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
     * @type {string}
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
     * @type {string}
     */
    get displayName()
    {
        if (this._displayName)
            return this._displayName;
        this._displayName = this.lastPathComponent;
        if (!this._displayName)
            this._displayName = this.displayDomain;
        if (!this._displayName && this.url)
            this._displayName = this.url.trimURL(WebInspector.inspectedPageDomain ? WebInspector.inspectedPageDomain : "");
        if (this._displayName === "/")
            this._displayName = this.url;
        return this._displayName;
    },

    /**
     * @type {string}
     */
    get folder()
    {
        var path = this.path;
        var indexOfQuery = path.indexOf("?");
        if (indexOfQuery !== -1)
            path = path.substring(0, indexOfQuery);
        var lastSlashIndex = path.lastIndexOf("/");
        return lastSlashIndex !== -1 ? path.substring(0, lastSlashIndex) : "";
    },

    /**
     * @type {string}
     */
    get displayDomain()
    {
        // WebInspector.Database calls this, so don't access more than this.domain.
        if (this.domain && (!WebInspector.inspectedPageDomain || (WebInspector.inspectedPageDomain && this.domain !== WebInspector.inspectedPageDomain)))
            return this.domain;
        return "";
    },

    /**
     * @type {WebInspector.ResourceType}
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
     * @type {WebInspector.Resource|undefined}
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
     * @type {Object}
     */
    get requestHeaders()
    {
        return this._requestHeaders || {};
    },

    set requestHeaders(x)
    {
        this._requestHeaders = x;
        delete this._sortedRequestHeaders;
        delete this._requestCookies;

        this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.RequestHeadersChanged);
    },

    /**
     * @type {string}
     */
    get requestHeadersText()
    {
        if (this._requestHeadersText === undefined) {
            this._requestHeadersText = this.requestMethod + " " + this.url + " HTTP/1.1\r\n";
            for (var key in this.requestHeaders)
                this._requestHeadersText += key + ": " + this.requestHeaders[key] + "\r\n";
        }
        return this._requestHeadersText;
    },

    set requestHeadersText(x)
    {
        this._requestHeadersText = x;

        this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.RequestHeadersChanged);
    },

    /**
     * @type {number}
     */
    get requestHeadersSize()
    {
        return this.requestHeadersText.length;
    },

    /**
     * @type {Array.<Object>}
     */
    get sortedRequestHeaders()
    {
        if (this._sortedRequestHeaders !== undefined)
            return this._sortedRequestHeaders;

        this._sortedRequestHeaders = [];
        for (var key in this.requestHeaders)
            this._sortedRequestHeaders.push({header: key, value: this.requestHeaders[key]});
        this._sortedRequestHeaders.sort(function(a,b) { return a.header.localeCompare(b.header) });

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
     * @type {Array.<WebInspector.Cookie>}
     */
    get requestCookies()
    {
        if (!this._requestCookies)
            this._requestCookies = WebInspector.CookieParser.parseCookie(this.requestHeaderValue("Cookie"));
        return this._requestCookies;
    },

    /**
     * @type {string|undefined}
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
     * @type {string|undefined}
     */
    get requestHttpVersion()
    {
        var firstLine = this.requestHeadersText.split(/\r\n/)[0];
        var match = firstLine.match(/(HTTP\/\d+\.\d+)$/);
        return match ? match[1] : undefined;
    },

    /**
     * @type {Object}
     */
    get responseHeaders()
    {
        return this._responseHeaders || {};
    },

    set responseHeaders(x)
    {
        this._responseHeaders = x;
        delete this._sortedResponseHeaders;
        delete this._responseCookies;

        this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.ResponseHeadersChanged);
    },

    /**
     * @type {string}
     */
    get responseHeadersText()
    {
        if (this._responseHeadersText === undefined) {
            this._responseHeadersText = "HTTP/1.1 " + this.statusCode + " " + this.statusText + "\r\n";
            for (var key in this.responseHeaders)
                this._responseHeadersText += key + ": " + this.responseHeaders[key] + "\r\n";
        }
        return this._responseHeadersText;
    },

    set responseHeadersText(x)
    {
        this._responseHeadersText = x;

        this.dispatchEventToListeners(WebInspector.NetworkRequest.Events.ResponseHeadersChanged);
    },

    /**
     * @type {number}
     */
    get responseHeadersSize()
    {
        return this.responseHeadersText.length;
    },

    /**
     * @type {Array.<Object>}
     */
    get sortedResponseHeaders()
    {
        if (this._sortedResponseHeaders !== undefined)
            return this._sortedResponseHeaders;

        this._sortedResponseHeaders = [];
        for (var key in this.responseHeaders)
            this._sortedResponseHeaders.push({header: key, value: this.responseHeaders[key]});
        this._sortedResponseHeaders.sort(function(a,b) { return a.header.localeCompare(b.header) });

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
     * @type {Array.<WebInspector.Cookie>}
     */
    get responseCookies()
    {
        if (!this._responseCookies)
            this._responseCookies = WebInspector.CookieParser.parseSetCookie(this.responseHeaderValue("Set-Cookie"));
        return this._responseCookies;
    },

    /**
     * @type {?Array.<Object>}
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
     * @type {?Array.<Object>}
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
     * @type {string|undefined}
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
        for (var header in headers) {
            if (header.toLowerCase() === headerName)
                return headers[header];
        }
    },

    /**
     * @type {string}
     */
    get content()
    {
        return this._content;
    },

    /**
     * @type {string}
     */
    get contentEncoded()
    {
        return this._contentEncoded;
    },

    /**
     * @param {function(?string, boolean)} callback
     */
    requestContent: function(callback)
    {
        // We do not support content retrieval for WebSockets at the moment.
        // Since WebSockets are potentially long-living, fail requests immediately
        // to prevent caller blocking until resource is marked as finished.
        if (this.type === WebInspector.resourceTypes.WebSocket) {
            callback(null, false);
            return;
        }
        if (typeof this._content !== "undefined") {
            callback(this.content, this._contentEncoded);
            return;
        }
        this._pendingContentCallbacks.push(callback);
        if (this.finished)
            this._innerRequestContent();
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
        function onResourceContent(error, content, contentEncoded)
        {
            this._content = error ? null : content;
            this._contentEncoded = contentEncoded;
            var callbacks = this._pendingContentCallbacks.slice();
            for (var i = 0; i < callbacks.length; ++i)
                callbacks[i](this._content, this._contentEncoded);
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
    }
}

WebInspector.NetworkRequest.prototype.__proto__ = WebInspector.Object.prototype;
