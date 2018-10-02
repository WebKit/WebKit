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

WI.ApplicationCacheManager = class ApplicationCacheManager extends WI.Object
{
    constructor()
    {
        super();

        if (window.ApplicationCacheAgent)
            ApplicationCacheAgent.enable();

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.ChildFrameWasRemoved, this._childFrameWasRemoved, this);

        this._online = true;

        this.initialize();
    }

    // Public

    initialize()
    {
        this._applicationCacheObjects = {};

        if (window.ApplicationCacheAgent)
            ApplicationCacheAgent.getFramesWithManifests(this._framesWithManifestsLoaded.bind(this));
    }

    get applicationCacheObjects()
    {
        var applicationCacheObjects = [];
        for (var id in this._applicationCacheObjects)
            applicationCacheObjects.push(this._applicationCacheObjects[id]);
        return applicationCacheObjects;
    }

    networkStateUpdated(isNowOnline)
    {
        this._online = isNowOnline;

        this.dispatchEventToListeners(WI.ApplicationCacheManager.Event.NetworkStateUpdated, {online: this._online});
    }

    get online()
    {
        return this._online;
    }

    applicationCacheStatusUpdated(frameId, manifestURL, status)
    {
        var frame = WI.networkManager.frameForIdentifier(frameId);
        if (!frame)
            return;

        this._frameManifestUpdated(frame, manifestURL, status);
    }

    requestApplicationCache(frame, callback)
    {
        function callbackWrapper(error, applicationCache)
        {
            if (error) {
                callback(null);
                return;
            }

            callback(applicationCache);
        }

        ApplicationCacheAgent.getApplicationCacheForFrame(frame.id, callbackWrapper);
    }

    // Private

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        if (event.target.isMainFrame()) {
            // If we are dealing with the main frame, we want to clear our list of objects, because we are navigating to a new page.
            this.initialize();

            this.dispatchEventToListeners(WI.ApplicationCacheManager.Event.Cleared);

            return;
        }

        if (window.ApplicationCacheAgent)
            ApplicationCacheAgent.getManifestForFrame(event.target.id, this._manifestForFrameLoaded.bind(this, event.target.id));
    }

    _childFrameWasRemoved(event)
    {
        this._frameManifestRemoved(event.data.childFrame);
    }

    _manifestForFrameLoaded(frameId, error, manifestURL)
    {
        if (error)
            return;

        var frame = WI.networkManager.frameForIdentifier(frameId);
        if (!frame)
            return;

        if (!manifestURL)
            this._frameManifestRemoved(frame);
    }

    _framesWithManifestsLoaded(error, framesWithManifests)
    {
        if (error)
            return;

        for (var i = 0; i < framesWithManifests.length; ++i) {
            var frame = WI.networkManager.frameForIdentifier(framesWithManifests[i].frameId);
            if (!frame)
                continue;

            this._frameManifestUpdated(frame, framesWithManifests[i].manifestURL, framesWithManifests[i].status);
        }
    }

    _frameManifestUpdated(frame, manifestURL, status)
    {
        if (status === WI.ApplicationCacheManager.Status.Uncached) {
            this._frameManifestRemoved(frame);
            return;
        }

        if (!manifestURL)
            return;

        var manifestFrame = this._applicationCacheObjects[frame.id];
        if (manifestFrame && manifestURL !== manifestFrame.manifest.manifestURL)
            this._frameManifestRemoved(frame);

        var oldStatus = manifestFrame ? manifestFrame.status : -1;
        var statusChanged = manifestFrame && status !== oldStatus;
        if (manifestFrame)
            manifestFrame.status = status;

        if (!this._applicationCacheObjects[frame.id]) {
            var cacheManifest = new WI.ApplicationCacheManifest(manifestURL);
            this._applicationCacheObjects[frame.id] = new WI.ApplicationCacheFrame(frame, cacheManifest, status);

            this.dispatchEventToListeners(WI.ApplicationCacheManager.Event.FrameManifestAdded, {frameManifest: this._applicationCacheObjects[frame.id]});
        }

        if (statusChanged)
            this.dispatchEventToListeners(WI.ApplicationCacheManager.Event.FrameManifestStatusChanged, {frameManifest: this._applicationCacheObjects[frame.id]});
    }

    _frameManifestRemoved(frame)
    {
        if (!this._applicationCacheObjects[frame.id])
            return;

        delete this._applicationCacheObjects[frame.id];

        this.dispatchEventToListeners(WI.ApplicationCacheManager.Event.FrameManifestRemoved, {frame});
    }
};

WI.ApplicationCacheManager.Event = {
    Cleared: "application-cache-manager-cleared",
    FrameManifestAdded: "application-cache-manager-frame-manifest-added",
    FrameManifestRemoved: "application-cache-manager-frame-manifest-removed",
    FrameManifestStatusChanged: "application-cache-manager-frame-manifest-status-changed",
    NetworkStateUpdated: "application-cache-manager-network-state-updated"
};

WI.ApplicationCacheManager.Status = {
    Uncached: 0,
    Idle: 1,
    Checking: 2,
    Downloading: 3,
    UpdateReady: 4,
    Obsolete: 5
};
