/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.Frame = class Frame extends WebInspector.Object
{
    constructor(id, name, securityOrigin, loaderIdentifier, mainResource)
    {
        super();

        console.assert(id);

        this._id = id;

        this._name = null;
        this._securityOrigin = null;

        this._resourceCollection = new WebInspector.ResourceCollection;
        this._provisionalResourceCollection = new WebInspector.ResourceCollection;
        this._extraScripts = [];

        this._childFrames = [];
        this._childFrameIdentifierMap = {};

        this._parentFrame = null;
        this._isMainFrame = false;

        this._domContentReadyEventTimestamp = NaN;
        this._loadEventTimestamp = NaN;

        this._executionContextList = new WebInspector.ExecutionContextList;

        this.initialize(name, securityOrigin, loaderIdentifier, mainResource);
    }

    // Public

    initialize(name, securityOrigin, loaderIdentifier, mainResource)
    {
        console.assert(loaderIdentifier);
        console.assert(mainResource);

        var oldName = this._name;
        var oldSecurityOrigin = this._securityOrigin;
        var oldMainResource = this._mainResource;

        this._name = name || null;
        this._securityOrigin = securityOrigin || null;
        this._loaderIdentifier = loaderIdentifier || null;

        this._mainResource = mainResource;
        this._mainResource._parentFrame = this;

        if (oldMainResource && this._mainResource !== oldMainResource)
            this._disassociateWithResource(oldMainResource);

        this.removeAllResources();
        this.removeAllChildFrames();
        this.clearExecutionContexts();
        this.clearProvisionalLoad();

        if (this._mainResource !== oldMainResource)
            this._dispatchMainResourceDidChangeEvent(oldMainResource);

        if (this._securityOrigin !== oldSecurityOrigin)
            this.dispatchEventToListeners(WebInspector.Frame.Event.SecurityOriginDidChange, {oldSecurityOrigin});

        if (this._name !== oldName)
            this.dispatchEventToListeners(WebInspector.Frame.Event.NameDidChange, {oldName});
    }

    startProvisionalLoad(provisionalMainResource)
    {
        console.assert(provisionalMainResource);

        this._provisionalMainResource = provisionalMainResource;
        this._provisionalMainResource._parentFrame = this;

        this._provisionalLoaderIdentifier = provisionalMainResource.loaderIdentifier;

        this._provisionalResourceCollection.removeAllResources();

        this.dispatchEventToListeners(WebInspector.Frame.Event.ProvisionalLoadStarted);
    }

    commitProvisionalLoad(securityOrigin)
    {
        console.assert(this._provisionalMainResource);
        console.assert(this._provisionalLoaderIdentifier);
        if (!this._provisionalLoaderIdentifier)
            return;

        var oldSecurityOrigin = this._securityOrigin;
        var oldMainResource = this._mainResource;

        this._securityOrigin = securityOrigin || null;
        this._loaderIdentifier = this._provisionalLoaderIdentifier;
        this._mainResource = this._provisionalMainResource;

        this._domContentReadyEventTimestamp = NaN;
        this._loadEventTimestamp = NaN;

        if (oldMainResource && this._mainResource !== oldMainResource)
            this._disassociateWithResource(oldMainResource);

        this.removeAllResources();

        this._resourceCollection = this._provisionalResourceCollection;
        this._provisionalResourceCollection = new WebInspector.ResourceCollection;
        this._extraScripts = [];

        this.clearExecutionContexts(true);
        this.clearProvisionalLoad(true);
        this.removeAllChildFrames();

        this.dispatchEventToListeners(WebInspector.Frame.Event.ProvisionalLoadCommitted);

        if (this._mainResource !== oldMainResource)
            this._dispatchMainResourceDidChangeEvent(oldMainResource);

        if (this._securityOrigin !== oldSecurityOrigin)
            this.dispatchEventToListeners(WebInspector.Frame.Event.SecurityOriginDidChange, {oldSecurityOrigin});
    }

    clearProvisionalLoad(skipProvisionalLoadClearedEvent)
    {
        if (!this._provisionalLoaderIdentifier)
            return;

        this._provisionalLoaderIdentifier = null;
        this._provisionalMainResource = null;
        this._provisionalResourceCollection.removeAllResources();

        if (!skipProvisionalLoadClearedEvent)
            this.dispatchEventToListeners(WebInspector.Frame.Event.ProvisionalLoadCleared);
    }

    get id()
    {
        return this._id;
    }

    get loaderIdentifier()
    {
        return this._loaderIdentifier;
    }

    get provisionalLoaderIdentifier()
    {
        return this._provisionalLoaderIdentifier;
    }

    get name()
    {
        return this._name;
    }

    get securityOrigin()
    {
        return this._securityOrigin;
    }

    get url()
    {
        return this._mainResource._url;
    }

    get domTree()
    {
        if (!this._domTree)
            this._domTree = new WebInspector.DOMTree(this);
        return this._domTree;
    }

    get pageExecutionContext()
    {
        return this._executionContextList.pageExecutionContext;
    }

    get executionContextList()
    {
        return this._executionContextList;
    }

    clearExecutionContexts(committingProvisionalLoad)
    {
        if (this._executionContextList.contexts.length) {
            let contexts = this._executionContextList.contexts.slice();
            this._executionContextList.clear();
            this.dispatchEventToListeners(WebInspector.Frame.Event.ExecutionContextsCleared, {committingProvisionalLoad:!!committingProvisionalLoad, contexts});
        }
    }

    addExecutionContext(context)
    {
        var changedPageContext = this._executionContextList.add(context);

        if (changedPageContext)
            this.dispatchEventToListeners(WebInspector.Frame.Event.PageExecutionContextChanged);
    }

    get mainResource()
    {
        return this._mainResource;
    }

    get provisionalMainResource()
    {
        return this._provisionalMainResource;
    }

    get parentFrame()
    {
        return this._parentFrame;
    }

    get childFrames()
    {
        return this._childFrames;
    }

    get domContentReadyEventTimestamp()
    {
        return this._domContentReadyEventTimestamp;
    }

    get loadEventTimestamp()
    {
        return this._loadEventTimestamp;
    }

    isMainFrame()
    {
        return this._isMainFrame;
    }

    markAsMainFrame()
    {
        this._isMainFrame = true;
    }

    unmarkAsMainFrame()
    {
        this._isMainFrame = false;
    }

    markDOMContentReadyEvent(timestamp)
    {
        this._domContentReadyEventTimestamp = timestamp || NaN;
    }

    markLoadEvent(timestamp)
    {
        this._loadEventTimestamp = timestamp || NaN;
    }

    isDetached()
    {
        var frame = this;
        while (frame) {
            if (frame.isMainFrame())
                return false;
            frame = frame.parentFrame;
        }

        return true;
    }

    childFrameForIdentifier(frameId)
    {
        return this._childFrameIdentifierMap[frameId] || null;
    }

    addChildFrame(frame)
    {
        console.assert(frame instanceof WebInspector.Frame);
        if (!(frame instanceof WebInspector.Frame))
            return;

        if (frame._parentFrame === this)
            return;

        if (frame._parentFrame)
            frame._parentFrame.removeChildFrame(frame);

        this._childFrames.push(frame);
        this._childFrameIdentifierMap[frame._id] = frame;

        frame._parentFrame = this;

        this.dispatchEventToListeners(WebInspector.Frame.Event.ChildFrameWasAdded, {childFrame: frame});
    }

    removeChildFrame(frameOrFrameId)
    {
        console.assert(frameOrFrameId);

        if (frameOrFrameId instanceof WebInspector.Frame)
            var childFrameId = frameOrFrameId._id;
        else
            var childFrameId = frameOrFrameId;

        // Fetch the frame by id even if we were passed a WebInspector.Frame.
        // We do this incase the WebInspector.Frame is a new object that isn't in _childFrames,
        // but the id is a valid child frame.
        var childFrame = this.childFrameForIdentifier(childFrameId);
        console.assert(childFrame instanceof WebInspector.Frame);
        if (!(childFrame instanceof WebInspector.Frame))
            return;

        console.assert(childFrame.parentFrame === this);

        this._childFrames.remove(childFrame);
        delete this._childFrameIdentifierMap[childFrame._id];

        childFrame._detachFromParentFrame();

        this.dispatchEventToListeners(WebInspector.Frame.Event.ChildFrameWasRemoved, {childFrame});
    }

    removeAllChildFrames()
    {
        this._detachFromParentFrame();

        for (let childFrame of this._childFrames)
            childFrame.removeAllChildFrames();

        this._childFrames = [];
        this._childFrameIdentifierMap = {};

        this.dispatchEventToListeners(WebInspector.Frame.Event.AllChildFramesRemoved);
    }

    get resources()
    {
        return this._resourceCollection.resources;
    }

    resourceForURL(url, recursivelySearchChildFrames)
    {
        var resource = this._resourceCollection.resourceForURL(url);
        if (resource)
            return resource;

        // Check the main resources of the child frames for the requested URL.
        for (var i = 0; i < this._childFrames.length; ++i) {
            resource = this._childFrames[i].mainResource;
            if (resource.url === url)
                return resource;
        }

        if (!recursivelySearchChildFrames)
            return null;

        // Recursively search resources of child frames.
        for (var i = 0; i < this._childFrames.length; ++i) {
            resource = this._childFrames[i].resourceForURL(url, true);
            if (resource)
                return resource;
        }

        return null;
    }

    resourcesWithType(type)
    {
        return this._resourceCollection.resourcesWithType(type);
    }

    addResource(resource)
    {
        console.assert(resource instanceof WebInspector.Resource);
        if (!(resource instanceof WebInspector.Resource))
            return;

        if (resource.parentFrame === this)
            return;

        if (resource.parentFrame)
            resource.parentFrame.removeResource(resource);

        this._associateWithResource(resource);

        if (this._isProvisionalResource(resource)) {
            this._provisionalResourceCollection.addResource(resource);
            this.dispatchEventToListeners(WebInspector.Frame.Event.ProvisionalResourceWasAdded, {resource});
        } else {
            this._resourceCollection.addResource(resource);
            this.dispatchEventToListeners(WebInspector.Frame.Event.ResourceWasAdded, {resource});
        }
    }

    removeResource(resourceOrURL)
    {
        // This does not remove provisional resources.

        var resource = this._resourceCollection.removeResource(resourceOrURL);
        if (!resource)
            return;

        this._disassociateWithResource(resource);

        this.dispatchEventToListeners(WebInspector.Frame.Event.ResourceWasRemoved, {resource});
    }

    removeAllResources()
    {
        // This does not remove provisional resources, use clearProvisionalLoad for that.

        var resources = this.resources;
        if (!resources.length)
            return;

        for (var i = 0; i < resources.length; ++i)
            this._disassociateWithResource(resources[i]);

        this._resourceCollection.removeAllResources();

        this.dispatchEventToListeners(WebInspector.Frame.Event.AllResourcesRemoved);
    }

    get extraScripts()
    {
        return this._extraScripts;
    }

    addExtraScript(script)
    {
        this._extraScripts.push(script);

        this.dispatchEventToListeners(WebInspector.Frame.Event.ExtraScriptAdded, {script});
    }

    saveIdentityToCookie(cookie)
    {
        cookie[WebInspector.Frame.MainResourceURLCookieKey] = this.mainResource.url.hash;
        cookie[WebInspector.Frame.IsMainFrameCookieKey] = this._isMainFrame;
    }

    // Private

    _detachFromParentFrame()
    {
        if (this._domTree) {
            this._domTree.disconnect();
            this._domTree = null;
        }

        this._parentFrame = null;
    }

    _isProvisionalResource(resource)
    {
        return (resource.loaderIdentifier && this._provisionalLoaderIdentifier && resource.loaderIdentifier === this._provisionalLoaderIdentifier);
    }

    _associateWithResource(resource)
    {
        console.assert(!resource._parentFrame);
        if (resource._parentFrame)
            return;

        resource._parentFrame = this;
    }

    _disassociateWithResource(resource)
    {
        console.assert(resource.parentFrame === this);
        if (resource.parentFrame !== this)
            return;

        resource._parentFrame = null;
    }

    _dispatchMainResourceDidChangeEvent(oldMainResource)
    {
        this.dispatchEventToListeners(WebInspector.Frame.Event.MainResourceDidChange, {oldMainResource});
    }
};

