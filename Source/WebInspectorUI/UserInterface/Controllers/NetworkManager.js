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

WI.NetworkManager = class NetworkManager extends WI.Object
{
    constructor()
    {
        super();

        this._frameIdentifierMap = new Map;
        this._mainFrame = null;
        this._resourceRequestIdentifierMap = new Map;
        this._orphanedResources = new Map;
        this._webSocketIdentifierToURL = new Map;

        this._waitingForMainFrameResourceTreePayload = true;

        if (window.PageAgent) {
            PageAgent.enable();
            PageAgent.getResourceTree(this._processMainFrameResourceTreePayload.bind(this));
        }

        if (window.ServiceWorkerAgent)
            ServiceWorkerAgent.getInitializationInfo(this._processServiceWorkerConfiguration.bind(this));

        if (window.NetworkAgent)
            NetworkAgent.enable();

        WI.notifications.addEventListener(WI.Notification.ExtraDomainsActivated, this._extraDomainsActivated, this);
    }

    // Public

    get mainFrame()
    {
        return this._mainFrame;
    }

    get frames()
    {
        return [...this._frameIdentifierMap.values()];
    }

    frameForIdentifier(frameId)
    {
        return this._frameIdentifierMap.get(frameId) || null;
    }

    resourceForRequestIdentifier(requestIdentifier)
    {
        return this._resourceRequestIdentifierMap.get(requestIdentifier) || null;
    }

    frameDidNavigate(framePayload)
    {
        // Called from WI.PageObserver.

        // Ignore this while waiting for the whole frame/resource tree.
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        var frameWasLoadedInstantly = false;

        var frame = this.frameForIdentifier(framePayload.id);
        if (!frame) {
            // If the frame wasn't known before now, then the main resource was loaded instantly (about:blank, etc.)
            // Make a new resource (which will make the frame). Mark will mark it as loaded at the end too since we
            // don't expect any more events about the load finishing for these frames.
            var frameResource = this._addNewResourceToFrameOrTarget(null, framePayload.id, framePayload.loaderId, framePayload.url, null, null, null, null, null, null, framePayload.name, framePayload.securityOrigin);
            frame = frameResource.parentFrame;
            frameWasLoadedInstantly = true;

            console.assert(frame);
            if (!frame)
                return;
        }

        if (framePayload.loaderId === frame.provisionalLoaderIdentifier) {
            // There was a provisional load in progress, commit it.
            frame.commitProvisionalLoad(framePayload.securityOrigin);
        } else {
            if (frame.mainResource.url !== framePayload.url || frame.loaderIdentifier !== framePayload.loaderId) {
                // Navigations like back/forward do not have provisional loads, so create a new main resource here.
                var mainResource = new WI.Resource(framePayload.url, framePayload.mimeType, null, framePayload.loaderId);
            } else {
                // The main resource is already correct, so reuse it.
                var mainResource = frame.mainResource;
            }

            frame.initialize(framePayload.name, framePayload.securityOrigin, framePayload.loaderId, mainResource);
        }

        var oldMainFrame = this._mainFrame;

        if (framePayload.parentId) {
            var parentFrame = this.frameForIdentifier(framePayload.parentId);
            console.assert(parentFrame);

            if (frame === this._mainFrame)
                this._mainFrame = null;

            if (frame.parentFrame !== parentFrame)
                parentFrame.addChildFrame(frame);
        } else {
            if (frame.parentFrame)
                frame.parentFrame.removeChildFrame(frame);
            this._mainFrame = frame;
        }

        if (this._mainFrame !== oldMainFrame)
            this._mainFrameDidChange(oldMainFrame);

        if (frameWasLoadedInstantly)
            frame.mainResource.markAsFinished();
    }

    frameDidDetach(frameId)
    {
        // Called from WI.PageObserver.

        // Ignore this while waiting for the whole frame/resource tree.
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        var frame = this.frameForIdentifier(frameId);
        if (!frame)
            return;

        if (frame.parentFrame)
            frame.parentFrame.removeChildFrame(frame);

        this._frameIdentifierMap.delete(frame.id);

        var oldMainFrame = this._mainFrame;

        if (frame === this._mainFrame)
            this._mainFrame = null;

        frame.clearExecutionContexts();

        this.dispatchEventToListeners(WI.NetworkManager.Event.FrameWasRemoved, {frame});

        if (this._mainFrame !== oldMainFrame)
            this._mainFrameDidChange(oldMainFrame);
    }

    resourceRequestWillBeSent(requestIdentifier, frameIdentifier, loaderIdentifier, request, type, redirectResponse, timestamp, walltime, initiator, targetId)
    {
        // Called from WI.NetworkObserver.

        // Ignore this while waiting for the whole frame/resource tree.
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        // COMPATIBILITY (iOS 8): Timeline timestamps for legacy backends are computed
        // dynamically from the first backend timestamp received. For navigations we
        // need to reset that base timestamp, and an appropriate timestamp to use is
        // the new main resource's will be sent timestamp. So save this value on the
        // resource in case it becomes a main resource.
        var originalRequestWillBeSentTimestamp = timestamp;

        var elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);
        let resource = this._resourceRequestIdentifierMap.get(requestIdentifier);
        if (resource) {
            // This is an existing request which is being redirected, update the resource.
            console.assert(redirectResponse);
            console.assert(!targetId);
            resource.updateForRedirectResponse(request.url, request.headers, elapsedTime);
            return;
        }

        var initiatorSourceCodeLocation = this._initiatorSourceCodeLocationFromPayload(initiator);

        // This is a new request, make a new resource and add it to the right frame.
        resource = this._addNewResourceToFrameOrTarget(requestIdentifier, frameIdentifier, loaderIdentifier, request.url, type, request.method, request.headers, request.postData, elapsedTime, walltime, null, null, initiatorSourceCodeLocation, originalRequestWillBeSentTimestamp, targetId);

        // Associate the resource with the requestIdentifier so it can be found in future loading events.
        this._resourceRequestIdentifierMap.set(requestIdentifier, resource);
    }

    webSocketCreated(requestId, url)
    {
        this._webSocketIdentifierToURL.set(requestId, url);
    }

    webSocketWillSendHandshakeRequest(requestId, timestamp, walltime, request)
    {
        let url = this._webSocketIdentifierToURL.get(requestId);
        console.assert(url);
        if (!url)
            return;

        // COMPATIBILITY(iOS 10.3): `walltime` did not exist in 10.3 and earlier.
        if (!NetworkAgent.hasEventParameter("webSocketWillSendHandshakeRequest", "walltime")) {
            request = arguments[2];
            walltime = NaN;
        }

        // FIXME: <webkit.org/b/168475> Web Inspector: Correctly display iframe's and worker's WebSockets
        let frameIdentifier = WI.networkManager.mainFrame.id;
        let loaderIdentifier = WI.networkManager.mainFrame.id;
        let targetId;

        let frame = this.frameForIdentifier(frameIdentifier);
        let requestData = null;
        let elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);
        let initiatorSourceCodeLocation = null;

        let resource = new WI.WebSocketResource(url, loaderIdentifier, targetId, requestId, request.headers, requestData, timestamp, walltime, elapsedTime, initiatorSourceCodeLocation);

        frame.addResource(resource);

        this._resourceRequestIdentifierMap.set(requestId, resource);
    }

    webSocketHandshakeResponseReceived(requestId, timestamp, response)
    {
        let resource = this._resourceRequestIdentifierMap.get(requestId);
        console.assert(resource);
        if (!resource)
            return;

        resource.readyState = WI.WebSocketResource.ReadyState.Open;

        let elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);

        // FIXME: <webkit.org/b/169166> Web Inspector: WebSockets: Implement timing information
        let responseTiming = response.timing || null;

        resource.updateForResponse(resource.url, resource.mimeType, resource.type, response.headers, response.status, response.statusText, elapsedTime, responseTiming);

        resource.markAsFinished(elapsedTime);
    }

    webSocketFrameReceived(requestId, timestamp, response)
    {
        this._webSocketFrameReceivedOrSent(requestId, timestamp, response);
    }

    webSocketFrameSent(requestId, timestamp, response)
    {
        this._webSocketFrameReceivedOrSent(requestId, timestamp, response);
    }

    webSocketClosed(requestId, timestamp)
    {
        let resource = this._resourceRequestIdentifierMap.get(requestId);
        console.assert(resource);
        if (!resource)
            return;

        resource.readyState = WI.WebSocketResource.ReadyState.Closed;

        let elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);
        resource.markAsFinished(elapsedTime);

        this._webSocketIdentifierToURL.delete(requestId);
        this._resourceRequestIdentifierMap.delete(requestId);
    }

    _webSocketFrameReceivedOrSent(requestId, timestamp, response)
    {
        let resource = this._resourceRequestIdentifierMap.get(requestId);
        console.assert(resource);
        if (!resource)
            return;

        // Data going from the client to the server is always masked.
        let isOutgoing = !!response.mask;

        let {payloadData, payloadLength, opcode} = response;
        let elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);

        resource.addFrame(payloadData, payloadLength, isOutgoing, opcode, timestamp, elapsedTime);
    }

    markResourceRequestAsServedFromMemoryCache(requestIdentifier)
    {
        // Called from WI.NetworkObserver.

        // Ignore this while waiting for the whole frame/resource tree.
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        let resource = this._resourceRequestIdentifierMap.get(requestIdentifier);

        // We might not have a resource if the inspector was opened during the page load (after resourceRequestWillBeSent is called).
        // We don't want to assert in this case since we do likely have the resource, via PageAgent.getResourceTree. The Resource
        // just doesn't have a requestIdentifier for us to look it up.
        if (!resource)
            return;

        resource.legacyMarkServedFromMemoryCache();
    }

    resourceRequestWasServedFromMemoryCache(requestIdentifier, frameIdentifier, loaderIdentifier, cachedResourcePayload, timestamp, initiator)
    {
        // Called from WI.NetworkObserver.

        // Ignore this while waiting for the whole frame/resource tree.
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        console.assert(!this._resourceRequestIdentifierMap.has(requestIdentifier));

        let elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);
        let initiatorSourceCodeLocation = this._initiatorSourceCodeLocationFromPayload(initiator);
        let response = cachedResourcePayload.response;
        const responseSource = NetworkAgent.ResponseSource.MemoryCache;

        let resource = this._addNewResourceToFrameOrTarget(requestIdentifier, frameIdentifier, loaderIdentifier, cachedResourcePayload.url, cachedResourcePayload.type, "GET", null, null, elapsedTime, null, null, null, initiatorSourceCodeLocation);
        resource.updateForResponse(cachedResourcePayload.url, response.mimeType, cachedResourcePayload.type, response.headers, response.status, response.statusText, elapsedTime, response.timing, responseSource);
        resource.increaseSize(cachedResourcePayload.bodySize, elapsedTime);
        resource.increaseTransferSize(cachedResourcePayload.bodySize);
        resource.setCachedResponseBodySize(cachedResourcePayload.bodySize);
        resource.markAsFinished(elapsedTime);

        console.assert(resource.cached, "This resource should be classified as cached since it was served from the MemoryCache", resource);

        if (cachedResourcePayload.sourceMapURL)
            WI.sourceMapManager.downloadSourceMap(cachedResourcePayload.sourceMapURL, resource.url, resource);

        // No need to associate the resource with the requestIdentifier, since this is the only event
        // sent for memory cache resource loads.
    }

    resourceRequestDidReceiveResponse(requestIdentifier, frameIdentifier, loaderIdentifier, type, response, timestamp)
    {
        // Called from WI.NetworkObserver.

        // Ignore this while waiting for the whole frame/resource tree.
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        var elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);
        let resource = this._resourceRequestIdentifierMap.get(requestIdentifier);

        // We might not have a resource if the inspector was opened during the page load (after resourceRequestWillBeSent is called).
        // We don't want to assert in this case since we do likely have the resource, via PageAgent.getResourceTree. The Resource
        // just doesn't have a requestIdentifier for us to look it up, but we can try to look it up by its URL.
        if (!resource) {
            var frame = this.frameForIdentifier(frameIdentifier);
            if (frame)
                resource = frame.resourceForURL(response.url);

            // If we find the resource this way we had marked it earlier as finished via PageAgent.getResourceTree.
            // Associate the resource with the requestIdentifier so it can be found in future loading events.
            // and roll it back to an unfinished state, we know now it is still loading.
            if (resource) {
                this._resourceRequestIdentifierMap.set(requestIdentifier, resource);
                resource.revertMarkAsFinished();
            }
        }

        // If we haven't found an existing Resource by now, then it is a resource that was loading when the inspector
        // opened and we just missed the resourceRequestWillBeSent for it. So make a new resource and add it.
        if (!resource) {
            resource = this._addNewResourceToFrameOrTarget(requestIdentifier, frameIdentifier, loaderIdentifier, response.url, type, null, response.requestHeaders, null, elapsedTime, null, null, null, null);

            // Associate the resource with the requestIdentifier so it can be found in future loading events.
            this._resourceRequestIdentifierMap.set(requestIdentifier, resource);
        }

        // COMPATIBILITY (iOS 10.3): `fromDiskCache` is legacy, replaced by `source`.
        if (response.fromDiskCache)
            resource.legacyMarkServedFromDiskCache();

        resource.updateForResponse(response.url, response.mimeType, type, response.headers, response.status, response.statusText, elapsedTime, response.timing, response.source);
    }

    resourceRequestDidReceiveData(requestIdentifier, dataLength, encodedDataLength, timestamp)
    {
        // Called from WI.NetworkObserver.

        // Ignore this while waiting for the whole frame/resource tree.
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        let resource = this._resourceRequestIdentifierMap.get(requestIdentifier);
        var elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);

        // We might not have a resource if the inspector was opened during the page load (after resourceRequestWillBeSent is called).
        // We don't want to assert in this case since we do likely have the resource, via PageAgent.getResourceTree. The Resource
        // just doesn't have a requestIdentifier for us to look it up.
        if (!resource)
            return;

        resource.increaseSize(dataLength, elapsedTime);

        if (encodedDataLength !== -1)
            resource.increaseTransferSize(encodedDataLength);
    }

    resourceRequestDidFinishLoading(requestIdentifier, timestamp, sourceMapURL, metrics)
    {
        // Called from WI.NetworkObserver.

        // Ignore this while waiting for the whole frame/resource tree.
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        // By now we should always have the Resource. Either it was fetched when the inspector first opened with
        // PageAgent.getResourceTree, or it was a currently loading resource that we learned about in resourceRequestDidReceiveResponse.
        let resource = this._resourceRequestIdentifierMap.get(requestIdentifier);
        console.assert(resource);
        if (!resource)
            return;

        if (metrics)
            resource.updateWithMetrics(metrics);

        let elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);
        resource.markAsFinished(elapsedTime);

        if (sourceMapURL)
            WI.sourceMapManager.downloadSourceMap(sourceMapURL, resource.url, resource);

        this._resourceRequestIdentifierMap.delete(requestIdentifier);
    }

    resourceRequestDidFailLoading(requestIdentifier, canceled, timestamp, errorText)
    {
        // Called from WI.NetworkObserver.

        // Ignore this while waiting for the whole frame/resource tree.
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        // By now we should always have the Resource. Either it was fetched when the inspector first opened with
        // PageAgent.getResourceTree, or it was a currently loading resource that we learned about in resourceRequestDidReceiveResponse.
        let resource = this._resourceRequestIdentifierMap.get(requestIdentifier);
        console.assert(resource);
        if (!resource)
            return;

        let elapsedTime = WI.timelineManager.computeElapsedTime(timestamp);
        resource.markAsFailed(canceled, elapsedTime, errorText);

        if (resource.parentFrame && resource === resource.parentFrame.provisionalMainResource)
            resource.parentFrame.clearProvisionalLoad();

        this._resourceRequestIdentifierMap.delete(requestIdentifier);
    }

    executionContextCreated(contextPayload)
    {
        // Called from WI.RuntimeObserver.

        var frame = this.frameForIdentifier(contextPayload.frameId);
        console.assert(frame);
        if (!frame)
            return;

        var displayName = contextPayload.name || frame.mainResource.displayName;
        var executionContext = new WI.ExecutionContext(WI.mainTarget, contextPayload.id, displayName, contextPayload.isPageContext, frame);
        frame.addExecutionContext(executionContext);
    }

    resourceForURL(url)
    {
        if (!this._mainFrame)
            return null;

        if (this._mainFrame.mainResource.url === url)
            return this._mainFrame.mainResource;

        return this._mainFrame.resourceForURL(url, true);
    }

    adoptOrphanedResourcesForTarget(target)
    {
        let resources = this._orphanedResources.take(target.identifier);
        if (!resources)
            return;

        for (let resource of resources)
            target.adoptResource(resource);
    }

    // Private

    _addNewResourceToFrameOrTarget(requestIdentifier, frameIdentifier, loaderIdentifier, url, type, requestMethod, requestHeaders, requestData, elapsedTime, walltime, frameName, frameSecurityOrigin, initiatorSourceCodeLocation, originalRequestWillBeSentTimestamp, targetId)
    {
        console.assert(!this._waitingForMainFrameResourceTreePayload);

        let resource = null;

        if (!frameIdentifier && targetId) {
            // This is a new resource for a ServiceWorker target.
            console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.ServiceWorker);
            console.assert(targetId === WI.mainTarget.identifier);
            resource = new WI.Resource(url, null, type, loaderIdentifier, targetId, requestIdentifier, requestMethod, requestHeaders, requestData, elapsedTime, walltime, initiatorSourceCodeLocation, originalRequestWillBeSentTimestamp);
            resource.target.addResource(resource);
            return resource;
        }

        let frame = this.frameForIdentifier(frameIdentifier);
        if (frame) {
            // This is a new request for an existing frame, which might be the main resource or a new resource.
            if (type === "Document" && frame.mainResource.url === url && frame.loaderIdentifier === loaderIdentifier)
                resource = frame.mainResource;
            else if (type === "Document" && frame.provisionalMainResource && frame.provisionalMainResource.url === url && frame.provisionalLoaderIdentifier === loaderIdentifier)
                resource = frame.provisionalMainResource;
            else {
                resource = new WI.Resource(url, null, type, loaderIdentifier, targetId, requestIdentifier, requestMethod, requestHeaders, requestData, elapsedTime, walltime, initiatorSourceCodeLocation, originalRequestWillBeSentTimestamp);
                if (resource.target === WI.pageTarget)
                    this._addResourceToFrame(frame, resource);
                else if (resource.target)
                    resource.target.addResource(resource);
                else
                    this._addOrphanedResource(resource, targetId);
            }
        } else {
            // This is a new request for a new frame, which is always the main resource.
            console.assert(WI.sharedApp.debuggableType !== WI.DebuggableType.ServiceWorker);
            console.assert(!targetId);
            resource = new WI.Resource(url, null, type, loaderIdentifier, targetId, requestIdentifier, requestMethod, requestHeaders, requestData, elapsedTime, walltime, initiatorSourceCodeLocation, originalRequestWillBeSentTimestamp);
            frame = new WI.Frame(frameIdentifier, frameName, frameSecurityOrigin, loaderIdentifier, resource);
            this._frameIdentifierMap.set(frame.id, frame);

            // If we don't have a main frame, assume this is it. This can change later in
            // frameDidNavigate when the parent frame is known.
            if (!this._mainFrame) {
                this._mainFrame = frame;
                this._mainFrameDidChange(null);
            }

            this._dispatchFrameWasAddedEvent(frame);
        }

        console.assert(resource);

        return resource;
    }

    _addResourceToFrame(frame, resource)
    {
        console.assert(!this._waitingForMainFrameResourceTreePayload);
        if (this._waitingForMainFrameResourceTreePayload)
            return;

        console.assert(frame);
        console.assert(resource);

        if (resource.loaderIdentifier !== frame.loaderIdentifier && !frame.provisionalLoaderIdentifier) {
            // This is the start of a provisional load which happens before frameDidNavigate is called.
            // This resource will be the new mainResource if frameDidNavigate is called.
            frame.startProvisionalLoad(resource);
            return;
        }

        // This is just another resource, either for the main loader or the provisional loader.
        console.assert(resource.loaderIdentifier === frame.loaderIdentifier || resource.loaderIdentifier === frame.provisionalLoaderIdentifier);
        frame.addResource(resource);
    }

    _addResourceToTarget(target, resource)
    {
        console.assert(target !== WI.pageTarget);
        console.assert(resource);

        target.addResource(resource);
    }

    _initiatorSourceCodeLocationFromPayload(initiatorPayload)
    {
        if (!initiatorPayload)
            return null;

        var url = null;
        var lineNumber = NaN;
        var columnNumber = 0;

        if (initiatorPayload.stackTrace && initiatorPayload.stackTrace.length) {
            var stackTracePayload = initiatorPayload.stackTrace;
            for (var i = 0; i < stackTracePayload.length; ++i) {
                var callFramePayload = stackTracePayload[i];
                if (!callFramePayload.url || callFramePayload.url === "[native code]")
                    continue;

                url = callFramePayload.url;

                // The lineNumber is 1-based, but we expect 0-based.
                lineNumber = callFramePayload.lineNumber - 1;

                columnNumber = callFramePayload.columnNumber;

                break;
            }
        } else if (initiatorPayload.url) {
            url = initiatorPayload.url;

            // The lineNumber is 1-based, but we expect 0-based.
            lineNumber = initiatorPayload.lineNumber - 1;
        }

        if (!url || isNaN(lineNumber) || lineNumber < 0)
            return null;

        var sourceCode = WI.networkManager.resourceForURL(url);
        if (!sourceCode)
            sourceCode = WI.debuggerManager.scriptsForURL(url, WI.mainTarget)[0];

        if (!sourceCode)
            return null;

        return sourceCode.createSourceCodeLocation(lineNumber, columnNumber);
    }

    _processServiceWorkerConfiguration(error, initializationPayload)
    {
        console.assert(this._waitingForMainFrameResourceTreePayload);
        this._waitingForMainFrameResourceTreePayload = false;

        if (error) {
            console.error(JSON.stringify(error));
            return;
        }

        console.assert(initializationPayload.targetId.startsWith("serviceworker:"));

        WI.mainTarget.identifier = initializationPayload.targetId;
        WI.mainTarget.name = initializationPayload.url;

        // Create a main resource with this content in case the content never shows up as a WI.Script.
        const type = WI.Script.SourceType.Program;
        let script = new WI.LocalScript(WI.mainTarget, initializationPayload.url, type, initializationPayload.content);
        WI.mainTarget.mainResource = script;

        InspectorBackend.runAfterPendingDispatches(() => {
            if (WI.mainTarget.mainResource === script) {
                // We've now received all the scripts, if we don't have a better main resource use this LocalScript.
                WI.debuggerManager.dataForTarget(WI.mainTarget).addScript(script);
                WI.debuggerManager.dispatchEventToListeners(WI.DebuggerManager.Event.ScriptAdded, {script});
            }
        });
    }

    _processMainFrameResourceTreePayload(error, mainFramePayload)
    {
        console.assert(this._waitingForMainFrameResourceTreePayload);
        this._waitingForMainFrameResourceTreePayload = false;

        if (error) {
            console.error(JSON.stringify(error));
            return;
        }

        console.assert(mainFramePayload);
        console.assert(mainFramePayload.frame);

        this._resourceRequestIdentifierMap = new Map;
        this._frameIdentifierMap = new Map;

        var oldMainFrame = this._mainFrame;

        this._mainFrame = this._addFrameTreeFromFrameResourceTreePayload(mainFramePayload, true);

        if (this._mainFrame !== oldMainFrame)
            this._mainFrameDidChange(oldMainFrame);
    }

    _createFrame(payload)
    {
        // If payload.url is missing or empty then this page is likely the special empty page. In that case
        // we will just say it is "about:blank" so we have a URL, which is required for resources.
        var mainResource = new WI.Resource(payload.url || "about:blank", payload.mimeType, null, payload.loaderId);
        var frame = new WI.Frame(payload.id, payload.name, payload.securityOrigin, payload.loaderId, mainResource);

        this._frameIdentifierMap.set(frame.id, frame);

        mainResource.markAsFinished();

        return frame;
    }

    _createResource(payload, framePayload)
    {
        var resource = new WI.Resource(payload.url, payload.mimeType, payload.type, framePayload.loaderId, payload.targetId);

        if (payload.sourceMapURL)
            WI.sourceMapManager.downloadSourceMap(payload.sourceMapURL, resource.url, resource);

        return resource;
    }

    _addFrameTreeFromFrameResourceTreePayload(payload, isMainFrame)
    {
        var frame = this._createFrame(payload.frame);
        if (isMainFrame)
            frame.markAsMainFrame();

        for (var i = 0; payload.childFrames && i < payload.childFrames.length; ++i)
            frame.addChildFrame(this._addFrameTreeFromFrameResourceTreePayload(payload.childFrames[i], false));

        for (var i = 0; payload.resources && i < payload.resources.length; ++i) {
            var resourcePayload = payload.resources[i];

            // The main resource is included as a resource. We can skip it since we already created
            // a main resource when we created the Frame. The resource payload does not include anything
            // didn't already get from the frame payload.
            if (resourcePayload.type === "Document" && resourcePayload.url === payload.frame.url)
                continue;

            var resource = this._createResource(resourcePayload, payload);
            if (resource.target === WI.pageTarget)
                frame.addResource(resource);
            else if (resource.target)
                resource.target.addResource(resource);
            else
                this._addOrphanedResource(resource, resourcePayload.targetId);

            if (resourcePayload.failed || resourcePayload.canceled)
                resource.markAsFailed(resourcePayload.canceled);
            else
                resource.markAsFinished();
        }

        this._dispatchFrameWasAddedEvent(frame);

        return frame;
    }

    _addOrphanedResource(resource, targetId)
    {
        let resources = this._orphanedResources.get(targetId);
        if (!resources) {
            resources = [];
            this._orphanedResources.set(targetId, resources);
        }

        resources.push(resource);
    }

    _dispatchFrameWasAddedEvent(frame)
    {
        this.dispatchEventToListeners(WI.NetworkManager.Event.FrameWasAdded, {frame});
    }

    _mainFrameDidChange(oldMainFrame)
    {
        if (oldMainFrame)
            oldMainFrame.unmarkAsMainFrame();
        if (this._mainFrame)
            this._mainFrame.markAsMainFrame();

        this.dispatchEventToListeners(WI.NetworkManager.Event.MainFrameDidChange, {oldMainFrame});
    }

    _extraDomainsActivated(event)
    {
        if (event.data.domains.includes("Page") && window.PageAgent)
            PageAgent.getResourceTree(this._processMainFrameResourceTreePayload.bind(this));
    }
};

WI.NetworkManager.Event = {
    FrameWasAdded: "network-manager-frame-was-added",
    FrameWasRemoved: "network-manager-frame-was-removed",
    MainFrameDidChange: "network-manager-main-frame-did-change",
};
