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

        this._previewContainerElement = null;
        this._previewImageElement = null;
        this._errorElement = null;

        if (representedObject.contextType === WI.Canvas.ContextType.Canvas2D) {
            const toolTip = WI.UIString("Request recording of actions. Shift-click to record a single frame.");
            const altToolTip = WI.UIString("Cancel recording");
            this._recordButtonNavigationItem = new WI.ToggleButtonNavigationItem("canvas-record", toolTip, altToolTip, "Images/Record.svg", "Images/Stop.svg", 13, 13);
            this._recordButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleRecording, this);
        }

        this._refreshButtonNavigationItem = new WI.ButtonNavigationItem("canvas-refresh", WI.UIString("Refresh"), "Images/ReloadFull.svg", 13, 13);
        this._refreshButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showPreview, this);

        this._showGridButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-grid", WI.UIString("Show Grid"), WI.UIString("Hide Grid"), "Images/NavigationItemCheckers.svg", 13, 13);
        this._showGridButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showGridButtonClicked, this);
        this._showGridButtonNavigationItem.activated = !!WI.settings.showImageGrid.value;
    }

    // Protected

    get navigationItems()
    {
        let navigationItems = [this._refreshButtonNavigationItem, this._showGridButtonNavigationItem];
        if (this._recordButtonNavigationItem)
            navigationItems.unshift(this._recordButtonNavigationItem);
        return navigationItems;
    }

    initialLayout()
    {
        super.initialLayout();

        WI.canvasManager.addEventListener(WI.CanvasManager.Event.RecordingFinished, this._recordingFinished, this);
    }

    shown()
    {
        super.shown();

        this._showPreview();

        WI.settings.showImageGrid.addEventListener(WI.Setting.Event.Changed, this._updateImageGrid, this);
    }

    hidden()
    {
        WI.settings.showImageGrid.removeEventListener(WI.Setting.Event.Changed, this._updateImageGrid, this);

        super.hidden();
    }

    closed()
    {
        WI.canvasManager.removeEventListener(null, null, this);

        super.closed();
    }

    // Private

    _toggleRecording(event)
    {
        let toggled = this._recordButtonNavigationItem.toggled;
        let singleFrame = event.data.nativeEvent.shiftKey;
        this.representedObject.toggleRecording(!toggled, singleFrame, (error) => {
            console.assert(!error);

            this._recordButtonNavigationItem.toggled = !toggled;
        });
    }

    _recordingFinished(event)
    {
        if (event.data.canvas !== this.representedObject)
            return;

        if (this._recordButtonNavigationItem)
            this._recordButtonNavigationItem.toggled = false;

        WI.showRepresentedObject(event.data.recording);
    }

    _showPreview()
    {
        let showError = () => {
            if (this._previewContainerElement)
                this._previewContainerElement.remove();

            if (!this._errorElement) {
                const isError = true;
                this._errorElement = WI.createMessageTextView(WI.UIString("No Preview Available"), isError);
            }

            this.element.appendChild(this._errorElement);
        };

        if (!this._previewContainerElement) {
            this._previewContainerElement = this.element.appendChild(document.createElement("div"));
            this._previewContainerElement.classList.add("preview");
        }

        this.representedObject.requestContent((content) => {
            if (!content) {
                showError();
                return;
            }

            if (this._errorElement)
                this._errorElement.remove();

            if (!this._previewImageElement) {
                this._previewImageElement = document.createElement("img");
                this._previewImageElement.addEventListener("error", showError);
            }

            this._previewImageElement.src = content;
            this._previewContainerElement.appendChild(this._previewImageElement);

            this._updateImageGrid();
        });
    }

    _updateImageGrid()
    {
        if (!this._previewImageElement)
            return;

        let activated = WI.settings.showImageGrid.value;
        this._showGridButtonNavigationItem.activated = activated;
        this._previewImageElement.classList.toggle("show-grid", activated);
    }

    _showGridButtonClicked(event)
    {
        WI.settings.showImageGrid.value = !this._showGridButtonNavigationItem.activated;

        this._updateImageGrid();
    }
};
