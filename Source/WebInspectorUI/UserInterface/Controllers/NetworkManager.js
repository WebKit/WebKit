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

// FIXME: NetworkManager lacks advanced multi-target support. (Network.loadResource invocations per-target)

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

        this._sourceMapURLMap = new Map;
        this._downloadingSourceMaps = new Set;

        WI.notifications.addEventListener(WI.Notification.ExtraDomainsActivated, this._extraDomainsActivated, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleFrameMainResourceDidChange, this);
    }

    // Target

    initializeTarget(target)
    {
        if (target.PageAgent) {
            target.PageAgent.enable();
            target.PageAgent.getResourceTree(this._processMainFrameResourceTreePayload.bind(this));
        }

        if (target.ServiceWorkerAgent)
            target.ServiceWorkerAgent.getInitializationInfo(this._processServiceWorkerConfiguration.bind(this));

        if (target.NetworkAgent) {
            target.NetworkAgent.enable();

            // COMPATIBILITY (iOS 10.3): Network.setDisableResourceCaching did not exist.
            if (target.NetworkAgent.setResourceCachingDisabled)
                target.NetworkAgent.setResourceCachingDisabled(WI.settings.resourceCachingDisabled.value);
        }

        if (target.type === WI.Target.Type.Worker)
            this.adoptOrphanedResourcesForTarget(target);
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

    downloadSourceMap(sourceMapURL, baseURL, originalSourceCode)
    {
        // The baseURL could have come from a "//# sourceURL". Attempt to get a
        // reasonable absolute URL for the base by using the main resource's URL.
        if (WI.networkManager.mainFrame)
            baseURL = absoluteURL(baseURL, WI.networkManager.mainFrame.url);

        if (sourceMapURL.startsWith("data:")) {
            this._loadAndParseSourceMap(sourceMapURL, baseURL, originalSourceCode);
            return;
        }

        sourceMapURL = absoluteURL(sourceMapURL, baseURL);
        if (!sourceMapURL)
            return;

        console.assert(originalSourceCode.url);
        if (!originalSourceCode.url)
            return;

        // FIXME: <rdar://problem/13265694> Source Maps: Better handle when multiple resources reference the same SourceMap

        if (this._sourceMapURLMap.has(sourceMapURL) || this._downloadingSourceMaps.has(sourceMapURL))
            return;

        let loadAndParseSourceMap = () => {
            this._loadAndParseSourceMap(sourceMapURL, baseURL, originalSourceCode);
        };

        if (!WI.networkManager.mainFrame) {
            // If we don't have a main frame, then we are likely in the middle of building the resource tree.
            // Delaying until the next runloop is enough in this case to then start loading the source map.
            setTimeout(loadAndParseSourceMap, 0);
            return;
        }

        loadAndParseSourceMap();
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
            let resourceOptions = {
                loaderIdentifier: framePayload.loaderId,
            };
            let frameOptions = {
                name: framePayload.name,
                securityOrigin: framePayload.securityOrigin,
            };
            let frameResource = this._addNewResourceToFrameOrTarget(framePayload.url, framePayload.id, resourceOptions, frameOptions);
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
            let mainResource = null;
            if (frame.mainResource.url !== framePayload.url || frame.loaderIdentifier !== framePayload.loaderId) {
                // Navigations like back/forward do not have provisional loads, so create a new main resource here.
                mainResource = new WI.Resource(framePayload.url, {
                    mimeType: framePayload.mimeType,
                    loaderIdentifier: framePayload.loaderId,
                });
            } else {
                // The main resource is already correct, so reuse it.
                mainResource = frame.mainResource;
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
            console.assert(resource.parentFrame.id === frameIdentifier);
            console.assert(resource.loaderIdentifier === loaderIdentifier);
            console.assert(!targetId);
            resource.updateForRedirectResponse(request, redirectResponse, elapsedTime, walltime);
            return;
        }

        // This is a new request, make a new resource and add it to the right frame.
        resource = this._addNewResourceToFrameOrTarget(request.url, frameIdentifier, {
            type,
            loaderIdentifier,
            targetId,
            requestIdentifier,
            requestMethod: request.method,
            requestHeaders: request.headers,
            requestData: request.postData,
            requestSentTimestamp: elapsedTime,
            requestSentWalltime: walltime,
            initiatorSourceCodeLocation: this._initiatorSourceCodeLocationFromPayload(initiator),
            initiatorNode: this._initiatorNodeFromPayload(initiator),
            originalRequestWillBeSentTimestamp,
        });

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
        if (!InspectorBackend.domains.Network.hasEventParameter("webSocketWillSendHandshakeRequest", "walltime")) {
            request = arguments[2];
            walltime = NaN;
        }

        // FIXME: <webkit.org/b/168475> Web Inspector: Correctly display iframe's and worker's WebSockets

        let resource = new WI.WebSocketResource(url, {
            loaderIdentifier: WI.networkManager.mainFrame.id,
            requestIdentifier: requestId,
            requestHeaders: request.headers,
            timestamp,
            walltime,
            requestSentTimestamp: WI.timelineManager.computeElapsedTime(timestamp),
        });

        let frame = this.frameForIdentifier(WI.networkManager.mainFrame.id);
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
        let response = cachedResourcePayload.response;
        const responseSource = NetworkAgent.ResponseSource.MemoryCache;

        let resource = this._addNewResourceToFrameOrTarget(cachedResourcePayload.url, frameIdentifier, {
            type: cachedResourcePayload.type,
            loaderIdentifier,
            requestIdentifier,
            requestMethod: "GET",
            requestSentTimestamp: elapsedTime,
            initiatorSourceCodeLocation: this._initiatorSourceCodeLocationFromPayload(initiator),
            initiatorNode: this._initiatorNodeFromPayload(initiator),
        });
        resource.updateForResponse(cachedResourcePayload.url, response.mimeType, cachedResourcePayload.type, response.headers, response.status, response.statusText, elapsedTime, response.timing, responseSource, response.security);
        resource.increaseSize(cachedResourcePayload.bodySize, elapsedTime);
        resource.increaseTransferSize(cachedResourcePayload.bodySize);
        resource.setCachedResponseBodySize(cachedResourcePayload.bodySize);
        resource.markAsFinished(elapsedTime);

        console.assert(resource.cached, "This resource should be classified as cached since it was served from the MemoryCache", resource);

        if (cachedResourcePayload.sourceMapURL)
            this.downloadSourceMap(cachedResourcePayload.sourceMapURL, resource.url, resource);

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
            resource = this._addNewResourceToFrameOrTarget(response.url, frameIdentifier, {
                type,
                loaderIdentifier,
                requestIdentifier,
                requestHeaders: response.requestHeaders,
                requestSentTimestamp: elapsedTime,
            });

            // Associate the resource with the requestIdentifier so it can be found in future loading events.
            this._resourceRequestIdentifierMap.set(requestIdentifier, resource);
        }

        // COMPATIBILITY (iOS 10.3): `fromDiskCache` is legacy, replaced by `source`.
        if (response.fromDiskCache)
            resource.legacyMarkServedFromDiskCache();

        resource.updateForResponse(response.url, response.mimeType, type, response.headers, response.status, response.statusText, elapsedTime, response.timing, response.source, response.security);
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
            this.downloadSourceMap(sourceMapURL, resource.url, resource);

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

    _addNewResourceToFrameOrTarget(url, frameIdentifier, resourceOptions = {}, frameOptions = {})
    {
        console.assert(!this._waitingForMainFrameResourceTreePayload);

        let resource = null;

        if (!frameIdentifier && resourceOptions.targetId) {
            // This is a new resource for a ServiceWorker target.
            console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.ServiceWorker);
            console.assert(resourceOptions.targetId === WI.mainTarget.identifier);
            resource = new WI.Resource(url, resourceOptions);
            resource.target.addResource(resource);
            return resource;
        }

        let frame = this.frameForIdentifier(frameIdentifier);
        if (frame) {
            // This is a new request for an existing frame, which might be the main resource or a new resource.
            if (resourceOptions.type === "Document" && frame.mainResource.url === url && frame.loaderIdentifier === resourceOptions.loaderIdentifier)
                resource = frame.mainResource;
            else if (resourceOptions.type === "Document" && frame.provisionalMainResource && frame.provisionalMainResource.url === url && frame.provisionalLoaderIdentifier === resourceOptions.loaderIdentifier)
                resource = frame.provisionalMainResource;
            else {
                resource = new WI.Resource(url, resourceOptions);
                if (resource.target === WI.pageTarget)
                    this._addResourceToFrame(frame, resource);
                else if (resource.target)
                    resource.target.addResource(resource);
                else
                    this._addOrphanedResource(resource, resourceOptions.targetId);
            }
        } else {
            // This is a new request for a new frame, which is always the main resource.
            console.assert(WI.sharedApp.debuggableType !== WI.DebuggableType.ServiceWorker);
            console.assert(!resourceOptions.targetId);
            resource = new WI.Resource(url, resourceOptions);
            frame = new WI.Frame(frameIdentifier, frameOptions.name, frameOptions.securityOrigin, resourceOptions.loaderIdentifier, resource);
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

    _initiatorNodeFromPayload(initiatorPayload)
    {
        return WI.domManager.nodeForId(initiatorPayload.nodeId);
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
        let mainResource = new WI.Resource(payload.url || "about:blank", {
            mimeType: payload.mimeType,
            loaderIdentifier: payload.loaderId,
        });
        var frame = new WI.Frame(payload.id, payload.name, payload.securityOrigin, payload.loaderId, mainResource);

        this._frameIdentifierMap.set(frame.id, frame);

        mainResource.markAsFinished();

        return frame;
    }

    _createResource(payload, framePayload)
    {
        let resource = new WI.Resource(payload.url, {
            mimeType: payload.mimeType,
            type: payload.type,
            loaderIdentifier: framePayload.loaderId,
            targetId: payload.targetId,
        });

        if (payload.sourceMapURL)
            this.downloadSourceMap(payload.sourceMapURL, resource.url, resource);

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

    _loadAndParseSourceMap(sourceMapURL, baseURL, originalSourceCode)
    {
        this._downloadingSourceMaps.add(sourceMapURL);

        let sourceMapLoaded = (error, content, mimeType, statusCode) => {
            if (error || statusCode >= 400) {
                this._sourceMapLoadAndParseFailed(sourceMapURL);
                return;
            }

            if (content.slice(0, 3) === ")]}") {
                let firstNewlineIndex = content.indexOf("\n");
                if (firstNewlineIndex === -1) {
                    this._sourceMapLoadAndParseFailed(sourceMapURL);
                    return;
                }

                content = content.substring(firstNewlineIndex);
            }

            try {
                let payload = JSON.parse(content);
                let baseURL = sourceMapURL.startsWith("data:") ? originalSourceCode.url : sourceMapURL;
                let sourceMap = new WI.SourceMap(baseURL, payload, originalSourceCode);
                this._sourceMapLoadAndParseSucceeded(sourceMapURL, sourceMap);
            } catch {
                this._sourceMapLoadAndParseFailed(sourceMapURL);
            }
        };

        if (sourceMapURL.startsWith("data:")) {
            let {mimeType, base64, data} = parseDataURL(sourceMapURL);
            let content = base64 ? atob(data) : data;
            sourceMapLoaded(null, content, mimeType, 0);
            return;
        }

        if (!window.NetworkAgent) {
            this._sourceMapLoadAndParseFailed(sourceMapURL);
            return;
        }

        let frameIdentifier = null;
        if (originalSourceCode instanceof WI.Resource && originalSourceCode.parentFrame)
            frameIdentifier = originalSourceCode.parentFrame.id;

        if (!frameIdentifier)
            frameIdentifier = WI.networkManager.mainFrame ? WI.networkManager.mainFrame.id : "";

        NetworkAgent.loadResource(frameIdentifier, sourceMapURL, sourceMapLoaded);
    }

    _sourceMapLoadAndParseFailed(sourceMapURL)
    {
        this._downloadingSourceMaps.delete(sourceMapURL);
    }

    _sourceMapLoadAndParseSucceeded(sourceMapURL, sourceMap)
    {
        if (!this._downloadingSourceMaps.has(sourceMapURL))
            return;

        this._downloadingSourceMaps.delete(sourceMapURL);

        this._sourceMapURLMap.set(sourceMapURL, sourceMap);

        for (let source of sourceMap.sources())
            sourceMap.addResource(new WI.SourceMapResource(source, sourceMap));

        // Associate the SourceMap with the originalSourceCode.
        sourceMap.originalSourceCode.addSourceMap(sourceMap);

        // If the originalSourceCode was not a Resource, be sure to also associate with the Resource if one exists.
        // FIXME: We should try to use the right frame instead of a global lookup by URL.
        if (!(sourceMap.originalSourceCode instanceof WI.Resource)) {
            console.assert(sourceMap.originalSourceCode instanceof WI.Script);
            let resource = sourceMap.originalSourceCode.resource;
            if (resource)
                resource.addSourceMap(sourceMap);
        }
    }

    _extraDomainsActivated(event)
    {
        if (event.data.domains.includes("Page") && window.PageAgent)
            PageAgent.getResourceTree(this._processMainFrameResourceTreePayload.bind(this));
    }

    _handleFrameMainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this._sourceMapURLMap.clear();
        this._downloadingSourceMaps.clear();
    }
};

WI.NetworkManager.Event = {
    FrameWasAdded: "network-manager-frame-was-added",
    FrameWasRemoved: "network-manager-frame-was-removed",
    MainFrameDidChange: "network-manager-main-frame-did-change",
};