WebInspector.Frame.Event = {
    NameDidChange: "frame-name-did-change",
    SecurityOriginDidChange: "frame-security-origin-did-change",
    MainResourceDidChange: "frame-main-resource-did-change",
    ProvisionalLoadStarted: "frame-provisional-load-started",
    ProvisionalLoadCommitted: "frame-provisional-load-committed",
    ProvisionalLoadCleared: "frame-provisional-load-cleared",
    ProvisionalResourceWasAdded: "frame-provisional-resource-was-added",
    ResourceWasAdded: "frame-resource-was-added",
    ResourceWasRemoved: "frame-resource-was-removed",
    AllResourcesRemoved: "frame-all-resources-removed",
    ExtraScriptAdded: "frame-extra-script-added",
    ChildFrameWasAdded: "frame-child-frame-was-added",
    ChildFrameWasRemoved: "frame-child-frame-was-removed",
    AllChildFramesRemoved: "frame-all-child-frames-removed",
    PageExecutionContextChanged: "frame-page-execution-context-changed",
    ExecutionContextsCleared: "frame-execution-contexts-cleared"
};

WebInspector.Frame.TypeIdentifier = "Frame";
WebInspector.Frame.MainResourceURLCookieKey = "frame-main-resource-url";
WebInspector.Frame.IsMainFrameCookieKey = "frame-is-main-frame";
