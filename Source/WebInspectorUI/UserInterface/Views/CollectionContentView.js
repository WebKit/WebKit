/*
 * Copyright (C) 2016 Devin Rousso <dcrousso+webkit@gmail.com>. All rights reserved.
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

WI.CollectionContentView = class CollectionContentView extends WI.ContentView
{
    constructor(collection)
    {
        console.assert(collection instanceof WI.Collection);

        super(collection);

        this.representedObject.addEventListener(WI.Collection.Event.ItemAdded, this._handleItemAdded, this);
        this.representedObject.addEventListener(WI.Collection.Event.ItemRemoved, this._handleItemRemoved, this);

        this._contentViewMap = new WeakMap;
        this._handleClickMap = new WeakMap;

        this._contentViewConstructor = null;
        let title = "";
        switch (this.representedObject.typeVerifier) {
        case WI.Collection.TypeVerifier.Frame:
            title = WI.UIString("Frames");
            break;

        case WI.Collection.TypeVerifier.ContentFlow:
            title = WI.UIString("Flows");
            break;

        case WI.Collection.TypeVerifier.Script:
            title = WI.UIString("Extra Scripts");
            break;

        case WI.Collection.TypeVerifier.Resource:
            title = WI.UIString("Resource");
            break;

        case WI.ResourceCollection.TypeVerifier.Document:
            title = WI.Resource.displayNameForType(WI.Resource.Type.Document, true);
            break;

        case WI.ResourceCollection.TypeVerifier.Stylesheet:
            title = WI.Resource.displayNameForType(WI.Resource.Type.Stylesheet, true);
            break;

        case WI.ResourceCollection.TypeVerifier.Image:
            this._contentViewConstructor = WI.ImageResourceContentView;
            title = WI.Resource.displayNameForType(WI.Resource.Type.Image, true);
            break;

        case WI.ResourceCollection.TypeVerifier.Font:
            title = WI.Resource.displayNameForType(WI.Resource.Type.Font, true);
            break;

        case WI.ResourceCollection.TypeVerifier.Script:
            title = WI.Resource.displayNameForType(WI.Resource.Type.Script, true);
            break;

        case WI.ResourceCollection.TypeVerifier.XHR:
            title = WI.Resource.displayNameForType(WI.Resource.Type.XHR, true);
            break;

        case WI.ResourceCollection.TypeVerifier.Fetch:
            title = WI.Resource.displayNameForType(WI.Resource.Type.Fetch, true);
            break;

        case WI.ResourceCollection.TypeVerifier.WebSocket:
            title = WI.Resource.displayNameForType(WI.Resource.Type.WebSocket, true);
            break;

        case WI.ResourceCollection.TypeVerifier.Other:
            title = WI.Resource.displayNameForType(WI.Resource.Type.Other, true);
            break;
        }

        this._contentPlaceholder = new WI.TitleView(title);

        this.element.classList.add("collection");
    }

     // Public

    initialLayout()
    {
        let items = this.representedObject.items;
        if (this._contentViewConstructor && items.size) {
            for (let item of items)
                this._addContentViewForItem(item);
        } else
            this.addSubview(this._contentPlaceholder);
    }

     // Private

    _addContentViewForItem(item)
    {
        if (!this._contentViewConstructor)
            return;

        if (this._contentPlaceholder.parentView)
            this.removeSubview(this._contentPlaceholder);

        let contentView = new this._contentViewConstructor(item);

        contentView.addEventListener(WI.ResourceContentView.Event.ContentError, this._handleContentError, this);

        let handleClick = (event) => {
            if (event.button !== 0 || event.ctrlKey)
                return;

            WI.showRepresentedObject(item);
        };
        this._handleClickMap.set(item, handleClick);
        contentView.element.addEventListener("click", handleClick);

        contentView.element.title = WI.displayNameForURL(item.url, item.urlComponents);

        this.addSubview(contentView);
        this._contentViewMap.set(item, contentView);
    }

    _removeContentViewForItem(item)
    {
        if (!this._contentViewConstructor)
            return;

        let contentView = this._contentViewMap.get(item);
        console.assert(contentView);
        if (!contentView)
            return;

        this.removeSubview(contentView);
        this._contentViewMap.delete(item);

        contentView.removeEventListener(null, null, this);

        let handleClick = this._handleClickMap.get(item);
        console.assert(handleClick);
        if (handleClick) {
            contentView.element.removeEventListener("click", handleClick);
            this._handleClickMap.delete(item);
        }

        if (!this.representedObject.resources.length)
            this.addSubview(this._contentPlaceholder);
    }

    _handleItemAdded(event)
    {
        let item = event.data.item;
        if (!item)
            return;

        this._addContentViewForItem(item);
    }

    _handleItemRemoved(event)
    {
        let item = event.data.item;
        if (!item)
            return;

        this._removeContentViewForItem(item);
    }

    _handleContentError(event)
    {
        if (event && event.target)
            this._removeContentViewForItem(event.target.representedObject);
    }
};
