/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

WI.LayerDetailsSidebarPanel = class LayerDetailsSidebarPanel extends WI.DetailsSidebarPanel
{
    constructor()
    {
        super("layer", WI.UIString("All Layers"));

        this.element.classList.add("layer");

        this._layers = [];
        this._dataGridNodesByLayerId = new Map;

        this._dataGrid = null;
        this._hoveredDataGridNode = null;
        this._bottomBar = null;
        this._layersCountLabel = null;
        this._layersMemoryLabel = null;
        this._popover = null;
    }

    // Public

    inspect(objects)
    {
        if (!(objects instanceof Array))
            objects = [objects];

        let layers = objects.filter((object) => object instanceof WI.Layer);
        this._updateLayers(layers);

        return !!layers.length;
    }

    willDismissPopover()
    {
        this._popover = null;
    }

    selectNodeByLayerId(layerId)
    {
        let node = this._dataGridNodesByLayerId.get(layerId);
        if (node === this._dataGrid.selectedNode)
            return;

        const suppressEvent = true;
        if (node) {
            node.revealAndSelect(suppressEvent);
            this._showPopoverForSelectedNode();
        } else if (this._dataGrid.selectedNode)
            this._dataGrid.selectedNode.deselect(suppressEvent);
    }

    // Private

    _buildDataGrid()
    {
        const columns = {
            name: {
                title: WI.UIString("Node"),
                sortable: false,
            },
            paintCount: {
                title: WI.UIString("Paints"),
                sortable: true,
                aligned: "right",
                width: "50px",
            },
            memory: {
                title: WI.UIString("Memory"),
                sortable: true,
                aligned: "right",
                width: "70px",
            }
        };

        this._dataGrid = new WI.DataGrid(columns);
        this._dataGrid.addEventListener(WI.DataGrid.Event.SortChanged, this._sortDataGrid, this);
        this._dataGrid.addEventListener(WI.DataGrid.Event.SelectedNodeChanged, this._dataGridSelectedNodeChanged, this);

        this._dataGrid.sortColumnIdentifier = "memory";
        this._dataGrid.sortOrder = WI.DataGrid.SortOrder.Descending;
        this._dataGrid.createSettings("layer-details-sidebar-panel");

        this._dataGrid.element.addEventListener("mousemove", this._dataGridMouseMove.bind(this));
        this._dataGrid.element.addEventListener("mouseleave", this._dataGridMouseLeave.bind(this));

        // FIXME: We can't use virtualized rows until DataGrid is able to scroll them programmatically.
        //        See TreeElement#reveal -> TreeOutline#updateVirtualizedElements for an analogy.
        this._dataGrid.inline = true;
        this._dataGrid.element.classList.remove("inline");

        this.contentView.addSubview(this._dataGrid);
    }

    _buildBottomBar()
    {
        this._bottomBar = this.element.appendChild(document.createElement("div"));
        this._bottomBar.className = "bottom-bar";

        this._layersCountLabel = this._bottomBar.appendChild(document.createElement("div"));
        this._layersCountLabel.className = "layers-count-label";

        this._layersMemoryLabel = this._bottomBar.appendChild(document.createElement("div"));
        this._layersMemoryLabel.className = "layers-memory-label";
    }

    _sortDataGrid()
    {
        let sortColumnIdentifier = this._dataGrid.sortColumnIdentifier;

        function comparator(a, b) {
            let itemA = a.layer[sortColumnIdentifier] || 0;
            let itemB = b.layer[sortColumnIdentifier] || 0;
            return itemA - itemB;
        }

        this._dataGrid.sortNodes(comparator);
    }

    _dataGridSelectedNodeChanged()
    {
        let layerId = this._dataGrid.selectedNode ? this._dataGrid.selectedNode.layer.layerId : null;
        this.dispatchEventToListeners(WI.LayerDetailsSidebarPanel.Event.SelectedLayerChanged, {layerId});

        this._showPopoverForSelectedNode();
    }

    _dataGridMouseMove(event)
    {
        let node = this._dataGrid.dataGridNodeFromNode(event.target);
        if (node === this._hoveredDataGridNode)
            return;

        if (!node) {
            this._hideDOMNodeHighlight();
            return;
        }

        this._hoveredDataGridNode = node;

        if (node.layer.isGeneratedContent || node.layer.isReflection || node.layer.isAnonymous) {
            const usePageCoordinates = true;
            WI.domTreeManager.highlightRect(node.layer.bounds, usePageCoordinates);
        } else
            WI.domTreeManager.highlightDOMNode(node.layer.nodeId);
    }

    _dataGridMouseLeave(event)
    {
        this._hideDOMNodeHighlight();
    }

    _hideDOMNodeHighlight()
    {
        WI.domTreeManager.hideDOMNodeHighlight();
        this._hoveredDataGridNode = null;
    }

    _updateLayers(newLayers)
    {
        if (this._popover)
            this._popover.dismiss();

        this._updateDataGrid(newLayers);
        this._updateBottomBar(newLayers);

        this._layers = newLayers;
    }

    _updateDataGrid(newLayers)
    {
        if (!this._dataGrid)
            this._buildDataGrid();

        let {removals, additions, preserved} = WI.layerTreeManager.layerTreeMutations(this._layers, newLayers);

        removals.forEach((layer) => {
            let node = this._dataGridNodesByLayerId.get(layer.layerId);
            this._dataGrid.removeChild(node);
            this._dataGridNodesByLayerId.delete(layer.layerId);
        });

        additions.forEach((layer) => {
            let node = new WI.LayerTreeDataGridNode(layer);
            this._dataGridNodesByLayerId.set(layer.layerId, node);
            this._dataGrid.appendChild(node);
        });

        preserved.forEach((layer) => {
            let node = this._dataGridNodesByLayerId.get(layer.layerId);
            node.layer = layer;
        });

        this._sortDataGrid();
    }

    _updateBottomBar(newLayers)
    {
        if (!this._bottomBar)
            this._buildBottomBar();

        this._layersCountLabel.textContent = WI.UIString("Layer Count: %d").format(newLayers.length);

        let totalMemory = newLayers.reduce((total, layer) => total + (layer.memory || 0), 0);
        this._layersMemoryLabel.textContent = WI.UIString("Memory: %s").format(Number.bytesToString(totalMemory));
    }

    _showPopoverForSelectedNode()
    {
        let dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode)
            return;

        this._contentForPopover(dataGridNode.layer, (content) => {
            if (dataGridNode !== this._dataGrid.selectedNode)
                return;

            this._popover = this._popover || new WI.Popover(this);
            this._popover.windowResizeHandler = () => { this._presentPopover(); };

            this._presentPopover(content);
        });
    }

    _presentPopover(content)
    {
        let targetFrame = WI.Rect.rectFromClientRect(this._dataGrid.selectedNode.element.getBoundingClientRect());
        if (content)
            this._popover.presentNewContentWithFrame(content, targetFrame.pad(2), [WI.RectEdge.MIN_X]);
        else
            this._popover.present(targetFrame.pad(2), [WI.RectEdge.MIN_X]);
    }

    _contentForPopover(layer, callback)
    {
        let content = document.createElement("div");
        content.className = "layer-popover";

        let dimensionsTitle = content.appendChild(document.createElement("div"));
        dimensionsTitle.textContent = WI.UIString("Dimensions");

        let dimensionsTable = content.appendChild(document.createElement("table"));

        let compositedRow = dimensionsTable.appendChild(document.createElement("tr"));
        let compositedLabel = compositedRow.appendChild(document.createElement("td"));
        let compositedValue = compositedRow.appendChild(document.createElement("td"));
        compositedLabel.textContent = WI.UIString("Composited");
        compositedValue.textContent = `${layer.compositedBounds.width}px ${multiplicationSign} ${layer.compositedBounds.height}px`;

        let visibleRow = dimensionsTable.appendChild(document.createElement("tr"));
        let visibleLabel = visibleRow.appendChild(document.createElement("td"));
        let visibleValue = visibleRow.appendChild(document.createElement("td"));
        visibleLabel.textContent = WI.UIString("Visible");
        visibleValue.textContent = `${layer.bounds.width}px ${multiplicationSign} ${layer.bounds.height}px`;

        let reasonsTitle = content.appendChild(document.createElement("div"));
        reasonsTitle.textContent = WI.UIString("Reasons for compositing:");

        let list = content.appendChild(document.createElement("ul"));

        WI.layerTreeManager.reasonsForCompositingLayer(layer, (compositingReasons) => {
            if (!isEmptyObject(compositingReasons))
                this._populateListOfCompositingReasons(list, compositingReasons);

            callback(content);
        });

        return content;
    }

    _populateListOfCompositingReasons(list, compositingReasons)
    {
        function addReason(reason) {
            let item = list.appendChild(document.createElement("li"));
            item.textContent = reason;
        }

        if (compositingReasons.transform3D)
            addReason(WI.UIString("Element has a 3D transform"));
        if (compositingReasons.video)
            addReason(WI.UIString("Element is <video>"));
        if (compositingReasons.canvas)
            addReason(WI.UIString("Element is <canvas>"));
        if (compositingReasons.plugin)
            addReason(WI.UIString("Element is a plug-in"));
        if (compositingReasons.iFrame)
            addReason(WI.UIString("Element is <iframe>"));
        if (compositingReasons.backfaceVisibilityHidden)
            addReason(WI.UIString("Element has “backface-visibility: hidden” style"));
        if (compositingReasons.clipsCompositingDescendants)
            addReason(WI.UIString("Element clips compositing descendants"));
        if (compositingReasons.animation)
            addReason(WI.UIString("Element is animated"));
        if (compositingReasons.filters)
            addReason(WI.UIString("Element has CSS filters applied"));
        if (compositingReasons.positionFixed)
            addReason(WI.UIString("Element has “position: fixed” style"));
        if (compositingReasons.positionSticky)
            addReason(WI.UIString("Element has “position: sticky” style"));
        if (compositingReasons.overflowScrollingTouch)
            addReason(WI.UIString("Element has “-webkit-overflow-scrolling: touch” style"));
        if (compositingReasons.stacking)
            addReason(WI.UIString("Element may overlap another compositing element"));
        if (compositingReasons.overlap)
            addReason(WI.UIString("Element overlaps other compositing element"));
        if (compositingReasons.negativeZIndexChildren)
            addReason(WI.UIString("Element has children with a negative z-index"));
        if (compositingReasons.transformWithCompositedDescendants)
            addReason(WI.UIString("Element has a 2D transform and composited descendants"));
        if (compositingReasons.opacityWithCompositedDescendants)
            addReason(WI.UIString("Element has opacity applied and composited descendants"));
        if (compositingReasons.maskWithCompositedDescendants)
            addReason(WI.UIString("Element is masked and composited descendants"));
        if (compositingReasons.reflectionWithCompositedDescendants)
            addReason(WI.UIString("Element has a reflection and composited descendants"));
        if (compositingReasons.filterWithCompositedDescendants)
            addReason(WI.UIString("Element has CSS filters applied and composited descendants"));
        if (compositingReasons.blendingWithCompositedDescendants)
            addReason(WI.UIString("Element has CSS blending applied and composited descendants"));
        if (compositingReasons.isolatesCompositedBlendingDescendants)
            addReason(WI.UIString("Element is a stacking context and has composited descendants with CSS blending applied"));
        if (compositingReasons.perspective)
            addReason(WI.UIString("Element has perspective applied"));
        if (compositingReasons.preserve3D)
            addReason(WI.UIString("Element has “transform-style: preserve-3d” style"));
        if (compositingReasons.willChange)
            addReason(WI.UIString("Element has “will-change” style with includes opacity, transform, transform-style, perspective, filter or backdrop-filter"));
        if (compositingReasons.root)
            addReason(WI.UIString("Element is the root element"));
        if (compositingReasons.blending)
            addReason(WI.UIString("Element has “blend-mode” style"));
    }
};

WI.LayerDetailsSidebarPanel.Event = {
    SelectedLayerChanged: "selected-layer-changed"
};
