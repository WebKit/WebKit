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

WebInspector.Collection = class Collection extends WebInspector.Object
{
    constructor(typeVerifier)
    {
        super();

        this._items = new Set;

        console.assert(!typeVerifier || typeof typeVerifier === "function");
        this._typeVerifier = typeVerifier || WebInspector.Collection.TypeVerifier.Any;
    }

     // Public

    get items() { return this._items; }
    get typeVerifier() { return this._typeVerifier; }

    add(item)
    {
        let isValidType = this._typeVerifier(item);
        console.assert(isValidType);
        if (!isValidType)
            return;

        console.assert(!this._items.has(item));
        this._items.add(item);

        this.itemAdded(item);

        this.dispatchEventToListeners(WebInspector.Collection.Event.ItemAdded, {item});
    }

    remove(item)
    {
        let wasRemoved = this._items.delete(item);
        console.assert(wasRemoved);

        this.itemRemoved(item);

        this.dispatchEventToListeners(WebInspector.Collection.Event.ItemRemoved, {item});
    }

    clear()
    {
        let items = new Set(this._items);

        this._items.clear();

        this.itemsCleared(items);

        for (let item of items)
            this.dispatchEventToListeners(WebInspector.Collection.Event.ItemRemoved, {item});
    }

    toArray()
    {
        return Array.from(this._items);
    }

    toJSON()
    {
        return this.toArray();
    }

     // Protected

    itemAdded(item)
    {
        // Implemented by subclasses.
    }

    itemRemoved(item)
    {
        // Implemented by subclasses.
    }

    itemsCleared(items)
    {
        // Implemented by subclasses.
    }
};

 WebInspector.Collection.Event = {
    ItemAdded: "collection-item-added",
    ItemRemoved: "collection-item-removed",
};

 WebInspector.Collection.TypeVerifier = {
    Any: (object) => true,
    ContentFlow: (object) => object instanceof WebInspector.ContentFlow,
    Frame: (object) => object instanceof WebInspector.Frame,
    Resource: (object) => object instanceof WebInspector.Resource,
    Script: (object) => object instanceof WebInspector.Script,
};
