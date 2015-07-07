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

WebInspector.ContentFlowDOMTreeContentView = function(contentFlow)
{
    console.assert(contentFlow);

    WebInspector.DOMTreeContentView.call(this, contentFlow);

    contentFlow.addEventListener(WebInspector.ContentFlow.Event.ContentNodeWasAdded, this._contentNodeWasAdded, this);
    contentFlow.addEventListener(WebInspector.ContentFlow.Event.ContentNodeWasRemoved, this._contentNodeWasRemoved, this);

    this._createContentTrees();
};

WebInspector.ContentFlowDOMTreeContentView.prototype = {
    constructor: WebInspector.ContentFlowDOMTreeContentView,
    __proto__: WebInspector.DOMTreeContentView.prototype,

    // Public

    closed: function()
    {
        this.representedObject.removeEventListener(WebInspector.ContentFlow.Event.ContentNodeWasAdded, this._contentNodeWasAdded, this);
        this.representedObject.removeEventListener(WebInspector.ContentFlow.Event.ContentNodeWasRemoved, this._contentNodeWasRemoved, this);
        WebInspector.DOMTreeContentView.prototype.closed.call(this);
    },

    getSearchContextNodes: function(callback)
    {
        callback(this.domTreeOutline.children.map(function(treeOutline) {
            return treeOutline.representedObject.id;
        }));
    },

    // Private

    _createContentTrees: function()
    {
        var contentNodes = this.representedObject.contentNodes;
        for (var contentNode of contentNodes)
            this.domTreeOutline.appendChild(new WebInspector.DOMTreeElement(contentNode));

        var documentURL = contentNodes.length ? contentNodes[0].ownerDocument.documentURL : null;
        this._restoreSelectedNodeAfterUpdate(documentURL, contentNodes[0]);
    },

    _contentNodeWasAdded: function(event)
    {
        var treeElement = new WebInspector.DOMTreeElement(event.data.node);
        if (!event.data.before) {
            this.domTreeOutline.appendChild(treeElement);
            return;
        }

        var beforeElement = this.domTreeOutline.findTreeElement(event.data.before);
        console.assert(beforeElement);

        var index = this.domTreeOutline.children.indexOf(beforeElement);
        console.assert(index !== -1);

        this.domTreeOutline.insertChild(treeElement, index);
    },

    _contentNodeWasRemoved: function(event)
    {
        var treeElement = this.domTreeOutline.findTreeElement(event.data.node);
        console.assert(treeElement);
        this.domTreeOutline.removeChild(treeElement);
    }
};
