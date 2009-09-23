/*
 * Copyright (C) 2009 Nokia Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.         IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DOMStorageDataGrid = function(columns, domStorage, keys)
{
    WebInspector.DataGrid.call(this, columns);
    this.dataTableBody.addEventListener("dblclick", this._ondblclick.bind(this), false);
    this._domStorage = domStorage;
    this._keys = keys;
}

WebInspector.DOMStorageDataGrid.prototype = {
    _ondblclick: function(event)
    {
        if (this._editing)
            return;
        if (this._editingNode)
            return;
        this._startEditing(event);
    },

    _startEditingColumnOfDataGridNode: function(node, column)
    {
        this._editing = true;
        this._editingNode = node;
        this._editingNode.select();

        var element = this._editingNode._element.children[column];
        WebInspector.startEditing(element, this._editingCommitted.bind(this), this._editingCancelled.bind(this), element.textContent);
        window.getSelection().setBaseAndExtent(element, 0, element, 1);
    },

    _startEditing: function(event)
    {
        var element = event.target.enclosingNodeOrSelfWithNodeName("td");
        if (!element)
            return;

        this._editingNode = this.dataGridNodeFromEvent(event);
        if (!this._editingNode) {
            if (!this.creationNode)
                return;
            this._editingNode = this.creationNode;
        }

        // Force editing the "Key" column when editing the creation node
        if (this._editingNode.isCreationNode)
            return this._startEditingColumnOfDataGridNode(this._editingNode, 0);

        this._editing = true;
        WebInspector.startEditing(element, this._editingCommitted.bind(this), this._editingCancelled.bind(this), element.textContent);
        window.getSelection().setBaseAndExtent(element, 0, element, 1);
    },

    _editingCommitted: function(element, newText, oldText, context, moveDirection)
    {
        var columnIdentifier = (element.hasStyleClass("0-column") ? 0 : 1);
        var textBeforeEditing = this._editingNode.data[columnIdentifier];
        var currentEditingNode = this._editingNode;

        function moveToNextIfNeeded(wasChange) {
            if (!moveDirection)
                return;

            if (moveDirection === "forward") {
                if (currentEditingNode.isCreationNode && columnIdentifier === 0 && !wasChange)
                    return;

                if (columnIdentifier === 0)
                    return this._startEditingColumnOfDataGridNode(currentEditingNode, 1);

                var nextDataGridNode = currentEditingNode.traverseNextNode(true, null, true);
                if (nextDataGridNode)
                    return this._startEditingColumnOfDataGridNode(nextDataGridNode, 0);
                if (currentEditingNode.isCreationNode && wasChange) {
                    addCreationNode(false);
                    return this._startEditingColumnOfDataGridNode(this.creationNode, 0);
                }
                return;
            }

            if (moveDirection === "backward") {
                if (columnIdentifier === 1)
                    return this._startEditingColumnOfDataGridNode(currentEditingNode, 0);
                    var nextDataGridNode = currentEditingNode.traversePreviousNode(true, null, true);

                if (nextDataGridNode)
                    return this._startEditingColumnOfDataGridNode(nextDataGridNode, 1);
                return;
            }
        }

        if (textBeforeEditing == newText) {
            this._editingCancelled(element);
            moveToNextIfNeeded.call(this, false);
            return;
        }

        var domStorage = this._domStorage;
        if (columnIdentifier === 0) {
            if (this._keys.indexOf(newText) !== -1) {
                element.textContent = this._editingNode.data[0];
                this._editingCancelled(element);
                moveToNextIfNeeded.call(this, false);
                return;
            }
            domStorage.removeItem(this._editingNode.data[0]);
            domStorage.setItem(newText, this._editingNode.data[1]);
            this._editingNode.data[0] = newText;
        } else {
            domStorage.setItem(this._editingNode.data[0], newText);
            this._editingNode.data[1] = newText;
        }

        if (this._editingNode.isCreationNode)
            this.addCreationNode(false);

        this._editingCancelled(element);
        moveToNextIfNeeded.call(this, true);
    },

    _editingCancelled: function(element, context)
    {
        delete this._editing;
        this._editingNode = null;
    },

    deleteSelectedRow: function()
    {
        var node = this.selectedNode;
        if (!node || node.isCreationNode)
            return;

        if (this._domStorage)
            this._domStorage.removeItem(node.data[0]);
    }
}

WebInspector.DOMStorageDataGrid.prototype.__proto__ = WebInspector.DataGrid.prototype;
