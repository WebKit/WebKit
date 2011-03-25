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

WebInspector.NetworkManager = function(resourceTreeModel)
{
    WebInspector.Object.call(this);
    this._resourceTreeModel = resourceTreeModel;
    this._dispatcher = new WebInspector.NetworkDispatcher(resourceTreeModel, this);
    NetworkAgent.enable(this._processCachedResources.bind(this));
}

WebInspector.NetworkManager.EventTypes = {
    ResourceStarted: "ResourceStarted",
    ResourceUpdated: "ResourceUpdated",
    ResourceFinished: "ResourceFinished",
    MainResourceCommitLoad: "MainResourceCommitLoad"
}

WebInspector.NetworkManager.prototype = {
    frontendReused: function()
    {
        WebInspector.panels.network.clear();
        this._resourceTreeModel.reset();
        NetworkAgent.enable(this._processCachedResources.bind(this));
    },

    requestContent: function(resource, base64Encode, callback)
    {
        function callbackWrapper(error, success, content)
        {
            callback((!error && success) ? content : null);
        }
        NetworkAgent.resourceContent(resource.frameId, resource.url, base64Encode, callbackWrapper);
    },

    _processCachedResources: function(error, mainFramePayload)
    {
        if (error)
            return;
        var mainResource = this._dispatcher._addFramesRecursively(mainFramePayload);
        WebInspector.mainResource = mainResource;
        mainResource.isMainResource = true;
    },

    inflightResourceForURL: function(url)
    {
        return this._dispatcher._inflightResourcesByURL[url];
    }
}

WebInspector.NetworkManager.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.NetworkDispatcher = function(resourceTreeModel, manager)
{
    this._manager = manager;
    this._inflightResourcesById = {};
    this._inflightResourcesByURL = {};
    this._resourceTreeModel = resourceTreeModel;
    this._lastIdentifierForCachedResource = 0;
    InspectorBackend.registerDomainDispatcher("Network", this);
}

