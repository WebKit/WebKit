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

WebInspector.DOMStorageDataGrid = function(columns)
{
    WebInspector.DataGrid.call(this, columns);
    this.dataTableBody.addEventListener("dblclick", this._ondblclick.bind(this), false);
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
    
    _startEditing: function(event)
    {
        var element = event.target.enclosingNodeOrSelfWithNodeName("td");
        if (!element)
            return;
        this._editingNode = this.dataGridNodeFromEvent(event);
        if (!this._editingNode)
            return;
        this._editing = true;
            
        WebInspector.startEditing(element, this._editingCommitted.bind(this), this._editingCancelled.bind(this), element.textContent);
        window.getSelection().setBaseAndExtent(element, 0, element, 1);
    },
    
    _editingCommitted: function(element, newText)
    {
        if (element.hasStyleClass("0-column"))
            columnIdentifier = 0;
        else
            columnIdentifier = 1;
        textBeforeEditing = this._editingNode.data[columnIdentifier];
        if (textBeforeEditing == newText) {
            this._editingCancelled(element);
            return;
        }
        
        var domStorage = WebInspector.panels.databases.visibleView.domStorage.domStorage;
        if (domStorage) {
            if (columnIdentifier == 0) {
                if (domStorage.getItem(newText) != null) {
                    element.textContent = this._editingNode.data[0];
                    this._editingCancelled(element);
                    return;
                }
                domStorage.removeItem(this._editingNode.data[0]);
                domStorage.setItem(newText, this._editingNode.data[1]);
                this._editingNode.data[0] = newText;            
            } else {
                domStorage.setItem(this._editingNode.data[0], newText);
                this._editingNode.data[1] = newText;
            }
        }
        
        this._editingCancelled(element);
    },
    
    _editingCancelled: function(element, context)
    {
        delete this._editing;
        this._editingNode = null;
    },
    
    deleteSelectedRow: function()
    {
        var node = this.selectedNode;
        var domStorage = WebInspector.panels.databases.visibleView.domStorage.domStorage;
        if (node && domStorage)
            domStorage.removeItem(node.data[0]);
    }
}

WebInspector.DOMStorageDataGrid.prototype.__proto__ = WebInspector.DataGrid.prototype;
