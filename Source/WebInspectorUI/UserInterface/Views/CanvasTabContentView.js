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

WI.CanvasTabContentView = class CanvasTabContentView extends WI.ContentBrowserTabContentView
{
    constructor(representedObject)
    {
        console.assert(!representedObject || representedObject instanceof WI.Canvas);

        let tabBarItem = WI.GeneralTabBarItem.fromTabInfo(WI.CanvasTabContentView.tabInfo());

        const navigationSidebarPanelConstructor = WI.RecordingNavigationSidebarPanel;
        const detailsSidebarPanelConstructors = [WI.RecordingStateDetailsSidebarPanel, WI.RecordingTraceDetailsSidebarPanel, WI.CanvasDetailsSidebarPanel];
        const disableBackForward = true;
        super("canvas", ["canvas"], tabBarItem, navigationSidebarPanelConstructor, detailsSidebarPanelConstructors, disableBackForward);

        this._canvasCollection = new WI.CanvasCollection;

        this._canvasTreeOutline = new WI.TreeOutline;
        this._canvasTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._canvasTreeOutlineSelectionDidChange, this);

        const leftArrow = "Images/BackForwardArrows.svg#left-arrow-mask";
        const rightArrow = "Images/BackForwardArrows.svg#right-arrow-mask";
        let isRTL = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL;
        let backButtonImage = isRTL ? rightArrow : leftArrow;
        this._overviewNavigationItem = new WI.ButtonNavigationItem("canvas-overview", WI.UIString("Canvas Overview"), backButtonImage, 8, 13);
        this._overviewNavigationItem.hidden = true;
        this._overviewNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.High;
        this._overviewNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => { this.showRepresentedObject(this._canvasCollection); });

        this.contentBrowser.navigationBar.insertNavigationItem(this._overviewNavigationItem, 2);
        this.contentBrowser.navigationBar.insertNavigationItem(new WI.DividerNavigationItem, 3);

        this.navigationSidebarPanel.contentTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._navigationSidebarTreeOutlineSelectionChanged, this);
    }

    static tabInfo()
    {
        return {
            image: "Images/Canvas.svg",
            title: WI.UIString("Canvas"),
        };
    }

    static isTabAllowed()
    {
        return !!window.CanvasAgent;
    }

    // Public

    treeElementForRepresentedObject(representedObject)
    {
        return this._canvasTreeOutline.findTreeElement(representedObject);
    }

    get type()
    {
        return WI.CanvasTabContentView.Type;
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WI.CanvasCollection
            || representedObject instanceof WI.Recording
            || representedObject instanceof WI.ShaderProgram;
    }

    showRepresentedObject(representedObject, cookie)
    {
        super.showRepresentedObject(representedObject, cookie);

        this.navigationSidebarPanel.recording = null;

        if (representedObject instanceof WI.CanvasCollection || representedObject instanceof WI.ShaderProgram) {
            this._overviewNavigationItem.hidden = true;
            return;
        }

        if (representedObject instanceof WI.Recording) {
            this._overviewNavigationItem.hidden = false;
            this.navigationSidebarPanel.recording = representedObject;
            return;
        }

        console.assert(false, "Should not be reached.");
    }

    shown()
    {
        super.shown();

        if (!this.contentBrowser.currentContentView)
            this.showRepresentedObject(this._canvasCollection);
    }

    restoreStateFromCookie(cookie)
    {
        // FIXME: implement once <https://webkit.org/b/177606> is complete.
    }

    saveStateToCookie(cookie)
    {
        // FIXME: implement once <https://webkit.org/b/177606> is complete.
    }

    // Protected

    attached()
    {
        super.attached();

        WI.canvasManager.addEventListener(WI.CanvasManager.Event.CanvasAdded, this._handleCanvasAdded, this);
        WI.canvasManager.addEventListener(WI.CanvasManager.Event.CanvasRemoved, this._handleCanvasRemoved, this);
        WI.canvasManager.addEventListener(WI.CanvasManager.Event.RecordingImported, this._recordingImportedOrStopped, this);
        WI.canvasManager.addEventListener(WI.CanvasManager.Event.RecordingStopped, this._recordingImportedOrStopped, this);
        WI.RecordingContentView.addEventListener(WI.RecordingContentView.Event.RecordingActionIndexChanged, this._recordingActionIndexChanged, this);

        let canvases = new Set(Array.from(this._canvasCollection.items).concat(WI.canvasManager.canvases));

        for (let canvas of this._canvasCollection.items) {
            if (!canvases.has(canvas))
                this._removeCanvas(canvas);
        }

        for (let canvas of canvases) {
            if (!this._canvasCollection.items.has(canvas))
                this._addCanvas(canvas);
        }
    }

    detached()
    {
        WI.canvasManager.removeEventListener(null, null, this);
        WI.RecordingContentView.removeEventListener(null, null, this);

        super.detached();
    }

    // Private

    _addCanvas(canvas)
    {
        this._canvasTreeOutline.appendChild(new WI.CanvasTreeElement(canvas));
        this._canvasCollection.add(canvas);

        for (let recording of canvas.recordingCollection.items)
            this._recordingAdded(recording, {suppressShowRecording: true});
    }

    _removeCanvas(canvas)
    {
        // Move all existing recordings for the removed canvas to be imported recordings, as the
        // recording's source is no longer valid.
        for (let recording of canvas.recordingCollection.items) {
            recording.source = null;
            recording.createDisplayName();

            const subtitle = null;
            this._canvasTreeOutline.appendChild(new WI.GeneralTreeElement(["recording"], recording.displayName, subtitle, recording));
        }

        let treeElement = this._canvasTreeOutline.findTreeElement(canvas);
        console.assert(treeElement, "Missing tree element for canvas.", canvas);
        this._canvasTreeOutline.removeChild(treeElement);
        this._canvasCollection.remove(canvas);

        let currentContentView = this.contentBrowser.currentContentView;
        if (currentContentView instanceof WI.RecordingContentView && canvas.recordingCollection.items.has(currentContentView.representedObject))
            this.contentBrowser.updateHierarchicalPathForCurrentContentView();
    }

    _handleCanvasAdded(event)
    {
        this._addCanvas(event.data.canvas);
    }

    _handleCanvasRemoved(event)
    {
        this._removeCanvas(event.data.canvas);
    }

    _canvasTreeOutlineSelectionDidChange(event)
    {
        let selectedElement = event.data.selectedElement;
        if (!selectedElement)
            return;

        let representedObject = selectedElement.representedObject;

        if (this.canShowRepresentedObject(representedObject)) {
            this.showRepresentedObject(representedObject);

            if (representedObject instanceof WI.Recording)
                this._updateActionIndex(0);
            return;
        }

        if (representedObject instanceof WI.Canvas) {
            this.showRepresentedObject(this._canvasCollection);
            this.contentBrowser.currentContentView.setSelectedItem(representedObject);
            return;
        }

        console.assert(false, "Unexpected representedObject.", representedObject);
    }

    _recordingImportedOrStopped(event)
    {
        let recording = event.data.recording;
        if (!recording)
            return;

        this._recordingAdded(recording, {
            suppressShowRecording: event.data.fromConsole,
        });
    }

    _navigationSidebarTreeOutlineSelectionChanged(event)
    {
        if (!event.data.selectedElement)
            return;

        let recordingContentView = this.contentBrowser.currentContentView;
        if (!(recordingContentView instanceof WI.RecordingContentView))
            return;

        let selectedTreeElement = event.data.selectedElement;
        if (selectedTreeElement instanceof WI.FolderTreeElement)
            selectedTreeElement = selectedTreeElement.children.lastValue;

        this._updateActionIndex(selectedTreeElement.index, {suppressNavigationSidebarUpdate: true});
    }

    _recordingAdded(recording, options = {})
    {
        if (!recording.source) {
            const subtitle = null;
            let recordingTreeElement = new WI.GeneralTreeElement(["recording"], recording.displayName, subtitle, recording);
            this._canvasTreeOutline.appendChild(recordingTreeElement);
        }

        if (!options.suppressShowRecording) {
            this.showRepresentedObject(recording);
            this._updateActionIndex(0, {suppressNavigationSidebarUpdate: true});
        }
    }

    _recordingActionIndexChanged(event)
    {
        if (event.target !== this.contentBrowser.currentContentView)
            return;

        this._updateActionIndex(event.data.index);
    }

    _updateActionIndex(index, options = {})
    {
        options.actionCompletedCallback = (action, context) => {
            for (let detailsSidebarPanel of this.detailsSidebarPanels) {
                if (detailsSidebarPanel.updateAction)
                    detailsSidebarPanel.updateAction(action, context, options);
            }
        };

        if (!options.suppressNavigationSidebarUpdate)
            this.navigationSidebarPanel.updateActionIndex(index, options);

        this.contentBrowser.currentContentView.updateActionIndex(index, options);
    }
};

WI.CanvasTabContentView.Type = "canvas";
