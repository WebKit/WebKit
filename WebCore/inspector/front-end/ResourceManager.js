/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

WebInspector.ResourceManager = function()
{
    this._registerNotifyHandlers(
        "identifierForInitialRequest",
        "willSendRequest",
        "markResourceAsCached",
        "didReceiveResponse",
        "didReceiveContentLength",
        "didFinishLoading",
        "didFailLoading",
        "didLoadResourceFromMemoryCache",
        "setOverrideContent",
        "didCommitLoad",
        "frameDetachedFromParent",
        "didCreateWebSocket",
        "willSendWebSocketHandshakeRequest",
        "didReceiveWebSocketHandshakeResponse",
        "didCloseWebSocket");

    this._resources = {};
    this._resourcesByFrame = {};
    this._lastCachedId = 0;
}

WebInspector.ResourceManager.prototype = {
    _registerNotifyHandlers: function()
    {
        for (var i = 0; i < arguments.length; ++i)
            WebInspector[arguments[i]] = this[arguments[i]].bind(this);
    },

    identifierForInitialRequest: function(identifier, url, frameID, isMainResource)
    {
        var resource = new WebInspector.Resource(identifier, url);
        if (isMainResource)
            resource.isMainResource = true;
        this._resources[identifier] = resource;

        if (frameID) {
            resource.frameID = frameID;
            var resourcesForFrame = this._resourcesByFrame[frameID];
            if (!resourcesForFrame) {
                resourcesForFrame = [];
                this._resourcesByFrame[frameID] = resourcesForFrame;
            }
            resourcesForFrame.push(resource);
        }

        if (WebInspector.panels.network)
            WebInspector.panels.network.addResource(resource);
    },

    willSendRequest: function(identifier, time, request, redirectResponse)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;

        // Redirect may have empty URL and we'd like to not crash with invalid HashMap entry.
        // See http/tests/misc/will-send-request-returns-null-on-redirect.html
        if (!redirectResponse.isNull && request.url.length) {
            resource.endTime = time;
            this.didReceiveResponse(identifier, time, "Other", redirectResponse);
            resource = this._appendRedirect(resource.identifier, request.url);
        }

        resource.requestMethod = request.httpMethod;
        resource.requestHeaders = request.httpHeaderFields;
        resource.requestFormData = request.requestFormData;
        resource.startTime = time;

        if (WebInspector.panels.network)
            WebInspector.panels.network.refreshResource(resource);
    },

    _appendRedirect: function(identifier, redirectURL)
    {
        // We always store last redirect by the original id key. Rest of the redirects are referenced from within the last one.

        var originalResource = this._resources[identifier];
        var redirectIdentifier = originalResource.identifier + ":" + (originalResource.redirects ? originalResource.redirects.length : 0);
        originalResource.identifier = redirectIdentifier;
        this._resources[redirectIdentifier] = originalResource;

        this.identifierForInitialRequest(identifier, redirectURL, originalResource.frameID);

        var newResource = this._resources[identifier];
        newResource.redirects = originalResource.redirects || [];
        delete originalResource.redirects;
        newResource.redirects.push(originalResource);
        return newResource;
    },

    markResourceAsCached: function(identifier)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;

        resource.cached = true;

        if (WebInspector.panels.network)
            WebInspector.panels.network.refreshResource(resource);
    },

    didReceiveResponse: function(identifier, time, resourceType, response)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;

        resource.type = WebInspector.Resource.Type[resourceType];
        resource.mimeType = response.mimeType;
        resource.expectedContentLength = response.expectedContentLength;
        resource.textEncodingName = response.textEncodingName;
        resource.suggestedFilename = response.suggestedFilename;
        resource.statusCode = response.httpStatusCode;
        resource.statusText = response.httpStatusText;

        resource.responseHeaders = response.httpHeaderFields;
        resource.connectionReused = response.connectionReused;
        resource.connectionID = response.connectionID;
        resource.responseReceivedTime = time;

        if (response.wasCached)
            resource.cached = true;
        else
            resource.timing = response.timing;

        if (response.rawHeaders) {
            resource.requestHeaders = response.rawHeaders.requestHeaders;
            resource.responseHeaders = response.rawHeaders.responseHeaders;
        }

        if (WebInspector.panels.network)
            WebInspector.panels.network.refreshResource(resource);
    },

    didReceiveContentLength: function(identifier, time, lengthReceived)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;

        resource.resourceSize += lengthReceived;
        resource.endTime = time;

        if (WebInspector.panels.network)
            WebInspector.panels.network.refreshResource(resource);
    },

    didFinishLoading: function(identifier, finishTime)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;

        resource.finished = true;
        resource.endTime = finishTime;

        if (WebInspector.panels.network)
            WebInspector.panels.network.refreshResource(resource);
    },

    didFailLoading: function(identifier, time, localizedDescription)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;

        resource.failed = true;
        resource.endTime = time;

        if (WebInspector.panels.network)
            WebInspector.panels.network.refreshResource(resource);
    },

    didLoadResourceFromMemoryCache: function(time, frameID, cachedResource)
    {
        var identifier = "cached:" + this._lastCachedId++;
        this.identifierForInitialRequest(identifier, cachedResource.url, frameID);

        var resource = this._resources[identifier];
        resource.cached = true;
        resource.startTime = resource.responseReceivedTime = time;
        resource.resourceSize = cachedResource.encodedSize();

        this.didReceiveResponse(identifier, time, cachedResource.response);
    },

    setOverrideContent: function(identifier, sourceString, type)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;

        resource.type = WebInspector.Resource.Type[type];
        resource.overridenContent = sourceString;

        if (WebInspector.panels.network)
            WebInspector.panels.network.addResource(resource);
    },

    didCommitLoad: function(frameID)
    {
    },

    frameDetachedFromParent: function(frameID)
    {
        var resourcesForFrame = this._resourcesByFrame[frameID];
        for (var i = 0; resourcesForFrame && i < resourcesForFrame.length; ++i)
            delete this._resources[resourcesForFrame[i].identifier];
        delete this._resourcesByFrame[frameID];
    },

    didCreateWebSocket: function(identifier, requestURL)
    {
        this.identifierForInitialRequest(identifier, requestURL);
        var resource = this._resources[identifier];
        resource.type = WebInspector.Resource.Type.WebSocket;

        if (WebInspector.panels.network)
            WebInspector.panels.network.addResource(resource);
    },

    willSendWebSocketHandshakeRequest: function(identifier, time, request)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;

        resource.requestMethod = "GET";
        resource.requestHeaders = request.webSocketHeaderFields;
        resource.webSocketRequestKey3 = request.webSocketRequestKey3;
        resource.startTime = time;

        if (WebInspector.panels.network)
            WebInspector.panels.network.refreshResource(resource);
    },

    didReceiveWebSocketHandshakeResponse: function(identifier, time, response)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;

        resource.statusCode = response.statusCode;
        resource.statusText = response.statusText;
        resource.responseHeaders = response.webSocketHeaderFields;
        resource.webSocketChallengeResponse = response.webSocketChallengeResponse;
        resource.responseReceivedTime = time;

        if (WebInspector.panels.network)
            WebInspector.panels.network.refreshResource(resource);
    },

    didCloseWebSocket: function(identifier, time)
    {
        var resource = this._resources[identifier];
        if (!resource)
            return;
        resource.endTime = time;

        if (WebInspector.panels.network)
            WebInspector.panels.network.refreshResource(resource);
    }
}
