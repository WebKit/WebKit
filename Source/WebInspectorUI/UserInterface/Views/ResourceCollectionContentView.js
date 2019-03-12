/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.ResourceCollectionContentView = class ResourceCollectionContentView extends WI.CollectionContentView
{
    constructor(collection)
    {
        console.assert(collection instanceof WI.ResourceCollection);

        let contentViewConstructor = null;
        if (collection.resourceType === WI.Resource.Type.Image)
            contentViewConstructor = WI.ImageResourceContentView;

        super(collection, contentViewConstructor);

        if (collection.resourceType === WI.Resource.Type.Image) {
            let allItem = new WI.ScopeBarItem("all", WI.UIString("All"));

            let items = [allItem];
            this._scopeBarItemTypeMap = {};

            let addItem = (key, label) => {
                let item = new WI.ScopeBarItem(key, label);
                items.push(item);
                this._scopeBarItemTypeMap[key] = item;
            };
            addItem("bmp", WI.UIString("BMP"));
            addItem("gif", WI.UIString("GIF"));
            addItem("ico", WI.UIString("ICO"));
            addItem("jp2", WI.UIString("JP2"));
            addItem("jpg", WI.UIString("JPEG"));
            addItem("pdf", WI.UIString("PDF"));
            addItem("png", WI.UIString("PNG"));
            addItem("svg", WI.UIString("SVG"));
            addItem("tiff", WI.UIString("TIFF"));
            addItem("webp", WI.UIString("WebP"));
            addItem("xbm", WI.UIString("XBM"));

            const shouldGroupNonExclusiveItems = true;
            this._imageTypeScopeBar = new WI.ScopeBar("resource-collection-image-type-scope-bar", items, allItem, shouldGroupNonExclusiveItems);
            this._imageTypeScopeBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._handleImageTypeSelectionChanged, this);
        }
    }

    // Public

    get navigationItems()
    {
        let navigationItems = super.navigationItems;
        if (this._imageTypeScopeBar)
            navigationItems.unshift(this._imageTypeScopeBar);
        return navigationItems;
    }

    // Protected

    contentViewAdded(contentView)
    {
        console.assert(contentView instanceof WI.ResourceContentView);

        let resource = contentView.representedObject;
        console.assert(resource instanceof WI.Resource);

        this._updateImageTypeScopeBar();

        contentView.addEventListener(WI.ResourceContentView.Event.ContentError, this._handleContentError, this);
        contentView.element.title = WI.displayNameForURL(resource.url, resource.urlComponents);
    }

    contentViewRemoved(contentView)
    {
        this._updateImageTypeScopeBar();
    }

    // Private

    _updateImageTypeScopeBar()
    {
        let extensions = new Set;

        for (let resource of this.representedObject)
            extensions.add(WI.fileExtensionForMIMEType(resource.mimeType));

        for (let [key, item] of Object.entries(this._scopeBarItemTypeMap)) {
            let hidden = !extensions.has(key);
            item.hidden = hidden;
            if (hidden && item.selected)
                item.selected = false;
        }
    }

    _handleImageTypeSelectionChanged()
    {
        let selectedTypes = this._imageTypeScopeBar.selectedItems.map((item) => item.id);
        let allTypesAllowed = selectedTypes.length === 1 && selectedTypes[0] === "all";
        for (let view of this.subviews) {
            let hidden = !allTypesAllowed;
            if (hidden && view instanceof WI.ResourceContentView)
                hidden = !selectedTypes.includes(WI.fileExtensionForMIMEType(view.representedObject.mimeType));
            view.element.hidden = hidden;
        }
    }

    _handleContentError(event)
    {
        if (event && event.target)
            this.removeContentViewForItem(event.target.representedObject);
    }
};
