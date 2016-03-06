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

WebInspector.ProfileDataGridTree = class ProfileDataGridTree extends WebInspector.Object
{
    constructor(callingContextTree, startTime, endTime, sortComparator)
    {
        super();

        console.assert(callingContextTree instanceof WebInspector.CallingContextTree);
        console.assert(typeof sortComparator === "function");

        this._children = [];
        this._sortComparator = sortComparator;

        this._callingContextTree = callingContextTree;
        this._startTime = startTime;
        this._endTime = endTime;
        this._numberOfSamples = this._callingContextTree.numberOfSamplesInTimeRange(startTime, endTime);

        this._focusNodes = [];
        this._modifiers = [];

        this._repopulate();
    }

    // Static

    static buildSortComparator(columnIdentifier, sortOrder)
    {
        let ascending = sortOrder == WebInspector.DataGrid.SortOrder.Ascending;
        return function(a, b) {
            let result = a.data[columnIdentifier] - b.data[columnIdentifier];
            return ascending ? result : -result;
        }
    }

    // Public

    get callingContextTree() { return this._callingContextTree; }

    get sampleInterval()
    {
        // FIXME: It would be good to bound this, but the startTime/endTime can be beyond the
        // bounds of when the calling context tree was sampling. This could be improved when a
        // subset within the timeline is selected (a better bounding start/end). For now just
        // assume a constant rate, as that roughly matches what the intent is.
        return 1 / 1000; // 1ms per sample
    }

    get focusNodes()
    {
        return this._focusNodes;
    }

    get currentFocusNode()
    {
        return this._focusNodes.lastValue;
    }

    get modifiers()
    {
        return this._modifiers;
    }

    get startTime()
    {
        if (this._focusNodes.length)
            return this._currentFocusStartTime;
        return this._startTime;
    }

    get endTime()
    {
        if (this._focusNodes.length)
            return this._currentFocusEndTime;
        return this._endTime;
    }

    get numberOfSamples()
    {
        if (this._focusNodes.length)
            return this._currentFocusNumberOfSamples;
        return this._numberOfSamples;
    }

    get children()
    {
        return this._children;
    }

    appendChild(node)
    {
        this._children.push(node);
    }

    insertChild(node, index)
    {
        this._children.splice(index, 0, node);
    }

    removeChildren()
    {
        this._children = [];
    }

    set sortComparator(comparator)
    {
        this._sortComparator = comparator;
        this.sort();
    }

    sort()
    {
        let children = this._children;
        children.sort(this._sortComparator);

        for (let i = 0; i < children.length; ++i) {
            children[i]._recalculateSiblings(i);
            children[i].sort();
        }
    }

    refresh()
    {
        for (let child of this._children)
            child.refreshRecursively();
    }

    addFocusNode(profileDataGridNode)
    {
        console.assert(profileDataGridNode instanceof WebInspector.ProfileDataGridNode);

        // Save the original parent for when we rollback.
        profileDataGridNode.__previousParent = profileDataGridNode.parent === profileDataGridNode.dataGrid ? this : profileDataGridNode.parent;

        this._focusNodes.push(profileDataGridNode);
        this._focusChanged();
    }

    rollbackFocusNode(profileDataGridNode)
    {
        console.assert(profileDataGridNode instanceof WebInspector.ProfileDataGridNode);

        let index = this._focusNodes.indexOf(profileDataGridNode);
        console.assert(index !== -1, "rollbackFocusNode should be rolling back to a previous focused node");
        console.assert(index !== this._focusNodes.length - 1, "rollbackFocusNode should be rolling back to a previous focused node");
        if (index === -1)
            return;

        for (let i = index + 1; i < this._focusNodes.length; ++i)
            this._restoreFocusedNodeToOriginalParent(this._focusNodes[i]);

        // Remove everything after this node.
        this._focusNodes.splice(index + 1);
        this._focusChanged();
    }

    clearFocusNodes()
    {
        for (let profileDataGridNode of this._focusNodes)
            this._restoreFocusedNodeToOriginalParent(profileDataGridNode);

        this._focusNodes = [];
        this._focusChanged();
    }

    hasModifiers()
    {
        return this._modifiers.length > 0;
    }

    addModifier(modifier)
    {
        this._modifiers.push(modifier);
        this._modifiersChanged();
    }

    clearModifiers()
    {
        this._modifiers = [];
        this._modifiersChanged();
    }

    // Private

    _repopulate()
    {
        this.removeChildren();

        if (this._focusNodes.length) {
            // The most recently focused node.
            this.appendChild(this.currentFocusNode);
        } else {
            // All nodes in the time range in the calling context tree.
            this._callingContextTree.forEachChild((child) => {
                if (child.hasStackTraceInTimeRange(this._startTime, this._endTime))
                    this.appendChild(new WebInspector.ProfileDataGridNode(child, this));
            });
        }

        this.sort();
    }

    _focusChanged()
    {
        let profileDataGridNode = this.currentFocusNode;
        if (profileDataGridNode) {
            this._updateCurrentFocusDetails(profileDataGridNode);

            if (profileDataGridNode.parent)
                profileDataGridNode.parent.removeChild(profileDataGridNode);
        }

        // FIXME: This re-creates top level children, without remembering their expanded / unexpanded state.
        this._repopulate();

        this.dispatchEventToListeners(WebInspector.ProfileDataGridTree.Event.FocusChanged);
    }

    _updateCurrentFocusDetails(focusDataGridNode)
    {
        let cctNode = focusDataGridNode.node;
        let timestampsInRange = cctNode.filteredTimestamps(this._startTime, this._endTime);

        this._currentFocusStartTime = timestampsInRange[0];
        this._currentFocusEndTime = timestampsInRange.lastValue;
        this._currentFocusNumberOfSamples = timestampsInRange.length;
    }

    _restoreFocusedNodeToOriginalParent(focusDataGridNode)
    {
        focusDataGridNode.__previousParent.appendChild(focusDataGridNode);
        focusDataGridNode.__previousParent = undefined;
    }

    _modifiersChanged()
    {
        this.dispatchEventToListeners(WebInspector.ProfileDataGridTree.Event.ModifiersChanged);
    }
}

WebInspector.ProfileDataGridTree.Event = {
    FocusChanged: "profile-data-grid-tree-focus-changed",
    ModifiersChanged: "profile-data-grid-tree-modifiers-changed",
};

WebInspector.ProfileDataGridTree.ModifierType = {
    ChargeToCaller: "charge",
};
