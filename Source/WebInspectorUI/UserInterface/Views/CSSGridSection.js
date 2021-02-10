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

WI.CSSGridSection = class CSSGridSection extends WI.View
{
    constructor()
    {
        super();

        this.element.classList.add("css-grid-section");

        this._gridNodeSet = new Set;
        this._checkboxElementByNodeMap = new WeakMap;
    }

    // Protected

    attached()
    {
        super.attached();

        WI.DOMNode.addEventListener(WI.DOMNode.Event.LayoutContextTypeChanged, this._handleLayoutContextTypeChanged, this);
        WI.overlayManager.addEventListener(WI.OverlayManager.Event.GridOverlayShown, this._handleGridOverlayStateChanged, this);
        WI.overlayManager.addEventListener(WI.OverlayManager.Event.GridOverlayHidden, this._handleGridOverlayStateChanged, this);

        this._refreshGridNodeSet();
    }

    detached()
    {
        WI.DOMNode.removeEventListener(WI.DOMNode.Event.LayoutContextTypeChanged, this._handleLayoutContextTypeChanged, this);
        WI.overlayManager.removeEventListener(WI.OverlayManager.Event.GridOverlayShown, this._handleGridOverlayStateChanged, this);
        WI.overlayManager.removeEventListener(WI.OverlayManager.Event.GridOverlayHidden, this._handleGridOverlayStateChanged, this);

        super.detached();
    }

    initialLayout()
    {
        super.initialLayout();

        let listHeading = this.element.appendChild(document.createElement("h2"));
        listHeading.classList.add("heading");
        listHeading.textContent = WI.UIString("Grid Overlays", "Page Overlays @ Layout Sidebar Section Header", "Heading for list of grid nodes");

        this._listElement = this.element.appendChild(document.createElement("ul"));
        this._listElement.classList.add("node-link-list");
    }

    layout()
    {
        super.layout();

        this._listElement.removeChildren();

        let nodesWithGridOverlay = WI.overlayManager.nodesWithGridOverlay;

        for (let domNode of this._gridNodeSet) {
            let itemElement = this._listElement.appendChild(document.createElement("li"));
            let labelElement = itemElement.appendChild(document.createElement("label"));
            let checkboxElement = labelElement.appendChild(document.createElement("input"));
            checkboxElement.type = "checkbox";
            checkboxElement.checked = nodesWithGridOverlay.includes(domNode);
            labelElement.appendChild(WI.linkifyNodeReference(domNode));

            this._checkboxElementByNodeMap.set(domNode, checkboxElement);

            checkboxElement.addEventListener("change", (event) => {
                if (checkboxElement.checked)
                    WI.overlayManager.showGridOverlay(domNode, {color: swatch.value});
                else
                    WI.overlayManager.hideGridOverlay(domNode);
            });

            let swatch = new WI.InlineSwatch(WI.InlineSwatch.Type.Color, WI.Color.fromString("magenta"));
            itemElement.append(swatch.element);

            swatch.addEventListener(WI.InlineSwatch.Event.ValueChanged, (event) => {
                if (checkboxElement.checked)
                    WI.overlayManager.showGridOverlay(domNode, {color: event.data.value});
            }, swatch);
        }
    }

    // Private

    _handleGridOverlayStateChanged(event)
    {
        let checkboxElement = this._checkboxElementByNodeMap.get(event.data.domNode);
        if (!checkboxElement)
            return;

        checkboxElement.checked = event.type === WI.OverlayManager.Event.GridOverlayShown;
    }

    _handleLayoutContextTypeChanged(event)
    {
        let domNode = event.target;
        if (domNode.layoutContextType === WI.DOMNode.LayoutContextType.Grid)
            this._gridNodeSet.add(domNode);
        else
            this._gridNodeSet.delete(domNode);

        this.needsLayout();
    }

    _refreshGridNodeSet()
    {
        this._gridNodeSet = new Set(WI.domManager.nodesWithLayoutContextType(WI.DOMNode.LayoutContextType.Grid));
        this.needsLayout();
    }
};
