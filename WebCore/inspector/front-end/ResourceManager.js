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
        "didCommitLoadForFrame",
        "frameDetachedFromParent",
        "didCreateWebSocket",
        "willSendWebSocketHandshakeRequest",
        "didReceiveWebSocketHandshakeResponse",
        "didCloseWebSocket");

    this._resourcesById = {};
    this._resourcesByFrame = {};
    this._lastCachedId = 0;
    InspectorBackend.cachedResources(this._processCachedResources.bind(this));
}

WebInspector.ResourceManager.prototype = {
    _registerNotifyHandlers: function()
    {
        for (var i = 0; i < arguments.length; ++i)
            WebInspector[arguments[i]] = this[arguments[i]].bind(this);
    },

    identifierForInitialRequest: function(identifier, url, loader, isMainResource)
    {
        var resource = this._createResource(identifier, url, loader);
        if (isMainResource)
            resource.isMainResource = true;

        WebInspector.panels.network.addResource(resource);
    },

    _createResource: function(identifier, url, loader)
    {
        var resource = new WebInspector.Resource(identifier, url);
        this._resourcesById[identifier] = resource;

        resource.loader = loader;
        var resourcesForFrame = this._resourcesByFrame[loader.frameId];
        if (!resourcesForFrame) {
            resourcesForFrame = {};
            this._resourcesByFrame[loader.frameId] = resourcesForFrame;
        }
        resourcesForFrame[resource.identifier] = resource;
        return resource;
    },

    willSendRequest: function(identifier, time, request, redirectResponse)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;

        // Redirect may have empty URL and we'd like to not crash with invalid HashMap entry.
        // See http/tests/misc/will-send-request-returns-null-on-redirect.html
        var isRedirect = !redirectResponse.isNull && request.url.length;
        if (isRedirect) {
            resource.endTime = time;
            this.didReceiveResponse(identifier, time, "Other", redirectResponse);
            resource = this._appendRedirect(resource.identifier, request.url);
        }

        resource.requestMethod = request.httpMethod;
        resource.requestHeaders = request.httpHeaderFields;
        resource.requestFormData = request.requestFormData;
        resource.startTime = time;

        if (isRedirect)
            WebInspector.panels.network.addResource(resource);
        else
            WebInspector.panels.network.refreshResource(resource);
    },

    _appendRedirect: function(identifier, redirectURL)
    {
        // We always store last redirect by the original id key. Rest of the redirects are referenced from within the last one.

        var originalResource = this._resourcesById[identifier];
        var redirectIdentifier = originalResource.identifier + ":" + (originalResource.redirects ? originalResource.redirects.length : 0);
        originalResource.identifier = redirectIdentifier;
        this._resourcesById[redirectIdentifier] = originalResource;

        var newResource = this._createResource(identifier, redirectURL, originalResource.loader);
        newResource.redirects = originalResource.redirects || [];
        delete originalResource.redirects;
        newResource.redirects.push(originalResource);
        return newResource;
    },

    markResourceAsCached: function(identifier)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;

        resource.cached = true;
        WebInspector.panels.network.refreshResource(resource);
    },

    didReceiveResponse: function(identifier, time, resourceType, response)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;
        this._updateResourceWithResponse(resource, response);
        resource.type = WebInspector.Resource.Type[resourceType];
        resource.responseReceivedTime = time;

        WebInspector.panels.network.refreshResource(resource);
    },

    _updateResourceWithResponse: function(resource, response)
    {
        resource.mimeType = response.mimeType;
        resource.expectedContentLength = response.expectedContentLength;
        resource.textEncodingName = response.textEncodingName;
        resource.suggestedFilename = response.suggestedFilename;
        resource.statusCode = response.httpStatusCode;
        resource.statusText = response.httpStatusText;

        resource.responseHeaders = response.httpHeaderFields;
        resource.connectionReused = response.connectionReused;
        resource.connectionID = response.connectionID;

        if (response.wasCached)
            resource.cached = true;
        else
            resource.timing = response.timing;

        if (response.rawHeaders) {
            resource.requestHeaders = response.rawHeaders.requestHeaders;
            resource.responseHeaders = response.rawHeaders.responseHeaders;
        }
    },

    didReceiveContentLength: function(identifier, time, lengthReceived)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;

        resource.resourceSize += lengthReceived;
        resource.endTime = time;

        WebInspector.panels.network.refreshResource(resource);
    },

    didFinishLoading: function(identifier, finishTime)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;

        resource.finished = true;
        resource.endTime = finishTime;

        WebInspector.panels.network.refreshResource(resource);
    },

    didFailLoading: function(identifier, time, localizedDescription)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;

        resource.failed = true;
        resource.endTime = time;

        WebInspector.panels.network.refreshResource(resource);
    },

    didLoadResourceFromMemoryCache: function(time, cachedResource)
    {
        var identifier = "cached:" + this._lastCachedId++;
        var resource = this._createResource(identifier, cachedResource.url, cachedResource.loader);
        this._updateResourceWithCachedResource(resource, cachedResource);
        resource.cached = true;
        resource.startTime = resource.responseReceivedTime = resource.endTime = time;

        WebInspector.panels.network.addResource(resource);
    },

    _updateResourceWithCachedResource: function(resource, cachedResource)
    {
        resource.type = WebInspector.Resource.Type[cachedResource.type];
        resource.resourceSize = cachedResource.encodedSize;
        this._updateResourceWithResponse(resource, cachedResource.response);
    },

    setOverrideContent: function(identifier, sourceString, type)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;

        resource.type = WebInspector.Resource.Type[type];
        resource.overridenContent = sourceString;

        WebInspector.panels.network.addResource(resource);
    },

    didCommitLoadForFrame: function(frameTree, loaderId)
    {
        this._clearResources(frameTree.id, loaderId);
        for (var i = 0; frameTree.children && frameTree.children.length; ++i)
            this.didCommitLoadForFrame(frameTree.children[i], loaderId);
    },

    frameDetachedFromParent: function(frameTree)
    {
        this.didCommitLoadForFrame(frameTree, 0);
    },

    _clearResources: function(frameId, loaderToPreserveId)
    {
        var resourcesForFrame = this._resourcesByFrame[frameId];
        if (resourcesForFrame)
            return;

        for (var id in resourcesForFrame) {
            var resource = this._resourcesById[id];
            if (resource.loaderId === loaderToPreserveId)
                continue;
            delete this._resourcesById[id];
            delete resourcesForFrame[id];
        }
        if (!Object.keys(resourcesForFrame).length)
            delete this._resourcesByFrame[frameId];
    },

    didCreateWebSocket: function(identifier, requestURL)
    {
        this.identifierForInitialRequest(identifier, requestURL);
        var resource = this._resourcesById[identifier];
        resource.type = WebInspector.Resource.Type.WebSocket;

        WebInspector.panels.network.addResource(resource);
    },

    willSendWebSocketHandshakeRequest: function(identifier, time, request)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;

        resource.requestMethod = "GET";
        resource.requestHeaders = request.webSocketHeaderFields;
        resource.webSocketRequestKey3 = request.webSocketRequestKey3;
        resource.startTime = time;

        WebInspector.panels.network.refreshResource(resource);
    },

    didReceiveWebSocketHandshakeResponse: function(identifier, time, response)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;

        resource.statusCode = response.statusCode;
        resource.statusText = response.statusText;
        resource.responseHeaders = response.webSocketHeaderFields;
        resource.webSocketChallengeResponse = response.webSocketChallengeResponse;
        resource.responseReceivedTime = time;

        WebInspector.panels.network.refreshResource(resource);
    },

    didCloseWebSocket: function(identifier, time)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;
        resource.endTime = time;

        WebInspector.panels.network.refreshResource(resource);
    },

    _processCachedResources: function(mainFramePayload)
    {
        this._appendFramesRecursively(null, mainFramePayload);
    },

    _appendFramesRecursively: function(parentFrameId, framePayload)
    {
        var frameResource = this._createResource(null, framePayload.resource.url, framePayload.resource.loader);
        frameResource.type = WebInspector.Resource.Type["Document"];
        WebInspector.panels.storage.addFrame(parentFrameId, framePayload.id, frameResource);

        for (var i = 0; framePayload.children && i < framePayload.children.length; ++i)
            this._appendFramesRecursively(framePayload.id, framePayload.children[i]);

        if (!framePayload.subresources)
            return;

        var resources = [];
        for (var i = 0; i < framePayload.subresources.length; ++i) {
            var cachedResource = framePayload.subresources[i];
            var resource = this._createResource(null, cachedResource.url, cachedResource.loader);
            this._updateResourceWithCachedResource(resource, cachedResource);
            resources.push(resource);
        }

        function comparator(a, b)
        {
            return a.displayName.localeCompare(b.displayName);
        }
        resources.sort(comparator);

        for (var i = 0; i < resources.length; ++i)
            WebInspector.panels.storage.addFrameResource(framePayload.id, resources[i]);
    }
}

