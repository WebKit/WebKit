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

        const navigationSidebarPanelConstructor = WI.CanvasSidebarPanel;
        const detailsSidebarPanelConstructors = [WI.RecordingStateDetailsSidebarPanel, WI.RecordingTraceDetailsSidebarPanel, WI.CanvasDetailsSidebarPanel];
        const disableBackForward = true;
        super("canvas", ["canvas"], tabBarItem, navigationSidebarPanelConstructor, detailsSidebarPanelConstructors, disableBackForward);

        this._canvasCollection = new WI.CanvasCollection;

        this._canvasTreeOutline = new WI.TreeOutline;
        this._canvasTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._canvasTreeOutlineSelectionDidChange, this);

        this._overviewTreeElement = new WI.GeneralTreeElement("canvas-overview", WI.UIString("Overview"), null, this._canvasCollection);
        this._canvasTreeOutline.appendChild(this._overviewTreeElement);

        this._importedRecordingsTreeElement = new WI.FolderTreeElement(WI.UIString("Imported Recordings"), WI.RecordingCollection);
        this._importedRecordingsTreeElement.hidden = true;
        this._canvasTreeOutline.appendChild(this._importedRecordingsTreeElement);

        this._recordShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Space, this._handleSpace.bind(this));
        this._recordShortcut.implicitlyPreventsDefault = false;

        this._recordSingleFrameShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Shift, WI.KeyboardShortcut.Key.Space, this._handleSpace.bind(this));
        this._recordSingleFrameShortcut.implicitlyPreventsDefault = false;
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

    get managesNavigationSidebarPanel()
    {
        return true;
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WI.Canvas
            || representedObject instanceof WI.CanvasCollection
            || representedObject instanceof WI.Recording
            || representedObject instanceof WI.ShaderProgram;
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
        WI.Canvas.addEventListener(WI.Canvas.Event.RecordingStopped, this._recordingImportedOrStopped, this);

        let canvases = new Set([...this._canvasCollection, ...WI.canvasManager.canvases]);

        for (let canvas of this._canvasCollection) {
            if (!canvases.has(canvas))
                this._removeCanvas(canvas);
        }

        for (let canvas of canvases) {
            if (!this._canvasCollection.has(canvas))
                this._addCanvas(canvas);
        }
    }

    detached()
    {
        WI.Canvas.removeEventListener(null, null, this);
        WI.canvasManager.removeEventListener(null, null, this);

        super.detached();
    }

    // Private

    _addCanvas(canvas)
    {
        this._overviewTreeElement.appendChild(new WI.CanvasTreeElement(canvas));
        this._canvasCollection.add(canvas);

        for (let recording of canvas.recordingCollection)
            this._recordingAdded(recording, {suppressShowRecording: true});
    }

    _removeCanvas(canvas)
    {
        let treeElement = this._canvasTreeOutline.findTreeElement(canvas);
        console.assert(treeElement, "Missing tree element for canvas.", canvas);
        this._overviewTreeElement.removeChild(treeElement);
        this._canvasCollection.remove(canvas);

        const options = {
            suppressShowRecording: true,
        };

        for (let recording of canvas.recordingCollection)
            this._recordingAdded(recording, options);

        let currentContentView = this.contentBrowser.currentContentView;
        if (currentContentView instanceof WI.RecordingContentView && canvas.recordingCollection.has(currentContentView.representedObject))
            this.contentBrowser.updateHierarchicalPathForCurrentContentView();

        let navigationSidebarPanel = this.navigationSidebarPanel;
        if (navigationSidebarPanel instanceof WI.CanvasSidebarPanel && navigationSidebarPanel.visible)
            navigationSidebarPanel.updateRepresentedObjects();
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
        if (!this.canShowRepresentedObject(representedObject)) {
            console.assert(false, "Unexpected representedObject.", representedObject);
            return;
        }

        this.showRepresentedObject(representedObject);
    }

    _recordingImportedOrStopped(event)
    {
        let recording = event.data.recording;
        if (!recording)
            return;

        this._recordingAdded(recording, {
            suppressShowRecording: event.data.fromConsole || this.contentBrowser.currentRepresentedObjects.some((representedObject) => representedObject instanceof WI.Recording),
        });
    }

    _recordingAdded(recording, options = {})
    {
        if (!recording.source) {
            const subtitle = null;
            let recordingTreeElement = new WI.GeneralTreeElement(["recording"], recording.displayName, subtitle, recording);
            this._importedRecordingsTreeElement.hidden = false;
            this._importedRecordingsTreeElement.appendChild(recordingTreeElement);
        }

        if (!options.suppressShowRecording)
            this.showRepresentedObject(recording);
    }

    _handleSpace(event)
    {
        if (WI.isEventTargetAnEditableField(event))
            return;

        if (!this.navigationSidebarPanel)
            return;

        let canvas = this.navigationSidebarPanel.canvas;
        if (!canvas)
            return;

        if (canvas.recordingActive)
            canvas.stopRecording();
        else {
            let singleFrame = !!event.shiftKey;
            canvas.startRecording(singleFrame);
        }

        event.preventDefault();
    }
};

WI.CanvasTabContentView.Type = "canvas";
