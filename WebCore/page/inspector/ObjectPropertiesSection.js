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

    WebInspector.PropertiesSection.call(this, title, subtitle);
}

WebInspector.ObjectPropertiesSection.prototype = {
    onpopulate: function()
    {
        var properties = [];
        for (var prop in this.object)
            properties.push(prop);
        if (this.extraProperties)
            for (var prop in this.extraProperties)
                properties.push(prop);
        properties.sort();

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

    var childObject = this.safePropertyValue(parentObject, propertyName);
    var isGetter = ("__lookupGetter__" in parentObject && parentObject.__lookupGetter__(propertyName));

    var title = "<span class=\"name\">" + propertyName.escapeHTML() + "</span>: ";
    if (!isGetter)
        title += "<span class=\"value\">" + Object.describe(childObject, true).escapeHTML() + "</span>";
    else
        // FIXME: this should show something like "getter" once we can change localization (bug 16734).
        title += "<span class=\"value dimmed\">&mdash;</span>";

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

    TreeElement.call(this, title, null, hasSubProperties);
}

WebInspector.ObjectPropertyTreeElement.prototype = {
    safePropertyValue: function(object, propertyName)
    {
        if ("__lookupGetter__" in object && object.__lookupGetter__(propertyName))
            return;
        return object[propertyName];
    },

    onpopulate: function()
    {
        if (this.children.length)
            return;

        var childObject = this.safePropertyValue(this.parentObject, this.propertyName);
        var properties = Object.sortedProperties(childObject);
        for (var i = 0; i < properties.length; ++i) {
            var propertyName = properties[i];
            if (propertyName === "__treeElementIdentifier")
                continue;
            this.appendChild(new this.treeOutline.section.treeElementConstructor(childObject, propertyName));
        }
    }
}

WebInspector.ObjectPropertyTreeElement.prototype.__proto__ = TreeElement.prototype;