WebInspector.ResourceManager.createResourceView = function(resource)
{
    switch (resource.category) {
    case WebInspector.resourceCategories.documents:
    case WebInspector.resourceCategories.stylesheets:
    case WebInspector.resourceCategories.scripts:
    case WebInspector.resourceCategories.xhr:
        return new WebInspector.SourceView(resource);
    case WebInspector.resourceCategories.images:
        return new WebInspector.ImageView(resource);
    case WebInspector.resourceCategories.fonts:
        return new WebInspector.FontView(resource);
    default:
        return new WebInspector.ResourceView(resource);
    }
}

WebInspector.ResourceManager.resourceViewTypeMatchesResource = function(resource, resourceView)
{
    switch (resource.category) {
    case WebInspector.resourceCategories.documents:
    case WebInspector.resourceCategories.stylesheets:
    case WebInspector.resourceCategories.scripts:
    case WebInspector.resourceCategories.xhr:
        return resourceView.__proto__ === WebInspector.SourceView.prototype;
    case WebInspector.resourceCategories.images:
        return resourceView.__proto__ === WebInspector.ImageView.prototype;
    case WebInspector.resourceCategories.fonts:
        return resourceView.__proto__ === WebInspector.FontView.prototype;
    default:
        return resourceView.__proto__ === WebInspector.ResourceView.prototype;
    }
}

WebInspector.ResourceManager.resourceViewForResource = function(resource)
{
    if (!resource)
        return null;
    if (!resource._resourcesView)
        resource._resourcesView = WebInspector.ResourceManager.createResourceView(resource);
    return resource._resourcesView;
}

WebInspector.ResourceManager.getContents = function(resource, callback)
{
    // FIXME: eventually, cached resources will have no identifiers.
    if (resource.loader)
        InspectorBackend.resourceContent(resource.loader.frameId, resource.url, callback);
    else
        InspectorBackend.getResourceContent(resource.identifier, false, callback);
}
