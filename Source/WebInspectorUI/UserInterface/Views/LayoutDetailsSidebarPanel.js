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

WI.LayoutDetailsSidebarPanel = class LayoutDetailsSidebarPanel extends WI.DOMDetailsSidebarPanel
{
    constructor()
    {
        super("layout-details", WI.UIString("Layout", "Layout @ Styles Sidebar", "Title of the CSS style panel."));

        this._flexNodeSet = null;
        this._gridNodeSet = null;
        this.element.classList.add("layout-panel");
    }

    // Protected

    attached()
    {
        super.attached();

        WI.domManager.addEventListener(WI.DOMManager.Event.NodeInserted, this._handleNodeInserted, this);
        WI.domManager.addEventListener(WI.DOMManager.Event.NodeRemoved, this._handleNodeRemoved, this);

        WI.DOMNode.addEventListener(WI.DOMNode.Event.LayoutFlagsChanged, this._handleLayoutFlagsChanged, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        WI.cssManager.layoutContextTypeChangedMode = WI.CSSManager.LayoutContextTypeChangedMode.All;

        this._invalidateNodeSets();
    }

    detached()
    {
        WI.cssManager.layoutContextTypeChangedMode = WI.CSSManager.LayoutContextTypeChangedMode.Observed;

        WI.domManager.removeEventListener(WI.DOMManager.Event.NodeInserted, this._handleNodeInserted, this);
        WI.domManager.removeEventListener(WI.DOMManager.Event.NodeRemoved, this._handleNodeRemoved, this);

        WI.DOMNode.removeEventListener(WI.DOMNode.Event.LayoutFlagsChanged, this._handleLayoutFlagsChanged, this);
        WI.Frame.removeEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        super.detached();
    }

    initialLayout()
    {
        this._gridDetailsSectionRow = new WI.DetailsSectionRow(WI.UIString("No CSS Grid Containers", "No CSS Grid Containers @ Layout Details Sidebar Panel", "Message shown when there are no CSS Grid containers on the inspected page."));
        let gridGroup = new WI.DetailsSectionGroup([this._gridDetailsSectionRow]);
        let gridDetailsSection = new WI.DetailsSection("layout-css-grid", WI.UIString("Grid", "Grid @ Elements details sidebar", "CSS Grid layout section name"), [gridGroup]);
        this.contentView.element.appendChild(gridDetailsSection.element);

        this._gridSection = new WI.CSSGridNodeOverlayListSection;

        this._flexDetailsSectionRow = new WI.DetailsSectionRow(WI.UIString("No CSS Flex Containers", "No CSS Flex Containers @ Layout Details Sidebar Panel", "Message shown when there are no CSS Flex containers on the inspected page."));
        let flexDetailsSection = new WI.DetailsSection("layout-css-flexbox", WI.UIString("Flexbox", "Flexbox @ Elements details sidebar", "Flexbox layout section name"), [new WI.DetailsSectionGroup([this._flexDetailsSectionRow])]);
        this.contentView.element.appendChild(flexDetailsSection.element);

        this._flexSection = new WI.CSSFlexNodeOverlayListSection;
    }

    layout()
    {
        super.layout();

        if (!this._gridNodeSet || !this._flexNodeSet)
            this._refreshNodeSets();

        if (this._gridNodeSet.size) {
            this._gridDetailsSectionRow.hideEmptyMessage();
            this._gridDetailsSectionRow.element.appendChild(this._gridSection.element);

            if (!this._gridSection.isAttached)
                this.addSubview(this._gridSection);

            this._gridSection.nodeSet = this._gridNodeSet;
        } else {
            this._gridDetailsSectionRow.showEmptyMessage();

            if (this._gridSection.isAttached)
                this.removeSubview(this._gridSection);
        }

        if (this._flexNodeSet.size) {
            this._flexDetailsSectionRow.hideEmptyMessage();
            this._flexDetailsSectionRow.element.appendChild(this._flexSection.element);

            if (!this._flexSection.isAttached)
                this.addSubview(this._flexSection);

            this._flexSection.nodeSet = this._flexNodeSet;
        } else {
            this._flexDetailsSectionRow.showEmptyMessage();

            if (this._flexSection.isAttached)
                this.removeSubview(this._flexSection);
        }
    }

    // Private

    _handleNodeInserted(event)
    {
        this._invalidateNodeSets();
        this.needsLayout();
    }

    _handleNodeRemoved(event)
    {
        let domNode = event.target.node;
        this._removeNodeFromNodeSets(domNode);
        this.needsLayout();
    }

    _handleLayoutFlagsChanged(event)
    {
        let domNode = event.target;
        if (domNode.layoutContextType)
            this._invalidateNodeSets();
        else
            this._removeNodeFromNodeSets(domNode);

        this.needsLayout();
    }

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this.needsLayout();
    }

    _removeNodeFromNodeSets(domNode)
    {
        this._flexNodeSet?.delete(domNode);
        this._gridNodeSet?.delete(domNode);
    }

    _invalidateNodeSets()
    {
        this._flexNodeSet = null;
        this._gridNodeSet = null;
    }

    _refreshNodeSets()
    {
        this._gridNodeSet = new Set;
        this._flexNodeSet = new Set;

        for (let node of WI.domManager.attachedNodes({filter: (node) => node.layoutContextType})) {
            switch (node.layoutContextType) {
            case WI.DOMNode.LayoutFlag.Grid:
                this._gridNodeSet.add(node);
                break;

            case WI.DOMNode.LayoutFlag.Flex:
                this._flexNodeSet.add(node);
                break;

            default:
                console.assert(false, this.representedObject.layoutContextType);
                break;
            }
        }
    }
};
