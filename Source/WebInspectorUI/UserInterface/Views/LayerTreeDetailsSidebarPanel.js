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

WebInspector.LayerTreeDetailsSidebarPanel = class LayerTreeDetailsSidebarPanel extends WebInspector.DOMDetailsSidebarPanel
{
    constructor()
    {
        super("layer-tree", WebInspector.UIString("Layers"), WebInspector.UIString("Layer"));

        this._dataGridNodesByLayerId = new Map;

        this.element.classList.add("layer-tree");

        WebInspector.showShadowDOMSetting.addEventListener(WebInspector.Setting.Event.Changed, this._showShadowDOMSettingChanged, this);

        window.addEventListener("resize", this._windowResized.bind(this));

        this._buildLayerInfoSection();
        this._buildDataGridSection();
        this._buildBottomBar();
    }

    // DetailsSidebarPanel Overrides.

    shown()
    {
        WebInspector.layerTreeManager.addEventListener(WebInspector.LayerTreeManager.Event.LayerTreeDidChange, this._layerTreeDidChange, this);

        console.assert(this.parentSidebar);

        this.needsRefresh();

        super.shown();
    }

    hidden()
    {
        WebInspector.layerTreeManager.removeEventListener(WebInspector.LayerTreeManager.Event.LayerTreeDidChange, this._layerTreeDidChange, this);

        super.hidden();
    }

    refresh()
    {
        if (!this.domNode)
            return;

        WebInspector.layerTreeManager.layersForNode(this.domNode, function(layerForNode, childLayers) {
            this._unfilteredChildLayers = childLayers;
            this._updateDisplayWithLayers(layerForNode, childLayers);
        }.bind(this));
    }

    // DOMDetailsSidebarPanel Overrides

    supportsDOMNode(nodeToInspect)
    {
        return WebInspector.layerTreeManager.supported && nodeToInspect.nodeType() === Node.ELEMENT_NODE;
    }

    // Private

    _layerTreeDidChange(event)
    {
        this.needsRefresh();
    }

    _showShadowDOMSettingChanged(event)
    {
        if (this.selected)
            this._updateDisplayWithLayers(this._layerForNode, this._unfilteredChildLayers);
    }

    _windowResized(event)
    {
        if (this._popover && this._popover.visible)
            this._updatePopoverForSelectedNode();
    }

    _buildLayerInfoSection()
    {
        var rows = this._layerInfoRows = {};
        var rowsArray = [];

        rowsArray.push(rows["Width"] = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Width")));
        rowsArray.push(rows["Height"] = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Height")));
        rowsArray.push(rows["Paints"] = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Paints")));
        rowsArray.push(rows["Memory"] = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Memory")));

        this._layerInfoGroup = new WebInspector.DetailsSectionGroup(rowsArray);

        var emptyRow = new WebInspector.DetailsSectionRow(WebInspector.UIString("No Layer Available"));
        emptyRow.showEmptyMessage();
        this._noLayerInformationGroup = new WebInspector.DetailsSectionGroup([emptyRow]);

        this._layerInfoSection = new WebInspector.DetailsSection("layer-info", WebInspector.UIString("Layer Info"), [this._noLayerInformationGroup]);

        this.contentView.element.appendChild(this._layerInfoSection.element);
    }

    _buildDataGridSection()
    {
        var columns = {name: {}, paintCount: {}, memory: {}};

        columns.name.title = WebInspector.UIString("Node");
        columns.name.sortable = false;

        columns.paintCount.title = WebInspector.UIString("Paints");
        columns.paintCount.sortable = true;
        columns.paintCount.aligned = "right";
        columns.paintCount.width = "50px";

        columns.memory.title = WebInspector.UIString("Memory");
        columns.memory.sortable = true;
        columns.memory.aligned = "right";
        columns.memory.width = "70px";

        this._dataGrid = new WebInspector.DataGrid(columns);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SortChanged, this._sortDataGrid, this);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._selectedDataGridNodeChanged, this);

        this.sortColumnIdentifierSetting = new WebInspector.Setting("layer-tree-details-sidebar-panel-sort", "memory");
        this.sortOrder = WebInspector.DataGrid.SortOrder.Descending;

        var element = this._dataGrid.element;
        element.classList.add("inline");
        element.addEventListener("focus", this._dataGridGainedFocus.bind(this), false);
        element.addEventListener("blur", this._dataGridLostFocus.bind(this), false);
        element.addEventListener("click", this._dataGridWasClicked.bind(this), false);

        this._childLayersRow = new WebInspector.DetailsSectionDataGridRow(null, WebInspector.UIString("No Child Layers"));
        var group = new WebInspector.DetailsSectionGroup([this._childLayersRow]);
        var section = new WebInspector.DetailsSection("layer-children", WebInspector.UIString("Child Layers"), [group], null, true);

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
            WebInspector.domTreeManager.hideDOMNodeHighlight();
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
        WebInspector.domTreeManager.hideDOMNodeHighlight();
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
        if (layer.isGeneratedContent || layer.isReflection || layer.isAnonymous)
            WebInspector.domTreeManager.highlightRect(layer.bounds, true);
        else
            WebInspector.domTreeManager.highlightDOMNode(layer.nodeId);
    }

    _updateDisplayWithLayers(layerForNode, childLayers)
    {
        if (!WebInspector.showShadowDOMSetting.value) {
            childLayers = childLayers.filter(function(layer) {
                return !layer.isInShadowTree;
            });
        }

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

        this._layerInfoRows["Memory"].value = Number.bytesToString(layer.memory);
        this._layerInfoRows["Width"].value = layer.compositedBounds.width + "px";
        this._layerInfoRows["Height"].value = layer.compositedBounds.height + "px";
        this._layerInfoRows["Paints"].value = layer.paintCount + "";
    }

    _updateDataGrid(layerForNode, childLayers)
    {
        let dataGrid = this._dataGrid;
        let mutations = WebInspector.layerTreeManager.layerTreeMutations(this._childLayers, childLayers);

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
        let node = new WebInspector.LayerTreeDataGridNode(layer);
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

        this._layersCountLabel.textContent = WebInspector.UIString("Layer Count: %d").format(layerCount);
        this._layersMemoryLabel.textContent = WebInspector.UIString("Memory: %s").format(Number.bytesToString(totalMemory));
    }

    _showPopoverForSelectedNode()
    {
        var dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode)
            return;

        this._contentForPopover(dataGridNode.layer, function(content) {
            if (dataGridNode === this._dataGrid.selectedNode)
                this._updatePopoverForSelectedNode(content);
        }.bind(this));
    }

    _updatePopoverForSelectedNode(content)
    {
        var dataGridNode = this._dataGrid.selectedNode;
        if (!dataGridNode)
            return;

        var popover = this._popover;
        if (!popover)
            popover = this._popover = new WebInspector.Popover;

        var targetFrame = WebInspector.Rect.rectFromClientRect(dataGridNode.element.getBoundingClientRect());

        if (content)
            popover.content = content;

        popover.present(targetFrame.pad(2), [WebInspector.RectEdge.MIN_X]);
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

        content.appendChild(document.createElement("p")).textContent = WebInspector.UIString("Reasons for compositing:");

        var list = content.appendChild(document.createElement("ul"));

        WebInspector.layerTreeManager.reasonsForCompositingLayer(layer, function(compositingReasons) {
            if (isEmptyObject(compositingReasons)) {
                callback(content);
                return;
            }

            this._populateListOfCompositingReasons(list, compositingReasons);

            callback(content);
        }.bind(this));

        return content;
    }

    _populateListOfCompositingReasons(list, compositingReasons)
    {
        function addReason(reason)
        {
            list.appendChild(document.createElement("li")).textContent = reason;
        }

        if (compositingReasons.transform3D)
            addReason(WebInspector.UIString("Element has a 3D transform"));
        if (compositingReasons.video)
            addReason(WebInspector.UIString("Element is <video>"));
        if (compositingReasons.canvas)
            addReason(WebInspector.UIString("Element is <canvas>"));
        if (compositingReasons.plugin)
            addReason(WebInspector.UIString("Element is a plug-in"));
        if (compositingReasons.iFrame)
            addReason(WebInspector.UIString("Element is <iframe>"));
        if (compositingReasons.backfaceVisibilityHidden)
            addReason(WebInspector.UIString("Element has “backface-visibility: hidden” style"));
        if (compositingReasons.clipsCompositingDescendants)
            addReason(WebInspector.UIString("Element clips compositing descendants"));
        if (compositingReasons.animation)
            addReason(WebInspector.UIString("Element is animated"));
        if (compositingReasons.filters)
            addReason(WebInspector.UIString("Element has CSS filters applied"));
        if (compositingReasons.positionFixed)
            addReason(WebInspector.UIString("Element has “position: fixed” style"));
        if (compositingReasons.positionSticky)
            addReason(WebInspector.UIString("Element has “position: sticky” style"));
        if (compositingReasons.overflowScrollingTouch)
            addReason(WebInspector.UIString("Element has “-webkit-overflow-scrolling: touch” style"));
        if (compositingReasons.stacking)
            addReason(WebInspector.UIString("Element establishes a stacking context"));
        if (compositingReasons.overlap)
            addReason(WebInspector.UIString("Element overlaps other compositing element"));
        if (compositingReasons.negativeZIndexChildren)
            addReason(WebInspector.UIString("Element has children with a negative z-index"));
        if (compositingReasons.transformWithCompositedDescendants)
            addReason(WebInspector.UIString("Element has a 2D transform and composited descendants"));
        if (compositingReasons.opacityWithCompositedDescendants)
            addReason(WebInspector.UIString("Element has opacity applied and composited descendants"));
        if (compositingReasons.maskWithCompositedDescendants)
            addReason(WebInspector.UIString("Element is masked and composited descendants"));
        if (compositingReasons.reflectionWithCompositedDescendants)
            addReason(WebInspector.UIString("Element has a reflection and composited descendants"));
        if (compositingReasons.filterWithCompositedDescendants)
            addReason(WebInspector.UIString("Element has CSS filters applied and composited descendants"));
        if (compositingReasons.blendingWithCompositedDescendants)
            addReason(WebInspector.UIString("Element has CSS blending applied and composited descendants"));
        if (compositingReasons.isolatesCompositedBlendingDescendants)
            addReason(WebInspector.UIString("Element is a stacking context and has composited descendants with CSS blending applied"));
        if (compositingReasons.perspective)
            addReason(WebInspector.UIString("Element has perspective applied"));
        if (compositingReasons.preserve3D)
            addReason(WebInspector.UIString("Element has “transform-style: preserve-3d” style"));
        if (compositingReasons.willChange)
            addReason(WebInspector.UIString("Element has “will-change” style with includes opacity, transform, transform-style, perspective, filter or backdrop-filter"));
        if (compositingReasons.root)
            addReason(WebInspector.UIString("Element is the root element"));
        if (compositingReasons.blending)
            addReason(WebInspector.UIString("Element has “blend-mode” style"));
    }
};
