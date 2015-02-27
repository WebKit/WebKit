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

WebInspector.ObjectTreeMapEntryTreeElement = function(object, propertyPath)
{
    console.assert(object instanceof WebInspector.RemoteObject);
    console.assert(propertyPath instanceof WebInspector.PropertyPath);

    this._object = object;
    this._propertyPath = propertyPath;

    // Treat the same as an array-index just with different strings and widths.
    WebInspector.GeneralTreeElement.call(this, ["object-tree-array-index", "object-tree-map-entry"], this._titleFragment(), null, this._object, false);

    this.small = true;
    this.toggleOnClick = false;
    this.selectable = false;
    this.tooltipHandledSeparately = true;
    this.hasChildren = false;
};

WebInspector.ObjectTreeMapEntryTreeElement.prototype = {
    constructor: WebInspector.ObjectTreeMapEntryTreeElement,
    __proto__: WebInspector.GeneralTreeElement.prototype,

    // Public

    get object()
    {
        return this._object;
    },

    // Private

    _propertyPathString: function(propertyPath)
    {
        if (propertyPath.isFullPathImpossible())
            return WebInspector.UIString("Unable to determine path to property from root");

        return propertyPath.displayPath(WebInspector.PropertyPath.Type.Value);
    },

    _titleFragment: function()
    {
        var container = document.createDocumentFragment();

        var propertyPath = this.resolvedValuePropertyPath();

        // Index name.
        var nameElement = container.appendChild(document.createElement("span"));
        nameElement.className = "index-name";
        nameElement.textContent = this.displayPropertyName();
        nameElement.title = this._propertyPathString(propertyPath);

        // Value.
        var valueElement = container.appendChild(document.createElement("span"));
        valueElement.className = "index-value";
        valueElement.appendChild(WebInspector.FormattedValue.createObjectTreeOrFormattedValueForRemoteObject(this._object, propertyPath));

        return container;
    }
};


WebInspector.ObjectTreeMapKeyTreeElement = function(object, propertyPath)
{
    WebInspector.ObjectTreeMapEntryTreeElement.call(this, object, propertyPath);
    this.addClassName("key");
}

WebInspector.ObjectTreeMapKeyTreeElement.prototype = {
    constructor: WebInspector.ObjectTreeMapKeyTreeElement,
    __proto__: WebInspector.ObjectTreeMapEntryTreeElement.prototype,

    // Protected

    displayPropertyName: function()
    {
        return WebInspector.UIString("key");
    },

    resolvedValuePropertyPath: function()
    {
        return this._propertyPath.appendMapKey(this._object);
    }
};


WebInspector.ObjectTreeMapValueTreeElement = function(object, propertyPath, key)
{
    this._key = key;
    WebInspector.ObjectTreeMapEntryTreeElement.call(this, object, propertyPath);
    this.addClassName("value");
}

WebInspector.ObjectTreeMapValueTreeElement.prototype = {
    constructor: WebInspector.ObjectTreeMapValueTreeElement,
    __proto__: WebInspector.ObjectTreeMapEntryTreeElement.prototype,

    // Protected

    displayPropertyName: function()
    {
        return WebInspector.UIString("value");
    },

    resolvedValuePropertyPath: function()
    {
        return this._propertyPath.appendMapValue(this._object, this._key);
    }
};
