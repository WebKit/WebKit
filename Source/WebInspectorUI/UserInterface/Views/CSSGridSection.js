/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

// FIXME: <https://webkit.org/b/152269> convert `WI.CSSGridSection` to be a subclass of `WI.DetailsSectionRow`

WI.CSSGridSection = class CSSGridSection extends WI.View
{
    constructor()
    {
        super();

        this.element.classList.add("css-grid-section");

        this._gridNodeSet = new Set;
        this._checkboxElementByNodeMap = new WeakMap;
        this._toggleAllCheckboxElement = null;
        this._suppressUpdateToggleAllCheckbox = false;
    }

    // Public

    set gridNodeSet(value)
    {
        this._gridNodeSet = value;
        this.needsLayout();
    }

    // Protected

    attached()
    {
        super.attached();

        WI.overlayManager.addEventListener(WI.OverlayManager.Event.GridOverlayShown, this._handleGridOverlayStateChanged, this);
        WI.overlayManager.addEventListener(WI.OverlayManager.Event.GridOverlayHidden, this._handleGridOverlayStateChanged, this);
    }

    detached()
    {
        WI.overlayManager.removeEventListener(WI.OverlayManager.Event.GridOverlayShown, this._handleGridOverlayStateChanged, this);
        WI.overlayManager.removeEventListener(WI.OverlayManager.Event.GridOverlayHidden, this._handleGridOverlayStateChanged, this);

        super.detached();
    }

    initialLayout()
    {
        super.initialLayout();

        let settingsGroup = new WI.SettingsGroup(WI.UIString("Page Overlay Options", "Page Overlay Options @ Layout Panel Section Header", "Heading for list of grid overlay options"));
        this.element.append(settingsGroup.element);

        settingsGroup.addSetting(WI.settings.gridOverlayShowTrackSizes, WI.UIString("Track Sizes", "Track sizes @ Layout Panel Overlay Options", "Label for option to toggle the track sizes setting for CSS grid overlays"));
        settingsGroup.addSetting(WI.settings.gridOverlayShowLineNumbers, WI.UIString("Line Numbers", "Line numbers @ Layout Panel Overlay Options", "Label for option to toggle the line numbers setting for CSS grid overlays"));
        settingsGroup.addSetting(WI.settings.gridOverlayShowLineNames, WI.UIString("Line Names", "Line names @ Layout Panel Overlay Options", "Label for option to toggle the line names setting for CSS grid overlays"));
        settingsGroup.addSetting(WI.settings.gridOverlayShowAreaNames, WI.UIString("Area Names", "Area names @ Layout Panel Overlay Options", "Label for option to toggle the area names setting for CSS grid overlays"));
        settingsGroup.addSetting(WI.settings.gridOverlayShowExtendedGridLines, WI.UIString("Extended Grid Lines", "Show extended lines @ Layout Panel Overlay Options", "Label for option to toggle the extended lines setting for CSS grid overlays"));

        let listHeading = this.element.appendChild(document.createElement("h2"));
        listHeading.classList.add("heading");

        let label = listHeading.createChild("label");
        this._toggleAllCheckboxElement = label.createChild("input");
        this._toggleAllCheckboxElement.type = "checkbox";
        this._toggleAllCheckboxElement.addEventListener("change", this._handleToggleAllCheckboxChanged.bind(this));

        let labelInner = label.createChild("span", "toggle-all");
        labelInner.textContent = WI.UIString("Grid Overlays", "Page Overlays @ Layout Sidebar Section Header", "Heading for list of grid nodes");

        this._listElement = this.element.appendChild(document.createElement("ul"));
        this._listElement.classList.add("node-overlay-list");
    }

    _handleToggleAllCheckboxChanged(event)
    {
        let isChecked = event.target.checked;
        this._suppressUpdateToggleAllCheckbox = true;
        for (let domNode of this._gridNodeSet) {
            if (isChecked)
                WI.overlayManager.showGridOverlay(domNode, {initiator: WI.GridOverlayDiagnosticEventRecorder.Initiator.Panel});
            else
                WI.overlayManager.hideGridOverlay(domNode);
        }
        this._suppressUpdateToggleAllCheckbox = false;
    }

    layout()
    {
        super.layout();

        this._listElement.removeChildren();

        for (let domNode of this._gridNodeSet) {
            let itemElement = this._listElement.appendChild(document.createElement("li"));
            let itemContainerElement = itemElement.appendChild(document.createElement("span"));
            itemContainerElement.classList.add("node-overlay-list-item-container");

            let labelElement = itemContainerElement.appendChild(document.createElement("label"));
            let checkboxElement = labelElement.appendChild(document.createElement("input"));
            checkboxElement.type = "checkbox";
            checkboxElement.checked = WI.overlayManager.isGridOverlayVisible(domNode);

            const nodeDisplayName = labelElement.appendChild(document.createElement("span"));
            nodeDisplayName.classList.add("node-display-name");
            nodeDisplayName.textContent = domNode.displayName;

            this._checkboxElementByNodeMap.set(domNode, checkboxElement);

            checkboxElement.addEventListener("change", (event) => {
                if (checkboxElement.checked)
                    WI.overlayManager.showGridOverlay(domNode, {color: swatch.value, initiator: WI.GridOverlayDiagnosticEventRecorder.Initiator.Panel});
                else
                    WI.overlayManager.hideGridOverlay(domNode);
            });

            let gridColor = WI.overlayManager.getGridColorForNode(domNode);
            let swatch = new WI.InlineSwatch(WI.InlineSwatch.Type.Color, gridColor);
            swatch.shiftClickColorEnabled = false;
            itemContainerElement.append(swatch.element);

            swatch.addEventListener(WI.InlineSwatch.Event.ValueChanged, (event) => {
                if (checkboxElement.checked)
                    // While changing the overlay color, WI.OverlayManager.Event.GridOverlayShown is dispatched with high frequency.
                    // An initiator is not provided so as not to skew usage count of overlay options when logging diagnostics in WI.GridOverlayDiagnosticEventRecorder.
                    WI.overlayManager.showGridOverlay(domNode, {color: event.data.value});
            }, swatch);

            swatch.addEventListener(WI.InlineSwatch.Event.Deactivated, (event) => {
                if (event.target.value === gridColor)
                    return;

                gridColor = event.target.value;
                WI.overlayManager.setGridColorForNode(domNode, gridColor);
            }, swatch);

            let buttonElement = itemContainerElement.appendChild(WI.createGoToArrowButton());
            buttonElement.title = WI.repeatedUIString.revealInDOMTree();
            WI.bindInteractionsForNodeToElement(domNode, buttonElement);
        }

        this._updateToggleAllCheckbox();
    }

    // Private

    _handleGridOverlayStateChanged(event)
    {
        let checkboxElement = this._checkboxElementByNodeMap.get(event.data.domNode);
        if (!checkboxElement)
            return;

        checkboxElement.checked = event.type === WI.OverlayManager.Event.GridOverlayShown;
        this._updateToggleAllCheckbox();
    }

    _updateToggleAllCheckbox()
    {
        if (this._suppressUpdateToggleAllCheckbox)
            return;

        let hasVisible = false;
        let hasHidden = false;
        for (let domNode of this._gridNodeSet) {
            let isVisible = WI.overlayManager.isGridOverlayVisible(domNode);
            if (isVisible)
                hasVisible = true;
            else
                hasHidden = true;

            // Exit early as soon as the partial state is determined.
            if (hasVisible && hasHidden)
                break;
        }

        if (hasVisible && hasHidden)
            this._toggleAllCheckboxElement.indeterminate = true;
        else {
            this._toggleAllCheckboxElement.indeterminate = false;
            this._toggleAllCheckboxElement.checked = hasVisible;
        }
    }
};
