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

WI.ResourceTreeElement = class ResourceTreeElement extends WI.SourceCodeTreeElement
{
    constructor(resource, representedObject)
    {
        console.assert(resource instanceof WI.Resource);

        const title = null;
        const subtitle = null;
        super(resource, ["resource", WI.ResourceTreeElement.ResourceIconStyleClassName, resource.type], title, subtitle, representedObject || resource);

        this._updateResource(resource);
    }

    // Static

    static compareResourceTreeElements(a, b)
    {
        // Compare by type first to keep resources grouped by type when not sorted into folders.
        var comparisonResult = a.resource.type.extendedLocaleCompare(b.resource.type);
        if (comparisonResult !== 0)
            return comparisonResult;

        // Compare async resource types by their first timestamp so they are in chronological order.
        if (a.resource.type === WI.Resource.Type.XHR
            || a.resource.type === WI.Resource.Type.Fetch
            || a.resource.type === WI.Resource.Type.WebSocket)
            return a.resource.firstTimestamp - b.resource.firstTimestamp || 0;

        // Compare by subtitle when the types are the same. The subtitle is used to show the
        // domain of the resource. This causes resources to group by domain. If the resource
        // is on the same domain as the frame it will have an empty subtitle. This is good
        // because empty string sorts first, so those will appear before external resources.
        comparisonResult = a.subtitle.extendedLocaleCompare(b.subtitle);
        if (comparisonResult !== 0)
            return comparisonResult;

        // Compare by title when the subtitles are the same.
        return a.mainTitle.extendedLocaleCompare(b.mainTitle);
    }

    static compareFolderAndResourceTreeElements(a, b)
    {
        var aIsFolder = a instanceof WI.FolderTreeElement;
        var bIsFolder = b instanceof WI.FolderTreeElement;

        if (aIsFolder && !bIsFolder)
            return -1;
        if (!aIsFolder && bIsFolder)
            return 1;
        if (aIsFolder && bIsFolder)
            return a.mainTitle.extendedLocaleCompare(b.mainTitle);

        return WI.ResourceTreeElement.compareResourceTreeElements(a, b);
    }

    // Public

    get resource()
    {
        return this._resource;
    }

    get filterableData()
    {
        let urlComponents = this._resource.urlComponents;
        return {text: [urlComponents.lastPathComponent, urlComponents.path, this._resource.url]};
    }

    ondblclick()
    {
        if (this._resource.type === WI.Resource.Type.WebSocket)
            return;

        InspectorFrontendHost.openInNewTab(this._resource.url);
    }

    // Protected (Used by FrameTreeElement)

    _updateResource(resource)
    {
        console.assert(resource instanceof WI.Resource);

        // This method is for subclasses like FrameTreeElement who don't use a resource as the representedObject.
        // This method should only be called once if the representedObject is a resource, since changing the resource
        // without changing the representedObject is bad. If you need to change the resource, make a new ResourceTreeElement.
        console.assert(!this._resource || !(this.representedObject instanceof WI.Resource));

        if (this._resource) {
            this._resource.removeEventListener(WI.Resource.Event.URLDidChange, this._urlDidChange, this);
            this._resource.removeEventListener(WI.Resource.Event.TypeDidChange, this._typeDidChange, this);
            this._resource.removeEventListener(WI.Resource.Event.LoadingDidFinish, this._updateStatus, this);
            this._resource.removeEventListener(WI.Resource.Event.LoadingDidFail, this._updateStatus, this);
        }

        this._updateSourceCode(resource);

        this._resource = resource;

        resource.addEventListener(WI.Resource.Event.URLDidChange, this._urlDidChange, this);
        resource.addEventListener(WI.Resource.Event.TypeDidChange, this._typeDidChange, this);
        resource.addEventListener(WI.Resource.Event.LoadingDidFinish, this._updateStatus, this);
        resource.addEventListener(WI.Resource.Event.LoadingDidFail, this._updateStatus, this);

        this._updateTitles();
        this._updateStatus();
        this._updateToolTip();
    }

    // Protected

    get mainTitleText()
    {
        return WI.displayNameForURL(this._resource.url, this._resource.urlComponents);
    }

    _updateTitles()
    {
        var frame = this._resource.parentFrame;
        var target = this._resource.target;

        var isMainResource = this._resource.isMainResource();
        var parentResourceHost = target.mainResource ? target.mainResource.urlComponents.host : null;
        if (isMainResource && frame) {
            // When the resource is a main resource, get the host from the current frame's parent frame instead of the current frame.
            parentResourceHost = frame.parentFrame ? frame.parentFrame.mainResource.urlComponents.host : null;
        } else if (frame) {
            // When the resource is a normal sub-resource, get the host from the current frame's main resource.
            parentResourceHost = frame.mainResource.urlComponents.host;
        }

        var urlComponents = this._resource.urlComponents;

        var oldMainTitle = this.mainTitle;
        this.mainTitle = this.mainTitleText;

        // Show the host as the subtitle if it is different from the main resource or if this is the main frame's main resource.
        var subtitle = parentResourceHost !== urlComponents.host || frame && frame.isMainFrame() && isMainResource ? WI.displayNameForHost(urlComponents.host) : null;
        this.subtitle = this.mainTitle !== subtitle ? subtitle : null;

        if (oldMainTitle !== this.mainTitle)
            this.callFirstAncestorFunction("descendantResourceTreeElementMainTitleDidChange", [this, oldMainTitle]);
    }

    populateContextMenu(contextMenu, event)
    {
        WI.appendContextMenuItemsForSourceCode(contextMenu, this._resource);

        super.populateContextMenu(contextMenu, event);
    }

    // Private

    _updateStatus()
    {
        if (this._resource.hadLoadingError())
            this.addClassName(WI.ResourceTreeElement.FailedStyleClassName);
        else
            this.removeClassName(WI.ResourceTreeElement.FailedStyleClassName);

        if (this._resource.isLoading()) {
            if (!this.status || !this.status[WI.ResourceTreeElement.SpinnerSymbol]) {
                let spinner = new WI.IndeterminateProgressSpinner;
                this.status = spinner.element;
                this.status[WI.ResourceTreeElement.SpinnerSymbol] = true;
            }
        } else {
            if (this.status && this.status[WI.ResourceTreeElement.SpinnerSymbol])
                this.status = "";
        }
    }

    _updateToolTip()
    {
        this.tooltip = this._resource.displayURL;
    }

    _urlDidChange(event)
    {
        this._updateTitles();
        this._updateToolTip();
    }

    _typeDidChange(event)
    {
        this.removeClassName(event.data.oldType);
        this.addClassName(this._resource.type);

        this.callFirstAncestorFunction("descendantResourceTreeElementTypeDidChange", [this, event.data.oldType]);
    }
};

WI.ResourceTreeElement.ResourceIconStyleClassName = "resource-icon";
WI.ResourceTreeElement.FailedStyleClassName = "failed";

WI.ResourceTreeElement.SpinnerSymbol = Symbol("spinner");
