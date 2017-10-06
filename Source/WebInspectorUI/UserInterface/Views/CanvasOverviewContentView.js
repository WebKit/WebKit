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

        super(representedObject, WI.CanvasContentView, WI.UIString("No canvas contexts found"));

        this.element.classList.add("canvas-overview");

        this._canvasTreeOutline = new WI.TreeOutline;

        this._refreshButtonNavigationItem = new WI.ButtonNavigationItem("refresh-all", WI.UIString("Refresh all"), "Images/ReloadFull.svg", 13, 13);
        this._refreshButtonNavigationItem.disabled = true;
        this._refreshButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._refreshPreviews, this);

        this._showGridButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-grid", WI.UIString("Show transparency grid"), WI.UIString("Hide Grid"), "Images/NavigationItemCheckers.svg", 13, 13);
        this._showGridButtonNavigationItem.activated = !!WI.settings.showImageGrid.value;
        this._showGridButtonNavigationItem.disabled = true;
        this._showGridButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showGridButtonClicked, this);

        this._selectedCanvasPathComponent = null;

        this.selectionEnabled = true;
    }

    // Public

    get navigationItems()
    {
        return [this._refreshButtonNavigationItem, this._showGridButtonNavigationItem];
    }

    get selectionPathComponents()
    {
        let components = [];
        let canvas = this.supplementalRepresentedObjects[0];
        if (canvas) {
            let treeElement = this._canvasTreeOutline.findTreeElement(canvas);
            console.assert(treeElement);
            if (treeElement) {
                let pathComponent = new WI.GeneralTreeElementPathComponent(treeElement, canvas);
                pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._selectedPathComponentChanged, this);
                components.push(pathComponent);
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
        let canvas = contentView.representedObject;
        let treeElement = this._canvasTreeOutline.findTreeElement(canvas);
        console.assert(!treeElement, "Already added tree element for canvas.", canvas);
        if (treeElement)
            return;

        treeElement = new WI.CanvasTreeElement(canvas);
        this._canvasTreeOutline.appendChild(treeElement);

        contentView.element.addEventListener("mouseenter", this._contentViewMouseEnter);
        contentView.element.addEventListener("mouseleave", this._contentViewMouseLeave);
    }

    contentViewRemoved(contentView)
    {
        let canvas = contentView.representedObject;
        let treeElement = this._canvasTreeOutline.findTreeElement(canvas);
        console.assert(treeElement, "Missing tree element for canvas.", canvas);
        if (!treeElement)
            return;

        this._canvasTreeOutline.removeChild(treeElement);

        contentView.element.removeEventListener("mouseenter", this._contentViewMouseEnter);
        contentView.element.removeEventListener("mouseleave", this._contentViewMouseLeave);
    }

    attached()
    {
        super.attached();

        WI.settings.showImageGrid.addEventListener(WI.Setting.Event.Changed, this._updateShowImageGrid, this);

        this.addEventListener(WI.ContentView.Event.SupplementalRepresentedObjectsDidChange, this._supplementalRepresentedObjectsDidChange, this);
    }

    detached()
    {
        WI.settings.showImageGrid.removeEventListener(null, null, this);

        this.removeEventListener(null, null, this);

        super.detached();
    }

    // Private

    _refreshPreviews()
    {
        for (let canvasContentView of this.subviews)
            canvasContentView.refresh();
    }

    _selectedPathComponentChanged(event)
    {
        let pathComponent = event.data.pathComponent;
        if (pathComponent.representedObject instanceof WI.Canvas)
            this.setSelectedItem(pathComponent.representedObject);
    }

    _showGridButtonClicked(event)
    {
        WI.settings.showImageGrid.value = !this._showGridButtonNavigationItem.activated;
    }

    _supplementalRepresentedObjectsDidChange()
    {
        function createCanvasPathComponent(canvas) {
            return new WI.HierarchicalPathComponent(canvas.displayName, "canvas", canvas);
        }

        let currentCanvas = this.supplementalRepresentedObjects[0];
        if (currentCanvas)
            this._selectedCanvasPathComponent = createCanvasPathComponent(currentCanvas);
        else
            this._selectedCanvasPathComponent = null;

        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _updateNavigationItems()
    {
        let disabled = !this.representedObject.items.size;
        this._refreshButtonNavigationItem.disabled = disabled;
        this._showGridButtonNavigationItem.disabled = disabled;
    }

    _updateShowImageGrid()
    {
        this._showGridButtonNavigationItem.activated = !!WI.settings.showImageGrid.value;
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
