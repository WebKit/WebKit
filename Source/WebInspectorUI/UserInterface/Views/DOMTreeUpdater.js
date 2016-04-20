/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DOMTreeUpdater = function(treeOutline)
{
    WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.NodeInserted, this._nodeInserted, this);
    WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.NodeRemoved, this._nodeRemoved, this);
    WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.AttributeModified, this._attributesUpdated, this);
    WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.AttributeRemoved, this._attributesUpdated, this);
    WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.CharacterDataModified, this._characterDataModified, this);
    WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.DocumentUpdated, this._documentUpdated, this);
    WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.ChildNodeCountUpdated, this._childNodeCountUpdated, this);

    this._treeOutline = treeOutline;
    this._recentlyModifiedNodes = [];
};

WebInspector.DOMTreeUpdater.prototype = {
    close: function()
    {
        WebInspector.domTreeManager.removeEventListener(null, null, this);
    },

    _documentUpdated: function(event)
    {
        this._reset();
    },

    _attributesUpdated: function(event)
    {
        this._recentlyModifiedNodes.push({node: event.data.node, updated: true, attribute: event.data.name});
        if (this._treeOutline._visible)
            this.soon._updateModifiedNodes();
    },

    _characterDataModified: function(event)
    {
        this._recentlyModifiedNodes.push({node: event.data.node, updated: true});
        if (this._treeOutline._visible)
            this.soon._updateModifiedNodes();
    },

    _nodeInserted: function(event)
    {
        this._recentlyModifiedNodes.push({node: event.data.node, parent: event.data.parent, inserted: true});
        if (this._treeOutline._visible)
            this.soon._updateModifiedNodes();
    },

    _nodeRemoved: function(event)
    {
        this._recentlyModifiedNodes.push({node: event.data.node, parent: event.data.parent, removed: true});
        if (this._treeOutline._visible)
            this.soon._updateModifiedNodes();
    },

    _childNodeCountUpdated: function(event)
    {
        var treeElement = this._treeOutline.findTreeElement(event.data);
        if (treeElement)
            treeElement.hasChildren = event.data.hasChildNodes();
    },

    _updateModifiedNodes: function()
    {
        this._updateModifiedNodes.cancelDebounce();

        let updatedParentTreeElements = [];
        for (let recentlyModifiedNode of this._recentlyModifiedNodes) {
            let parent = recentlyModifiedNode.parent;
            let node = recentlyModifiedNode.node;
            let changeInfo = null;
            if (recentlyModifiedNode.attribute)
                changeInfo = {type: WebInspector.DOMTreeElement.ChangeType.Attribute, attribute: recentlyModifiedNode.attribute};

            if (recentlyModifiedNode.updated) {
                let nodeTreeElement = this._treeOutline.findTreeElement(node);
                if (!nodeTreeElement)
                    continue;

                if (changeInfo)
                    nodeTreeElement.nodeStateChanged(changeInfo);

                nodeTreeElement.updateTitle();
            }

            if (!parent)
                continue;

            let parentNodeItem = this._treeOutline.findTreeElement(parent);
            if (parentNodeItem && !parentNodeItem.alreadyUpdatedChildren) {
                parentNodeItem.updateTitle();
                parentNodeItem.updateChildren();
                parentNodeItem.alreadyUpdatedChildren = true;
                updatedParentTreeElements.push(parentNodeItem);
            }
        }

        for (let i = 0; i < updatedParentTreeElements.length; ++i)
            updatedParentTreeElements[i].alreadyUpdatedChildren = null;

        this._recentlyModifiedNodes = [];
    },

    _reset: function()
    {
        WebInspector.domTreeManager.hideDOMNodeHighlight();
        this._recentlyModifiedNodes = [];
    }
};
