/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.LocalResourceOverrideWarningView = class LocalResourceOverrideWarningView extends WI.View
{
    constructor(resource)
    {
        console.assert(!resource.isLocalResourceOverride);

        super();

        this._resource = resource;

        this.element.classList.add("local-resource-override-warning-view");
    }

    // Protected

    attached()
    {
        super.attached();

        WI.networkManager.addEventListener(WI.NetworkManager.Event.LocalResourceOverrideAdded, this._handleLocalResourceOverrideAddedOrRemoved, this);
        WI.networkManager.addEventListener(WI.NetworkManager.Event.LocalResourceOverrideRemoved, this._handleLocalResourceOverrideAddedOrRemoved, this);

        this._updateContent();
    }

    detached()
    {
        WI.networkManager.removeEventListener(null, null, this);

        super.detached();
    }

    initialLayout()
    {
        this._revealButton = document.createElement("button");
        this._revealButton.textContent = WI.UIString("Reveal");
        this._revealButton.addEventListener("click", (event) => {
            const cookie = null;
            const options = {ignoreNetworkTab: true, ignoreSearchTab: true};
            let overrideResource = WI.networkManager.localResourceOverrideForURL(this._resource.url);
            WI.showRepresentedObject(overrideResource, cookie, options);
        });

        let container = this.element.appendChild(document.createElement("div"));
        container.append(this._revealButton, WI.UIString("This resource came from a local override"));

        this._updateContent();
    }

    // Private

    _updateContent()
    {
        if (!this._revealButton)
            return;

        let hasLocalResourceOverride = !!WI.networkManager.localResourceOverrideForURL(this._resource.url);
        this._revealButton.hidden = !hasLocalResourceOverride;

        let resourceShowingOverrideContent = this._resource.responseSource === WI.Resource.ResponseSource.InspectorOverride;
        this.element.hidden = !resourceShowingOverrideContent;
    }

    _handleLocalResourceOverrideAddedOrRemoved(event)
    {
        if (this._resource.url !== event.data.localResourceOverride.url)
            return;

        this._updateContent();
    }
};
