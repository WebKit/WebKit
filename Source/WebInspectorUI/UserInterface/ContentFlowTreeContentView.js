/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

WebInspector.ContentFlowTreeContentView = function(contentFlow)
{
    console.assert(contentFlow);

    WebInspector.ContentView.call(this, contentFlow);

    this._selectedTreeElement = null;

    // Map of contentNode ids to DOMTreeOutline objects.
    this._nodesMap = new Map();

    this._createContentTrees();

    contentFlow.addEventListener(WebInspector.ContentFlow.Event.ContentNodeWasAdded, this._contentNodeWasAdded, this);
    contentFlow.addEventListener(WebInspector.ContentFlow.Event.ContentNodeWasRemoved, this._contentNodeWasRemoved, this);
};

WebInspector.ContentFlowTreeContentView.StyleClassName = "content-flow-tree";

WebInspector.ContentFlowTreeContentView.prototype = {
    constructor: WebInspector.ContentFlowTreeContentView,
    __proto__: WebInspector.ContentView.prototype,

    // Public

    get selectionPathComponents()
    {
        var treeElement = this._selectedTreeElement;
        var pathComponents = [];

        while (treeElement && !treeElement.root) {
            // The close tag is contained within the element it closes. So skip it since we don't want to
            // show the same node twice in the hierarchy.
            if (treeElement.isCloseTag()) {
                treeElement = treeElement.parent;
                continue;
            }

            // FIXME: ContentFlow.contentNodes should be linked to each other.
            var pathComponent = new WebInspector.DOMTreeElementPathComponent(treeElement, treeElement.representedObject);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
            pathComponents.unshift(pathComponent);

            // Do not display elements outside the ContentFlow.
            if (this._nodesMap.has(treeElement.representedObject.id))
                break;

            treeElement = treeElement.parent;
        }

        return pathComponents;
    },

    updateLayout: function()
    {
        this._nodesMap.forEach(function(node) {
            node.updateSelection();
        });
    },

    shown: function()
    {
        var omitFocus = WebInspector.isConsoleFocused();
        this._nodesMap.forEach(function(node) {
            node.setVisible(true, omitFocus);
        });
    },

    hidden: function()
    {
        WebInspector.domTreeManager.hideDOMNodeHighlight();
        this._nodesMap.forEach(function(node) {
            node.setVisible(false);
        });
    },

    closed: function()
    {
        this.representedObject.removeEventListener(WebInspector.ContentFlow.Event.ContentNodeWasAdded, this._contentNodeWasAdded, this);
        this.representedObject.removeEventListener(WebInspector.ContentFlow.Event.ContentNodeWasRemoved, this._contentNodeWasRemoved, this);
        this._nodesMap.forEach(function(node) {
            node.close();
        });
    },

    // Private

    _selectedNodeDidChange: function(contentNodeOutline, event)
    {
        var selectedTreeElement = contentNodeOutline.selectedTreeElement;
        if (this._selectedTreeElement === selectedTreeElement)
            return;

        // Make sure that moving from one tree to the other will deselect the previous element.
        if (this._selectedTreeElement && this._selectedTreeElement.treeOutline !== contentNodeOutline)
            this._selectedTreeElement.deselect();

        this._selectedTreeElement = selectedTreeElement;
        if (selectedTreeElement) {
            // FIXME: Switching between different ContentFlowTreeContentView or DOMTreeContentView elements should call ConsoleAgent.addInspectedNode.
            ConsoleAgent.addInspectedNode(selectedTreeElement.representedObject.id);
        }

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    },

    _pathComponentSelected: function(event)
    {
        console.assert(event.data.pathComponent instanceof WebInspector.DOMTreeElementPathComponent);
        console.assert(event.data.pathComponent.domTreeElement instanceof WebInspector.DOMTreeElement);

        var treeElement = event.data.pathComponent.domTreeElement;
        treeElement.treeOutline.selectDOMNode(treeElement.representedObject, true);
    },

    _createContentNodeTree: function(node)
    {
        console.assert(!this._nodesMap.has(node.id));

        // FIXME: DOMTree's should be linked to each other when navigating with keyboard up/down events.
        var contentNodeOutline = new WebInspector.DOMTreeOutline(false, true, true);
        contentNodeOutline.addEventListener(WebInspector.DOMTreeOutline.Event.SelectedNodeChanged, this._selectedNodeDidChange.bind(this, contentNodeOutline), this);
        contentNodeOutline.setVisible(this.visible, WebInspector.isConsoleFocused());
        contentNodeOutline.wireToDomAgent();
        contentNodeOutline.rootDOMNode = node;

        this._nodesMap.set(node.id, contentNodeOutline);

        return contentNodeOutline;
    },

    _createContentTrees: function()
    {
        for (var contentNode of this.representedObject.contentNodes) {
            var contentNodeOutline = this._createContentNodeTree(contentNode);
            this.element.appendChild(contentNodeOutline.element);
        }
    },

    _contentNodeWasAdded: function(event)
    {
        var treeElement = this._createContentNodeTree(event.data.node);
        if (event.data.before) {
            var beforeElement = this._nodesMap.get(event.data.before.id);
            console.assert(beforeElement);
            this.element.insertBefore(treeElement.element, beforeElement.element);
        } else
            this.element.appendChild(treeElement.element);
    },

    _contentNodeWasRemoved: function(event)
    {
        var contentNodeOutline = this._nodesMap.get(event.data.node.id);
        contentNodeOutline.close();
        contentNodeOutline.element.remove();

        this._nodesMap.delete(event.data.node.id);
    }
};
