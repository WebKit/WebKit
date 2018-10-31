/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

// FIXME: DOMStorageManager lacks advanced multi-target support. (DOMStorage per-target)

WI.DOMStorageManager = class DOMStorageManager extends WI.Object
{
    constructor()
    {
        super();

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.SecurityOriginDidChange, this._securityOriginDidChange, this);

        this.initialize();
    }

    // Target

    initializeTarget(target)
    {
        if (target.DOMStorageAgent)
            target.DOMStorageAgent.enable();
    }

    // Public

    initialize()
    {
        this._domStorageObjects = [];
        this._cookieStorageObjects = {};
    }

    get domStorageObjects()
    {
        return this._domStorageObjects;
    }

    get cookieStorageObjects()
    {
        var cookieStorageObjects = [];
        for (var host in this._cookieStorageObjects)
            cookieStorageObjects.push(this._cookieStorageObjects[host]);
        return cookieStorageObjects;
    }

    domStorageWasAdded(id, host, isLocalStorage)
    {
        var domStorage = new WI.DOMStorageObject(id, host, isLocalStorage);

        this._domStorageObjects.push(domStorage);
        this.dispatchEventToListeners(WI.DOMStorageManager.Event.DOMStorageObjectWasAdded, {domStorage});
    }

    itemsCleared(storageId)
    {
        // Called from WI.DOMStorageObserver.

        let domStorage = this._domStorageForIdentifier(storageId);
        if (domStorage)
            domStorage.itemsCleared(storageId);
    }

    itemRemoved(storageId, key)
    {
        // Called from WI.DOMStorageObserver.

        let domStorage = this._domStorageForIdentifier(storageId);
        if (domStorage)
            domStorage.itemRemoved(key);
    }

    itemAdded(storageId, key, value)
    {
        // Called from WI.DOMStorageObserver.

        let domStorage = this._domStorageForIdentifier(storageId);
        if (domStorage)
            domStorage.itemAdded(key, value);
    }

    itemUpdated(storageId, key, oldValue, value)
    {
        // Called from WI.DOMStorageObserver.

        let domStorage = this._domStorageForIdentifier(storageId);
        if (domStorage)
            domStorage.itemUpdated(key, oldValue, value);
    }

    inspectDOMStorage(id)
    {
        var domStorage = this._domStorageForIdentifier(id);
        console.assert(domStorage);
        if (!domStorage)
            return;
        this.dispatchEventToListeners(WI.DOMStorageManager.Event.DOMStorageObjectWasInspected, {domStorage});
    }

    // Private

    _domStorageForIdentifier(id)
    {
        for (var storageObject of this._domStorageObjects) {
            // The id is an object, so we need to compare the properties using Object.shallowEqual.
            if (Object.shallowEqual(storageObject.id, id))
                return storageObject;
        }

        return null;
    }

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        if (event.target.isMainFrame()) {
            // If we are dealing with the main frame, we want to clear our list of objects, because we are navigating to a new page.
            this.initialize();
            this.dispatchEventToListeners(WI.DOMStorageManager.Event.Cleared);

            this._addDOMStorageIfNeeded(event.target);
        }

        // Add the host of the frame that changed the main resource to the list of hosts there could be cookies for.
        var host = parseURL(event.target.url).host;
        if (!host)
            return;

        if (this._cookieStorageObjects[host])
            return;

        this._cookieStorageObjects[host] = new WI.CookieStorageObject(host);
        this.dispatchEventToListeners(WI.DOMStorageManager.Event.CookieStorageObjectWasAdded, {cookieStorage: this._cookieStorageObjects[host]});
    }

    _addDOMStorageIfNeeded(frame)
    {
        if (!window.DOMStorageAgent)
            return;

        // Don't show storage if we don't have a security origin (about:blank).
        if (!frame.securityOrigin || frame.securityOrigin === "://")
            return;

        // FIXME: Consider passing the other parts of the origin along to domStorageWasAdded.

        var localStorageIdentifier = {securityOrigin: frame.securityOrigin, isLocalStorage: true};
        if (!this._domStorageForIdentifier(localStorageIdentifier))
            this.domStorageWasAdded(localStorageIdentifier, frame.mainResource.urlComponents.host, true);

        var sessionStorageIdentifier = {securityOrigin: frame.securityOrigin, isLocalStorage: false};
        if (!this._domStorageForIdentifier(sessionStorageIdentifier))
            this.domStorageWasAdded(sessionStorageIdentifier, frame.mainResource.urlComponents.host, false);
    }

    _securityOriginDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        this._addDOMStorageIfNeeded(event.target);
    }
};

WI.DOMStorageManager.Event = {
    CookieStorageObjectWasAdded: "dom-storage-manager-cookie-storage-object-was-added",
    DOMStorageObjectWasAdded: "dom-storage-manager-dom-storage-object-was-added",
    DOMStorageObjectWasInspected: "dom-storage-manager-dom-storage-object-was-inspected",
    Cleared: "dom-storage-manager-cleared",
};
