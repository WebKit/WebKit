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

WebInspector.HeapSnapshotNode = class HeapSnapshotNode
{
    constructor(identifier, className, size, internal)
    {
        this.id = identifier;
        this.className = className;
        this.size = size; 
        this.internal = internal;
        this.gcRoot = false;
        this.outgoingEdges = [];
        this.incomingEdges = [];
    }

    // Public

    get shortestGCRootPath()
    {
        // Returns an array from this node to a gcRoot node.
        // E.g. [Node, Edge, Node, Edge, Node].
        // Internal nodes are avoided, so if the path is empty this
        // node is either a gcRoot or only reachable via Internal nodes.

        if (this._shortestGCRootPath !== undefined)
            return this._shortestGCRootPath;

        let paths = this._gcRootPaths();
        paths.sort((a, b) => a.length - b.length);
        this._shortestGCRootPath = paths[0] || null;

        return this._shortestGCRootPath;
    }

    // Private

    _gcRootPaths()
    {
        if (this.gcRoot)
            return [];

        let paths = [];
        let currentPath = [];
        let visitedSet = new Set;

        function visitNode(node) {
            if (node.gcRoot) {
                let fullPath = currentPath.slice();
                fullPath.push(node);
                paths.push(fullPath);
                return;
            }

            if (visitedSet.has(node))
                return;
            visitedSet.add(node);

            currentPath.push(node);
            for (let parentEdge of node.incomingEdges) {
                if (parentEdge.from.internal)
                    continue;
                currentPath.push(parentEdge);
                visitNode(parentEdge.from);
                currentPath.pop();
            }
            currentPath.pop();
        }

        visitNode(this);

        return paths;
    }
};
