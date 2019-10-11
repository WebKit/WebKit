/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

WI.ImageResourceContentView = class ImageResourceContentView extends WI.ResourceContentView
{
    constructor(resource)
    {
        console.assert(resource instanceof WI.Resource);

        super(resource, "image");

        this._imageElement = null;
        this._draggingInternalImageElement = false;

        const toolTip = WI.UIString("Show transparency grid");
        const activatedToolTip = WI.UIString("Hide transparency grid");
        this._showGridButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-grid", toolTip, activatedToolTip, "Images/NavigationItemCheckers.svg", 13, 13);
        this._showGridButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._showGridButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showGridButtonClicked, this);
        this._showGridButtonNavigationItem.activated = !!WI.settings.showImageGrid.value;

        if (this.showingLocalResourceOverride)
            this._dropZoneView = new WI.DropZoneView(this, {text: WI.UIString("Drop Image")});
    }

    // Public

    get navigationItems()
    {
        let items = super.navigationItems;

        items.push(this._showGridButtonNavigationItem);

        return items;
    }

    contentAvailable(content, base64Encoded)
    {
        this.removeLoadingIndicator();

        if (!content) {
            this.showGenericNoContentMessage();
            return;
        }

        let objectURL = this.resource.createObjectURL();
        if (!objectURL) {
            this.showGenericErrorMessage();
            return;
        }

        let imageContainer = this.element.appendChild(document.createElement("div"));
        imageContainer.className = "img-container";

        this._imageElement = imageContainer.appendChild(document.createElement("img"));
        this._imageElement.addEventListener("load", function() { URL.revokeObjectURL(objectURL); });
        this._imageElement.src = objectURL;
        this._imageElement.setAttribute("filename", this.resource.urlComponents.lastPathComponent || "");
        this._updateImageGrid();

        this._imageElement.addEventListener("dragstart", (event) => {
            console.assert(!this._draggingInternalImageElement);
            this._draggingInternalImageElement = true;
        });
        this._imageElement.addEventListener("dragend", (event) => {
            console.assert(this._draggingInternalImageElement);
            this._draggingInternalImageElement = false;
        });

        if (this._dropZoneView) {
            this._dropZoneView.targetElement = imageContainer;
            this.addSubview(this._dropZoneView);
        }
    }

    // Protected

    shown()
    {
        super.shown();

        this._updateImageGrid();

        WI.settings.showImageGrid.addEventListener(WI.Setting.Event.Changed, this._updateImageGrid, this);
    }

    hidden()
    {
        WI.settings.showImageGrid.removeEventListener(WI.Setting.Event.Changed, this._updateImageGrid, this);

        super.hidden();
    }

    closed()
    {
        WI.networkManager.removeEventListener(null, null, this);

        super.closed();
    }

    // DropZoneView delegate

    dropZoneShouldAppearForDragEvent(dropZone, event)
    {
        // Do not appear if the drag is the current image inside this view.
        if (this._draggingInternalImageElement)
            return false;

        // Appear if the drop contains a file.
        return event.dataTransfer.types.includes("Files");
    }

    dropZoneHandleDrop(dropZone, event)
    {
        let files = event.dataTransfer.files;
        let file = files.length === 1 ? files[0] : null;
        if (!file) {
            InspectorFrontendHost.beep();
            return;
        }

        let fileReader = new FileReader;
        fileReader.addEventListener("loadend", (event) => {
            let localResourceOverride = WI.networkManager.localResourceOverrideForURL(this.resource.url);
            if (!localResourceOverride)
                return;

            let dataURL = fileReader.result;
            this._imageElement.src = dataURL;

            let {base64, data, mimeType} = parseDataURL(dataURL);

            // In case no mime type was determined, try to derive one from the file extension.
            if (!mimeType || mimeType === "text/plain") {
                let extension = WI.fileExtensionForFilename(file.name);
                if (extension)
                    mimeType = WI.mimeTypeForFileExtension(extension);
            }

            let revision = localResourceOverride.localResource.currentRevision;
            revision.updateRevisionContent(data, {base64Encoded: base64, mimeType});
        });
        fileReader.readAsDataURL(file);
    }

    // Private

    _updateImageGrid()
    {
        if (!this._imageElement)
            return;

        let activated = WI.settings.showImageGrid.value;
        this._showGridButtonNavigationItem.activated = activated;
        this._imageElement.classList.toggle("show-grid", activated);
    }

    _showGridButtonClicked(event)
    {
        WI.settings.showImageGrid.value = !this._showGridButtonNavigationItem.activated;

        this._updateImageGrid();
    }
};
