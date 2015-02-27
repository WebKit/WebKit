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

// FIXME: This should share more code with ObjectTreePropertyTreeElement.

WebInspector.ObjectTreeArrayIndexTreeElement = function(property, propertyPath)
{
    console.assert(property instanceof WebInspector.PropertyDescriptor);
    console.assert(propertyPath instanceof WebInspector.PropertyPath);
    console.assert(property.isIndexProperty(), "ArrayIndexTreeElement expects numeric property names");

    this._property = property;
    this._propertyPath = propertyPath;

    var classNames = ["object-tree-property", "object-tree-array-index"];
    if (!this._property.hasValue())
        classNames.push("accessor");

    WebInspector.GeneralTreeElement.call(this, classNames, this._titleFragment(), null, this._property, false);

    this.small = true;
    this.toggleOnClick = false;
    this.selectable = false;
    this.tooltipHandledSeparately = true;
    this.hasChildren = false;
};

WebInspector.ObjectTreeArrayIndexTreeElement.prototype = {
    constructor: WebInspector.ObjectTreeArrayIndexTreeElement,
    __proto__: WebInspector.GeneralTreeElement.prototype,

    // Public

    get property()
    {
        return this._property;
    },

    // Private

    _resolvedValue: function()
    {
        if (this._getterValue)
            return this._getterValue;
        if (this._property.hasValue())
            return this._property.value;
        return null;
    },

    _propertyPathType: function()
    {
        if (this._getterValue || this._property.hasValue())
            return WebInspector.PropertyPath.Type.Value;
        if (this._property.hasGetter())
            return WebInspector.PropertyPath.Type.Getter;
        if (this._property.hasSetter())
            return WebInspector.PropertyPath.Type.Setter;
        return WebInspector.PropertyPath.Type.Value;
    },

    _resolvedValuePropertyPath: function()
    {
        if (this._getterValue)
            return this._propertyPath.appendPropertyDescriptor(this._getterValue, this._property, WebInspector.PropertyPath.Type.Value);
        if (this._property.hasValue())
            return this._propertyPath.appendPropertyDescriptor(this._property.value, this._property, WebInspector.PropertyPath.Type.Value);
        return null;
    },

    _thisPropertyPath: function()
    {
        return this._propertyPath.appendPropertyDescriptor(null, this._property, this._propertyPathType());
    },

    _propertyPathString: function(propertyPath)
    {
        if (propertyPath.isFullPathImpossible())
            return WebInspector.UIString("Unable to determine path to property from root");

        return propertyPath.displayPath(this._propertyPathType());
    },

    _updateTitle: function()
    {
        this.mainTitle = this._titleFragment();

        if (this._getterValue)
            this.removeClassName("accessor");

        this._updateHasChildren();
    },

    _titleFragment: function()
    {
        var container = document.createDocumentFragment();

        // Array index name.
        var nameElement = container.appendChild(document.createElement("span"));
        nameElement.className = "index-name";
        nameElement.textContent = this._property.name;
        nameElement.title = this._propertyPathString(this._thisPropertyPath());

        // Value.
        var valueElement = container.appendChild(document.createElement("span"));
        valueElement.className = "index-value";

        var resolvedValue = this._resolvedValue();
        if (resolvedValue)
            valueElement.appendChild(WebInspector.FormattedValue.createObjectTreeOrFormattedValueForRemoteObject(resolvedValue, this._resolvedValuePropertyPath()));
        else {
            if (this._property.hasGetter())
                container.appendChild(this._createInteractiveGetterElement());
            if (!this._property.hasSetter())
                container.appendChild(this._createReadOnlyIconElement());
            // FIXME: What if just a setter?
        }

        valueElement.classList.add("value");
        if (this._property.wasThrown || this._getterHadError)
            valueElement.classList.add("error");

        return container;
    },

    _createInteractiveGetterElement: function()
    {
        var getterElement = document.createElement("img");
        getterElement.className = "getter";
        getterElement.title = WebInspector.UIString("Invoke getter");

        getterElement.addEventListener("click", function(event) {
            event.stopPropagation();
            var lastNonPrototypeObject = this._propertyPath.lastNonPrototypeObject;
            var getterObject = this._property.get;
            lastNonPrototypeObject.invokeGetter(getterObject, function(error, result, wasThrown) {
                this._getterHadError = !!(error || wasThrown);
                this._getterValue = result;
                this._updateTitle();
            }.bind(this));
        }.bind(this));

        return getterElement;
    },

    _createReadOnlyIconElement: function()
    {
        var readOnlyElement = document.createElement("img");
        readOnlyElement.className = "read-only";
        readOnlyElement.title = WebInspector.UIString("Read only");
        return readOnlyElement;
    },
};
