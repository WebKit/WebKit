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

WebInspector.ScopeBar = class ScopeBar extends WebInspector.NavigationItem
{
    constructor(identifier, items, defaultItem, shouldGroupNonExclusiveItems)
    {
        super(identifier);

        this._element.classList.add("scope-bar");

        this._items = items;
        this._defaultItem = defaultItem;
        this._shouldGroupNonExclusiveItems = shouldGroupNonExclusiveItems || false;

        this._minimumWidth = 0;

        this._populate();
    }

    // Public

    get minimumWidth()
    {
        if (!this._minimumWidth) {
            // If a "display: flex;" element is too small for its contents and
            // has "flex-wrap: wrap;" set as well, this will cause the contents
            // to wrap (potentially overflowing), thus preventing a proper
            // measurement of the width of the element. Removing the "flex-wrap"
            // property will ensure that the contents are rendered on one line.
            this.element.style.flexWrap = "initial !important";

            if (this._multipleItem)
                this._multipleItem.displayWidestItem();

            this._minimumWidth = this.element.realOffsetWidth;
            this.element.style.flexWrap = null;

            if (this._multipleItem)
                this._multipleItem.displaySelectedItem();
        }
        return this._minimumWidth;
    }

    get defaultItem()
    {
        return this._defaultItem;
    }

    get items()
    {
        return this._items;
    }

    item(id)
    {
        return this._itemsById.get(id);
    }

    get selectedItems()
    {
        return this._items.filter(function(item) {
            return item.selected;
        });
    }

    hasNonDefaultItemSelected()
    {
        return this._items.some(function(item) {
            return item.selected && item !== this._defaultItem;
        }, this);
    }

    // Private

    _populate()
    {
        this._itemsById = new Map;

        if (this._shouldGroupNonExclusiveItems) {
            var nonExclusiveItems = [];

            for (var item of this._items) {
                this._itemsById.set(item.id, item);

                if (item.exclusive)
                    this._element.appendChild(item.element);
                else
                    nonExclusiveItems.push(item);

                item.addEventListener(WebInspector.ScopeBarItem.Event.SelectionChanged, this._itemSelectionDidChange, this);
            }

            this._multipleItem = new WebInspector.MultipleScopeBarItem(nonExclusiveItems);
            this._element.appendChild(this._multipleItem.element);
        } else {
            for (var item of this._items) {
                this._itemsById.set(item.id, item);
                this._element.appendChild(item.element);

                item.addEventListener(WebInspector.ScopeBarItem.Event.SelectionChanged, this._itemSelectionDidChange, this);
            }
        }

        if (!this.selectedItems.length && this._defaultItem)
            this._defaultItem.selected = true;

        this._element.classList.toggle("default-item-selected", this._defaultItem.selected);
    }

    _itemSelectionDidChange(event)
    {
        var sender = event.target;
        var item;

        // An exclusive item was selected, unselect everything else.
        if (sender.exclusive && sender.selected) {
            for (var i = 0; i < this._items.length; ++i) {
                item = this._items[i];
                if (item !== sender)
                    item.selected = false;
            }
        } else {
            var replacesCurrentSelection = this._shouldGroupNonExclusiveItems || !event.data.withModifier;
            for (var i = 0; i < this._items.length; ++i) {
                item = this._items[i];
                if (item.exclusive && item !== sender && sender.selected)
                    item.selected = false;
                else if (sender.selected && replacesCurrentSelection && sender !== item)
                    item.selected = false;
            }
        }

        // If nothing is selected anymore, select the default item.
        if (!this.selectedItems.length && this._defaultItem)
            this._defaultItem.selected = true;

        this._element.classList.toggle("default-item-selected", this._defaultItem.selected);
        this.dispatchEventToListeners(WebInspector.ScopeBar.Event.SelectionChanged);
    }
};

WebInspector.ScopeBar.Event = {
    SelectionChanged: "scopebar-selection-did-change"
};