WebInspector.NetworkDispatcher.prototype = {
    _updateResourceWithRequest: function(resource, request)
    {
        resource.requestMethod = request.method;
        resource.requestHeaders = request.headers;
        resource.requestFormData = request.postData;
    },

    _updateResourceWithResponse: function(resource, response)
    {
        if (!("status" in response))
            return;

        resource.mimeType = response.mimeType;
        resource.statusCode = response.status;
        resource.statusText = response.statusText;
        resource.responseHeaders = response.headers;
        // Raw request headers can be a part of response as well.
        if (response.requestHeaders)
            resource.requestHeaders = response.requestHeaders;

        resource.connectionReused = response.connectionReused;
        resource.connectionID = response.connectionID;

        if (response.fromDiskCache)
            resource.cached = true;
        else
            resource.timing = response.timing;
    },

    _updateResourceWithCachedResource: function(resource, cachedResource)
    {
        resource.type = WebInspector.Resource.Type[cachedResource.type];
        resource.resourceSize = cachedResource.bodySize;
        this._updateResourceWithResponse(resource, cachedResource.response);
    },

    willSendRequest: function(identifier, frameId, loaderId, documentURL, request, redirectResponse, time, callStack)
    {
        var resource = this._inflightResourcesById[identifier];
        if (resource) {
            this.didReceiveResponse(identifier, time, "Other", redirectResponse);
            resource = this._appendRedirect(identifier, time, request.url);
        } else
            resource = this._createResource(identifier, frameId, loaderId, request.url, documentURL, callStack);
        this._updateResourceWithRequest(resource, request);
        resource.startTime = time;

        this._startResource(resource);
    },

    markResourceAsCached: function(identifier)
    {
        var resource = this._inflightResourcesById[identifier];
        if (!resource)
            return;

        resource.cached = true;
        this._updateResource(resource);
    },

    didReceiveResponse: function(identifier, time, resourceType, response)
    {
        var resource = this._inflightResourcesById[identifier];
        if (!resource)
            return;

        resource.responseReceivedTime = time;
        resource.type = WebInspector.Resource.Type[resourceType];

        this._updateResourceWithResponse(resource, response);

        this._updateResource(resource);
        this._resourceTreeModel.addResourceToFrame(resource.frameId, resource);
    },

    didReceiveContentLength: function(identifier, time, dataLength, lengthReceived)
    {
        var resource = this._inflightResourcesById[identifier];
        if (!resource)
            return;

        resource.resourceSize += dataLength;
        if (lengthReceived != -1)
            resource.increaseTransferSize(lengthReceived);
        resource.endTime = time;

        this._updateResource(resource);
    },

    didFinishLoading: function(identifier, finishTime)
    {
        var resource = this._inflightResourcesById[identifier];
        if (!resource)
            return;

        this._finishResource(resource, finishTime);
    },

    didFailLoading: function(identifier, time, localizedDescription)
    {
        var resource = this._inflightResourcesById[identifier];
        if (!resource)
            return;

        resource.failed = true;
        resource.localizedFailDescription = localizedDescription;
        this._finishResource(resource, time);
    },

    didLoadResourceFromMemoryCache: function(frameId, loaderId, documentURL, time, cachedResource)
    {
        var resource = this._createResource("cached:" + ++this._lastIdentifierForCachedResource, frameId, loaderId, cachedResource.url, documentURL);
        this._updateResourceWithCachedResource(resource, cachedResource);
        resource.cached = true;
        resource.requestMethod = "GET";
        this._startResource(resource);
        resource.startTime = resource.responseReceivedTime = time;
        this._finishResource(resource, time);
        this._resourceTreeModel.addResourceToFrame(resource.frameId, resource);
    },

    frameDetachedFromParent: function(frameId)
    {
        this._resourceTreeModel.frameDetachedFromParent(frameId);
    },

    setInitialContent: function(identifier, sourceString, type)
    {
        var resource = WebInspector.networkResourceById(identifier);
        if (!resource)
            return;

        resource.type = WebInspector.Resource.Type[type];
        resource.setInitialContent(sourceString);
        this._updateResource(resource);
    },

    didCommitLoadForFrame: function(frame, loaderId)
    {
        this._resourceTreeModel.didCommitLoadForFrame(frame, loaderId);
        if (!frame.parentId) {
            var mainResource = this._resourceTreeModel.resourceForURL(frame.url);
            if (mainResource) {
                WebInspector.mainResource = mainResource;
                mainResource.isMainResource = true;
                mainResource.documentURL = frame.url;
                this._dispatchEventToListeners(WebInspector.NetworkManager.EventTypes.MainResourceCommitLoad, mainResource);
            }
        }
    },

    didCreateWebSocket: function(identifier, requestURL)
    {
        var resource = this._createResource(identifier, null, null, requestURL);
        resource.type = WebInspector.Resource.Type.WebSocket;
        this._startResource(resource);
    },

    willSendWebSocketHandshakeRequest: function(identifier, time, request)
    {
        var resource = this._inflightResourcesById[identifier];
        if (!resource)
            return;

        resource.requestMethod = "GET";
        resource.requestHeaders = request.headers;
        resource.webSocketRequestKey3 = request.requestKey3;
        resource.startTime = time;

        this._updateResource(resource);
    },

    didReceiveWebSocketHandshakeResponse: function(identifier, time, response)
    {
        var resource = this._inflightResourcesById[identifier];
        if (!resource)
            return;

        resource.statusCode = response.status;
        resource.statusText = response.statusText;
        resource.responseHeaders = response.headers;
        resource.webSocketChallengeResponse = response.challengeResponse;
        resource.responseReceivedTime = time;

        this._updateResource(resource);
    },

    didCloseWebSocket: function(identifier, time)
    {
        var resource = this._inflightResourcesById[identifier];
        if (!resource)
            return;
        this._finishResource(resource, time);
    },

    _appendRedirect: function(identifier, time, redirectURL)
    {
        var originalResource = this._inflightResourcesById[identifier];
        var previousRedirects = originalResource.redirects || [];
        originalResource.identifier = "redirected:" + identifier + "." + previousRedirects.length;
        delete originalResource.redirects;
        this._finishResource(originalResource, time);
        var newResource = this._createResource(identifier, originalResource.frameId, originalResource.loaderId,
             redirectURL, originalResource.documentURL, originalResource.stackTrace);
        newResource.redirects = previousRedirects.concat(originalResource);
        return newResource;
    },

    _startResource: function(resource)
    {
        this._inflightResourcesById[resource.identifier] = resource;
        this._inflightResourcesByURL[resource.url] = resource;
        this._dispatchEventToListeners(WebInspector.NetworkManager.EventTypes.ResourceStarted, resource);
    },

    _updateResource: function(resource)
    {
        this._dispatchEventToListeners(WebInspector.NetworkManager.EventTypes.ResourceUpdated, resource);
    },

    _finishResource: function(resource, finishTime)
    {
        resource.endTime = finishTime;
        resource.finished = true;
        this._dispatchEventToListeners(WebInspector.NetworkManager.EventTypes.ResourceFinished, resource);
        delete this._inflightResourcesById[resource.identifier];
        delete this._inflightResourcesByURL[resource.url];
    },

    _addFramesRecursively: function(frameTreePayload)
    {
        var framePayload = frameTreePayload.frame;
        var mainResource = frameTreePayload.mainResource;

        var frameResource = this._createResource(null, framePayload.id, framePayload.loaderId, mainResource.url, framePayload.url);
        this._updateResourceWithRequest(frameResource, mainResource.request);
        this._updateResourceWithResponse(frameResource, mainResource.response);
        frameResource.type = WebInspector.Resource.Type["Document"];
        frameResource.finished = true;

        this._resourceTreeModel.addOrUpdateFrame(framePayload);
        this._resourceTreeModel.addResourceToFrame(framePayload.id, frameResource);

        for (var i = 0; frameTreePayload.childFrames && i < frameTreePayload.childFrames.length; ++i)
            this._addFramesRecursively(frameTreePayload.childFrames[i]);

        if (!frameTreePayload.subresources)
            return;

        for (var i = 0; i < frameTreePayload.subresources.length; ++i) {
            var cachedResource = frameTreePayload.subresources[i];
            var resource = this._createResource(null, framePayload.id, framePayload.loaderId, cachedResource.url, framePayload.url);
            this._updateResourceWithCachedResource(resource, cachedResource);
            resource.finished = true;
            this._resourceTreeModel.addResourceToFrame(framePayload.id, resource);
        }
        return frameResource;
    },

    _dispatchEventToListeners: function(eventType, resource)
    {
        this._manager.dispatchEventToListeners(eventType, resource);
    },

    _createResource: function(identifier, frameId, loaderId, url, documentURL, stackTrace)
    {
        var resource = new WebInspector.Resource(identifier, url);
        resource.documentURL = documentURL;
        resource.frameId = frameId;
        resource.loaderId = loaderId;
        resource.stackTrace = stackTrace;
        return resource;
    }
}
