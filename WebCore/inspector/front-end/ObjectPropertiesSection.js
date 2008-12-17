/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
    if (!title) {
        title = Object.describe(object);
        if (title.match(/Prototype$/)) {
            title = title.replace(/Prototype$/, "");
            if (!subtitle)
                subtitle = WebInspector.UIString("Prototype");
        }
    }

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
        var properties = [];
        for (var prop in this.object)
            properties.push(prop);
        if (this.extraProperties)
            for (var prop in this.extraProperties)
                properties.push(prop);
        properties.sort();

        this.propertiesTreeOutline.removeChildren();

        for (var i = 0; i < properties.length; ++i) {
            var object = this.object;
            var propertyName = properties[i];
            if (this.extraProperties && propertyName in this.extraProperties)
                object = this.extraProperties;
            if (propertyName === "__treeElementIdentifier")
                continue;
            if (!this.ignoreHasOwnProperty && "hasOwnProperty" in object && !object.hasOwnProperty(propertyName))
                continue;
            this.propertiesTreeOutline.appendChild(new this.treeElementConstructor(object, propertyName));
        }

        if (!this.propertiesTreeOutline.children.length) {
            var title = "<div class=\"info\">" + this.emptyPlaceholder + "</div>";
            var infoElement = new TreeElement(title, null, false);
            this.propertiesTreeOutline.appendChild(infoElement);
        }
    }
}

WebInspector.ObjectPropertiesSection.prototype.__proto__ = WebInspector.PropertiesSection.prototype;

WebInspector.ObjectPropertyTreeElement = function(parentObject, propertyName)
{
    this.parentObject = parentObject;
    this.propertyName = propertyName;

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

        this.removeChildren();

        var childObject = this.safePropertyValue(this.parentObject, this.propertyName);
        var properties = Object.sortedProperties(childObject);
        for (var i = 0; i < properties.length; ++i) {
            var propertyName = properties[i];
            if (propertyName === "__treeElementIdentifier")
                continue;
            this.appendChild(new this.treeOutline.section.treeElementConstructor(childObject, propertyName));
        }
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
        var childObject = this.safePropertyValue(this.parentObject, this.propertyName);
        var isGetter = ("__lookupGetter__" in this.parentObject && this.parentObject.__lookupGetter__(this.propertyName));

        var nameElement = document.createElement("span");
        nameElement.className = "name";
        nameElement.textContent = this.propertyName;

        this.valueElement = document.createElement("span");
        this.valueElement.className = "value";
        if (!isGetter) {
            this.valueElement.textContent = Object.describe(childObject, true);
        } else {
            // FIXME: this should show something like "getter" (bug 16734).
            this.valueElement.textContent = "\u2014"; // em dash
            this.valueElement.addStyleClass("dimmed");
        }

        this.listItemElement.removeChildren();

        this.listItemElement.appendChild(nameElement);
        this.listItemElement.appendChild(document.createTextNode(": "));
        this.listItemElement.appendChild(this.valueElement);

        var hasSubProperties = false;
        var type = typeof childObject;
        if (childObject && (type === "object" || type === "function")) {
            for (subPropertyName in childObject) {
                if (subPropertyName === "__treeElementIdentifier")
                    continue;
                hasSubProperties = true;
                break;
            }
        }

        this.hasChildren = hasSubProperties;
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

    evaluateExpression: function(expression)
    {
        // Evaluate in the currently selected call frame if the debugger is paused.
        // Otherwise evaluate in against the inspected window.
        if (WebInspector.panels.scripts && WebInspector.panels.scripts.paused && this.treeOutline.section.editInSelectedCallFrameWhenPaused)
            return WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression, false);
        return InspectorController.inspectedWindow().eval(expression);
    },

    applyExpression: function(expression, updateInterface)
    {
        var expressionLength = expression.trimWhitespace().length;

        if (!expressionLength) {
            // The user deleted everything, so try to delete the property.
            delete this.parentObject[this.propertyName];

            if (updateInterface) {
                if (this.propertyName in this.parentObject) {
                    // The property was not deleted, so update.
                    this.update();
                } else {
                    // The property was deleted, so remove this tree element.
                    this.parent.removeChild(this);
                }
            }

            return;
        }

        try {
            // Surround the expression in parenthesis so the result of the eval is the result
            // of the whole expression not the last potential sub-expression.
            var result = this.evaluateExpression("(" + expression + ")");

            // Store the result in the property.
            this.parentObject[this.propertyName] = result;
        } catch(e) {
            try {
                // Try to update as a string
                var result = this.evaluateExpression("\"" + expression.escapeCharacters("\"") + "\"");

                // Store the result in the property.
                this.parentObject[this.propertyName] = result;
            } catch(e) {
                // The expression failed so don't change the value. So just update and return.
                if (updateInterface)
                    this.update();
                return;
            }
        }

        if (updateInterface) {
            // Call updateSiblings since their value might be based on the value that just changed.
            this.updateSiblings();
        }
    }
}

WebInspector.ObjectPropertyTreeElement.prototype.__proto__ = TreeElement.prototype;
