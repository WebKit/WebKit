/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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
    constructor(delegate)
    {
        super();

        console.assert(delegate);
        this._delegate = delegate;

        this._allowsEmptySelection = true;
        this._allowsMultipleSelection = false;
        this._lastSelectedIndex = NaN;
        this._shiftAnchorIndex = NaN;
        this._selectedIndexes = new WI.IndexSet;
        this._suppressSelectionDidChange = false;

        console.assert(this._delegate.selectionControllerNumberOfItems, "SelectionController delegate must implement selectionControllerNumberOfItems.");
        console.assert(this._delegate.selectionControllerNextSelectableIndex, "SelectionController delegate must implement selectionControllerNextSelectableIndex.");
        console.assert(this._delegate.selectionControllerPreviousSelectableIndex, "SelectionController delegate must implement selectionControllerPreviousSelectableIndex.");
    }

    // Public

    get delegate() { return this._delegate; }
    get lastSelectedItem() { return this._lastSelectedIndex; }
    get selectedItems() { return this._selectedIndexes; }

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

        if (this._selectedIndexes.size > 1) {
            console.assert(this._lastSelectedIndex >= 0);
            this._updateSelectedItems(new WI.IndexSet([this._lastSelectedIndex]));
        }
    }

    get numberOfItems()
    {
        return this._delegate.selectionControllerNumberOfItems(this);
    }

    hasSelectedItem(index)
    {
        return this._selectedIndexes.has(index);
    }

    selectItem(index, extendSelection = false)
    {
        console.assert(!extendSelection || this._allowsMultipleSelection, "Cannot extend selection with multiple selection disabled.");
        console.assert(index >= 0 && index < this.numberOfItems);

        if (!this._allowsMultipleSelection)
            extendSelection = false;

        if (this.hasSelectedItem(index)) {
            if (!extendSelection)
                this._deselectAllAndSelect(index);
            return;
        }

        let newSelectedItems = extendSelection ? this._selectedIndexes.copy() : new WI.IndexSet;
        newSelectedItems.add(index);

        this._shiftAnchorIndex = NaN;
        this._lastSelectedIndex = index;

        this._updateSelectedItems(newSelectedItems);
    }

    deselectItem(index)
    {
        console.assert(index >= 0 && index < this.numberOfItems);

        if (!this.hasSelectedItem(index))
            return;

        if (!this._allowsEmptySelection && this._selectedIndexes.size === 1)
            return;

        let newSelectedItems = this._selectedIndexes.copy();
        newSelectedItems.delete(index);

        if (this._shiftAnchorIndex === index)
            this._shiftAnchorIndex = NaN;

        if (this._lastSelectedIndex === index) {
            this._lastSelectedIndex = NaN;
            if (newSelectedItems.size) {
                // Find selected item closest to deselected item.
                let preceding = newSelectedItems.indexLessThan(index);
                let following = newSelectedItems.indexGreaterThan(index);

                if (isNaN(preceding))
                    this._lastSelectedIndex = following;
                else if (isNaN(following))
                    this._lastSelectedIndex = preceding;
                else {
                    if ((following - index) < (index - preceding))
                        this._lastSelectedIndex = following;
                    else
                        this._lastSelectedIndex = preceding;
                }
            }
        }

        this._updateSelectedItems(newSelectedItems);
    }

    selectAll()
    {
        if (!this.numberOfItems || !this._allowsMultipleSelection)
            return;

        if (this._selectedIndexes.size === this.numberOfItems)
            return;

        let newSelectedItems = new WI.IndexSet;
        newSelectedItems.addRange(0, this.numberOfItems);

        this._lastSelectedIndex = newSelectedItems.lastIndex;
        if (isNaN(this._shiftAnchorIndex))
            this._shiftAnchorIndex = this._lastSelectedIndex;

        this._updateSelectedItems(newSelectedItems);
    }

    deselectAll()
    {
        const index = NaN;
        this._deselectAllAndSelect(index);
    }

    removeSelectedItems()
    {
        let numberOfSelectedItems = this._selectedIndexes.size;
        if (!numberOfSelectedItems)
            return;

        // Try selecting the item following the selection.
        let lastSelectedIndex = this._selectedIndexes.lastIndex;
        let indexToSelect = this._nextSelectableIndex(lastSelectedIndex);
        if (isNaN(indexToSelect)) {
            // If no item exists after the last item in the selection, try selecting
            // a deselected item (hole) within the selection.
            let firstSelectedIndex = this._selectedIndexes.firstIndex;
            if (lastSelectedIndex - firstSelectedIndex > numberOfSelectedItems) {
                indexToSelect = this._nextSelectableIndex(firstSelectedIndex);
                while (this._selectedIndexes.has(indexToSelect))
                    indexToSelect = this._nextSelectableIndex(firstSelectedIndex);
            } else {
                // If the selection contains no holes, try selecting the item
                // preceding the selection.
                indexToSelect = firstSelectedIndex > 0 ? this._previousSelectableIndex(firstSelectedIndex) : NaN;
            }
        }

        this._deselectAllAndSelect(indexToSelect);
    }

    reset()
    {
        this._shiftAnchorIndex = NaN;
        this._lastSelectedIndex = NaN;
        this._selectedIndexes.clear();
    }

    didInsertItem(index)
    {
        let current = this._selectedIndexes.lastIndex;
        while (current >= index) {
            this._selectedIndexes.delete(current);
            this._selectedIndexes.add(current + 1);

            current = this._selectedIndexes.indexLessThan(current);
        }

        if (this._lastSelectedIndex >= index)
            this._lastSelectedIndex += 1;
        if (this._shiftAnchorIndex >= index)
            this._shiftAnchorIndex += 1;
    }

    didRemoveItems(indexes)
    {
        console.assert(indexes instanceof WI.IndexSet);

        if (!this._selectedIndexes.size)
            return;

        let firstRemovedIndex = indexes.firstIndex;
        if (this._selectedIndexes.lastIndex < firstRemovedIndex)
            return;

        let newSelectedIndexes = new WI.IndexSet;

        let lastRemovedIndex = indexes.lastIndex;
        if (this._selectedIndexes.firstIndex < lastRemovedIndex) {
            let removedCount = 0;
            let removedIndex = firstRemovedIndex;

            this._suppressSelectionDidChange = true;

            // Adjust the selected indexes that are in the range between the
            // first and last removed index (inclusive).
            for (let current = this._selectedIndexes.firstIndex; current < lastRemovedIndex; current = this._selectedIndexes.indexGreaterThan(current)) {
                if (this.hasSelectedItem(current)) {
                    this.deselectItem(current);
                    removedCount++;
                    continue;
                }

                while (removedIndex < current) {
                    removedCount++;
                    removedIndex = indexes.indexGreaterThan(removedIndex);
                }

                let newIndex = current - removedCount;
                newSelectedIndexes.add(newIndex);

                if (this._lastSelectedIndex === current)
                    this._lastSelectedIndex = newIndex;
                if (this._shiftAnchorIndex === current)
                    this._shiftAnchorIndex = newIndex;
            }

            this._suppressSelectionDidChange = false;
        }

        let removedCount = indexes.size;
        let current = lastRemovedIndex;
        while (current = this._selectedIndexes.indexGreaterThan(current))
            newSelectedIndexes.add(current - removedCount);

        if (this._lastSelectedIndex > lastRemovedIndex)
            this._lastSelectedIndex -= removedCount;
        if (this._shiftAnchorIndex > lastRemovedIndex)
            this._shiftAnchorIndex -= removedCount;

        this._selectedIndexes = newSelectedIndexes;
    }

    handleKeyDown(event)
    {
        if (!this.numberOfItems)
            return false;

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

    handleItemMouseDown(index, event)
    {
        if (event.button !== 0 || event.ctrlKey)
            return;

        // Command (macOS) or Control (Windows) key takes precedence over shift
        // whether or not multiple selection is enabled, so handle it first.
        if (event.commandOrControlKey) {
            if (this.hasSelectedItem(index))
                this.deselectItem(index);
            else
                this.selectItem(index, this._allowsMultipleSelection);
            return;
        }

        let shiftExtendSelection = this._allowsMultipleSelection && event.shiftKey;
        if (!shiftExtendSelection) {
            this.selectItem(index);
            return;
        }

        let newSelectedItems = this._selectedIndexes.copy();

        // Shift-clicking when nothing is selected should cause the first item
        // through the clicked item to be selected.
        if (!newSelectedItems.size) {
            this._shiftAnchorIndex = 0;
            this._lastSelectedIndex = index;
            newSelectedItems.addRange(0, index + 1);
            this._updateSelectedItems(newSelectedItems);
            return;
        }

        if (isNaN(this._shiftAnchorIndex))
            this._shiftAnchorIndex = this._lastSelectedIndex;

        // Shift-clicking will add to or delete from the current selection, or
        // pivot the selection around the anchor (a delete followed by an add).
        // We could check for all three cases, and add or delete only those items
        // that are necessary, but it is simpler to throw out the previous shift-
        // selected range and add the new range between the anchor and clicked item.

        function normalizeRange(startIndex, endIndex) {
            return startIndex > endIndex ? [endIndex, startIndex] : [startIndex, endIndex];
        }

        if (this._shiftAnchorIndex !== this._lastSelectedIndex) {
            let [startIndex, endIndex] = normalizeRange(this._shiftAnchorIndex, this._lastSelectedIndex);
            newSelectedItems.deleteRange(startIndex, endIndex - startIndex + 1);
        }

        let [startIndex, endIndex] = normalizeRange(this._shiftAnchorIndex, index);
        newSelectedItems.addRange(startIndex, endIndex - startIndex + 1);

        this._lastSelectedIndex = index;

        this._updateSelectedItems(newSelectedItems);
    }

    // Private

    _deselectAllAndSelect(index)
    {
        if (!this._selectedIndexes.size)
            return;

        if (this._selectedIndexes.size === 1 && this._selectedIndexes.firstIndex === index)
            return;

        this._shiftAnchorIndex = NaN;
        this._lastSelectedIndex = index;

        let newSelectedItems = new WI.IndexSet;
        if (!isNaN(index))
            newSelectedItems.add(index);

        this._updateSelectedItems(newSelectedItems);
    }

    _selectItemsFromArrowKey(goingUp, shiftKey)
    {
        if (!this._selectedIndexes.size) {
            let index = goingUp ? this.numberOfItems - 1 : 0;
            this.selectItem(index);
            return;
        }

        let index = goingUp ? this._previousSelectableIndex(this._lastSelectedIndex) : this._nextSelectableIndex(this._lastSelectedIndex);
        if (isNaN(index))
            return;

        let extendSelection = shiftKey && this._allowsMultipleSelection;
        if (!extendSelection || !this.hasSelectedItem(index)) {
            this.selectItem(index, extendSelection);
            return;
        }

        // Since the item in the direction of movement is selected, we are either
        // extending the selection into the item, or deselecting. Determine which
        // by checking whether the item opposite the anchor item is selected.
        let priorIndex = goingUp ? this._nextSelectableIndex(this._lastSelectedIndex) : this._previousSelectableIndex(this._lastSelectedIndex);
        if (!this.hasSelectedItem(priorIndex)) {
            this.deselectItem(this._lastSelectedIndex);
            return;
        }

        // The selection is being extended into the item; make it the new
        // anchor item then continue searching in the direction of movement
        // for an unselected item to select.
        while (!isNaN(index)) {
            if (!this.hasSelectedItem(index)) {
                this.selectItem(index, extendSelection);
                break;
            }

            this._lastSelectedIndex = index;
            index = goingUp ? this._previousSelectableIndex(index) : this._nextSelectableIndex(index);
        }
    }

    _nextSelectableIndex(index)
    {
        return this._delegate.selectionControllerNextSelectableIndex(this, index);
    }

    _previousSelectableIndex(index)
    {
        return this._delegate.selectionControllerPreviousSelectableIndex(this, index);
    }

    _updateSelectedItems(indexes)
    {
        if (this._selectedIndexes.equals(indexes))
            return;

        let oldSelectedIndexes = this._selectedIndexes.copy();
        this._selectedIndexes = indexes;

        if (this._suppressSelectionDidChange || !this._delegate.selectionControllerSelectionDidChange)
            return;

        let deselectedItems = oldSelectedIndexes.difference(indexes);
        let selectedItems = indexes.difference(oldSelectedIndexes);
        this._delegate.selectionControllerSelectionDidChange(this, deselectedItems, selectedItems);
    }
};
