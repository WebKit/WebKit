/*
 * Copyright (C) 2018, 2019 Apple Inc. All Rights Reserved.
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

WI.SelectionController = class SelectionController extends WI.Object
{
    constructor(delegate, comparator)
    {
        super();

        console.assert(delegate);
        console.assert(typeof comparator === "function");

        this._delegate = delegate;
        this._comparator = comparator;

        this._allowsEmptySelection = true;
        this._allowsMultipleSelection = false;
        this._lastSelectedItem = null;
        this._shiftAnchorItem = null;
        this._selectedItems = new Set;
        this._suppressSelectionDidChange = false;

        console.assert(this._delegate.selectionControllerFirstSelectableItem, "SelectionController delegate must implement selectionControllerFirstSelectableItem.");
        console.assert(this._delegate.selectionControllerLastSelectableItem, "SelectionController delegate must implement selectionControllerLastSelectableItem.");
        console.assert(this._delegate.selectionControllerNextSelectableItem, "SelectionController delegate must implement selectionControllerNextSelectableItem.");
        console.assert(this._delegate.selectionControllerPreviousSelectableItem, "SelectionController delegate must implement selectionControllerPreviousSelectableItem.");
    }

    // Public

    get delegate() { return this._delegate; }
    get lastSelectedItem() { return this._lastSelectedItem; }
    get selectedItems() { return this._selectedItems; }

    get allowsEmptySelection() { return this._allowsEmptySelection; }
    set allowsEmptySelection(flag) { this._allowsEmptySelection = flag; }

    get allowsMultipleSelection()
    {
        return this._allowsMultipleSelection;
    }

    set allowsMultipleSelection(flag)
    {
        if (this._allowsMultipleSelection === flag)
            return;

        this._allowsMultipleSelection = flag;
        if (this._allowsMultipleSelection)
            return;

        if (this._selectedItems.size > 1)
            this._updateSelectedItems(new Set([this._lastSelectedItem]));
    }

    hasSelectedItem(item)
    {
        return this._selectedItems.has(item);
    }

    selectItem(item, extendSelection = false)
    {
        console.assert(item, "Invalid item for selection.");
        console.assert(!extendSelection || this._allowsMultipleSelection, "Cannot extend selection with multiple selection disabled.");

        if (!this._allowsMultipleSelection)
            extendSelection = false;

        this._lastSelectedItem = item;
        this._shiftAnchorItem = null;

        let newItems = new Set(extendSelection ? this._selectedItems : null);
        newItems.add(item);

        this._updateSelectedItems(newItems);
    }

    selectItems(items)
    {
        console.assert(this._allowsMultipleSelection, "Cannot select multiple items with multiple selection disabled.");
        if (!this._allowsMultipleSelection)
            return;

        if (!this._lastSelectedItem || !items.has(this._lastSelectedItem))
            this._lastSelectedItem = items.lastValue;

        if (!this._shiftAnchorItem || !items.has(this._shiftAnchorItem))
            this._shiftAnchorItem = this._lastSelectedItem;

        this._updateSelectedItems(items);
    }

    deselectItem(item)
    {
        console.assert(item, "Invalid item for selection.");

        if (!this.hasSelectedItem(item))
            return;

        if (!this._allowsEmptySelection && this._selectedItems.size === 1)
            return;

        let newItems = new Set(this._selectedItems);
        newItems.delete(item);

        if (this._lastSelectedItem === item) {
            this._lastSelectedItem = null;

            if (newItems.size) {
                // Find selected item closest to deselected item.
                let previous = item;
                let next = item;
                while (!this._lastSelectedItem && previous && next) {
                    previous = this._previousSelectableItem(previous);
                    if (this.hasSelectedItem(previous)) {
                        this._lastSelectedItem = previous;
                        break;
                    }

                    next = this._nextSelectableItem(next);
                    if (this.hasSelectedItem(next)) {
                        this._lastSelectedItem = next;
                        break;
                    }
                }
            }
        }

        if (this._shiftAnchorItem === item)
            this._shiftAnchorItem = null;

        this._updateSelectedItems(newItems);
    }

    selectAll()
    {
        if (!this._allowsMultipleSelection)
            return;

        this.reset();

        let newItems = new Set;
        this._addRange(newItems, this._firstSelectableItem(), this._lastSelectableItem());
        this.selectItems(newItems);
    }

    deselectAll()
    {
        this._deselectAllAndSelect(null);
    }

    removeSelectedItems()
    {
        if (!this._selectedItems.size)
            return;

        let orderedSelection = Array.from(this._selectedItems).sort(this._comparator);

        // Try selecting the item preceding the selection.
        let firstSelectedItem = orderedSelection[0];
        let itemToSelect = this._previousSelectableItem(firstSelectedItem);
        if (!itemToSelect) {
            // If no item exists before the first item in the selection, try selecting
            // a deselected item (hole) within the selection.
            itemToSelect = firstSelectedItem;
            while (itemToSelect && this.hasSelectedItem(itemToSelect))
                itemToSelect = this._nextSelectableItem(itemToSelect);

            if (!itemToSelect || this.hasSelectedItem(itemToSelect)) {
                // If the selection contains no holes, try selecting the item
                // following the selection.
                itemToSelect = this._nextSelectableItem(orderedSelection.lastValue);
            }
        }

        this._deselectAllAndSelect(itemToSelect);
    }

    reset()
    {
        this._lastSelectedItem = null;
        this._shiftAnchorItem = null;
        this._selectedItems.clear();
    }

    didRemoveItems(items)
    {
        console.assert(items instanceof Set);

        if (!items.size || !this._selectedItems.size)
           return;

        this._updateSelectedItems(this._selectedItems.difference(items));
    }

    handleKeyDown(event)
    {
        if (event.key === "a" && event.commandOrControlKey) {
            this.selectAll();
            return true;
        }

        if (event.metaKey || event.ctrlKey)
            return false;

        if (event.keyIdentifier === "Up" || event.keyIdentifier === "Down") {
            this._selectItemsFromArrowKey(event.keyIdentifier === "Up", event.shiftKey);

            event.preventDefault();
            event.stopPropagation();
            return true;
        }

        return false;
    }

    handleItemMouseDown(item, event)
    {
        console.assert(item, "Invalid item for selection.");

        if (event.button !== 0 || event.ctrlKey)
            return;

        // Command (macOS) or Control (Windows) key takes precedence over shift
        // whether or not multiple selection is enabled, so handle it first.
        if (event.commandOrControlKey) {
            if (this.hasSelectedItem(item))
                this.deselectItem(item);
            else
                this.selectItem(item, this._allowsMultipleSelection);
            return;
        }

        let shiftExtendSelection = this._allowsMultipleSelection && event.shiftKey;
        if (!shiftExtendSelection) {
            this.selectItem(item);
            return;
        }

        let newItems = new Set(this._selectedItems);

        // Shift-clicking when nothing is selected should cause the first item
        // through the clicked item to be selected.
        if (!newItems.size) {
            this._lastSelectedItem = item;
            this._shiftAnchorItem = this._firstSelectableItem();

            this._addRange(newItems, this._shiftAnchorItem, this._lastSelectedItem);
            this._updateSelectedItems(newItems);
            return;
        }

        if (!this._shiftAnchorItem)
            this._shiftAnchorItem = this._lastSelectedItem;

        // Shift-clicking will add to or delete from the current selection, or
        // pivot the selection around the anchor (a delete followed by an add).
        // We could check for all three cases, and add or delete only those items
        // that are necessary, but it is simpler to throw out the previous shift-
        // selected range and add the new range between the anchor and clicked item.

        let sortItemPair = (a, b) => {
            return [a, b].sort(this._comparator);
        };

        if (this._shiftAnchorItem !== this._lastSelectedItem) {
            let [startItem, endItem] = sortItemPair(this._shiftAnchorItem, this._lastSelectedItem);
            this._deleteRange(newItems, startItem, endItem);
        }

        let [startItem, endItem] = sortItemPair(this._shiftAnchorItem, item);
        this._addRange(newItems, startItem, endItem);

        this._lastSelectedItem = item;

        this._updateSelectedItems(newItems);
    }

    // Private

    _deselectAllAndSelect(item)
    {
        if (!this._selectedItems.size && !item)
            return;

        if (this._selectedItems.size === 1 && this.hasSelectedItem(item))
            return;

        this._lastSelectedItem = item;
        this._shiftAnchorItem = null;

        let newItems = new Set;
        if (item)
            newItems.add(item);

        this._updateSelectedItems(newItems);
    }

    _selectItemsFromArrowKey(goingUp, shiftKey)
    {
        if (!this._selectedItems.size) {
            this.selectItem(goingUp ? this._lastSelectableItem() : this._firstSelectableItem());
            return;
        }

        let item = goingUp ? this._previousSelectableItem(this._lastSelectedItem) : this._nextSelectableItem(this._lastSelectedItem);
        if (!item)
            return;

        let extendSelection = shiftKey && this._allowsMultipleSelection;
        if (!extendSelection || !this.hasSelectedItem(item)) {
            this.selectItem(item, extendSelection);
            return;
        }

        // Since the item in the direction of movement is selected, we are either
        // extending the selection into the item, or deselecting. Determine which
        // by checking whether the item opposite the anchor item is selected.
        let priorItem = goingUp ? this._nextSelectableItem(this._lastSelectedItem) : this._previousSelectableItem(this._lastSelectedItem);
        if (!priorItem || !this.hasSelectedItem(priorItem)) {
            this.deselectItem(this._lastSelectedItem);
            return;
        }

        // The selection is being extended into the item; make it the new
        // anchor item then continue searching in the direction of movement
        // for an unselected item to select.
        while (item) {
            if (!this.hasSelectedItem(item)) {
                this.selectItem(item, extendSelection);
                break;
            }

            this._lastSelectedItem = item;
            item = goingUp ? this._previousSelectableItem(item) : this._nextSelectableItem(item);
        }
    }

    _firstSelectableItem()
    {
        return this._delegate.selectionControllerFirstSelectableItem(this);
    }

    _lastSelectableItem()
    {
        return this._delegate.selectionControllerLastSelectableItem(this);
    }

    _previousSelectableItem(item)
    {
        return this._delegate.selectionControllerPreviousSelectableItem(this, item);
    }

    _nextSelectableItem(item)
    {
        return this._delegate.selectionControllerNextSelectableItem(this, item);
    }

    _updateSelectedItems(items)
    {
        let oldSelectedItems = this._selectedItems;
        this._selectedItems = items;

        if (this._suppressSelectionDidChange || !this._delegate.selectionControllerSelectionDidChange)
            return;

        let deselectedItems = oldSelectedItems.difference(items);
        let selectedItems = items.difference(oldSelectedItems);
        if (deselectedItems.size || selectedItems.size)
            this._delegate.selectionControllerSelectionDidChange(this, deselectedItems, selectedItems);
    }

    _addRange(items, firstItem, lastItem)
    {
        let current = firstItem;
        while (current) {
            items.add(current);
            if (current === lastItem)
                break;
            current = this._nextSelectableItem(current);
        }

        console.assert(!lastItem || items.has(lastItem), "End of range could not be reached.");
    }

    _deleteRange(items, firstItem, lastItem)
    {
        let current = firstItem;
        while (current) {
            items.delete(current);
            if (current === lastItem)
                break;
            current = this._nextSelectableItem(current);
        }

        console.assert(!lastItem || !items.has(lastItem), "End of range could not be reached.");
    }
};
