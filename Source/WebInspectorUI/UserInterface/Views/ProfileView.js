/*
* Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.ProfileView = class ProfileView extends WebInspector.ContentView
{
    constructor(callingContextTree)
    {
        super(callingContextTree);

        console.assert(callingContextTree instanceof WebInspector.CallingContextTree);

        this._startTime = 0;
        this._endTime = Infinity;
        this._callingContextTree = callingContextTree;

        this._hoveredDataGridNode = null;

        this.element.classList.add("profile");

        let columns = {
            totalTime: {
                title: WebInspector.UIString("Total Time"),
                width: "120px",
                sortable: true,
                aligned: "right",
            },
            selfTime: {
                title: WebInspector.UIString("Self Time"),
                width: "75px",
                sortable: true,
                aligned: "right",
            },
            function: {
                title: WebInspector.UIString("Function"),
                disclosure: true,
            },
        };

        this._dataGrid = new WebInspector.DataGrid(columns);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SortChanged, this._dataGridSortChanged, this);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SelectedNodeChanged, this._dataGridNodeSelected, this);
        this._dataGrid.addEventListener(WebInspector.DataGrid.Event.ExpandedNode, this._dataGridNodeExpanded, this);
        this._dataGrid.element.addEventListener("mouseover", this._mouseOverDataGrid.bind(this));
        this._dataGrid.element.addEventListener("mouseleave", this._mouseLeaveDataGrid.bind(this));
        this._dataGrid.indentWidth = 20;
        this._dataGrid.sortColumnIdentifierSetting = new WebInspector.Setting("profile-view-sort", "totalTime");
        this._dataGrid.sortOrderSetting = new WebInspector.Setting("profile-view-sort-order", WebInspector.DataGrid.SortOrder.Descending);

        this.addSubview(this._dataGrid);
    }

    // Public

    get callingContextTree() { return this._callingContextTree; }
    get startTime() { return this._startTime; }
    get endTime() { return this._endTime; }

    setStartAndEndTime(startTime, endTime)
    {
        console.assert(startTime >= 0);
        console.assert(endTime >= 0);
        console.assert(startTime <= endTime);

        this._startTime = startTime;
        this._endTime = endTime;

        // FIXME: It would be ideal to update the existing tree, maintaining nodes that were expanded.
        // For now just recreate the tree for the new time range.

        this._recreate();
    }

    hasFocusNodes()
    {
        if (!this._profileDataGridTree)
            return false;
        return this._profileDataGridTree.focusNodes.length > 0
    }

    clearFocusNodes()
    {
        if (!this._profileDataGridTree)
            return;
        this._profileDataGridTree.clearFocusNodes();
    }

    // Protected

    get selectionPathComponents()
    {
        let pathComponents = [];

        if (this._profileDataGridTree) {
            for (let profileDataGridNode of this._profileDataGridTree.focusNodes) {
                let displayName = profileDataGridNode.displayName();
                let className = profileDataGridNode.iconClassName();
                let pathComponent = new WebInspector.HierarchicalPathComponent(displayName, className, profileDataGridNode);
                pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.Clicked, this._pathComponentClicked, this);
                pathComponents.push(pathComponent);
            }
        }

        return pathComponents;
    }

    // Private

    _recreate()
    {
        let sortComparator = WebInspector.ProfileDataGridTree.buildSortComparator(this._dataGrid.sortColumnIdentifier, this._dataGrid.sortOrder);
        this._profileDataGridTree = new WebInspector.ProfileDataGridTree(this._callingContextTree, this._startTime, this._endTime, sortComparator);
        this._profileDataGridTree.addEventListener(WebInspector.ProfileDataGridTree.Event.FocusChanged, this._dataGridTreeFocusChanged, this);
        this._profileDataGridTree.addEventListener(WebInspector.ProfileDataGridTree.Event.ModifiersChanged, this._dataGridTreeModifiersChanged, this);
        this._repopulateDataGridFromTree();

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _repopulateDataGridFromTree(skipRefresh)
    {
        this._dataGrid.removeChildren();
        for (let child of this._profileDataGridTree.children)
            this._dataGrid.appendChild(child);
    }

    _pathComponentClicked(event)
    {
        if (!event.data.pathComponent)
            return;

        let profileDataGridNode = event.data.pathComponent.representedObject;
        if (profileDataGridNode === this._profileDataGridTree.currentFocusNode)
            return;

        this._profileDataGridTree.rollbackFocusNode(event.data.pathComponent.representedObject);
    }

    _dataGridTreeFocusChanged(event)
    {
        this._repopulateDataGridFromTree();
        this._profileDataGridTree.refresh();

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _dataGridTreeModifiersChanged(event)
    {
        this._profileDataGridTree.refresh();
    }

    _dataGridSortChanged()
    {
        if (!this._profileDataGridTree)
            return;

        this._profileDataGridTree.sortComparator = WebInspector.ProfileDataGridTree.buildSortComparator(this._dataGrid.sortColumnIdentifier, this._dataGrid.sortOrder);
        this._repopulateDataGridFromTree();
    }

    _dataGridNodeSelected(event)
    {
        let oldSelectedNode = event.data.oldSelectedNode;
        if (oldSelectedNode) {
            this._removeGuidanceElement(WebInspector.ProfileView.GuidanceType.Selected, oldSelectedNode);
            oldSelectedNode.forEachChildInSubtree((node) => this._removeGuidanceElement(WebInspector.ProfileView.GuidanceType.Selected, node));
        }

        let newSelectedNode = this._dataGrid.selectedNode;
        if (newSelectedNode) {
            this._removeGuidanceElement(WebInspector.ProfileView.GuidanceType.Selected, newSelectedNode);
            newSelectedNode.forEachChildInSubtree((node) => this._appendGuidanceElement(WebInspector.ProfileView.GuidanceType.Selected, node, newSelectedNode));
        }
    }

    _dataGridNodeExpanded(event)
    {
        let expandedNode = event.data.dataGridNode;

        if (this._dataGrid.selectedNode) {
            if (expandedNode.isInSubtreeOfNode(this._dataGrid.selectedNode))
                expandedNode.forEachImmediateChild((node) => this._appendGuidanceElement(WebInspector.ProfileView.GuidanceType.Selected, node, this._dataGrid.selectedNode));
        }

        if (this._hoveredDataGridNode) {
            if (expandedNode.isInSubtreeOfNode(this._hoveredDataGridNode))
                expandedNode.forEachImmediateChild((node) => this._appendGuidanceElement(WebInspector.ProfileView.GuidanceType.Hover, node, this._hoveredDataGridNode));
        }
    }

    _mouseOverDataGrid(event)
    {
        let hoveredDataGridNode = this._dataGrid.dataGridNodeFromNode(event.target);
        if (hoveredDataGridNode === this._hoveredDataGridNode)
            return;

        if (this._hoveredDataGridNode) {
            this._removeGuidanceElement(WebInspector.ProfileView.GuidanceType.Hover, this._hoveredDataGridNode);
            this._hoveredDataGridNode.forEachChildInSubtree((node) => this._removeGuidanceElement(WebInspector.ProfileView.GuidanceType.Hover, node));
        }

        this._hoveredDataGridNode = hoveredDataGridNode;

        if (this._hoveredDataGridNode) {
            this._appendGuidanceElement(WebInspector.ProfileView.GuidanceType.Hover, this._hoveredDataGridNode, this._hoveredDataGridNode);
            this._hoveredDataGridNode.forEachChildInSubtree((node) => this._appendGuidanceElement(WebInspector.ProfileView.GuidanceType.Hover, node, this._hoveredDataGridNode));
        }
    }

    _mouseLeaveDataGrid(event)
    {
        if (!this._hoveredDataGridNode)
            return;

        this._removeGuidanceElement(WebInspector.ProfileView.GuidanceType.Hover, this._hoveredDataGridNode);
        this._hoveredDataGridNode.forEachChildInSubtree((node) => this._removeGuidanceElement(WebInspector.ProfileView.GuidanceType.Hover, node));

        this._hoveredDataGridNode = null;
    }

    _guidanceElementKey(type)
    {
        return "guidance-element-" + type;
    }

    _removeGuidanceElement(type, node)
    {
        let key = this._guidanceElementKey(type);
        let element = node.elementWithColumnIdentifier("function");
        if (!element || !element[key])
            return;

        element[key].remove();
        element[key] = null;
    }

    _appendGuidanceElement(type, node, baseElement)
    {
        let depth = baseElement.depth;
        let guidanceMarkerLeft = depth ? (depth * this._dataGrid.indentWidth) + 1.5 : 7.5;

        let key = this._guidanceElementKey(type);
        let element = node.elementWithColumnIdentifier("function");
        let guidanceElement = element[key] || element.appendChild(document.createElement("div"));
        element[key] = guidanceElement;
        guidanceElement.classList.add("guidance", type);
        guidanceElement.classList.toggle("base", node === baseElement);
        guidanceElement.style.left = guidanceMarkerLeft + "px";
    }
};

WebInspector.ProfileView.GuidanceType = {
    Selected: "selected",
    Hover: "hover",
};
