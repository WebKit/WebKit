/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// 1()RenderView|2(1)RenderBlock|3(2)RenderBody|4(3)RenderBlock|5(3)AnonymousRenderBlock|
class TreeBuilder {

    createTree(document, renderTreeDump) {
        // Root.
        let initialBlockContainer = new Layout.BlockContainer(document, parseInt(renderTreeDump.substring(0, renderTreeDump.indexOf("("))));
        initialBlockContainer.setRendererName("RenderView");
        renderTreeDump = renderTreeDump.substring(renderTreeDump.indexOf("|") + 1);

        while (true) {
            let endOfId = renderTreeDump.indexOf("(");
            let boxId = parseInt(renderTreeDump.substring(0, endOfId));
            let endOfParentId = renderTreeDump.indexOf(")");
            let parentId = parseInt(renderTreeDump.substring(endOfId + 1, endOfParentId));
            let endOfName = renderTreeDump.indexOf("|");
            let name = renderTreeDump.substring(endOfParentId + 1, endOfName);

            this._createAndAttachBox(initialBlockContainer, boxId, name, parentId);
            if (endOfName == renderTreeDump.length - 1)
                break;
            renderTreeDump = renderTreeDump.substring(endOfName + 1);
        }
        return initialBlockContainer;
    }

    _createAndAttachBox(initialBlockContainer, id, name, parentId) {
        let box = null;
        let text = null;
        let node = this._findNode(initialBlockContainer.node(), id, name);
        if (name == "RenderBlock" || name == "RenderBody")
            box = new Layout.BlockContainer(node, id);
        else if (name == "RenderInline")
            box = new Layout.InlineContainer(node, id);
        else if (name == "RenderText")
            text = new Text(node, id);
        else if (name == "RenderImage")
            box = new Layout.InlineBox(node, id);
        else
            box = new Layout.Box(node, id);

        if (box)
            box.setRendererName(name);

        let parentBox = Utils.layoutBoxById(parentId, initialBlockContainer);
        // WebKit does not construct anonymous inline container for text if the text
        // is a direct child of a block container.
        if (text) {
            box = new Layout.InlineBox(null, -1);
            box.setIsAnonymous();
            box.setText(text);
        }
        this._appendChild(parentBox, box);
        return box;
    }

    _appendChild(parent, child) {
        child.setParent(parent);
        let firstChild = parent.firstChild();
        if (!firstChild) {
            parent.setFirstChild(child);
            parent.setLastChild(child);
            return;
        }
        let lastChild = parent.lastChild();
        lastChild.setNextSibling(child);
        child.setPreviousSibling(lastChild);
        parent.setLastChild(child);
    }

    _findNode(node, boxId) {
        // Super inefficient but this is all temporary anyway.
        for (let currentNode = node.firstChild; currentNode; currentNode = currentNode.nextSibling) {
            if (currentNode.rendererId == boxId)
                return currentNode;
            let candidate = this._findNode(currentNode, boxId);
            if (candidate)
                return candidate;
        }
    }
}
