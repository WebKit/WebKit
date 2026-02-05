/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    distribution and/or other materials provided with the distribution.
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

WI.NetworkRedirectDetailView = class NetworkRedirectDetailView extends WI.NetworkDetailView
{
    constructor(redirect, parentResource, redirectIndex, parentEntry, delegate)
    {
        console.assert(redirect instanceof WI.Redirect, redirect);
        console.assert(parentResource instanceof WI.Resource, parentResource);

        super(redirect, delegate);

        this._redirect = redirect;
        this._parentResource = parentResource;
        this._redirectIndex = redirectIndex;
        this._parentEntry = parentEntry;

        this.element.classList.add("redirect");

        this._headersContentView = null;
    }

    // Public

    showParentResource()
    {
        this._delegate?.networkRedirectDetailViewShowParentResource(this, this._parentResource);
    }

    // Protected

    initialLayout()
    {
        this.createDetailNavigationItem("headers", WI.UIString("Headers"));

        super.initialLayout();
    }

    // Private

    showContentViewForIdentifier(identifier)
    {
        super.showContentViewForIdentifier(identifier);

        switch (identifier) {
        case "headers":
            this._headersContentView ||= new WI.NetworkRedirectHeadersContentView(this._redirect, this._parentResource, this._redirectIndex, this._parentEntry, this._delegate);
            this._contentBrowser.showContentView(this._headersContentView);
            break;
        }
    }
};
