/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.CanvasSidebarPanel = class CanvasSidebarPanel extends WI.NavigationSidebarPanel
{
    constructor()
    {
        super("canvas", WI.UIString("Canvas"));

        this._canvas = null;
        this._recording = null;

        this._navigationBar = new WI.NavigationBar;
        this._scopeBar = null;

        const toolTip = WI.UIString("Start recording canvas actions.\nShift-click to record a single frame.");
        const altToolTip = WI.UIString("Stop recording canvas actions");
        this._recordButtonNavigationItem = new WI.ToggleButtonNavigationItem("record-start-stop", toolTip, altToolTip, "Images/Record.svg", "Images/Stop.svg", 13, 13);
        this._recordButtonNavigationItem.enabled = false;
        this._recordButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleRecording, this);
        this._navigationBar.addNavigationItem(this._recordButtonNavigationItem);

        this.addSubview(this._navigationBar);

        const suppressFiltering = true;
        this._canvasTreeOutline = this.createContentTreeOutline(suppressFiltering);
        this._canvasTreeOutline.element.classList.add("canvas");

        this._recordingNavigationBar = new WI.NavigationBar;
        this._recordingNavigationBar.element.classList.add("hidden");
        this.contentView.addSubview(this._recordingNavigationBar);

        this._recordingContentContainer = this.contentView.element.appendChild(document.createElement("div"));
        this._recordingContentContainer.className = "recording-content";

        this._recordingTreeOutline = this.contentTreeOutline;
        this._recordingContentContainer.appendChild(this._recordingTreeOutline.element);

        this._recordingTreeOutline.customIndent = true;
        this._recordingTreeOutline.registerScrollVirtualizer(this._recordingContentContainer, 20);

        this._canvasTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeOutlineSelectionDidChange, this);
        this._recordingTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeOutlineSelectionDidChange, this);

        WI.canvasManager.addEventListener(WI.CanvasManager.Event.RecordingStarted, this._updateRecordNavigationItem, this);
        WI.canvasManager.addEventListener(WI.CanvasManager.Event.RecordingStopped, this._updateRecordNavigationItem, this);

        this._recordingProcessPromise = null;
        this._recordingProcessSpinner = null;
    }

    // Public

    get canvas()
    {
        return this._canvas;
    }

    set canvas(canvas)
    {
        if (this._canvas === canvas)
            return;

        if (this._canvas)
            this._canvas.recordingCollection.removeEventListener(null, null, this);

        this._canvas = canvas;
        if (this._canvas) {
            this._canvas.recordingCollection.addEventListener(WI.Collection.Event.ItemAdded, this._recordingAdded, this);
            this._canvas.recordingCollection.addEventListener(WI.Collection.Event.ItemRemoved, this._recordingRemoved, this);
        }

        this._canvasChanged();
        this._updateRecordNavigationItem();
        this._updateRecordingScopeBar();
    }

    set recording(recording)
    {
        if (recording === this._recording)
            return;

        if (recording)
            this.canvas = recording.source;

        this._recording = recording;
        this._recordingChanged();
    }

    set action(action)
    {
        if (!this._recording || this._recordingProcessPromise)
            return;

        let selectedTreeElement = this._recordingTreeOutline.selectedTreeElement;
        if (!action) {
            if (selectedTreeElement)
                selectedTreeElement.deselect();
            return;
        }

        if (selectedTreeElement && selectedTreeElement instanceof WI.FolderTreeElement) {
            let lastActionTreeElement = selectedTreeElement.children.lastValue;
            if (action === lastActionTreeElement.representedObject)
                return;
        }

        let treeElement = this._recordingTreeOutline.findTreeElement(action);
        console.assert(treeElement, "Missing tree element for recording action.", action);
        if (!treeElement)
            return;

        this._recording[WI.CanvasSidebarPanel.SelectedActionSymbol] = action;

        const omitFocus = false;
        const selectedByUser = false;
        treeElement.revealAndSelect(omitFocus, selectedByUser);
    }

    shown()
    {
        super.shown();

        this.contentBrowser.addEventListener(WI.ContentBrowser.Event.CurrentRepresentedObjectsDidChange, this._currentRepresentedObjectsDidChange, this);
        this._currentRepresentedObjectsDidChange();
    }

    hidden()
    {
        this.contentBrowser.removeEventListener(null, null, this);

        super.hidden();
    }

    canShowRepresentedObject(representedObject)
    {
        if (representedObject instanceof WI.CanvasCollection)
            return false;

        return super.canShowRepresentedObject(representedObject);
    }

    // Protected

    hasCustomFilters()
    {
        return true;
    }

    matchTreeElementAgainstCustomFilters(treeElement)
    {
        // Keep recording frame tree elements.
        if (treeElement instanceof WI.FolderTreeElement)
            return true;

        // Always show the Initial State tree element.
        if (treeElement instanceof WI.RecordingActionTreeElement && treeElement.representedObject instanceof WI.RecordingInitialStateAction)
            return true;

        return super.matchTreeElementAgainstCustomFilters(treeElement);
    }

    initialLayout()
    {
        super.initialLayout();

        let filterFunction = (treeElement) => {
            if (!(treeElement.representedObject instanceof WI.RecordingAction))
                return false;

            return treeElement.representedObject.isVisual || treeElement.representedObject instanceof WI.RecordingInitialStateAction;
        };

        const activatedByDefault = false;
        const defaultToolTip = WI.UIString("Only show visual actions");
        const activatedToolTip = WI.UIString("Show all actions");
        this.filterBar.addFilterBarButton("recording-show-visual-only", filterFunction, activatedByDefault, defaultToolTip, activatedToolTip, "Images/Paint.svg", 15, 15);
    }

    // Private

    _recordingAdded(event)
    {
        this.recording = event.data.item;

        this._updateRecordNavigationItem();
        this._updateRecordingScopeBar();
    }

    _recordingRemoved(event)
    {
        let recording = event.data.item;
        if (recording === this.recording)
            this.recording = this._canvas ? Array.from(this._canvas.recordingCollection).lastValue : null;

        this._updateRecordingScopeBar();
    }

    _scopeBarSelectionChanged()
    {
        let selectedScopeBarItem = this._scopeBar.selectedItems[0];
        this.recording = selectedScopeBarItem.__recording || null;
    }

    _toggleRecording(event)
    {
        if (!this._canvas)
            return;

        if (this._canvas.isRecording)
            WI.canvasManager.stopRecording();
        else if (!WI.canvasManager.recordingCanvas) {
            let singleFrame = event.data.nativeEvent.shiftKey;
            WI.canvasManager.startRecording(this._canvas, singleFrame);
        }
    }

    _currentRepresentedObjectsDidChange(event)
    {
        let objects = this.contentBrowser.currentRepresentedObjects;

        let canvas = objects.find((object) => object instanceof WI.Canvas);
        if (canvas) {
            this.canvas = canvas;
            return;
        }

        let shaderProgram = objects.find((object) => object instanceof WI.ShaderProgram);
        if (shaderProgram) {
            this.canvas = shaderProgram.canvas;
            let treeElement = this._canvasTreeOutline.findTreeElement(shaderProgram);
            const omitFocus = false;
            const selectedByUser = false;
            treeElement.revealAndSelect(omitFocus, selectedByUser);
            return;
        }

        let recording = objects.find((object) => object instanceof WI.Recording);
        if (recording) {
            this.canvas = recording.source;
            this.recording = recording;
            this.action = objects.find((object) => object instanceof WI.RecordingAction);
            return;
        }

        this.canvas = null;
        this.recording = null;
    }

    _treeOutlineSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement)
            return;

        if ((treeElement instanceof WI.CanvasTreeElement) || (treeElement instanceof WI.ShaderProgramTreeElement)) {
            this.showDefaultContentViewForTreeElement(treeElement);
            return;
        }

        if (treeElement instanceof WI.FolderTreeElement)
            treeElement = treeElement.children.lastValue;

        if (!(treeElement instanceof WI.RecordingActionTreeElement))
            return;

        console.assert(this._recording, "Missing recording for action tree element.", treeElement);
        this._recording[WI.CanvasSidebarPanel.SelectedActionSymbol] = treeElement.representedObject;

        const onlyExisting = true;
        let recordingContentView = this.contentBrowser.contentViewForRepresentedObject(this._recording, onlyExisting);
        if (recordingContentView)
            recordingContentView.updateActionIndex(treeElement.index);
    }

    _canvasChanged()
    {
        this._canvasTreeOutline.removeChildren();

        if (!this._canvas) {
            this._recordingNavigationBar.element.classList.add("hidden");
            return;
        }

        const showRecordings = false;
        let canvasTreeElement = new WI.CanvasTreeElement(this._canvas, showRecordings);
        canvasTreeElement.expanded = true;
        this._canvasTreeOutline.appendChild(canvasTreeElement);

        const omitFocus = false;
        const selectedByUser = false;
        canvasTreeElement.revealAndSelect(omitFocus, selectedByUser);

        if (WI.Canvas.ContextType.Canvas2D || this._canvas.contextType === WI.Canvas.ContextType.WebGL)
            this._recordButtonNavigationItem.enabled = true;

        let defaultSelectedRecording = null;
        if (this._canvas.recordingCollection.size)
            defaultSelectedRecording = Array.from(this._canvas.recordingCollection).lastValue;

        this.recording = defaultSelectedRecording;
    }

    _recordingChanged()
    {
        this._recordingTreeOutline.removeChildren();
        this.element.classList.toggle("has-recordings", !!this._recording);

        if (!this._recording)
            return;

        if (!this._recordingProcessSpinner) {
            this._recordingProcessSpinner = new WI.IndeterminateProgressSpinner;
            this._recordingContentContainer.appendChild(this._recordingProcessSpinner.element);
        }

        this.contentBrowser.showContentViewForRepresentedObject(this._recording);

        let recording = this._recording;

        let promise = this._recording.process().then(() => {
            if (recording !== this._recording || promise !== this._recordingProcessPromise)
                return;

            if (this._recordingProcessSpinner) {
                this._recordingProcessSpinner.element.remove();
                this._recordingProcessSpinner = null;
            }

            this._recordingTreeOutline.element.dataset.indent = Number.countDigits(this._recording.actions.length);

            if (this._recording.actions[0] instanceof WI.RecordingInitialStateAction)
                this._recordingTreeOutline.appendChild(new WI.RecordingActionTreeElement(this._recording.actions[0], 0, this._recording.type));

            let cumulativeActionIndex = 1;
            this._recording.frames.forEach((frame, frameIndex) => {
                let folder = new WI.FolderTreeElement(WI.UIString("Frame %d").format((frameIndex + 1).toLocaleString()));
                this._recordingTreeOutline.appendChild(folder);

                for (let i = 0; i < frame.actions.length; ++i)
                    folder.appendChild(new WI.RecordingActionTreeElement(frame.actions[i], cumulativeActionIndex + i, this._recording.type));

                if (!isNaN(frame.duration)) {
                    const higherResolution = true;
                    folder.status = Number.secondsToString(frame.duration / 1000, higherResolution);
                }

                if (frame.incomplete)
                    folder.subtitle = WI.UIString("Incomplete");

                if (this._recording.frames.length === 1)
                    folder.expand();

                cumulativeActionIndex += frame.actions.length;
            });

            if (this._scopeBar) {
                let scopeBarItem = this._scopeBar.item(this._recording.displayName);
                console.assert(scopeBarItem, "Missing scopeBarItem for recording.", this._recording);
                scopeBarItem.selected = true;
            }

            this.action = this._recording[WI.CanvasSidebarPanel.SelectedActionSymbol] || this._recording.actions[0];

            this._recordingProcessPromise = null;
        });

        this._recordingProcessPromise = promise;
    }

    _updateRecordNavigationItem()
    {
        if (!this._canvas || !(this._canvas.contextType === WI.Canvas.ContextType.Canvas2D || this._canvas.contextType === WI.Canvas.ContextType.WebGL)) {
            this._recordButtonNavigationItem.enabled = false;
            return;
        }

        let isRecording = this._canvas.isRecording;
        this._recordButtonNavigationItem.enabled = isRecording || !WI.canvasManager.recordingCanvas;
        this._recordButtonNavigationItem.toggled = isRecording;
    }

    _updateRecordingScopeBar()
    {
        if (this._scopeBar) {
            this._recordingNavigationBar.removeNavigationItem(this._scopeBar);
            this._scopeBar = null;
        }

        this._recordingNavigationBar.element.classList.toggle("hidden", !this._canvas || !this._recording);
        if (!this._recording || !this._canvas)
            return;

        let scopeBarItems = [];
        let selectedScopeBarItem = null;
        for (let recording of this._canvas.recordingCollection) {
            let scopeBarItem = new WI.ScopeBarItem(recording.displayName, recording.displayName);
            if (recording === this._recording)
                selectedScopeBarItem = scopeBarItem;
            scopeBarItem.__recording = recording;
            scopeBarItems.push(scopeBarItem);
        }

        if (!selectedScopeBarItem)
            selectedScopeBarItem = scopeBarItems[0];

        this._scopeBar = new WI.ScopeBar("canvas-recordinga-scope-bar", scopeBarItems, selectedScopeBarItem, true);
        this._scopeBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionChanged, this);
        this._recordingNavigationBar.insertNavigationItem(this._scopeBar, 0);
    }
};

WI.CanvasSidebarPanel.SelectedActionSymbol = Symbol("selected-action");
