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

WebInspector.ObjectTreePropertyTreeElement = function(property, mode)
{
    console.assert(property instanceof WebInspector.PropertyDescriptor);

    this._property = property;
    this._mode = mode || WebInspector.ObjectTreeView.Mode.Properties;

    // FIXME: API Mode not turned on yet.
    this._mode = WebInspector.ObjectTreeView.Mode.Properties;

    TreeElement.call(this, "", null, false);
    this.toggleOnClick = true;
    this.selectable = false;
};

WebInspector.ObjectTreePropertyTreeElement.prototype = {
    constructor: WebInspector.ObjectTreePropertyTreeElement,
    __proto__: TreeElement.prototype,

    // Public

    get property()
    {
        return this._property;
    },

    // Protected

    onpopulate: function()
    {
        this._updateChildren();
    },

    onattach: function()
    {
        this.listItemElement.classList.add("object-tree-property");

        this._updateTitle();
    },

    // Private

    _updateTitle: function()
    {
        this.listItemElement.removeChildren();

        if (this._mode === WebInspector.ObjectTreeView.Mode.Properties) {
            this._updateTitlePropertyStyle();
            this.hasChildren = this._property.hasValue() && this._property.value.hasChildren && !this._property.wasThrown;
        } else {
            this._updateTitleAPIStyle();
            this.hasChildren = this._property.hasValue() && this._property.value.hasChildren && !this._property.wasThrown && this._property.name === "__proto__";
        }
    },

    _updateTitlePropertyStyle: function()
    {
        // Property name.
        var nameElement = document.createElement("span");
        nameElement.className = "name";
        nameElement.textContent = this._property.name;

        // Property attributes.
        if (this._mode === WebInspector.ObjectTreeView.Mode.Properties) {
            if (!this._property.enumerable)
                nameElement.classList.add("not-enumerable");
        }

        // Separator.
        var separatorElement = document.createElement("span");
        separatorElement.className = "separator";
        separatorElement.textContent = ": ";

        // Value / Getter.
        var valueOrGetterElement = document.createElement("span");
        valueOrGetterElement.className = "value";

        if (this._property.hasValue()) {
            valueOrGetterElement.textContent = this._descriptionString();
            valueOrGetterElement.classList.add(WebInspector.ObjectTreeView.classNameForObject(this._property.value));
            // FIXME: Context Menu for Value. (See ObjectPropertiesSection).
            // FIXME: Option+Click for Value.
        } else {
            console.assert(this._property.hasGetter());
            valueOrGetterElement.textContent = "(...)";
            // FIXME: Click to Populate Value.
            // FIXME: Context Menu to Populate Value.
        }

        if (this._property.wasThrown)
            valueOrGetterElement.classList.add("error");

        this.listItemElement.appendChild(nameElement);
        this.listItemElement.appendChild(separatorElement);
        this.listItemElement.appendChild(valueOrGetterElement);
    },

    _updateTitleAPIStyle: function()
    {
        // Fixed value. Display like a property.
        const propertyNamesToDisplayAsValues = ["__proto__", "constructor"];
        if (propertyNamesToDisplayAsValues.contains(this._property.name) || (this._property.hasValue() && this._property.value.type !== "function")) {
            this._updateTitlePropertyStyle();
            return;
        }

        // No API to display.
        var isFunction = this._property.hasValue() && this._property.value.type === "function";
        if (!isFunction && !this._property.hasGetter() && !this._property.hasSetter())
            return;

        // Function / Getter / Setter.
        var nameElement = document.createElement("span");
        nameElement.className = "name";
        nameElement.textContent = this._property.name;
        this.listItemElement.appendChild(nameElement);

        if (isFunction) {
            var paramElement = document.createElement("span");
            paramElement.textContent = this._functionParameterString();
            this.listItemElement.appendChild(paramElement);
        }

        if (this._property.hasGetter()) {
            var icon = document.createElement("span");
            icon.textContent += "[G]";
            this.listItemElement.appendChild(icon);
        }
        if (this._property.hasSetter()) {
            var icon = document.createElement("span");
            icon.textContent += "[S]";
            this.listItemElement.appendChild(icon);
        }
    },

    _descriptionString: function()
    {
        var value = this._property.value;
        var description = value.description;

        // Exception.
        if (this._property.wasThrown)
            return "[Exception: " + description + "]";

        // String: replace newlines as nice unicode symbols.
        if (value.type === "string")
            return "\"" + description.replace(/\n/g, "\u21B5").replace(/"/g, "\\\"") + "\"";

        // Function: Collapse whitespace in function display strings.
        if (value.type === "function")
            return /.*/.exec(description)[0].replace(/ +$/g, "");

        return description;
    },

    _functionParameterString: function()
    {
        console.assert(this._property.value.type === "function");

        var match = this._property.value.description.match(/^function.*?(\([^)]+?\))/);
        return match ? match[1] : "()";
    },

    _updateChildren: function()
    {
        if (this.children.length && !this.shouldRefreshChildren)
            return;

        function callback(mode, properties)
        {
            this.removeChildren();

            if (properties) {
                properties.sort(WebInspector.ObjectTreeView.ComparePropertyDescriptors);
                for (var propertyDescriptor of properties)
                    this.appendChild(new WebInspector.ObjectTreePropertyTreeElement(propertyDescriptor, mode));
            }

            if (mode === WebInspector.ObjectTreeView.Mode.Properties) {
                if (this._property.value.isCollectionType())
                    this.appendChild(new WebInspector.ObjectTreeCollectionTreeElement(this._property.value));
            }
        };

        if (this._property.name === "__proto__")
            this._property.value.getOwnPropertyDescriptors(callback.bind(this, WebInspector.ObjectTreeView.Mode.API));
        else
            this._property.value.getOwnAndGetterPropertyDescriptors(callback.bind(this, this._mode));
    }
};
