/*
 * Copyright (C) 2017 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.SVGImageResourceClusterContentView = class SVGImageResourceClusterContentView extends WI.ClusterContentView
{
    constructor(resource)
    {
        super(resource);

        this._resource = resource;

        let createPathComponent = (displayName, className, identifier) => {
            const textOnly = false;
            const showSelectorArrows = true;
            let pathComponent = new WI.HierarchicalPathComponent(displayName, className, identifier, textOnly, showSelectorArrows);
            pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
            pathComponent.comparisonData = resource;
            return pathComponent;
        };

        this._imagePathComponent = createPathComponent(WI.UIString("Image"), "image-icon", WI.SVGImageResourceClusterContentView.Identifier.Image);
        this._sourcePathComponent = createPathComponent(WI.UIString("Source"), "source-icon", WI.SVGImageResourceClusterContentView.Identifier.Source);

        this._imagePathComponent.nextSibling = this._sourcePathComponent;
        this._sourcePathComponent.previousSibling = this._imagePathComponent;

        this._currentContentViewSetting = new WI.Setting("svg-image-resource-cluster-current-view-" + this._resource.url.hash, WI.SVGImageResourceClusterContentView.Identifier.Image);
    }

    // Public

    get resource() { return this._resource; }

    get selectionPathComponents()
    {
        let currentContentView = this._contentViewContainer.currentContentView;
        if (!currentContentView)
            return [];

        // Append the current view's path components to the path component representing the current view.
        let components = [this._pathComponentForContentView(currentContentView)];
        return components.concat(currentContentView.selectionPathComponents);
    }

    // Protected

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

    saveToCookie(cookie)
    {
        cookie[WI.SVGImageResourceClusterContentView.ContentViewIdentifierCookieKey] = this._currentContentViewSetting.value;
    }

    restoreFromCookie(cookie)
    {
        let contentView = this._showContentViewForIdentifier(cookie[WI.SVGImageResourceClusterContentView.ContentViewIdentifierCookieKey]);
        if (typeof contentView.revealPosition === "function" && "lineNumber" in cookie && "columnNumber" in cookie)
            contentView.revealPosition(new WI.SourceCodePosition(cookie.lineNumber, cookie.columnNumber));
    }

    // Private

    _pathComponentForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView instanceof WI.ImageResourceContentView)
            return this._imagePathComponent;
        if (contentView instanceof WI.TextContentView)
            return this._sourcePathComponent;
        console.error("Unknown contentView.");
        return null;
    }

    _identifierForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView instanceof WI.ImageResourceContentView)
            return WI.SVGImageResourceClusterContentView.Identifier.Image;
        if (contentView instanceof WI.TextContentView)
            return WI.SVGImageResourceClusterContentView.Identifier.Source;
        console.error("Unknown contentView.");
        return null;
    }

    _showContentViewForIdentifier(identifier)
    {
        let contentViewToShow = null;

        switch (identifier) {
        case WI.SVGImageResourceClusterContentView.Identifier.Image:
            contentViewToShow = new WI.ImageResourceContentView(this._resource);
            break;

        case WI.SVGImageResourceClusterContentView.Identifier.Source:
            contentViewToShow = new WI.TextContentView("", this._resource.mimeType);

            this._resource.requestContent().then((result) => {
                if (typeof result.content === "string") {
                    contentViewToShow.textEditor.string = result.content;
                    return;
                }

                blobAsText(result.content, (text) => {
                    contentViewToShow.textEditor.string = text;
                });
            });
            break;

        default:
            // Default to showing the image.
            contentViewToShow = new WI.ImageResourceContentView(this._resource);
            break;
        }

        this._currentContentViewSetting.value = this._identifierForContentView(contentViewToShow);

        return this.contentViewContainer.showContentView(contentViewToShow);
    }

    _pathComponentSelected(event)
    {
        this._showContentViewForIdentifier(event.data.pathComponent.representedObject);
    }
};

WI.SVGImageResourceClusterContentView.ContentViewIdentifierCookieKey = "svg-image-resource-cluster-content-view-identifier";

WI.SVGImageResourceClusterContentView.Identifier = {
    Image: "image",
    Source: "source",
};
