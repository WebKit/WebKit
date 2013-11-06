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

WebInspector.ContentFlow = function(documentNodeIdentifier, name, overset, contentNodes)
{
    WebInspector.Object.call(this);

    this._documentNodeIdentifier = documentNodeIdentifier;
    this._name = name;
    this._overset = overset;
    this._contentNodes = contentNodes;
};

WebInspector.ContentFlow.Event = {
    OversetWasChanged: "content-flow-overset-was-changed",
    ContentNodeWasAdded: "content-flow-content-node-was-added",
    ContentNodeWasRemoved: "content-flow-content-node-was-removed"
};

WebInspector.ContentFlow.prototype = {

    constructor: WebInspector.ContentFlow,
    __proto__: WebInspector.Object.prototype,

    // Public

    get id()
    {
        // Use the flow node id, to avoid collisions when we change main document id.
        return this._documentNodeIdentifier + ":" + this._name;
    },

    get documentNodeIdentifier()
    {
        return this._documentNodeIdentifier;
    },

    get name()
    {
        return this._name;
    },

    get overset()
    {
        return this._overset;
    },

    set overset(overset)
    {
        if (this._overset === overset)
            return;
        this._overset = overset;
        this.dispatchEventToListeners(WebInspector.ContentFlow.Event.FlowOversetWasChanged);
    },

    get contentNodes()
    {
        return this._contentNodes;
    },

    insertContentNodeBefore: function(contentNode, referenceNode)
    {
        var index = this._contentNodes.indexOf(referenceNode);
        console.assert(index !== -1);
        this._contentNodes.splice(index, 0, contentNode);
        this.dispatchEventToListeners(WebInspector.ContentFlow.Event.ContentNodeWasAdded, {node: contentNode, before: referenceNode});
    },

    appendContentNode: function(contentNode)
    {
        this._contentNodes.push(contentNode);
        this.dispatchEventToListeners(WebInspector.ContentFlow.Event.ContentNodeWasAdded, {node: contentNode});
    },

    removeContentNode: function(contentNode)
    {
        var index = this._contentNodes.indexOf(contentNode);
        console.assert(index !== -1);
        this._contentNodes.splice(index, 1);
        this.dispatchEventToListeners(WebInspector.ContentFlow.Event.ContentNodeWasRemoved, {node: contentNode});
    }
};
