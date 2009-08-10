/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

WebInspector.ObjectPropertiesSection = function(object, title, subtitle, emptyPlaceholder, ignoreHasOwnProperty, extraProperties, treeElementConstructor)
{
    this.emptyPlaceholder = (emptyPlaceholder || WebInspector.UIString("No Properties"));
    this.object = object;
    this.ignoreHasOwnProperty = ignoreHasOwnProperty;
    this.extraProperties = extraProperties;
    this.treeElementConstructor = treeElementConstructor || WebInspector.ObjectPropertyTreeElement;
    this.editable = true;

    WebInspector.PropertiesSection.call(this, title, subtitle);
}

WebInspector.ObjectPropertiesSection.prototype = {
    onpopulate: function()
    {
        this.update();
    },

    update: function()
    {
        var self = this;
        var callback = function(properties) {
            if (!properties)
                return;
            self._update(properties);
        };
        InspectorController.getProperties(this.object, this.ignoreHasOwnProperty, callback);
    },

    _update: function(properties)
    {
        if (this.extraProperties)
            for (var prop in this.extraProperties)
                properties.push(new WebInspector.ObjectPropertyProxy(prop, this.extraProperties[prop]));
        properties.sort(this._displaySort);

        this.propertiesTreeOutline.removeChildren();

        for (var i = 0; i < properties.length; ++i)
            this.propertiesTreeOutline.appendChild(new this.treeElementConstructor(properties[i]));

        if (!this.propertiesTreeOutline.children.length) {
            var title = "<div class=\"info\">" + this.emptyPlaceholder + "</div>";
            var infoElement = new TreeElement(title, null, false);
            this.propertiesTreeOutline.appendChild(infoElement);
        }
    },

    _displaySort: function(propertyA, propertyB) {
        var a = propertyA.name;
        var b = propertyB.name;

        // if used elsewhere make sure to
        //  - convert a and b to strings (not needed here, properties are all strings)
        //  - check if a == b (not needed here, no two properties can be the same)

        var diff = 0;
        var chunk = /^\d+|^\D+/;
        var chunka, chunkb, anum, bnum;
        while (diff === 0) {
            if (!a && b)
                return -1;
            if (!b && a)
                return 1;
            chunka = a.match(chunk)[0];
            chunkb = b.match(chunk)[0];
            anum = !isNaN(chunka);
            bnum = !isNaN(chunkb);
            if (anum && !bnum)
                return -1;
            if (bnum && !anum)
                return 1;
            if (anum && bnum) {
                diff = chunka - chunkb;
                if (diff === 0 && chunka.length !== chunkb.length) {
                    if (!+chunka && !+chunkb) // chunks are strings of all 0s (special case)
                        return chunka.length - chunkb.length;
                    else
                        return chunkb.length - chunka.length;
                }
            } else if (chunka !== chunkb)
                return (chunka < chunkb) ? -1 : 1;
            a = a.substring(chunka.length);
            b = b.substring(chunkb.length);
        }
        return diff;
    }
}

WebInspector.ObjectPropertiesSection.prototype.__proto__ = WebInspector.PropertiesSection.prototype;

WebInspector.ObjectPropertyTreeElement = function(property)
{
    this.property = property;

    // Pass an empty title, the title gets made later in onattach.
    TreeElement.call(this, "", null, false);
}

WebInspector.ObjectPropertyTreeElement.prototype = {
    safePropertyValue: function(object, propertyName)
    {
        if (object["__lookupGetter__"] && object.__lookupGetter__(propertyName))
            return;
        return object[propertyName];
    },

    onpopulate: function()
    {
        if (this.children.length && !this.shouldRefreshChildren)
            return;

        var self = this;
        var callback = function(properties) {
            self.removeChildren();
            if (!properties)
                return;

            properties.sort(self._displaySort);
            for (var i = 0; i < properties.length; ++i) {
                self.appendChild(new self.treeOutline.section.treeElementConstructor(properties[i]));
            }
        };
        InspectorController.getProperties(this.property.childObjectProxy, false, callback);
    },

    ondblclick: function(element, event)
    {
        this.startEditing();
    },

    onattach: function()
    {
        this.update();
    },

    update: function()
    {
        var nameElement = document.createElement("span");
        nameElement.className = "name";
        nameElement.textContent = this.property.name;

        this.valueElement = document.createElement("span");
        this.valueElement.className = "value";
        this.valueElement.textContent = this.property.textContent;
        if (this.property.isGetter)
           this.valueElement.addStyleClass("dimmed");

        this.listItemElement.removeChildren();

        this.listItemElement.appendChild(nameElement);
        this.listItemElement.appendChild(document.createTextNode(": "));
        this.listItemElement.appendChild(this.valueElement);
        this.hasChildren = this.property.hasChildren;
    },

    updateSiblings: function()
    {
        if (this.parent.root)
            this.treeOutline.section.update();
        else
            this.parent.shouldRefreshChildren = true;
    },

    startEditing: function()
    {
        if (WebInspector.isBeingEdited(this.valueElement) || !this.treeOutline.section.editable)
            return;

        var context = { expanded: this.expanded };

        // Lie about our children to prevent expanding on double click and to collapse subproperties.
        this.hasChildren = false;

        this.listItemElement.addStyleClass("editing-sub-part");

        WebInspector.startEditing(this.valueElement, this.editingCommitted.bind(this), this.editingCancelled.bind(this), context);
    },

    editingEnded: function(context)
    {
        this.listItemElement.scrollLeft = 0;
        this.listItemElement.removeStyleClass("editing-sub-part");
        if (context.expanded)
            this.expand();
    },

    editingCancelled: function(element, context)
    {
        this.update();
        this.editingEnded(context);
    },

    editingCommitted: function(element, userInput, previousContent, context)
    {
        if (userInput === previousContent)
            return this.editingCancelled(element, context); // nothing changed, so cancel

        this.applyExpression(userInput, true);

        this.editingEnded(context);
    },

    applyExpression: function(expression, updateInterface)
    {
        expression = expression.trimWhitespace();
        var expressionLength = expression.length;
        var self = this;
        var callback = function(success) {
            if (!updateInterface)
                return;

            if (!success)
                self.update();

            if (!expressionLength) {
                // The property was deleted, so remove this tree element.
                self.parent.removeChild(this);
            } else {
                // Call updateSiblings since their value might be based on the value that just changed.
                self.updateSiblings();
            }
        };
        InspectorController.setPropertyValue(this.property.parentObjectProxy, this.property.name, expression.trimWhitespace(), callback);
    }
}

WebInspector.ObjectPropertyTreeElement.prototype.__proto__ = TreeElement.prototype;
