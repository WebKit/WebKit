/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WebInspector.TypePropertiesSection = function(types, title, subtitle)
{
    this.emptyPlaceholder = WebInspector.UIString("No Properties");
    this.types = types;

    WebInspector.PropertiesSection.call(this, title, subtitle);
};

WebInspector.TypePropertiesSection.prototype = {
    constructor: WebInspector.TypePropertiesSection,
    __proto__: WebInspector.PropertiesSection.prototype,

    onpopulate: function()
    {
        this.propertiesTreeOutline.removeChildren();

        var primitiveTypeNames = this.types.primitiveTypeNames;
        var structures = this.types.structures;
        var properties = [];
        for (var struct of structures) {
            properties.push({
                name: struct.constructorName,
                structure: struct
            });
        }
        for (var primitiveName of primitiveTypeNames) {
            properties.push({
                name: primitiveName,
                structure: null
            });
        }

        properties.sort(WebInspector.TypePropertiesSection.PropertyComparator);

        if (this.types.isTruncated)
            properties.push({name: "\u2026", structure: null});

        for (var property of properties)
            this.propertiesTreeOutline.appendChild(new WebInspector.TypePropertyTreeElement(property));

        if (!this.propertiesTreeOutline.children.length) {
            var title = document.createElement("div");
            title.className = "info";
            title.textContent = this.emptyPlaceholder;
            var infoElement = new TreeElement(title, null, false);
            this.propertiesTreeOutline.appendChild(infoElement);
        }

        this.dispatchEventToListeners(WebInspector.Section.Event.VisibleContentDidChange);

        if (properties.length === 1)
            this.propertiesTreeOutline.children[0].expandRecursively();
    }
};

// This is mostly identical to ObjectPropertiesSection.compareProperties.
// But this checks for equality because we can have two objects named the same thing.
WebInspector.TypePropertiesSection.PropertyComparator = function(propertyA, propertyB)
{
    var a = propertyA.name;
    var b = propertyB.name;
    if (a.indexOf("__proto__") !== -1)
        return 1;
    if (b.indexOf("__proto__") !== -1)
        return -1;
    if (a === b)
        return 1;

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
};

WebInspector.TypePropertyTreeElement = function(property)
{
    this.property = property;

    this.nameElement = document.createElement("span");
    this.nameElement.className = "name";
    this.nameElement.textContent = this.property.name;

    TreeElement.call(this, this.nameElement, null, false);

    this.toggleOnClick = true;
    this.hasChildren = !!this.property.structure;
};

WebInspector.TypePropertyTreeElement.prototype = {
    constructor: WebInspector.TypePropertyTreeElement,
    __proto__: TreeElement.prototype,

    onpopulate: function()
    {
        this.removeChildren();

        var properties = [];
        for (var fieldName of this.property.structure.fields) {
            properties.push({
                name: fieldName,
                structure: null
            });
        }

        properties.sort(WebInspector.TypePropertiesSection.PropertyComparator);

        for (var property of properties)
            this.appendChild(new WebInspector.TypePropertyTreeElement(property));

        properties = [];
        for (var fieldName of this.property.structure.optionalFields) {
            properties.push({
                name: fieldName + "?",
                structure: null
            });
        }

        properties.sort(WebInspector.TypePropertiesSection.PropertyComparator);

        if (this.property.structure.isImprecise)
            properties.push({name: "\u2026", structure: null});

        if (this.property.structure.prototypeStructure) {
            properties.push({
                name: this.property.structure.prototypeStructure.constructorName + " (" + WebInspector.UIString("Prototype") + ")",
                structure: this.property.structure.prototypeStructure
            });
        }

        for (var property of properties)
            this.appendChild(new WebInspector.TypePropertyTreeElement(property));
    }
};

