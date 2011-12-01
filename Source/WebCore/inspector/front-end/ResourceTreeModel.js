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
 * @param {WebInspector.NetworkManager} networkManager
 */
WebInspector.ResourceTreeModel = function(networkManager)
{
    networkManager.addEventListener(WebInspector.NetworkManager.EventTypes.ResourceUpdated, this._onResourceUpdated, this);
    networkManager.addEventListener(WebInspector.NetworkManager.EventTypes.ResourceFinished, this._onResourceUpdated, this);
    networkManager.addEventListener(WebInspector.NetworkManager.EventTypes.ResourceUpdateDropped, this._onResourceUpdateDropped, this);

    WebInspector.console.addEventListener(WebInspector.ConsoleModel.Events.MessageAdded, this._consoleMessageAdded, this);
    WebInspector.console.addEventListener(WebInspector.ConsoleModel.Events.RepeatCountUpdated, this._consoleMessageAdded, this);
    WebInspector.console.addEventListener(WebInspector.ConsoleModel.Events.ConsoleCleared, this._consoleCleared, this);

    PageAgent.enable();

    this._fetchResourceTree();
    InspectorBackend.registerPageDispatcher(new WebInspector.PageDispatcher(this));

    this._pendingConsoleMessages = {};
}

WebInspector.ResourceTreeModel.EventTypes = {
    FrameAdded: "FrameAdded",
    FrameNavigated: "FrameNavigated",
    FrameDetached: "FrameDetached",
    MainFrameNavigated: "MainFrameNavigated",
    ResourceAdded: "ResourceAdded",
    ResourceContentCommitted: "resource-content-committed",
    WillLoadCachedResources: "WillLoadCachedResources",
    CachedResourcesLoaded: "CachedResourcesLoaded",
    DOMContentLoaded: "DOMContentLoaded",
    OnLoad: "OnLoad",
    InspectedURLChanged: "InspectedURLChanged"
}

