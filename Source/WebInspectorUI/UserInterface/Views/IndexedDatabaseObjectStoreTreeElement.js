/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

WI.IndexedDatabaseObjectStoreTreeElement = class IndexedDatabaseObjectStoreTreeElement extends WI.GeneralTreeElement
{
    constructor(objectStore)
    {
        console.assert(objectStore instanceof WI.IndexedDatabaseObjectStore);

        const subtitle = null;
        super("database-table-icon", objectStore.name, subtitle, objectStore, {hasChildren: !!objectStore.indexes.length});

        this._objectStore = objectStore;
    }

    // Public

    get objectStore()
    {
        return this._objectStore;
    }

    // Overrides from TreeElement (Protected)

    oncollapse()
    {
        this.shouldRefreshChildren = true;
    }

    onpopulate()
    {
        if (this.children.length && !this.shouldRefreshChildren)
            return;

        this.shouldRefreshChildren = false;

        this.removeChildren();

        for (var objectStoreIndex of this._objectStore.indexes)
            this.appendChild(new WI.IndexedDatabaseObjectStoreIndexTreeElement(objectStoreIndex));
    }
};
