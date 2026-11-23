/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.LayerTreeDetailsSidebarPanel = class LayerTreeDetailsSidebarPanel extends WI.DOMDetailsSidebarPanel
{
    constructor()
    {
        super("layer-tree", WI.UIString("Layers"));

        this._dataGridNodesByLayerId = new Map;

        this.element.classList.add("layer-tree");
    }

    // DetailsSidebarPanel Overrides.

    attached()
    {
        super.attached();

        WI.layerTreeManager.addEventListener(WI.LayerTreeManager.Event.LayerTreeDidChange, this._layerTreeDidChange, this);
    }

    detached()
    {
        WI.layerTreeManager.removeEventListener(WI.LayerTreeManager.Event.LayerTreeDidChange, this._layerTreeDidChange, this);

        super.detached();
    }

    // DOMDetailsSidebarPanel Overrides

    supportsDOMNode(nodeToInspect)
    {
        return WI.layerTreeManager.supported && nodeToInspect.nodeType() === Node.ELEMENT_NODE;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this._buildLayerInfoSection();
        this._buildDataGridSection();
        this._buildBottomBar();
    }

    layout()
    {
        super.layout();

        if (!this.domNode || this.domNode.destroyed)
            return;

        let domNode = this.domNode;
        WI.layerTreeManager.layersForNodeIncludingFrames(domNode, (layers) => {
            if (this.domNode !== domNode)
                return;

            let layerForNode = layers[0]?.nodeId === domNode.id && !layers[0].isGeneratedContent ? layers[0] : null;
            let childLayers = layers.slice(layerForNode ? 1 : 0);
            this._unfilteredChildLayers = childLayers;
            this._updateDisplayWithLayers(layerForNode, childLayers);
        });
    }

    sizeDidChange()
    {
        super.sizeDidChange();

        // FIXME: <https://webkit.org/b/152269> Web Inspector: Convert DetailsSection classes to use View
        this._childLayersRow.sizeDidChange();
    }

    // Private

    _layerTreeDidChange(event)
    {
        this.needsLayout();
    }

    _buildLayerInfoSection()
    {
        var rows = this._layerInfoRows = {};
        var rowsArray = [];

        rowsArray.push(rows["Width"] = new WI.DetailsSectionSimpleRow(WI.UIString("Width")));
        rowsArray.push(rows["Height"] = new WI.DetailsSectionSimpleRow(WI.UIString("Height")));
        rowsArray.push(rows["Paints"] = new WI.DetailsSectionSimpleRow(WI.UIString("Paints")));
        rowsArray.push(rows["Memory"] = new WI.DetailsSectionSimpleRow(WI.UIString("Memory")));

        this._layerInfoGroup = new WI.DetailsSectionGroup(rowsArray);

        var emptyRow = new WI.DetailsSectionRow(WI.UIString("No Layer Available"));
        emptyRow.showEmptyMessage();
        this._noLayerInformationGroup = new WI.DetailsSectionGroup([emptyRow]);

        this._layerInfoSection = new WI.DetailsSection("layer-info", WI.UIString("Layer Info"), [this._noLayerInformationGroup]);

        this.contentView.element.appendChild(this._layerInfoSection.element);
    }

    _buildDataGridSection()
    {
        var columns = {name: {}, paintCount: {}, memory: {}};

        columns.name.title = WI.UIString("Node");
        columns.name.sortable = false;

        columns.paintCount.title = WI.UIString("Paints", "Paints @ Column title", "A count of how many times an element was painted (rendered)");
        columns.paintCount.sortable = true;
        columns.paintCount.aligned = "right";
        columns.paintCount.width = "70px";

        columns.memory.title = WI.UIString("Memory");
        columns.memory.sortable = true;
        columns.memory.aligned = "right";
        columns.memory.width = "70px";

        this._dataGrid = new WI.DataGrid(columns);
        this._dataGrid.inline = true;
        this._dataGrid.addEventListener(WI.DataGrid.Event.SortChanged, this._sortDataGrid, this);
        this._dataGrid.addEventListener(WI.DataGrid.Event.SelectedNodeChanged, this._selectedDataGridNodeChanged, this);

        this._dataGrid.sortColumnIdentifier = "memory";
        this._dataGrid.sortOrder = WI.DataGrid.SortOrder.Descending;
        this._dataGrid.createSettings("layer-tree-details-sidebar-panel");

        var element = this._dataGrid.element;
        element.addEventListener("focus", this._dataGridGainedFocus.bind(this), false);
        element.addEventListener("blur", this._dataGridLostFocus.bind(this), false);
        element.addEventListener("click", this._dataGridWasClicked.bind(this), false);

        this._childLayersRow = new WI.DetailsSectionDataGridRow(null, WI.UIString("No Child Layers"));
        var group = new WI.DetailsSectionGroup([this._childLayersRow]);
        var section = new WI.DetailsSection("layer-children", WI.UIString("Child Layers"), [group], null, true);

        this.contentView.element.appendChild(section.element);
    }

    _buildBottomBar()
    {
        var bottomBar = this.element.appendChild(document.createElement("div"));
        bottomBar.className = "bottom-bar";

        this._layersCountLabel = bottomBar.appendChild(document.createElement("div"));
        this._layersCountLabel.className = "layers-count-label";

        this._layersMemoryLabel = bottomBar.appendChild(document.createElement("div"));
        this._layersMemoryLabel.className = "layers-memory-label";
    }

    _sortDataGrid()
    {
        var sortColumnIdentifier = this._dataGrid.sortColumnIdentifier;

        function comparator(a, b)
        {
            var item1 = a.layer[sortColumnIdentifier] || 0;
            var item2 = b.layer[sortColumnIdentifier] || 0;
            return item1 - item2;
        }

        this._dataGrid.sortNodes(comparator);
        this._updatePopoverForSelectedNode();
    }

    _selectedDataGridNodeChanged()
    {
        if (this._dataGrid.selectedNode) {
            this._highlightSelectedNode();
            this._showPopoverForSelectedNode();
        } else {
            WI.domManager.hideDOMNodeHighlight();
            this._hidePopover();
        }
    }

    _dataGridGainedFocus(event)
    {
        this._highlightSelectedNode();
        this._showPopoverForSelectedNode();
    }

    _dataGridLostFocus(event)
    {
        WI.domManager.hideDOMNodeHighlight();
        this._hidePopover();
    }

    _dataGridWasClicked(event)
    {
        if (this._dataGrid.selectedNode && event.target.parentNode.classList.contains("filler"))
            this._dataGrid.selectedNode.deselect();
    }

    _highlightSelectedNode()
    {
        var dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode)
            return;

        var layer = dataGridNode.layer;
        if (layer.isGeneratedContent || layer.isReflection || layer.isAnonymous) {
            let target = this.domNode?.owningTarget || WI.assumingMainTarget();
            let usePageCoordinates = !(target instanceof WI.FrameTarget);
            WI.domManager.highlightRect(layer.bounds, usePageCoordinates, target);
        } else {
            let domNode = WI.domManager.nodeForId(layer.nodeId);
            if (domNode)
                domNode.highlight();
            else
                WI.domManager.hideDOMNodeHighlight();
        }
    }

    _updateDisplayWithLayers(layerForNode, childLayers)
    {
        this._updateLayerInfoSection(layerForNode);
        this._updateDataGrid(layerForNode, childLayers);
        this._updateMetrics(layerForNode, childLayers);

        this._layerForNode = layerForNode;
        this._childLayers = childLayers;
    }

    _updateLayerInfoSection(layer)
    {
        this._layerInfoSection.groups = layer ? [this._layerInfoGroup] : [this._noLayerInformationGroup];

        if (!layer)
            return;

        this._layerInfoRows["Memory"].value = Number.bytesToString(layer.memory || 0);
        this._layerInfoRows["Width"].value = layer.compositedBounds.width + "px";
        this._layerInfoRows["Height"].value = layer.compositedBounds.height + "px";
        this._layerInfoRows["Paints"].value = layer.paintCount + "";
    }

    _updateDataGrid(layerForNode, childLayers)
    {
        let dataGrid = this._dataGrid;
        let mutations = WI.layerTreeManager.layerTreeMutations(this._childLayers, childLayers);

        mutations.removals.forEach(function(layer) {
            let node = this._dataGridNodesByLayerId.get(layer.layerId);
            if (node) {
                dataGrid.removeChild(node);
                this._dataGridNodesByLayerId.delete(layer.layerId);
            }
        }, this);

        mutations.additions.forEach(function(layer) {
            let node = this._dataGridNodeForLayer(layer);
            if (node)
                dataGrid.appendChild(node);
        }, this);

        mutations.preserved.forEach(function(layer) {
            let node = this._dataGridNodesByLayerId.get(layer.layerId);
            if (node)
                node.layer = layer;
        }, this);

        this._sortDataGrid();

        this._childLayersRow.dataGrid = !isEmptyObject(childLayers) ? this._dataGrid : null;
    }

    _dataGridNodeForLayer(layer)
    {
        let node = new WI.LayerTreeDataGridNode(layer);
        this._dataGridNodesByLayerId.set(layer.layerId, node);

        return node;
    }

    _updateMetrics(layerForNode, childLayers)
    {
        var layerCount = 0;
        var totalMemory = 0;

        if (layerForNode) {
            layerCount++;
            totalMemory += layerForNode.memory || 0;
        }

        childLayers.forEach(function(layer) {
            layerCount++;
            totalMemory += layer.memory || 0;
        });

        this._layersCountLabel.textContent = WI.UIString("Layer Count: %d").format(layerCount);
        this._layersMemoryLabel.textContent = WI.UIString("Memory: %s").format(Number.bytesToString(totalMemory));
    }

    _showPopoverForSelectedNode()
    {
        var dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode)
            return;

        this._contentForPopover(dataGridNode.layer, (content) => {
            if (dataGridNode === this._dataGrid.selectedNode)
                this._updatePopoverForSelectedNode(content);
        });
    }

    _updatePopoverForSelectedNode(content)
    {
        var dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode)
            return;

        var popover = this._popover;
        if (!popover) {
            popover = this._popover = new WI.Popover;
            popover.windowResizeHandler = () => { this._updatePopoverForSelectedNode(); };
        }

        var targetFrame = WI.Rect.rectFromClientRect(dataGridNode.element.getBoundingClientRect());

        if (content)
            this._popover.presentNewContentWithFrame(content, targetFrame.pad(2), [WI.RectEdge.MIN_X]);
        else
            popover.present(targetFrame.pad(2), [WI.RectEdge.MIN_X]);
    }

    _hidePopover()
    {
        if (this._popover)
            this._popover.dismiss();
    }

    _contentForPopover(layer, callback)
    {
        var content = document.createElement("div");
        content.className = "layer-tree-popover";

        content.appendChild(document.createElement("p")).textContent = WI.UIString("Reasons for compositing:");

        var list = content.appendChild(document.createElement("ul"));

        WI.layerTreeManager.reasonsForCompositingLayer(layer, (compositingReasons) => {
            if (isEmptyObject(compositingReasons)) {
                callback(content);
                return;
            }

            this._populateListOfCompositingReasons(list, compositingReasons);

            callback(content);
        });

        return content;
    }

    _populateListOfCompositingReasons(list, compositingReasons)
    {
        for (let description of WI.LayerTreeManager.descriptionsForCompositingReasons(compositingReasons))
            list.appendChild(document.createElement("li")).textContent = description;
    }
};