WebInspector.ResourceTreeModel.prototype = {
    _fetchResourceTree: function()
    {
        this._frames = {};
        delete this._cachedResourcesProcessed;
        PageAgent.getResourceTree(this._processCachedResources.bind(this));
    },

    _processCachedResources: function(error, mainFramePayload)
    {
        if (error) {
            console.error(JSON.stringify(error));
            return;
        }

        this.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.WillLoadCachedResources);
        WebInspector.inspectedPageURL = mainFramePayload.frame.url;
        this._addFramesRecursively(null, mainFramePayload);
        this._dispatchInspectedURLChanged();
        this.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.CachedResourcesLoaded);
        WebInspector.Resource.restoreRevisions();

        this._cachedResourcesProcessed = true;
    },

    _dispatchInspectedURLChanged: function()
    {
        InspectorFrontendHost.inspectedURLChanged(WebInspector.inspectedPageURL);
        this.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.InspectedURLChanged, WebInspector.inspectedPageURL);
    },

    /**
     * @param {WebInspector.ResourceTreeFrame} frame
     */
    _addFrame: function(frame)
    {
        this._frames[frame.id] = frame;
        if (frame.isMainFrame())
            this.mainFrame = frame;
        this.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.FrameAdded, frame);
    },

    /**
     * @param {PageAgent.Frame} framePayload
     */
    _frameNavigated: function(framePayload)
    {
        if (this._frontendReused(framePayload))
            return;

        // Do nothing unless cached resource tree is processed - it will overwrite everything.
        if (!this._cachedResourcesProcessed)
            return;
        var frame = this._frames[framePayload.id];
        if (frame) {
            // Navigation within existing frame.
            frame._navigate(framePayload);
        } else {
            // Either a new frame or a main frame navigation to the new backend process. 
            var parentFrame = this._frames[framePayload.parentId];
            frame = new WebInspector.ResourceTreeFrame(this, parentFrame, framePayload);
            if (frame.isMainFrame() && this.mainFrame) {
                // Definitely a navigation to the new backend process.
                this._frameDetached(this.mainFrame.id);
            }
            this._addFrame(frame);
        }

        if (frame.isMainFrame())
            WebInspector.inspectedPageURL = frame.url;

        this.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.FrameNavigated, frame);
        if (frame.isMainFrame())
            this.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.MainFrameNavigated, frame);

        // Fill frame with retained resources (the ones loaded using new loader).
        var resources = frame.resources();
        for (var i = 0; i < resources.length; ++i)
            this.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, resources[i]);

        if (frame.isMainFrame())
            this._dispatchInspectedURLChanged();
    },

    _frontendReused: function(framePayload)
    {
        if (!framePayload.parentId && !WebInspector.networkLog.resources.length) {
            // We are navigating main frame to the existing loaded backend (no provisioual loaded resources are there). 
            this._fetchResourceTree();
            return true;
        }
        return false;
    },

    _frameDetached: function(frameId)
    {
        // Do nothing unless cached resource tree is processed - it will overwrite everything.
        if (!this._cachedResourcesProcessed)
            return;

        var frame = this._frames[frameId];
        if (!frame)
            return;

        if (frame.parentFrame)
            frame.parentFrame._removeChildFrame(frame);
        else {
            // Report that root is detached
            frame._removeChildFrames();
            this.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.FrameDetached, frame);
        }
    },

    _onResourceUpdated: function(event)
    {
        if (!this._cachedResourcesProcessed)
            return;

        this._addPendingConsoleMessagesToResource(event.data);

        var resource = event.data;
        if (resource.failed || resource.type === WebInspector.Resource.Type.XHR)
            return;

        var frame = this._frames[resource.frameId];
        if (frame)
            frame._addResource(resource);
    },

    _onResourceUpdateDropped: function(event)
    {
        if (!this._cachedResourcesProcessed)
            return;

        var frameId = event.data.frameId;
        var frame = this._frames[frameId];
        if (!frame)
            return;

        var url = event.data.url;
        if (frame._resourcesMap[url])
            return;

        var resource = this._createResource(url, frame.url, frameId, event.data.loaderId);
        resource.type = WebInspector.Resource.Type[event.data.resourceType];
        resource.mimeType = event.data.mimeType;
        resource.finished = true;
        frame._addResource(resource);
    },

    frameForId: function(frameId)
    {
        return this._frames[frameId];
    },

    forAllResources: function(callback)
    {
        if (this.mainFrame)
            return this.mainFrame._callForFrameResources(callback);
        return false;
    },

    _consoleMessageAdded: function(event)
    {
        var msg = event.data;
        var resource = this.resourceForURL(msg.url);
        if (resource)
            this._addConsoleMessageToResource(msg, resource);
        else
            this._addPendingConsoleMessage(msg);
    },

    _addPendingConsoleMessage: function(msg)
    {
        if (!msg.url)
            return;
        if (!this._pendingConsoleMessages[msg.url])
            this._pendingConsoleMessages[msg.url] = [];
        this._pendingConsoleMessages[msg.url].push(msg);
    },

    _addPendingConsoleMessagesToResource: function(resource)
    {
        var messages = this._pendingConsoleMessages[resource.url];
        if (messages) {
            for (var i = 0; i < messages.length; i++)
                this._addConsoleMessageToResource(messages[i], resource);
            delete this._pendingConsoleMessages[resource.url];
        }
    },

    _addConsoleMessageToResource: function(msg, resource)
    {
        switch (msg.level) {
        case WebInspector.ConsoleMessage.MessageLevel.Warning:
            resource.warnings += msg.repeatDelta;
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Error:
            resource.errors += msg.repeatDelta;
            break;
        }
        resource.addMessage(msg);
    },

    _consoleCleared: function()
    {
        function callback(resource)
        {
            resource.clearErrorsAndWarnings();
        }

        this._pendingConsoleMessages = {};
        this.forAllResources(callback);
    },

    resourceForURL: function(url)
    {
        // Workers call into this with no frames available.
        return this.mainFrame ? this.mainFrame.resourceForURL(url) : null;
    },

    _addFramesRecursively: function(parentFrame, frameTreePayload)
    {
        var framePayload = frameTreePayload.frame;
        var frame = new WebInspector.ResourceTreeFrame(this, parentFrame, framePayload);

        // Create frame resource.
        var frameResource = this._createResourceFromFramePayload(framePayload, framePayload.url);
        frameResource.mimeType = framePayload.mimeType;
        frameResource.type = WebInspector.Resource.Type.Document;
        frameResource.finished = true;

        if (frame.isMainFrame())
            WebInspector.inspectedPageURL = frameResource.url;

        this._addFrame(frame);
        frame._addResource(frameResource);

        for (var i = 0; frameTreePayload.childFrames && i < frameTreePayload.childFrames.length; ++i)
            this._addFramesRecursively(frame, frameTreePayload.childFrames[i]);

        if (!frameTreePayload.resources)
            return;

        // Create frame subresources.
        for (var i = 0; i < frameTreePayload.resources.length; ++i) {
            var subresource = frameTreePayload.resources[i];
            var resource = this._createResourceFromFramePayload(framePayload, subresource.url);
            resource.type = WebInspector.Resource.Type[subresource.type];
            resource.mimeType = subresource.mimeType;
            resource.finished = true;
            frame._addResource(resource);
        }
    },

    /**
     * @param {PageAgent.Frame} frame
     * @param {string} url
     */
    _createResourceFromFramePayload: function(frame, url)
    {
        return this._createResource(url, frame.url, frame.id, frame.loaderId);
    },

    /**
     * @param {string} url
     * @param {string} documentURL
     * @param {NetworkAgent.FrameId} frameId
     * @param {NetworkAgent.LoaderId} loaderId
     */
    _createResource: function(url, documentURL, frameId, loaderId)
    {
        var resource = new WebInspector.Resource("", url, frameId, loaderId);
        resource.documentURL = documentURL;
        return resource;
    }
}

