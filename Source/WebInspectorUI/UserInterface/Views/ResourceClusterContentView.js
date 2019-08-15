/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.ResourceClusterContentView = class ResourceClusterContentView extends WI.ClusterContentView
{
    constructor(resource)
    {
        super(resource);

        this._resource = resource;
        this._resource.addEventListener(WI.Resource.Event.TypeDidChange, this._resourceTypeDidChange, this);
        this._resource.addEventListener(WI.Resource.Event.LoadingDidFinish, this._resourceLoadingDidFinish, this);

        function createPathComponent(displayName, className, identifier)
        {
            const textOnly = false;
            const showSelectorArrows = true;
            let pathComponent = new WI.HierarchicalPathComponent(displayName, className, identifier, textOnly, showSelectorArrows);
            pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
            pathComponent.comparisonData = resource;
            return pathComponent;
        }

        this._requestContentView = null;
        this._responseContentView = null;
        this._customRequestContentView = null;
        this._customRequestContentViewInitializer = null;
        this._customResponseContentView = null;
        this._customResponseContentViewInitializer = null;

        this._requestPathComponent = createPathComponent.call(this, WI.UIString("Request"), WI.ResourceClusterContentView.RequestIconStyleClassName, WI.ResourceClusterContentView.RequestIdentifier);
        this._customRequestPathComponent = createPathComponent.call(this, WI.UIString("Custom Request"), WI.ResourceClusterContentView.RequestIconStyleClassName, WI.ResourceClusterContentView.CustomRequestIdentifier);
        this._responsePathComponent = createPathComponent.call(this, WI.UIString("Response"), WI.ResourceClusterContentView.ResponseIconStyleClassName, WI.ResourceClusterContentView.ResponseIdentifier);
        this._customResponsePathComponent = createPathComponent.call(this, WI.UIString("Custom Response"), WI.ResourceClusterContentView.ResponseIconStyleClassName, WI.ResourceClusterContentView.CustomResponseIdentifier);

        if (this._canShowRequestContentView()) {
            this._requestPathComponent.nextSibling = this._responsePathComponent;
            this._responsePathComponent.previousSibling = this._requestPathComponent;

            this._tryEnableCustomRequestContentView();
        }

        // FIXME: Since a custom response content view may only become available after a response is received
        // we need to figure out a way to restore / prefer the custom content view. For example if users
        // always want to prefer the JSON view to the normal Response text view.

        this._currentContentViewSetting = new WI.Setting("resource-current-view-" + this._resource.url.hash, WI.ResourceClusterContentView.CustomResponseIdentifier);

        this._tryEnableCustomResponseContentView();
    }

    // Public

    get resource() { return this._resource; }

    get requestContentView()
    {
        if (!this._canShowRequestContentView())
            return null;

        if (this._requestContentView)
            return this._requestContentView;

        this._requestContentView = new WI.TextContentView(this._resource.requestData || "", this._resource.requestDataContentType);

        return this._requestContentView;
    }

    get responseContentView()
    {
        if (this._responseContentView)
            return this._responseContentView;

        this._responseContentView = this._contentViewForResourceType(this._resource.type);
        if (this._responseContentView)
            return this._responseContentView;

        let typeFromMIMEType = WI.Resource.typeFromMIMEType(this._resource.mimeType);
        this._responseContentView = this._contentViewForResourceType(typeFromMIMEType);
        if (this._responseContentView)
            return this._responseContentView;

        if (WI.shouldTreatMIMETypeAsText(this._resource.mimeType)) {
            this._responseContentView = new WI.TextResourceContentView(this._resource);
            return this._responseContentView;
        }

        this._responseContentView = new WI.GenericResourceContentView(this._resource);
        return this._responseContentView;
    }

    get customRequestContentView()
    {
        if (!this._customRequestContentView && this._customRequestContentViewInitializer) {
            this._customRequestContentView = this._customRequestContentViewInitializer();
            this._customRequestContentViewInitializer = null;
        }
        return this._customRequestContentView;
    }

    get customResponseContentView()
    {
        if (!this._customResponseContentView && this._customResponseContentViewInitializer) {
            this._customResponseContentView = this._customResponseContentViewInitializer();
            this._customResponseContentViewInitializer = null;
        }
        return this._customResponseContentView;
    }

    get selectionPathComponents()
    {
        let currentContentView = this._contentViewContainer.currentContentView;
        if (!currentContentView)
            return [];

        if (!this._canShowRequestContentView() && !this._canShowCustomRequestContentView() && !this._canShowCustomResponseContentView())
            return currentContentView.selectionPathComponents;

        // Append the current view's path components to the path component representing the current view.
        let components = [this._pathComponentForContentView(currentContentView)];
        return components.concat(currentContentView.selectionPathComponents);
    }

    shown()
    {
        super.shown();

        if (this._shownInitialContent)
            return;

        this._showContentViewForIdentifier(this._currentContentViewSetting.value);
    }

    closed()
    {
        super.closed();

        this._shownInitialContent = false;
    }

    restoreFromCookie(cookie)
    {
        let contentView = this._showContentViewForIdentifier(cookie[WI.ResourceClusterContentView.ContentViewIdentifierCookieKey]);
        if (typeof contentView.revealPosition === "function" && "lineNumber" in cookie && "columnNumber" in cookie)
            contentView.revealPosition(new WI.SourceCodePosition(cookie.lineNumber, cookie.columnNumber));
    }

    showRequest()
    {
        this._shownInitialContent = true;

        return this._showContentViewForIdentifier(WI.ResourceClusterContentView.CustomRequestIdentifier);
    }

    showResponse(positionToReveal, textRangeToSelect, forceUnformatted)
    {
        this._shownInitialContent = true;

        if (!this._resource.finished) {
            this._positionToReveal = positionToReveal;
            this._textRangeToSelect = textRangeToSelect;
            this._forceUnformatted = forceUnformatted;
        }

        let responseContentView = this._showContentViewForIdentifier(WI.ResourceClusterContentView.ResponseIdentifier);
        if (typeof responseContentView.revealPosition === "function")
            responseContentView.revealPosition(positionToReveal, textRangeToSelect, forceUnformatted);
        return responseContentView;
    }

    // Private

    _canShowRequestContentView()
    {
        let requestData = this._resource.requestData;
        if (!requestData)
            return false;

        if (this._resource.hasRequestFormParameters())
            return false;

        return true;
    }

    _canShowCustomRequestContentView()
    {
        return !!(this._customRequestContentView || this._customRequestContentViewInitializer);
    }

    _canShowCustomResponseContentView()
    {
        return !!(this._customResponseContentView || this._customResponseContentViewInitializer);
    }

    _contentViewForResourceType(type)
    {
        switch (type) {
        case WI.Resource.Type.Document:
        case WI.Resource.Type.Script:
        case WI.Resource.Type.StyleSheet:
            return new WI.TextResourceContentView(this._resource);

        case WI.Resource.Type.Image:
            if (WI.fileExtensionForMIMEType(this._resource.mimeType) === "svg")
                return new WI.SVGImageResourceClusterContentView(this._resource);
            return new WI.ImageResourceContentView(this._resource);

        case WI.Resource.Type.Font:
            return new WI.FontResourceContentView(this._resource);

        case WI.Resource.Type.WebSocket:
            return new WI.WebSocketContentView(this._resource);

        default:
            return null;
        }
    }

    _pathComponentForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView === this._requestContentView)
            return this._requestPathComponent;
        if (contentView === this._responseContentView)
            return this._responsePathComponent;
        if (contentView === this._customRequestContentView)
            return this._customRequestPathComponent;
        if (contentView === this._customResponseContentView)
            return this._customResponsePathComponent;
        console.error("Unknown contentView.");
        return null;
    }

    _identifierForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView === this._requestContentView)
            return WI.ResourceClusterContentView.RequestIdentifier;
        if (contentView === this._responseContentView)
            return WI.ResourceClusterContentView.ResponseIdentifier;
        if (contentView === this._customRequestContentView)
            return WI.ResourceClusterContentView.CustomRequestIdentifier;
        if (contentView === this._customResponseContentView)
            return WI.ResourceClusterContentView.CustomResponseIdentifier;
        console.error("Unknown contentView.");
        return null;
    }

    _showContentViewForIdentifier(identifier)
    {
        let contentViewToShow = null;

        // This is expected to fall through all the way to the `default`.
        switch (identifier) {
        case WI.ResourceClusterContentView.CustomRequestIdentifier:
            contentViewToShow = this.customRequestContentView;
            if (contentViewToShow)
                break;
            // fallthrough
        case WI.ResourceClusterContentView.RequestIdentifier:
            contentViewToShow = this.requestContentView;
            if (contentViewToShow)
                break;
            // fallthrough
        case WI.ResourceClusterContentView.CustomResponseIdentifier:
            contentViewToShow = this.customResponseContentView;
            if (contentViewToShow)
                break;
            // fallthrough
        case WI.ResourceClusterContentView.ResponseIdentifier:
        default:
            contentViewToShow = this.responseContentView;
            break;
        }

        console.assert(contentViewToShow);

        this._currentContentViewSetting.value = this._identifierForContentView(contentViewToShow);

        return this.contentViewContainer.showContentView(contentViewToShow);
    }

    _pathComponentSelected(event)
    {
        this._showContentViewForIdentifier(event.data.pathComponent.representedObject);
    }

    _resourceTypeDidChange(event)
    {
        // Since resource views are based on the type, we need to make a new content view and tell the container to replace this
        // content view with the new one. Make a new ResourceContentView which will use the new resource type to make the correct
        // concrete ResourceContentView subclass.

        let currentResponseContentView = this._responseContentView;
        if (!currentResponseContentView)
            return;

        this._responseContentView = null;

        this.contentViewContainer.replaceContentView(currentResponseContentView, this.responseContentView);
    }

    _resourceLoadingDidFinish(event)
    {
        this._tryEnableCustomResponseContentView();

        if ("_positionToReveal" in this) {
            if (this._contentViewContainer.currentContentView === this._responseContentView)
                this._responseContentView.revealPosition(this._positionToReveal, this._textRangeToSelect, this._forceUnformatted);

            delete this._positionToReveal;
            delete this._textRangeToSelect;
            delete this._forceUnformatted;
        }
    }

    _canUseJSONContentViewForContent(content)
    {
        return typeof content === "string" && content.isJSON((json) => json && (typeof json === "object" || Array.isArray(json)));
    }

    _tryEnableCustomRequestContentView()
    {
        if (!this._canUseJSONContentViewForContent(this._resource.requestData))
            return;

        this._customRequestContentViewInitializer = () => new WI.JSONContentView(this._resource.requestData, this._resource);

        this._customRequestPathComponent.displayName = WI.UIString("Request (JSON)");
        this._customRequestPathComponent.previousSibling = this._requestPathComponent;
        this._customRequestPathComponent.nextSibling = this._responsePathComponent;
        this._requestPathComponent.nextSibling = this._customRequestPathComponent;
        this._responsePathComponent.previousSibling = this._customRequestPathComponent;
    }

    _tryEnableCustomResponseContentView()
    {
        if (!this._resource.hasResponse())
            return;

        // WebSocket resources already use a "custom" response content view.
        if (this._resource instanceof WI.WebSocketResource)
            return;

        this._resource.requestContent()
        .then(({error, content}) => {
            if (error || !content || !this._canUseJSONContentViewForContent(content))
                return;

            this._customResponseContentViewInitializer = () => new WI.JSONContentView(content, this._resource);

            this._customResponsePathComponent.displayName = WI.UIString("Response (JSON)");
            this._customResponsePathComponent.previousSibling = this._responsePathComponent;
            this._responsePathComponent.nextSibling = this._customResponsePathComponent;

            this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
        });
    }
};

WI.ResourceClusterContentView.ContentViewIdentifierCookieKey = "resource-cluster-content-view-identifier";

WI.ResourceClusterContentView.RequestIconStyleClassName = "request-icon";
WI.ResourceClusterContentView.ResponseIconStyleClassName = "response-icon";
WI.ResourceClusterContentView.RequestIdentifier = "request";
WI.ResourceClusterContentView.ResponseIdentifier = "response";
WI.ResourceClusterContentView.CustomRequestIdentifier = "custom-request";
WI.ResourceClusterContentView.CustomResponseIdentifier = "custom-response";
