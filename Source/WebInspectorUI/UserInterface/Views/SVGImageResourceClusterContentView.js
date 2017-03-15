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

WebInspector.SVGImageResourceClusterContentView = class SVGImageResourceClusterContentView extends WebInspector.ClusterContentView
{
    constructor(resource)
    {
        super(resource);

        this._resource = resource;

        let createPathComponent = (displayName, className, identifier) => {
            const textOnly = true;
            const showSelectorArrows = true;
            let pathComponent = new WebInspector.HierarchicalPathComponent(displayName, className, identifier, textOnly, showSelectorArrows);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
            return pathComponent;
        };

        this._imagePathComponent = createPathComponent(WebInspector.UIString("Image"), "image", WebInspector.SVGImageResourceClusterContentView.Identifier.Image);
        this._sourcePathComponent = createPathComponent(WebInspector.UIString("Source"), "source", WebInspector.SVGImageResourceClusterContentView.Identifier.Source);

        this._imagePathComponent.nextSibling = this._sourcePathComponent;
        this._sourcePathComponent.previousSibling = this._imagePathComponent;

        this._currentContentViewSetting = new WebInspector.Setting("svg-image-resource-cluster-current-view-" + this._resource.url.hash, WebInspector.SVGImageResourceClusterContentView.Identifier.Image);
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
        cookie[WebInspector.SVGImageResourceClusterContentView.ContentViewIdentifierCookieKey] = this._currentContentViewSetting.value;
    }

    restoreFromCookie(cookie)
    {
        let contentView = this._showContentViewForIdentifier(cookie[WebInspector.SVGImageResourceClusterContentView.ContentViewIdentifierCookieKey]);
        if (typeof contentView.revealPosition === "function" && "lineNumber" in cookie && "columnNumber" in cookie)
            contentView.revealPosition(new WebInspector.SourceCodePosition(cookie.lineNumber, cookie.columnNumber));
    }

    // Private

    _pathComponentForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView instanceof WebInspector.ImageResourceContentView)
            return this._imagePathComponent;
        if (contentView instanceof WebInspector.TextContentView)
            return this._sourcePathComponent;
        console.error("Unknown contentView.");
        return null;
    }

    _identifierForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView instanceof WebInspector.ImageResourceContentView)
            return WebInspector.SVGImageResourceClusterContentView.Identifier.Image;
        if (contentView instanceof WebInspector.TextContentView)
            return WebInspector.SVGImageResourceClusterContentView.Identifier.Source;
        console.error("Unknown contentView.");
        return null;
    }

    _showContentViewForIdentifier(identifier)
    {
        let contentViewToShow = null;

        switch (identifier) {
        case WebInspector.SVGImageResourceClusterContentView.Identifier.Image:
            contentViewToShow = new WebInspector.ImageResourceContentView(this._resource);
            break;

        case WebInspector.SVGImageResourceClusterContentView.Identifier.Source:
            contentViewToShow = new WebInspector.TextContentView("", this._resource.mimeType);

            this._resource.requestContent().then((result) => {
                let fileReader = new FileReader;
                fileReader.addEventListener("loadend", () => {
                    contentViewToShow.textEditor.string = fileReader.result;
                });
                fileReader.readAsText(result.content);
            });
            break;

        default:
            // Default to showing the image.
            contentViewToShow = new WebInspector.ImageResourceContentView(this._resource);
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

WebInspector.SVGImageResourceClusterContentView.ContentViewIdentifierCookieKey = "svg-image-resource-cluster-content-view-identifier";

WebInspector.SVGImageResourceClusterContentView.Identifier = {
    Image: "image",
    Source: "source",
};
