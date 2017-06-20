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

WebInspector.CanvasContentView = class CanvasContentView extends WebInspector.ContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WebInspector.Canvas);

        super(representedObject);

        this.element.classList.add("canvas");

        this._previewContainerElement = null;
        this._previewImageElement = null;
        this._errorElement = null;

        this._refreshButtonNavigationItem = new WebInspector.ButtonNavigationItem("canvas-refresh", WebInspector.UIString("Refresh"), "Images/ReloadFull.svg", 13, 13);
        this._refreshButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._showPreview, this);

        this._showGridButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("show-grid", WebInspector.UIString("Show Grid"), WebInspector.UIString("Hide Grid"), "Images/NavigationItemCheckers.svg", 13, 13);
        this._showGridButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._showGridButtonClicked, this);
        this._showGridButtonNavigationItem.activated = !!WebInspector.settings.showImageGrid.value;
    }

    // Protected

    get navigationItems()
    {
        return [this._refreshButtonNavigationItem, this._showGridButtonNavigationItem];
    }

    shown()
    {
        super.shown();

        this._showPreview();

        WebInspector.settings.showImageGrid.addEventListener(WebInspector.Setting.Event.Changed, this._updateImageGrid, this);
    }

    hidden()
    {
        WebInspector.settings.showImageGrid.removeEventListener(WebInspector.Setting.Event.Changed, this._updateImageGrid, this);

        super.hidden();
    }

    // Private

    _showPreview()
    {
        let showError = () => {
            if (this._previewContainerElement)
                this._previewContainerElement.remove();

            if (!this._errorElement) {
                const isError = true;
                this._errorElement = WebInspector.createMessageTextView(WebInspector.UIString("No Preview Available"), isError);
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

        let activated = WebInspector.settings.showImageGrid.value;
        this._showGridButtonNavigationItem.activated = activated;
        this._previewImageElement.classList.toggle("show-grid", activated);
    }

    _showGridButtonClicked(event)
    {
        WebInspector.settings.showImageGrid.value = !this._showGridButtonNavigationItem.activated;

        this._updateImageGrid();
    }
};