WebInspector.ResourceTreeModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @param {WebInspector.ResourceTreeModel} model
 * @param {?WebInspector.ResourceTreeFrame} parentFrame
 * @param {PageAgent.Frame} payload
 */
WebInspector.ResourceTreeFrame = function(model, parentFrame, payload)
{
    this._model = model;
    this._parentFrame = parentFrame; 

    this._id = payload.id;
    this._loaderId = payload.loaderId;
    this._name = payload.name;
    this._url = payload.url;
    this._mimeType = payload.mimeType;

    /**
     * @type {Array.<WebInspector.ResourceTreeFrame>}
     */
    this._childFrames = [];

    /**
     * @type {Object.<string, WebInspector.Resource>}
     */
    this._resourcesMap = {};

    if (this._parentFrame)
        this._parentFrame._childFrames.push(this);
}

WebInspector.ResourceTreeFrame.prototype = {
    get id()
    {
        return this._id;
    },

    get name()
    {
        return this._name;
    },

    get url()
    {
        return this._url;
    },

    get loaderId()
    {
        return this._loaderId;
    },

    get parentFrame()
    {
        return this._parentFrame;
    },

    get childFrames()
    {
        return this._childFrames;
    },

    /**
     * @return {boolean}
     */
    isMainFrame: function()
    {
        return !this._parentFrame;
    },

    /**
     * @param {PageAgent.Frame} framePayload
     */
    _navigate: function(framePayload)
    {
        this._loaderId = framePayload.loaderId;
        this._name = framePayload.name;
        this._url = framePayload.url;
        this._mimeType = framePayload.mimeType;

        var mainResource = this._resourcesMap[this._url];
        this._resourcesMap = {};
        this._removeChildFrames();
        if (mainResource && mainResource.loaderId === this._loaderId)
            this._addResource(mainResource);
    },

    get mainResource()
    {
        return this._resourcesMap[this._url];
    },

    /**
     * @param {WebInspector.ResourceTreeFrame} frame
     */
    _removeChildFrame: function(frame)
    {
        frame._removeChildFrames();
        this._childFrames.remove(frame);
        this._model.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.FrameDetached, frame);
    },

    _removeChildFrames: function()
    {
        var copy = this._childFrames.slice();
        for (var i = 0; i < copy.length; ++i)
            this._removeChildFrame(copy[i]); 
    },

    /**
     * @param {WebInspector.Resource} resource
     */
    _addResource: function(resource)
    {
        if (this._resourcesMap[resource.url] === resource) {
            // Already in the tree, we just got an extra update.
            return;
        }
        this._resourcesMap[resource.url] = resource;
        this._model.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, resource);
    },

    /**
     * @return {Array.<WebInspector.Resource>}
     */
    resources: function()
    {
        var result = [];
        for (var url in this._resourcesMap)
            result.push(this._resourcesMap[url]);
        return result;
    },

    /**
     * @param {string} url
     * @return {?WebInspector.Resource}
     */
    resourceForURL: function(url)
    {
        var result;
        function filter(resource)
        {
            if (resource.url === url) {
                result = resource;
                return true;
            }
        }
        this._callForFrameResources(filter);
        return result;
    },

    /**
     * @param {function(WebInspector.Resource)} callback
     */
    _callForFrameResources: function(callback)
    {
        for (var url in this._resourcesMap) {
            if (callback(this._resourcesMap[url]))
                return true;
        }

        for (var i = 0; i < this._childFrames.length; ++i) {
            if (this._childFrames[i]._callForFrameResources(callback))
                return true;
        }
        return false;
    }
}

/**
 * @constructor
 * @implements {PageAgent.Dispatcher}
 */
WebInspector.PageDispatcher = function(resourceTreeModel)
{
    this._resourceTreeModel = resourceTreeModel;
}

WebInspector.PageDispatcher.prototype = {
    domContentEventFired: function(time)
    {
        this._resourceTreeModel.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.DOMContentLoaded, time);

        // FIXME: the only client is HAR, fix it there.
        WebInspector.mainResourceDOMContentTime = time;
    },

    loadEventFired: function(time)
    {
        this._resourceTreeModel.dispatchEventToListeners(WebInspector.ResourceTreeModel.EventTypes.OnLoad, time);

        // FIXME: the only client is HAR, fix it there.
        WebInspector.mainResourceLoadTime = time;
    },

    frameNavigated: function(frame)
    {
        this._resourceTreeModel._frameNavigated(frame);
    },

    frameDetached: function(frameId)
    {
        this._resourceTreeModel._frameDetached(frameId);
    }
}

/**
 * @type {WebInspector.ResourceTreeModel}
 */
WebInspector.resourceTreeModel = null;
