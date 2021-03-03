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

        this._nodeStyles = null;
        this.element.classList.add("layout-panel");
    }

    // Public

    get minimumWidth()
    {
        return this._boxModelDiagramRow?.minimumWidth ?? 0;
    }

    inspect(objects)
    {
        // Layout panel doesn't show when hasDOMNode is false.
        let hasDOMNode = super.inspect(objects);
        if (!hasDOMNode)
            return false;

        let stylesForNode = WI.cssManager.stylesForNode(this.domNode);
        stylesForNode.refreshIfNeeded().then((nodeStyles) => {
            if (nodeStyles === this._nodeStyles)
                return;

            if (this._nodeStyles) {
                this._nodeStyles.removeEventListener(WI.DOMNodeStyles.Event.Refreshed, this._nodeStylesRefreshed, this);
                this._nodeStyles.removeEventListener(WI.DOMNodeStyles.Event.NeedsRefresh, this._nodeStylesNeedsRefreshed, this);
            }

            this._nodeStyles = nodeStyles;

            if (this._nodeStyles) {
                this._nodeStyles.addEventListener(WI.DOMNodeStyles.Event.Refreshed, this._nodeStylesRefreshed, this);
                this._nodeStyles.addEventListener(WI.DOMNodeStyles.Event.NeedsRefresh, this._nodeStylesNeedsRefreshed, this);
            }

            this.needsLayout();
        });

        return hasDOMNode;
    }

    supportsDOMNode(nodeToInspect)
    {
        return nodeToInspect.nodeType() === Node.ELEMENT_NODE;
    }

    attached()
    {
        super.attached();

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        WI.cssManager.layoutContextTypeChangedMode = WI.CSSManager.LayoutContextTypeChangedMode.All;
    }

    detached()
    {
        WI.cssManager.layoutContextTypeChangedMode = WI.CSSManager.LayoutContextTypeChangedMode.Observed;

        WI.Frame.removeEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        super.detached();
    }

    // Protected

    initialLayout()
    {
        this._boxModelDiagramRow = new WI.BoxModelDetailsSectionRow;
        let boxModelGroup = new WI.DetailsSectionGroup([this._boxModelDiagramRow]);
        let boxModelSection = new WI.DetailsSection("layout-box-model", WI.UIString("Box Model"), [boxModelGroup]);
        this.contentView.element.appendChild(boxModelSection.element);

        let cssGridRow = new WI.DetailsSectionRow;
        let gridGroup = new WI.DetailsSectionGroup([cssGridRow]);
        let gridSection = new WI.DetailsSection("layout-css-grid", WI.UIString("Grid", "Grid @ Elements details sidebar", "CSS Grid layout section name"), [gridGroup]);
        this.contentView.element.appendChild(gridSection.element);

        let cssGridSection = new WI.CSSGridSection;
        cssGridRow.element.appendChild(cssGridSection.element);
        this.addSubview(cssGridSection);
    }

    layout()
    {
        super.layout();

        if (this._boxModelDiagramRow.nodeStyles !== this._nodeStyles)
            this._boxModelDiagramRow.nodeStyles = this._nodeStyles;
    }

    // Private

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this.needsLayout();
    }

    _nodeStylesRefreshed()
    {
        if (this.isAttached)
            this.needsLayout();
    }

    _nodeStylesNeedsRefreshed()
    {
        if (this.isAttached)
            this._nodeStyles?.refresh();
    }
};
