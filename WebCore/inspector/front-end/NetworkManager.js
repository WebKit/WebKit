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

WebInspector.NetworkManager = function(resourceTreeModel)
{
    this._resourcesById = {};
    this._resourceTreeModel = resourceTreeModel;
    InspectorBackend.registerDomainDispatcher("Network", this);
}

WebInspector.NetworkManager.requestContent = function(resource, base64Encode, callback)
{
    InspectorBackend.resourceContent(resource.loader.frameId, resource.url, base64Encode, callback);
}

WebInspector.NetworkManager.updateResourceWithRequest = function(resource, request)
{
    resource.requestMethod = request.httpMethod;
    resource.requestHeaders = request.httpHeaderFields;
    resource.requestFormData = request.requestFormData;
}

WebInspector.NetworkManager.updateResourceWithResponse = function(resource, response)
{
    if (resource.isNull)
        return;

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

    if (response.loadInfo) {
        if (response.loadInfo.httpStatusCode)
            resource.statusCode = response.loadInfo.httpStatusCode;
        if (response.loadInfo.httpStatusText)
            resource.statusText = response.loadInfo.httpStatusText;
        resource.requestHeaders = response.loadInfo.requestHeaders;
        resource.responseHeaders = response.loadInfo.responseHeaders;
    }
}

WebInspector.NetworkManager.updateResourceWithCachedResource = function(resource, cachedResource)
{
    resource.type = WebInspector.Resource.Type[cachedResource.type];
    resource.resourceSize = cachedResource.encodedSize;
    WebInspector.NetworkManager.updateResourceWithResponse(resource, cachedResource.response);
}

WebInspector.NetworkManager.prototype = {
    identifierForInitialRequest: function(identifier, url, loader, callStack)
    {
        var resource = this._createResource(identifier, url, loader, callStack);

        // It is important to bind resource url early (before scripts compile).
        this._resourceTreeModel.bindResourceURL(resource);

        WebInspector.panels.network.refreshResource(resource);
        WebInspector.panels.audits.resourceStarted(resource);
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

        WebInspector.NetworkManager.updateResourceWithRequest(resource, request);
        resource.startTime = time;

        if (isRedirect) {
            WebInspector.panels.network.refreshResource(resource);
            WebInspector.panels.audits.resourceStarted(resource);
        } else
            WebInspector.panels.network.refreshResource(resource);
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

        resource.responseReceivedTime = time;
        resource.type = WebInspector.Resource.Type[resourceType];

        WebInspector.NetworkManager.updateResourceWithResponse(resource, response);

        WebInspector.panels.network.refreshResource(resource);
        this._resourceTreeModel.addResourceToFrame(resource.loader.frameId, resource);
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

        resource.endTime = finishTime;
        resource.finished = true;

        WebInspector.panels.network.refreshResource(resource);
        WebInspector.panels.audits.resourceFinished(resource);
        WebInspector.extensionServer.notifyResourceFinished(resource);
        delete this._resourcesById[identifier];
    },

    didFailLoading: function(identifier, time, localizedDescription)
    {
        var resource = this._resourcesById[identifier];
        if (!resource)
            return;

        resource.failed = true;
        resource.localizedFailDescription = localizedDescription;
        resource.finished = true;
        resource.endTime = time;

        WebInspector.panels.network.refreshResource(resource);
        WebInspector.panels.audits.resourceFinished(resource);
        WebInspector.extensionServer.notifyResourceFinished(resource);
        delete this._resourcesById[identifier];
    },

    didLoadResourceFromMemoryCache: function(time, cachedResource)
    {
        var resource = this._createResource(null, cachedResource.url, cachedResource.loader);
        WebInspector.NetworkManager.updateResourceWithCachedResource(resource, cachedResource);
        resource.cached = true;
        resource.requestMethod = "GET";
        resource.startTime = resource.responseReceivedTime = resource.endTime = time;
        resource.finished = true;

        WebInspector.panels.network.refreshResource(resource);
        WebInspector.panels.audits.resourceStarted(resource);
        WebInspector.panels.audits.resourceFinished(resource);
        this._resourceTreeModel.addResourceToFrame(resource.loader.frameId, resource);
    },

    setInitialContent: function(identifier, sourceString, type)
    {
        var resource = WebInspector.panels.network.resources[identifier];
        if (!resource)
            return;

        resource.type = WebInspector.Resource.Type[type];
        resource.setInitialContent(sourceString);
        WebInspector.panels.resources.refreshResource(resource);
        WebInspector.panels.network.refreshResource(resource);
    },

    didCommitLoadForFrame: function(frame, loader)
    {
        this._resourceTreeModel.didCommitLoadForFrame(frame, loader);
        if (!frame.parentId) {
            var mainResource = this._resourceTreeModel.resourceForURL(frame.url);
            if (mainResource) {
                WebInspector.mainResource = mainResource;
                mainResource.isMainResource = true;
            }
        }
    },

    didCreateWebSocket: function(identifier, requestURL)
    {
        var resource = this._createResource(identifier, requestURL);
        resource.type = WebInspector.Resource.Type.WebSocket;
        WebInspector.panels.network.refreshResource(resource);
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

    _createResource: function(identifier, url, loader, callStack)
    {
        var resource = WebInspector.ResourceTreeModel.createResource(identifier, url, loader, callStack);
        this._resourcesById[identifier] = resource;
        return resource;
    },

    _appendRedirect: function(identifier, redirectURL)
    {
        var originalResource = this._resourcesById[identifier];
        originalResource.finished = true;
        originalResource.identifier = null;

        var newResource = this._createResource(identifier, redirectURL, originalResource.loader, originalResource.stackTrace);
        newResource.redirects = originalResource.redirects || [];
        delete originalResource.redirects;
        newResource.redirects.push(originalResource);
        return newResource;
    }
}
