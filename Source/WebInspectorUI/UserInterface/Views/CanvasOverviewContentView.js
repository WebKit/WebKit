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

WI.CanvasOverviewContentView = class CanvasOverviewContentView extends WI.CollectionContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WI.CanvasCollection);

        let contentPlaceholder = WI.createMessageTextView(WI.UIString("No Canvas Contexts"));
        let descriptionElement = contentPlaceholder.appendChild(document.createElement("div"));
        descriptionElement.className = "description";
        descriptionElement.textContent = WI.UIString("Waiting for canvas contexts created by script or CSS.");

        let importNavigationItem = new WI.ButtonNavigationItem("import-recording", WI.UIString("Import"), "Images/Import.svg", 15, 15);
        importNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
        importNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => { WI.canvasManager.importRecording(); });

        let importHelpElement = WI.createNavigationItemHelp(WI.UIString("Press %s to load a recording from file."), importNavigationItem);
        contentPlaceholder.appendChild(importHelpElement);

        super(representedObject, WI.CanvasContentView, contentPlaceholder);

        this.element.classList.add("canvas-overview");

        this._refreshButtonNavigationItem = new WI.ButtonNavigationItem("refresh-all", WI.UIString("Refresh all"), "Images/ReloadFull.svg", 13, 13);
        this._refreshButtonNavigationItem.enabled = false;
        this._refreshButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._refreshPreviews, this);

        this._showGridButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-grid", WI.UIString("Show transparency grid"), WI.UIString("Hide Grid"), "Images/NavigationItemCheckers.svg", 13, 13);
        this._showGridButtonNavigationItem.activated = !!WI.settings.showImageGrid.value;
        this._showGridButtonNavigationItem.enabled = false;
        this._showGridButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showGridButtonClicked, this);

        this.selectionEnabled = true;

        this._keyboardShortcuts = [
            new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Up, this._handleUp.bind(this)),
            new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Right, this._handleRight.bind(this)),
            new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Down, this._handleDown.bind(this)),
            new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Left, this._handleLeft.bind(this)),
        ];

        let recordShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Space, this._handleSpace.bind(this));
        recordShortcut.implicitlyPreventsDefault = false;
        this._keyboardShortcuts.push(recordShortcut);

        let recordSingleFrameShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Shift, WI.KeyboardShortcut.Key.Space, this._handleSpace.bind(this));
        recordSingleFrameShortcut.implicitlyPreventsDefault = false;
        this._keyboardShortcuts.push(recordSingleFrameShortcut);

        for (let shortcut of this._keyboardShortcuts)
            shortcut.disabled = true;
    }

    // Public

    get navigationItems()
    {
        return [this._refreshButtonNavigationItem, this._showGridButtonNavigationItem];
    }

    get selectionPathComponents()
    {
        let components = [];

        if (this.supplementalRepresentedObjects.length) {
            let [canvas] = this.supplementalRepresentedObjects;
            let tabContentView = WI.tabBrowser.selectedTabContentView;
            if (tabContentView) {
                let treeElement = tabContentView.treeElementForRepresentedObject(canvas);
                console.assert(treeElement);
                if (treeElement) {
                    let pathComponent = new WI.GeneralTreeElementPathComponent(treeElement);
                    pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._selectionPathComponentsChanged, this);
                    components.push(pathComponent);
                }
            }
        }

        return components;
    }

    hidden()
    {
        WI.domTreeManager.hideDOMNodeHighlight();

        super.hidden();
    }

    // Protected

    contentViewAdded(contentView)
    {
        contentView.element.addEventListener("mouseenter", this._contentViewMouseEnter);
        contentView.element.addEventListener("mouseleave", this._contentViewMouseLeave);

        this._updateNavigationItems();
    }

    contentViewRemoved(contentView)
    {
        contentView.element.removeEventListener("mouseenter", this._contentViewMouseEnter);
        contentView.element.removeEventListener("mouseleave", this._contentViewMouseLeave);

        this._updateNavigationItems();
    }

    attached()
    {
        super.attached();

        WI.settings.showImageGrid.addEventListener(WI.Setting.Event.Changed, this._updateShowImageGrid, this);

        this.addEventListener(WI.ContentView.Event.SupplementalRepresentedObjectsDidChange, this._supplementalRepresentedObjectsDidChange, this);

        for (let shortcut of this._keyboardShortcuts)
            shortcut.disabled = false;
    }

    detached()
    {
        WI.settings.showImageGrid.removeEventListener(null, null, this);

        this.removeEventListener(null, null, this);

        for (let shortcut of this._keyboardShortcuts)
            shortcut.disabled = true;

        super.detached();
    }

    // Private

    get _itemMargin()
    {
        return parseInt(window.getComputedStyle(this.element).getPropertyValue("--item-margin"));
    }

    _refreshPreviews()
    {
        for (let canvasContentView of this.subviews)
            canvasContentView.refresh();
    }

    _changeSelectedItemVertically(shift)
    {
        let itemElementWidth = this.element.firstElementChild.offsetWidth + (2 * this._itemMargin);
        let itemsPerRow = Math.floor(this.element.offsetWidth / itemElementWidth);

        let items = Array.from(this.representedObject.items);
        let index = items.indexOf(this._selectedItem);
        if (index === -1)
            index = shift < 0 ? items.length + 1 : itemsPerRow;

        index += shift * itemsPerRow;
        if (index < 0)
            index = items.length + index;

        this.setSelectedItem(items[index % items.length]);
    }

    _changeSelectedItemHorizontally(shift)
    {
        let itemElementWidth = this.element.firstElementChild.offsetWidth + (2 * this._itemMargin);
        let itemsPerRow = Math.floor(this.element.offsetWidth / itemElementWidth);

        let items = Array.from(this.representedObject.items);
        let index = items.indexOf(this._selectedItem);
        if (index === -1)
            index = shift >= 0 ? itemsPerRow - 1 : 0;

        let selectedRow = Math.floor(index / itemsPerRow);
        index += shift;
        if (index < selectedRow * itemsPerRow)
            index += itemsPerRow;
        else if (index >= (selectedRow + 1) * itemsPerRow)
            index -= itemsPerRow;

        this.setSelectedItem(items[index]);
    }

    _updateNavigationItems()
    {
        let hasItems = !!this.representedObject.items.size;
        this._refreshButtonNavigationItem.enabled = hasItems;
        this._showGridButtonNavigationItem.enabled = hasItems;
    }

    _selectionPathComponentsChanged(event)
    {
        let pathComponent = event.data.pathComponent;
        if (pathComponent.representedObject instanceof WI.Canvas)
            this.setSelectedItem(pathComponent.representedObject);
        else if (pathComponent.representedObject instanceof WI.Recording)
            WI.showRepresentedObject(pathComponent.representedObject);
    }

    _showGridButtonClicked(event)
    {
        WI.settings.showImageGrid.value = !this._showGridButtonNavigationItem.activated;
    }

    _handleUp(event)
    {
        this._changeSelectedItemVertically(-1);
    }

    _handleRight(event)
    {
        let shift = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? -1 : 1;
        this._changeSelectedItemHorizontally(shift);
    }

    _handleDown(event)
    {
        this._changeSelectedItemVertically(1);
    }

    _handleLeft(event)
    {
        let shift = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? 1 : -1;
        this._changeSelectedItemHorizontally(shift);
    }

    _handleSpace(event)
    {
        if (WI.isEventTargetAnEditableField(event))
            return;

        if (!this._selectedItem)
            return;

        if (this._selectedItem.isRecording)
            WI.canvasManager.stopRecording();
        else if (!WI.canvasManager.recordingCanvas) {
            let singleFrame = !!event.shiftKey;
            WI.canvasManager.startRecording(this._selectedItem, singleFrame);
        }

        event.preventDefault();
    }

    _updateShowImageGrid()
    {
        this._showGridButtonNavigationItem.activated = !!WI.settings.showImageGrid.value;
    }

    _supplementalRepresentedObjectsDidChange()
    {
        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _contentViewMouseEnter(event)
    {
        let contentView = WI.View.fromElement(event.target);
        if (!(contentView instanceof WI.CanvasContentView))
            return;

        let canvas = contentView.representedObject;
        if (canvas.cssCanvasName) {
            canvas.requestCSSCanvasClientNodes((cssCanvasClientNodes) => {
                WI.domTreeManager.highlightDOMNodeList(cssCanvasClientNodes.map((node) => node.id));
            });
            return;
        }

        canvas.requestNode().then((node) => {
            if (!node || !node.ownerDocument)
                return;
            WI.domTreeManager.highlightDOMNode(node.id);
        });
    }

    _contentViewMouseLeave(event)
    {
        WI.domTreeManager.hideDOMNodeHighlight();
    }
};
