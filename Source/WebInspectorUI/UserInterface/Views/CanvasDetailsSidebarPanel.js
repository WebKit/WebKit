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

WI.CanvasDetailsSidebarPanel = class CanvasDetailsSidebarPanel extends WI.DetailsSidebarPanel
{
    constructor()
    {
        super("canvas", WI.UIString("Canvas"));

        this.element.classList.add("canvas");

        this._canvas = null;
        this._nodeRequestPromise = null;

        this._sections = [];
        this._emptyContentPlaceholder = null;
    }

    // Public

    inspect(objects)
    {
        if (!(objects instanceof Array))
            objects = [objects];

        objects = objects.map((object) => {
            if (object instanceof WI.Recording)
                return object.source;
            if (object instanceof WI.ShaderProgram)
                return object.canvas;
            return object;
        });

        this.canvas = objects.find((object) => object instanceof WI.Canvas);

        return !!this.canvas;
    }

    get canvas()
    {
        return this._canvas;
    }

    set canvas(canvas)
    {
        if (canvas === this._canvas)
            return;

        this._nodeRequestPromise = null;

        if (this._canvas) {
            this._canvas.removeEventListener(WI.Canvas.Event.MemoryChanged, this._canvasMemoryChanged, this);
            this._canvas.removeEventListener(WI.Canvas.Event.ExtensionEnabled, this._refreshExtensionsSection, this);
            this._canvas.removeEventListener(WI.Canvas.Event.SizeChanged, this._refreshSourceSection, this);
            this._canvas.removeEventListener(WI.Canvas.Event.ClientNodesChanged, this._handleClientNodesChanged, this);
        }

        this._canvas = canvas || null;

        if (this._canvas) {
            this._canvas.addEventListener(WI.Canvas.Event.MemoryChanged, this._canvasMemoryChanged, this);
            this._canvas.addEventListener(WI.Canvas.Event.ExtensionEnabled, this._refreshExtensionsSection, this);
            this._canvas.addEventListener(WI.Canvas.Event.SizeChanged, this._refreshSourceSection, this);
            this._canvas.addEventListener(WI.Canvas.Event.ClientNodesChanged, this._handleClientNodesChanged, this);
        }

        this.needsLayout();
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this._nameRow = new WI.DetailsSectionSimpleRow(WI.UIString("Name"));
        this._typeRow = new WI.DetailsSectionSimpleRow(WI.UIString("Type"));
        this._memoryRow = new WI.DetailsSectionSimpleRow(WI.UIString("Memory"));
        this._memoryRow.tooltip = WI.UIString("Memory usage of this canvas");

        let identitySection = new WI.DetailsSection("canvas-details", WI.UIString("Identity"));
        identitySection.groups = [new WI.DetailsSectionGroup([this._nameRow, this._typeRow, this._memoryRow])];
        this._sections.push(identitySection);

        this._nodeRow = new WI.DetailsSectionSimpleRow(WI.UIString("Node"));
        this._cssCanvasRow = new WI.DetailsSectionSimpleRow(WI.UIString("CSS Canvas"));
        this._widthRow = new WI.DetailsSectionSimpleRow(WI.UIString("Width"));
        this._heightRow = new WI.DetailsSectionSimpleRow(WI.UIString("Height"));
        this._detachedRow = new WI.DetailsSectionSimpleRow(WI.UIString("Detached"));

        let sourceSection = new WI.DetailsSection("canvas-source", WI.UIString("Source"));
        sourceSection.groups = [new WI.DetailsSectionGroup([this._nodeRow, this._cssCanvasRow, this._widthRow, this._heightRow, this._detachedRow])];
        this._sections.push(sourceSection);

        this._attributesDataGridRow = new WI.DetailsSectionDataGridRow(null, WI.UIString("No Attributes"));

        this._attributesSection = new WI.DetailsSection("canvas-attributes", WI.UIString("Attributes"));
        this._attributesSection.groups = [new WI.DetailsSectionGroup([this._attributesDataGridRow])];
        this._attributesSection.element.hidden = true;
        this._sections.push(this._attributesSection);

        this._extensionsSection = new WI.DetailsSection("canvas-extensions", WI.UIString("Extensions"));
        this._extensionsSection.element.hidden = true;
        this._sections.push(this._extensionsSection);

        this._featuresSection = new WI.DetailsSection("canvas-features", WI.UIString("Features"));
        this._featuresSection.element.hidden = true;
        this._sections.push(this._featuresSection);

        this._clientNodesRow = new WI.DetailsSectionSimpleRow(WI.UIString("Nodes"));

        this._clientsSection = new WI.DetailsSection("canvas-clients", WI.UIString("Clients"));
        this._clientsSection.groups = [new WI.DetailsSectionGroup([this._clientNodesRow])];
        this._clientsSection.element.hidden = true;
        this._sections.push(this._clientsSection);

        const selectable = false;
        let backtraceTreeOutline = new WI.TreeOutline(selectable);
        backtraceTreeOutline.disclosureButtons = false;
        this._backtraceTreeController = new WI.StackTraceTreeController(backtraceTreeOutline);

        let backtraceRow = new WI.DetailsSectionRow;
        backtraceRow.element.appendChild(backtraceTreeOutline.element);
        this._backtraceSection = new WI.DetailsSection("canvas-backtrace", WI.UIString("Backtrace"));
        this._backtraceSection.groups = [new WI.DetailsSectionGroup([backtraceRow])];

        this._backtraceSection.element.hidden = true;
        this._sections.push(this._backtraceSection);

        this._emptyContentPlaceholder = WI.createMessageTextView(WI.UIString("No Canvas Selected"));
    }

    layout()
    {
        super.layout();

        this.contentView.element.removeChildren();

        if (!this._canvas) {
            this.contentView.element.appendChild(this._emptyContentPlaceholder);
            return;
        }

        this.contentView.element.append(...this._sections.map(section => section.element));

        this._refreshIdentitySection();
        this._refreshSourceSection();
        this._refreshAttributesSection();
        this._refreshExtensionsSection();
        this._refreshFeaturesSection();
        this._refreshClientsSection();
        this._refreshBacktraceSection();
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
        this._nameRow.value = this._canvas.displayName;
        this._typeRow.value = WI.Canvas.displayNameForContextType(this._canvas.contextType);
        this._formatMemoryRow();
    }

    _refreshSourceSection()
    {
        if (!this.didInitialLayout)
            return;

        this._detachedRow.value = null;

        if (this._canvas.cssCanvasNames.length)
            this._nodeRow.value = null;
        else {
            let nodeRequestPromise = this._canvas.requestNode();
            this._nodeRequestPromise = nodeRequestPromise;
            this._nodeRequestPromise.then((node) => {
                if (this._nodeRequestPromise !== nodeRequestPromise)
                    return;

                if (!node) {
                    this._nodeRow.value = null;
                    return;
                }

                this._nodeRow.value = WI.linkifyNodeReference(node);
                this._detachedRow.value = node.parentNode ? null : WI.UIString("Yes");
            });
        }

        this._cssCanvasRow.value = this._canvas.cssCanvasNames.join(", ") || null;

        this._widthRow.value = this._canvas.sizes.length > 1 ? WI.UIString("(multiple)") : (this._canvas.sizes[0]?.width ?? emDash);
        this._widthRow.element.classList.toggle("multiple", this._canvas.sizes.length > 1);

        this._heightRow.value = this._canvas.sizes.length > 1 ? WI.UIString("(multiple)") : (this._canvas.sizes[0]?.height ?? emDash);
        this._heightRow.element.classList.toggle("multiple", this._canvas.sizes.length > 1);
    }

    _refreshAttributesSection()
    {
        let hasAttributes = !isEmptyObject(this._canvas.contextAttributes);
        this._attributesSection.element.hidden = !hasAttributes;
        if (!hasAttributes)
            return;

        let dataGrid = this._attributesDataGridRow.dataGrid;
        if (!dataGrid) {
            dataGrid = this._attributesDataGridRow.dataGrid = new WI.DataGrid({
                name: {title: WI.UIString("Name")},
                value: {title: WI.UIString("Value"), width: "30%"},
            });
        }

        dataGrid.removeChildren();

        for (let attribute in this._canvas.contextAttributes) {
            let data = {name: attribute, value: this._canvas.contextAttributes[attribute]};
            let dataGridNode = new WI.DataGridNode(data);
            dataGrid.appendChild(dataGridNode);
        }

        dataGrid.updateLayoutIfNeeded();
    }

    _refreshExtensionsSection()
    {
        let hasEnabledExtensions = this._canvas.extensions.size > 0;
        this._extensionsSection.element.hidden = !hasEnabledExtensions;
        if (!hasEnabledExtensions)
            return;

        let element = document.createElement("ul");
        for (let extension of this._canvas.extensions) {
            let listElement = element.appendChild(document.createElement("li"));
            listElement.textContent = extension;
        }
        this._extensionsSection.groups = [{element}];
    }

    _refreshFeaturesSection()
    {
        let hasFeatures = this._canvas.features.length > 0;
        this._featuresSection.element.hidden = !hasFeatures;
        if (!hasFeatures)
            return;

        let element = document.createElement("ul");
        for (let feature of this._canvas.features) {
            let listElement = element.appendChild(document.createElement("li"));
            listElement.textContent = feature;
        }
        this._featuresSection.groups = [{element}];
    }

    _refreshClientsSection()
    {
        if (!this.didInitialLayout)
            return;

        if (!this._canvas.cssCanvasNames.length && this._canvas.contextType !== WI.Canvas.ContextType.WebGPU) {
            this._clientsSection.element.hidden = true;
            return;
        }

        this._clientNodesRow.value = emDash;

        this._clientsSection.element.hidden = false;

        this._canvas.requestClientNodes((clientNodes) => {
            this._refreshSourceSection();

            if (!clientNodes.length)
                return;

            let fragment = document.createDocumentFragment();
            for (let clientNode of clientNodes)
                fragment.appendChild(WI.linkifyNodeReference(clientNode));
            this._clientNodesRow.value = fragment;
        });
    }

    _refreshBacktraceSection()
    {
        let stackTrace = this._canvas.stackTrace;
        this._backtraceTreeController.stackTrace = stackTrace;
        this._backtraceSection.element.hidden = !stackTrace?.callFrames.length;
    }

    _formatMemoryRow()
    {
        if (!this.didInitialLayout)
            return;

        if (!this._canvas.memoryCost || isNaN(this._canvas.memoryCost)) {
            this._memoryRow.value = emDash;
            return;
        }

        this._memoryRow.value = Number.bytesToString(this._canvas.memoryCost);
    }

    _canvasMemoryChanged(event)
    {
        this._formatMemoryRow();
    }

    _handleClientNodesChanged(event)
    {
        if (!this.didInitialLayout)
            return;

        this._refreshClientsSection();
    }
};
