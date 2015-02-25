/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.PropertyPath = function(object, pathComponent, parent, isPrototype)
{
    WebInspector.Object.call(this);

    console.assert(object instanceof WebInspector.RemoteObject || object === null);
    console.assert(!pathComponent || typeof pathComponent === "string");
    console.assert(!parent || parent instanceof WebInspector.PropertyPath);
    console.assert(!parent || pathComponent.length);

    // We allow property pathes with null objects as end-caps only.
    // Disallow appending to a PropertyPath with null objects.
    if (parent && !parent.object)
        throw new Error("Attempted to append to a PropertyPath with null object.");

    this._object = object;
    this._pathComponent = pathComponent || null;
    this._parent = parent || null;
    this._isPrototype = isPrototype || false;
};

WebInspector.PropertyPath.SpecialPathComponent = {
    CollectionIndex: "@collection[?]",
    InternalPropertyName: "@internal",
    SymbolPropertyName: "@symbol",
    GetterPropertyName: "@getter",
};

WebInspector.PropertyPath.prototype = {
    constructor: WebInspector.PropertyPath,
    __proto__: WebInspector.Object.prototype,

    // Public

    get object()
    {
        return this._object;
    },

    get parent()
    {
        return this._parent;
    },

    get isPrototype()
    {
        return this._isPrototype;
    },

    get rootObject()
    {
        return this._parent ? this._parent.rootObject : this._object;
    },

    get lastNonPrototypeObject()
    {
        if (!this._parent)
            return this._object;

        var p = this._parent;
        while (p) {
            if (!p.isPrototype)
                break;
            if (!p.parent)
                break;
            p = p.parent;
        }

        return p.object;
    },

    get pathComponent()
    {
        return this._pathComponent;
    },

    get fullPath()
    {
        var components = [];
        for (var p = this; p && p.pathComponent; p = p.parent)
            components.push(p.pathComponent);
        components.reverse()
        return components.join("");
    },

    isRoot: function()
    {
        return !this._parent;
    },

    isPathComponentImpossible: function()
    {
        return this._pathComponent && this._pathComponent.startsWith("@");
    },

    isFullPathImpossible: function()
    {
        if (this.isPathComponentImpossible())
            return true;

        if (this._parent)
            return this._parent.isFullPathImpossible();

        return false;
    },

    appendPropertyName: function(object, propertyName)
    {
        var isPrototype = propertyName === "__proto__";
        var component = this._canPropertyNameBeDotAccess(propertyName) ? "." + propertyName : "[\"" + propertyName.replace(/"/, "\\\"") + "\"]";
        return new WebInspector.PropertyPath(object, component, this, isPrototype);
    },

    appendPropertySymbol: function(object, symbolName)
    {
        var component = WebInspector.PropertyPath.SpecialPathComponent.SymbolPropertyName + (symbolName.length ? "(" + symbolName + ")" : "");
        return new WebInspector.PropertyPath(object, component, this);
    },

    appendInternalPropertyName: function(object, propertyName)
    {
        var component = WebInspector.PropertyPath.SpecialPathComponent.InternalPropertyName + "[" + propertyName + "]";
        return new WebInspector.PropertyPath(object, component, this);
    },

    appendArrayIndex: function(object, indexString)
    {
        var component = "[" + indexString + "]";
        return new WebInspector.PropertyPath(object, component, this);
    },

    appendCollectionIndex: function(object)
    {
        var component = WebInspector.PropertyPath.SpecialPathComponent.CollectionIndex;
        return new WebInspector.PropertyPath(object, component, this);
    },

    appendPropertyDescriptor: function(object, descriptor)
    {
        if (descriptor.isInternalProperty)
            return this.appendInternalPropertyName(object, descriptor.name);

        // FIXME: We don't yet have Symbol descriptors.

        if (this._object.subtype === "array" && !isNaN(parseInt(descriptor.name)))
            return this.appendArrayIndex(object, descriptor.name);

        return this.appendPropertyName(object, descriptor.name)
    },

    // Private

    _canPropertyNameBeDotAccess: function(propertyName)
    {
        return /^(?![0-9])\w+$/.test(propertyName);
    }
};
