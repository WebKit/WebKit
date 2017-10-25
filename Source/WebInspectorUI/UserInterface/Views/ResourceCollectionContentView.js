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
    }

    // Protected

    contentViewAdded(contentView)
    {
        console.assert(contentView instanceof WI.ResourceContentView);

        let resource = contentView.representedObject;
        console.assert(resource instanceof WI.Resource);

        contentView.addEventListener(WI.ResourceContentView.Event.ContentError, this._handleContentError, this);
        contentView.element.title = WI.displayNameForURL(resource.url, resource.urlComponents);
    }

    // Private

    _handleContentError(event)
    {
        if (event && event.target)
            this.removeContentViewForItem(event.target.representedObject);
    }
};
