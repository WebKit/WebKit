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

WebInspector.CanvasDetailsSidebarPanel = class CanvasDetailsSidebarPanel extends WebInspector.DetailsSidebarPanel
{
    constructor()
    {
        super("canvas-details", WebInspector.UIString("Canvas"));

        this.element.classList.add("canvas");

        this._canvas = null;
        this._node = null;
    }

    // Public

    inspect(objects)
    {
        if (!(objects instanceof Array))
            objects = [objects];

        this.canvas = objects.find((object) => object instanceof WebInspector.Canvas);

        return !!this._canvas;
    }

    get canvas()
    {
        return this._canvas;
    }

    set canvas(canvas)
    {
        if (canvas === this._canvas)
            return;

        this._canvas = canvas || null;

        if (this._node) {
            this._node.removeEventListener(WebInspector.DOMNode.Event.AttributeModified, this._refreshSourceSection, this);
            this._node.removeEventListener(WebInspector.DOMNode.Event.AttributeRemoved, this._refreshSourceSection, this);

            this._node = null;
        }

        this.needsLayout();
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this._nameRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Name"));
        this._typeRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Type"));

        let identitySection = new WebInspector.DetailsSection("canvas-details", WebInspector.UIString("Identity"));
        identitySection.groups = [new WebInspector.DetailsSectionGroup([this._nameRow, this._typeRow])];
        this.contentView.element.appendChild(identitySection.element);

        this._nodeRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Node"));
        this._cssCanvasRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("CSS Canvas"));
        this._widthRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Width"));
        this._heightRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Height"));
        this._datachedRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Detached"));

        let sourceSection = new WebInspector.DetailsSection("canvas-source", WebInspector.UIString("Source"));
        sourceSection.groups = [new WebInspector.DetailsSectionGroup([this._nodeRow, this._cssCanvasRow, this._widthRow, this._heightRow, this._datachedRow])];
        this.contentView.element.appendChild(sourceSection.element);

        this._attributesDataGridRow = new WebInspector.DetailsSectionDataGridRow(null, WebInspector.UIString("No Attributes"));

        let attributesSection = new WebInspector.DetailsSection("canvas-attributes", WebInspector.UIString("Attributes"));
        attributesSection.groups = [new WebInspector.DetailsSectionGroup([this._attributesDataGridRow])];
        this.contentView.element.appendChild(attributesSection.element);
    }

    layout()
    {
        super.layout();

        if (!this._canvas)
            return;

        this._refreshIdentitySection();
        this._refreshSourceSection();
        this._refreshAttributesSection();
    }

    sizeDidChange()
    {
        super.sizeDidChange();

        // FIXME: <https://webkit.org/b/152269> Web Inspector: Convert DetailsSection classes to use View
        this._attributesDataGridRow.sizeDidChange();
    }

    // Private

    _refreshIdentitySection()
    {
        if (!this._canvas)
            return;

        this._nameRow.value = this._canvas.displayName;
        this._typeRow.value = WebInspector.Canvas.displayNameForContextType(this._canvas.contextType);
    }

    _refreshSourceSection()
    {
        if (!this._canvas)
            return;

        this._nodeRow.value = this._canvas.cssCanvasName ? null : emDash;
        this._cssCanvasRow.value = this._canvas.cssCanvasName || null;
        this._widthRow.value = emDash;
        this._heightRow.value = emDash;
        this._datachedRow.value = null;

        this._canvas.requestNode((node) => {
            if (!node)
                return;

            if (node !== this._node) {
                if (this._node) {
                    this._node.removeEventListener(WebInspector.DOMNode.Event.AttributeModified, this._refreshSourceSection, this);
                    this._node.removeEventListener(WebInspector.DOMNode.Event.AttributeRemoved, this._refreshSourceSection, this);

                    this._node = null;
                }

                this._node = node;

                this._node.addEventListener(WebInspector.DOMNode.Event.AttributeModified, this._refreshSourceSection, this);
                this._node.addEventListener(WebInspector.DOMNode.Event.AttributeRemoved, this._refreshSourceSection, this);
            }

            if (!this._canvas.cssCanvasName)
                this._nodeRow.value = WebInspector.linkifyNodeReference(this._node);

            let setRowValueIfValidAttributeValue = (row, attribute) => {
                let value = Number(this._node.getAttribute(attribute));
                if (!Number.isInteger(value) || value < 0)
                    return false;

                row.value = value;
                return true;
            };

            let validWidth = setRowValueIfValidAttributeValue(this._widthRow, "width");
            let validHeight = setRowValueIfValidAttributeValue(this._heightRow, "height");
            if (!validWidth || !validHeight) {
                // Since the "width" and "height" properties of canvas elements are more than just
                // attributes, we need to invoke the getter for each to get the actual value.
                //  - https://html.spec.whatwg.org/multipage/canvas.html#attr-canvas-width
                //  - https://html.spec.whatwg.org/multipage/canvas.html#attr-canvas-height
                WebInspector.RemoteObject.resolveNode(node, "", (remoteObject) => {
                    if (!remoteObject)
                        return;

                    function setRowValueToPropertyValue(row, property) {
                        remoteObject.getProperty(property, (error, result, wasThrown) => {
                            if (!error && result.type === "number")
                                row.value = `${result.value}px`;
                        });
                    }

                    setRowValueToPropertyValue(this._widthRow, "width");
                    setRowValueToPropertyValue(this._heightRow, "height");

                    remoteObject.release();
                });
            }

            if (!this._canvas.cssCanvasName && !this._node.parentNode)
                this._datachedRow.value = WebInspector.UIString("Yes");
        });
    }

    _refreshAttributesSection()
    {
        if (!this._canvas)
            return;

        if (isEmptyObject(this._canvas.contextAttributes)) {
            // Remove the DataGrid to show the placeholder text.
            this._attributesDataGridRow.dataGrid = null;
            return;
        }

        let dataGrid = this._attributesDataGridRow.dataGrid;
        if (!dataGrid) {
            dataGrid = this._attributesDataGridRow.dataGrid = new WebInspector.DataGrid({
                name: {title: WebInspector.UIString("Name")},
                value: {title: WebInspector.UIString("Value"), width: "30%"},
            });
        }

        dataGrid.removeChildren();

        for (let attribute in this._canvas.contextAttributes) {
            let data = {name: attribute, value: this._canvas.contextAttributes[attribute]};
            let dataGridNode = new WebInspector.DataGridNode(data);
            dataGrid.appendChild(dataGridNode);
        }

        dataGrid.updateLayoutIfNeeded();
    }
};
