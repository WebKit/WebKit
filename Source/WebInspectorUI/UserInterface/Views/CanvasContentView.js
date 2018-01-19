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

WI.CanvasContentView = class CanvasContentView extends WI.ContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WI.Canvas);

        super(representedObject);

        this.element.classList.add("canvas");

        this._recordingProgressElement = null;
        this._previewContainerElement = null;
        this._previewImageElement = null;
        this._errorElement = null;
        this._memoryCostElement = null;
        this._pendingContent = null;
        this._pixelSize = null;
        this._pixelSizeElement = null;
        this._canvasNode = null;
        this._recordingOptionElementMap = new WeakMap;

        if (this.representedObject.contextType === WI.Canvas.ContextType.Canvas2D || this.representedObject.contextType === WI.Canvas.ContextType.WebGL) {
            const toolTip = WI.UIString("Start recording canvas actions. Shift-click to record a single frame.");
            const altToolTip = WI.UIString("Stop recording canvas actions");
            this._recordButtonNavigationItem = new WI.ToggleButtonNavigationItem("record-start-stop", toolTip, altToolTip, "Images/Record.svg", "Images/Stop.svg", 13, 13);
            this._recordButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.High;
            this._recordButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleRecording, this);
        }

        this._refreshButtonNavigationItem = new WI.ButtonNavigationItem("refresh", WI.UIString("Refresh"), "Images/ReloadFull.svg", 13, 13);
        this._refreshButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._refreshButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this.refresh, this);

        this._showGridButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-grid", WI.UIString("Show Grid"), WI.UIString("Hide Grid"), "Images/NavigationItemCheckers.svg", 13, 13);
        this._showGridButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showGridButtonClicked, this);
        this._showGridButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._showGridButtonNavigationItem.activated = !!WI.settings.showImageGrid.value;
    }

    // Public

    get navigationItems()
    {
        let navigationItems = [this._refreshButtonNavigationItem, this._showGridButtonNavigationItem];
        if (this._recordButtonNavigationItem)
            navigationItems.unshift(this._recordButtonNavigationItem);
        return navigationItems;
    }

    refresh()
    {
        this._pendingContent = null;

        this.representedObject.requestContent().then((content) => {
            this._pendingContent = content;
            if (!this._pendingContent) {
                console.error("Canvas content not available.", this.representedObject);
                return;
            }

            this.needsLayout();
        })
        .catch(() => {
            this._showError();
        });
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        let header = this.element.appendChild(document.createElement("header"));
        let titles = header.appendChild(document.createElement("div"));
        titles.className = "titles";

        let title = titles.appendChild(document.createElement("span"));
        title.className = "title";
        title.textContent = this.representedObject.displayName;

        let subtitle = titles.appendChild(document.createElement("span"));
        subtitle.className = "subtitle";
        subtitle.textContent = WI.Canvas.displayNameForContextType(this.representedObject.contextType);

        let navigationBar = new WI.NavigationBar;
        if (this._recordButtonNavigationItem)
            navigationBar.addNavigationItem(this._recordButtonNavigationItem);

        navigationBar.addNavigationItem(this._refreshButtonNavigationItem);

        header.append(navigationBar.element);

        this._previewContainerElement = this.element.appendChild(document.createElement("div"));
        this._previewContainerElement.className = "preview";

        let footer = this.element.appendChild(document.createElement("footer"));

        this._recordingSelectContainer = footer.appendChild(document.createElement("div"));
        this._recordingSelectContainer.classList.add("recordings", "hidden");

        this._recordingSelectText = this._recordingSelectContainer.appendChild(document.createElement("span"));

        this._recordingSelectElement = this._recordingSelectContainer.appendChild(document.createElement("select"));
        this._recordingSelectElement.addEventListener("change", this._handleRecordingSelectElementChange.bind(this));

        for (let recording of this.representedObject.recordingCollection.items)
            this._addRecording(recording);

        let flexibleSpaceElement = footer.appendChild(document.createElement("div"));
        flexibleSpaceElement.className = "flexible-space";

        let metrics = footer.appendChild(document.createElement("div"));
        this._pixelSizeElement = metrics.appendChild(document.createElement("span"));
        this._pixelSizeElement.className = "pixel-size";
        this._memoryCostElement = metrics.appendChild(document.createElement("span"));
        this._memoryCostElement.className = "memory-cost";
    }

    layout()
    {
        super.layout();

        if (!this._pendingContent)
            return;

        if (this._errorElement) {
            this._errorElement.remove();
            this._errorElement = null;
        }

        if (!this._previewImageElement) {
            this._previewImageElement = document.createElement("img");
            this._previewImageElement.addEventListener("error", this._showError.bind(this));
        }

        this._previewImageElement.src = this._pendingContent;
        this._pendingContent = null;

        if (!this._previewImageElement.parentNode)
            this._previewContainerElement.appendChild(this._previewImageElement);

        this._updateImageGrid();

        this._refreshPixelSize();
        this._updateMemoryCost();
    }

    shown()
    {
        super.shown();

        this.refresh();

        this._updateRecordNavigationItem();
    }

    attached()
    {
        super.attached();

        this.representedObject.addEventListener(WI.Canvas.Event.MemoryChanged, this._updateMemoryCost, this);

        this.representedObject.requestNode().then((node) => {
            console.assert(!this._canvasNode || this._canvasNode === node);
            if (this._canvasNode === node)
                return;

            this._canvasNode = node;
            this._canvasNode.addEventListener(WI.DOMNode.Event.AttributeModified, this._refreshPixelSize, this);
            this._canvasNode.addEventListener(WI.DOMNode.Event.AttributeRemoved, this._refreshPixelSize, this);
        });

        WI.canvasManager.addEventListener(WI.CanvasManager.Event.RecordingStarted, this._recordingStarted, this);
        WI.canvasManager.addEventListener(WI.CanvasManager.Event.RecordingProgress, this._recordingProgress, this);
        WI.canvasManager.addEventListener(WI.CanvasManager.Event.RecordingStopped, this._recordingStopped, this);

        WI.settings.showImageGrid.addEventListener(WI.Setting.Event.Changed, this._updateImageGrid, this);

        if (this.didInitialLayout)
            this._recordingSelectElement.selectedIndex = -1;
    }

    detached()
    {
        this.representedObject.removeEventListener(null, null, this);

        if (this._canvasNode) {
            this._canvasNode.removeEventListener(null, null, this);
            this._canvasNode = null;
        }

        WI.canvasManager.removeEventListener(null, null, this);
        WI.settings.showImageGrid.removeEventListener(null, null, this);

        super.detached();
    }

    // Private

    _showError()
    {
        console.assert(!this._errorElement, "Error element already exists.");

        if (this._previewImageElement)
            this._previewImageElement.remove();

        const isError = true;
        this._errorElement = WI.createMessageTextView(WI.UIString("No Preview Available"), isError);
        this._previewContainerElement.appendChild(this._errorElement);
    }

    _addRecording(recording)
    {
        let optionElement = this._recordingSelectElement.appendChild(document.createElement("option"));
        optionElement.textContent = recording.displayName;

        this._recordingOptionElementMap.set(optionElement, recording);

        let recordingCount = this._recordingSelectElement.options.length;
        this._recordingSelectText.textContent = WI.UIString("View Recordings... (%d)").format(recordingCount);
        this._recordingSelectContainer.classList.remove("hidden");

        this._recordingSelectElement.selectedIndex = -1;
    }

    _toggleRecording(event)
    {
        if (this.representedObject.isRecording)
            WI.canvasManager.stopRecording();
        else if (!WI.canvasManager.recordingCanvas) {
            let singleFrame = event.data.nativeEvent.shiftKey;
            WI.canvasManager.startRecording(this.representedObject, singleFrame);
        }
    }

    _recordingStarted(event)
    {
        this._updateRecordNavigationItem();

        if (!this.representedObject.isRecording)
            return;

        if (!this._recordingProgressElement) {
            this._recordingProgressElement = this._previewContainerElement.insertAdjacentElement("beforebegin", document.createElement("div"));
            this._recordingProgressElement.className = "progress";
        }

        this._recordingProgressElement.textContent = WI.UIString("Waiting for frames…");
    }

    _recordingProgress(event)
    {
        let {canvas, frameCount, bufferUsed} = event.data;
        if (canvas !== this.representedObject)
            return;

        this._recordingProgressElement.removeChildren();

        let frameCountElement = this._recordingProgressElement.appendChild(document.createElement("span"));
        frameCountElement.className = "frame-count";

        let frameString = frameCount === 1 ? WI.UIString("%d Frame") : WI.UIString("%d Frames");
        frameCountElement.textContent = frameString.format(frameCount);

        this._recordingProgressElement.append(" ");

        let bufferUsedElement = this._recordingProgressElement.appendChild(document.createElement("span"));
        bufferUsedElement.className = "buffer-used";
        bufferUsedElement.textContent = "(" + Number.bytesToString(bufferUsed) + ")";
    }

    _recordingStopped(event)
    {
        this._updateRecordNavigationItem();

        let {canvas, recording} = event.data;
        if (canvas !== this.representedObject)
            return;

        if (recording)
            this._addRecording(recording);

        if (this._recordingProgressElement) {
            this._recordingProgressElement.remove();
            this._recordingProgressElement = null;
        }
    }

    _handleRecordingSelectElementChange(event)
    {
        let selectedOption = this._recordingSelectElement.options[this._recordingSelectElement.selectedIndex];
        console.assert(selectedOption, "An option should have been selected.");
        if (!selectedOption)
            return;

        let representedObject = this._recordingOptionElementMap.get(selectedOption);
        console.assert(representedObject, "Missing map entry for option.");
        if (!representedObject)
            return;

        WI.showRepresentedObject(representedObject);
    }

    _refreshPixelSize()
    {
        this._pixelSize = null;

        this.representedObject.requestSize().then((size) => {
            this._pixelSize = size;
            this._updatePixelSize();
        }).catch((error) => {
            this._updatePixelSize();
        });
    }

    _showGridButtonClicked()
    {
        WI.settings.showImageGrid.value = !this._showGridButtonNavigationItem.activated;
    }

    _updateImageGrid()
    {
        let activated = WI.settings.showImageGrid.value;
        this._showGridButtonNavigationItem.activated = activated;

        if (this._previewImageElement)
            this._previewImageElement.classList.toggle("show-grid", activated);
    }

    _updateMemoryCost()
    {
        if (!this._memoryCostElement)
            return;

        let memoryCost = this.representedObject.memoryCost;
        if (isNaN(memoryCost))
            this._memoryCostElement.textContent = emDash;
        else {
            const higherResolution = false;
            let bytesString = Number.bytesToString(memoryCost, higherResolution);
            this._memoryCostElement.textContent = `(${bytesString})`;
        }
    }

    _updatePixelSize()
    {
        if (!this._pixelSizeElement)
            return;

        if (this._pixelSize)
            this._pixelSizeElement.textContent = `${this._pixelSize.width} ${multiplicationSign} ${this._pixelSize.height}`;
        else
            this._pixelSizeElement.textContent = emDash;
    }

    _updateRecordNavigationItem()
    {
        if (!this._recordButtonNavigationItem)
            return;

        let isRecording = this.representedObject.isRecording;
        this._recordButtonNavigationItem.enabled = isRecording || !WI.canvasManager.recordingCanvas;
        this._recordButtonNavigationItem.toggled = isRecording;

        this.element.classList.toggle("is-recording", isRecording);
    }
};
