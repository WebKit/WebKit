/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

WI.LocalResourceOverrideRequestContentView = class LocalResourceOverrideRequestContentView extends WI.TextContentView
{
    constructor(localResourceOverride)
    {
        console.assert(localResourceOverride instanceof WI.LocalResourceOverride, localResourceOverride);
        console.assert(localResourceOverride.type === WI.LocalResourceOverride.InterceptType.Request, localResourceOverride);
        console.assert(WI.NetworkManager.supportsOverridingRequests());

        let localResource = localResourceOverride.localResource;
        let string = localResource.requestData || "";
        let mimeType = localResource.requestDataContentType;
        super(string, mimeType, localResourceOverride);

        this.element.classList.add("local-resource-override-request");

        this._removeLocalResourceOverrideButtonNavigationItem = new WI.ButtonNavigationItem("remove-local-resource-override", WI.UIString("Delete Local Override"), "Images/NavigationItemTrash.svg", 15, 15);
        this._removeLocalResourceOverrideButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleRemoveLocalResourceOverride, this);
        this._removeLocalResourceOverrideButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
    }

    // Public

    get navigationItems()
    {
        return [this._removeLocalResourceOverrideButtonNavigationItem];
    }

    get saveData()
    {
        let saveData = super.saveData;
        saveData.suggestedName = WI.UIString("%s Request Data", "%s Request Data @ Local Override Request Content View", "Format string for the suggested filename when saving the content for a request local override.").format(this.representedObject.url) + ".txt";
        return saveData;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this.insertSubviewBefore(new WI.LocalResourceOverrideLabelView(this.representedObject), this.textEditor);

        this.textEditor.readOnly = false;
        this.textEditor.addEventListener(WI.TextEditor.Event.ContentDidChange, this._handleTextEditorContentDidChange, this);

        let requestMethod = this.representedObject.localResource.requestMethod;
        if (!WI.HTTPUtilities.RequestMethodsWithBody.has(requestMethod)) {
            let message = WI.createMessageTextView(WI.UIString("%s requests do not have a body").format(requestMethod));
            this.element.appendChild(message);
        }
    }

    // Private

    _handleRemoveLocalResourceOverride(event)
    {
        WI.networkManager.removeLocalResourceOverride(this.representedObject);
    }

    _handleTextEditorContentDidChange(event)
    {
        this.representedObject.localResource.updateLocalResourceOverrideRequestData(this.textEditor.string);
    }
};
