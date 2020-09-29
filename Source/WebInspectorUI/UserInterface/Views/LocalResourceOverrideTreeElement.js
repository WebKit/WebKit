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

WI.LocalResourceOverrideTreeElement = class LocalResourceOverrideTreeElement extends WI.ResourceTreeElement
{
    constructor(localResource, representedObject, options)
    {
        console.assert(localResource instanceof WI.LocalResource);
        console.assert(localResource.isLocalResourceOverride);
        console.assert(representedObject instanceof WI.LocalResourceOverride);

        super(localResource, representedObject, options);

        this._localResourceOverride = representedObject;

        this._popover = null;
    }

    // Protected

    get mainTitleText()
    {
        let text;
        if (this.representedObject.isRegex) {
            text = "/" + this.resource.url + "/";
            if (!this.representedObject.isCaseSensitive)
                text += "i";
        } else {
            text = super.mainTitleText;
            if (!this.representedObject.isCaseSensitive)
                text = WI.UIString("%s (Case Insensitive)").format(text);
        }
        return text;
    }

    onattach()
    {
        super.onattach();

        this._localResourceOverride.addEventListener(WI.LocalResourceOverride.Event.DisabledChanged, this._handleLocalResourceOverrideDisabledChanged, this);

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleFrameMainResourceDidChange, this);

        this._updateStatusCheckbox();
    }

    ondetach()
    {
        this._localResourceOverride.removeEventListener(WI.LocalResourceOverride.Event.DisabledChanged, this._handleLocalResourceOverrideDisabledChanged, this);

        WI.Frame.removeEventListener(WI.Frame.Event.MainResourceDidChange, this._handleFrameMainResourceDidChange, this);

        super.ondetach();
    }

    ondelete()
    {
        WI.networkManager.removeLocalResourceOverride(this._localResourceOverride);

        return true;
    }

    onspace()
    {
        this._localResourceOverride.disabled = !this._localResourceOverride.disabled;

        return true;
    }

    canSelectOnMouseDown(event)
    {
        if (this.status.contains(event.target))
            return false;

        return super.canSelectOnMouseDown(event);
    }

    populateContextMenu(contextMenu, event)
    {
        contextMenu.__localOverrideItemsAdded = true;

        contextMenu.appendItem(WI.UIString("Edit Local Override\u2026"), (event) => {
            let popover = new WI.LocalResourceOverridePopover(this);
            popover.show(this._localResourceOverride, this.status, [WI.RectEdge.MAX_X, WI.RectEdge.MIN_X]);
        });

        let toggleEnabledString = this._localResourceOverride.disabled ? WI.UIString("Enable Local Override") : WI.UIString("Disable Local Override");
        contextMenu.appendItem(toggleEnabledString, () => {
            this._localResourceOverride.disabled = !this._localResourceOverride.disabled;
        });

        contextMenu.appendItem(WI.UIString("Delete Local Override"), () => {
            WI.networkManager.removeLocalResourceOverride(this._localResourceOverride);
        });

        super.populateContextMenu(contextMenu, event);
    }

    updateStatus()
    {
        // Do nothing. Do not allow ResourceTreeElement / SourceCodeTreeElement to modify our status element.
    }

    // Popover delegate

    willDismissPopover(popover)
    {
        let serializedData = popover.serializedData;
        if (!serializedData)
            return;

        let {type, url, isCaseSensitive, isRegex, mimeType, statusCode, statusText, headers} = serializedData;

        // Do not conflict with an existing override unless we are modifying ourselves.
        let existingOverride = WI.networkManager.localResourceOverrideForURL(url);
        if (existingOverride && existingOverride !== this._localResourceOverride) {
            InspectorFrontendHost.beep();
            return;
        }

        let wasSelected = this.selected;

        let revision = this._localResourceOverride.localResource.currentRevision;
        let newLocalResourceOverride = WI.LocalResourceOverride.create(type, {
            url,
            isCaseSensitive,
            isRegex,
            mimeType,
            statusCode,
            statusText,
            headers,
            content: revision.content,
            base64Encoded: revision.base64Encoded,
        });

        WI.networkManager.removeLocalResourceOverride(this._localResourceOverride);
        WI.networkManager.addLocalResourceOverride(newLocalResourceOverride);

        if (wasSelected) {
            const cookie = null;
            const options = {ignoreNetworkTab: true, ignoreSearchTab: true};
            WI.showRepresentedObject(newLocalResourceOverride, cookie, options);
        }
    }

    // Private

    _updateStatusCheckbox()
    {
        this.status = document.createElement("input");
        this.status.type = "checkbox";
        this.status.checked = !this._localResourceOverride.disabled;
        this.status.addEventListener("change", (event) => {
            this._localResourceOverride.disabled = !event.target.checked;
        });
    }

    _handleLocalResourceOverrideDisabledChanged(event)
    {
        this.status.checked = !this._localResourceOverride.disabled;
    }

    _handleFrameMainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this._updateTitles();
    }
};
