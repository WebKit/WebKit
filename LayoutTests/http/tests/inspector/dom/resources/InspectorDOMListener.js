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

TestPage.registerInitializer(function() {

window.createDOMListener = function()
{
    var nodesById = {};

    InspectorProtocol.addEventListener("DOM.setChildNodes", onSetChildNodes);
    InspectorProtocol.addEventListener("DOM.childNodeRemoved", onChildNodeRemoved);
    InspectorProtocol.addEventListener("DOM.childNodeInserted", onChildNodeInserted);

    function createNodeAttributesMap(attributes)
    {
        var attributesMap = {};
        for (var i = 0; i < attributes.length; i += 2)
            attributesMap[attributes[i]] = attributes[i + 1];
        return attributesMap;
    }

    function collectNode(node)
    {
        if (node.nodeType === 1)
            node.attributes = createNodeAttributesMap(node.attributes);
        if (node.children)
            node.children.forEach(collectNode);
        nodesById[node.nodeId] = node;
    }

    function nodeToString(node)
    {
        switch (node.nodeType) {
        case 1:
            var name = node.localName;
            if (node.attributes.id)
                name += "#" + node.attributes.id;
            if (node.attributes["class"])
                name += node.attributes["class"].split(" ").map(function(className) { return "." + className; }).join("");
            return name;
        case 3:
            return "<text node " + JSON.stringify(node.nodeValue) + ">";
        default:
            return "<nodeType " + node.nodeType + ">";
        }
    }

    function onSetChildNodes(response)
    {
        response.params.nodes.forEach(collectNode);
    }

    function onChildNodeRemoved(response)
    {
        delete nodesById[response.params.nodeId];
    }

    function onChildNodeInserted(response)
    {
        collectNode(response.params.node);
    }

    return {
        getNodeById: function(id)
        {
            return nodesById[id];
        },

        getNodeIdentifier: function(nodeId)
        {
            if (!nodeId)
                return "<invalid node id>";
            var node = nodesById[nodeId];
            return node ? nodeToString(node) : "<unknown node>";
        },

        collectNode: collectNode
    };
}

});
