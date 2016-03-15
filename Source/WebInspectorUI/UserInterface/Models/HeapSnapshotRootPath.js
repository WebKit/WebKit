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

WebInspector.HeapSnapshotRootPath = class HeapSnapshotRootPath extends WebInspector.Object
{
    constructor(node, pathComponent, parent, isGlobalScope)
    {
        super();

        console.assert(!node || node instanceof WebInspector.HeapSnapshotNode);
        console.assert(!pathComponent || typeof pathComponent === "string");
        console.assert(!parent || parent instanceof WebInspector.HeapSnapshotRootPath);

        this._node = node || null;
        this._parent = parent || null;
        this._pathComponent = typeof pathComponent === "string" ? pathComponent : null;
        this._isGlobalScope = isGlobalScope || false;

        // Become the new root when appended to an empty path.
        if (this._parent && this._parent.isEmpty())
            this._parent = null;
    }

    // Static

    static emptyPath()
    {
        return new WebInspector.HeapSnapshotRootPath(null);
    }

    // Public

    get node() { return this._node; }
    get parent() { return this._parent; }
    get pathComponent() { return this._pathComponent; }

    get rootNode()
    {
        return this._parent ? this._parent.rootNode : this._node;
    }

    get fullPath()
    {
        let components = [];
        for (let p = this; p && p.pathComponent; p = p.parent)
            components.push(p.pathComponent);
        components.reverse();
        return components.join("");
    }

    isRoot()
    {
        return !this._parent;
    }

    isEmpty()
    {
        return !this._node;
    }

    isGlobalScope()
    {
        return this._isGlobalScope;
    }

    isPathComponentImpossible()
    {
        return this._pathComponent && this._pathComponent.startsWith("@");
    }

    isFullPathImpossible()
    {
        if (this.isEmpty())
            return true;

        if (this.isPathComponentImpossible())
            return true;

        if (this._parent)
            return this._parent.isFullPathImpossible();

        return false;
    }

    appendInternal(node)
    {
        return new WebInspector.HeapSnapshotRootPath(node, WebInspector.HeapSnapshotRootPath.SpecialPathComponent.InternalPropertyName, this);
    }

    appendArrayIndex(node, index)
    {
        let component = "[" + index + "]";
        return new WebInspector.HeapSnapshotRootPath(node, component, this);
    }

    appendPropertyName(node, propertyName)
    {
        let component = this._canPropertyNameBeDotAccess(propertyName) ? "." + propertyName : "[" + doubleQuotedString(propertyName) + "]";
        return new WebInspector.HeapSnapshotRootPath(node, component, this);
    }

    appendVariableName(node, variableName)
    {
        // Treat as a property of the global object, e.g. "window.foo".
        if (this._isGlobalScope)
            return this.appendPropertyName(node, variableName);
        return new WebInspector.HeapSnapshotRootPath(node, variableName, this);
    }

    appendGlobalScopeName(node, globalScopeName)
    {
        return new WebInspector.HeapSnapshotRootPath(node, globalScopeName, this, true);
    }

    appendEdge(edge)
    {
        console.assert(edge instanceof WebInspector.HeapSnapshotEdge);

        switch (edge.type) {
        case WebInspector.HeapSnapshotEdge.EdgeType.Internal:
            return this.appendInternal(edge.to);
        case WebInspector.HeapSnapshotEdge.EdgeType.Index:
            return this.appendArrayIndex(edge.to, edge.data);
        case WebInspector.HeapSnapshotEdge.EdgeType.Property:
            return this.appendPropertyName(edge.to, edge.data);
        case WebInspector.HeapSnapshotEdge.EdgeType.Variable:
            return this.appendVariableName(edge.to, edge.data);
        }

        console.error("Unexpected edge type", edge.type);
    }

    // Private

    _canPropertyNameBeDotAccess(propertyName)
    {
        return /^(?![0-9])\w+$/.test(propertyName);
    }
};

WebInspector.HeapSnapshotRootPath.SpecialPathComponent = {
    InternalPropertyName: "@internal",
};
