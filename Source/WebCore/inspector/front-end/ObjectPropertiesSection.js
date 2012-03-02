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

/**
 * @constructor
 * @extends {WebInspector.PropertiesSection}
 * @param {*=} object
 * @param {string=} title
 * @param {string=} subtitle
 * @param {string=} emptyPlaceholder
 * @param {boolean=} ignoreHasOwnProperty
 * @param {Array.<WebInspector.RemoteObjectProperty>=} extraProperties
 * @param {function()=} treeElementConstructor
 */
WebInspector.ObjectPropertiesSection = function(object, title, subtitle, emptyPlaceholder, ignoreHasOwnProperty, extraProperties, treeElementConstructor)
{
    this.emptyPlaceholder = (emptyPlaceholder || WebInspector.UIString("No Properties"));
    this.object = object;
    this.ignoreHasOwnProperty = ignoreHasOwnProperty;
    this.extraProperties = extraProperties;
    this.treeElementConstructor = treeElementConstructor || WebInspector.ObjectPropertyTreeElement;
    this.editable = true;
    this.skipProto = false;

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
        function callback(properties)
        {
            if (!properties)
                return;
            self.updateProperties(properties);
        }
        if (this.ignoreHasOwnProperty)
            this.object.getAllProperties(callback);
        else
            this.object.getOwnProperties(callback);
    },

    updateProperties: function(properties, rootTreeElementConstructor, rootPropertyComparer)
    {
        if (!rootTreeElementConstructor)
            rootTreeElementConstructor = this.treeElementConstructor;

        if (!rootPropertyComparer)
            rootPropertyComparer = WebInspector.ObjectPropertiesSection.CompareProperties;

        if (this.extraProperties)
            for (var i = 0; i < this.extraProperties.length; ++i)
                properties.push(this.extraProperties[i]);

        properties.sort(rootPropertyComparer);

        this.propertiesTreeOutline.removeChildren();

        for (var i = 0; i < properties.length; ++i) {
            if (this.skipProto && properties[i].name === "__proto__")
                continue;
            properties[i].parentObject = this.object;
        }

        this.propertiesForTest = properties;

        if (this.object && this.object.arrayLength() > WebInspector.ArrayGroupingTreeElement._bucketThreshold) {
            WebInspector.ArrayGroupingTreeElement.populateAsArray(this.propertiesTreeOutline, rootTreeElementConstructor, properties);
            return;
        } 

        for (var i = 0; i < properties.length; ++i)
            this.propertiesTreeOutline.appendChild(new rootTreeElementConstructor(properties[i]));

        if (!this.propertiesTreeOutline.children.length) {
            var title = document.createElement("div");
            title.className = "info";
            title.textContent = this.emptyPlaceholder;
            var infoElement = new TreeElement(title, null, false);
            this.propertiesTreeOutline.appendChild(infoElement);
        }
    }
}

WebInspector.ObjectPropertiesSection.prototype.__proto__ = WebInspector.PropertiesSection.prototype;

WebInspector.ObjectPropertiesSection.CompareProperties = function(propertyA, propertyB)
{
    var a = propertyA.name;
    var b = propertyB.name;
    if (a === "__proto__")
        return 1;
    if (b === "__proto__")
        return -1;

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

/**
 * @constructor
 * @extends {TreeElement}
 */
WebInspector.ObjectPropertyTreeElement = function(property)
{
    this.property = property;

    // Pass an empty title, the title gets made later in onattach.
    TreeElement.call(this, "", null, false);
    this.toggleOnClick = true;
    this.selectable = false;
}

WebInspector.ObjectPropertyTreeElement.prototype = {
    onpopulate: function()
    {
        if (this.children.length && !this.shouldRefreshChildren)
            return;

        var callback = function(properties) {
            this.removeChildren();
            if (!properties)
                return;

            if (this.property.value.type === "array") {
                WebInspector.ArrayGroupingTreeElement.populateAsArray(this, this.treeOutline.section.treeElementConstructor, properties);
                return;
            }

            properties.sort(WebInspector.ObjectPropertiesSection.CompareProperties);
            for (var i = 0; i < properties.length; ++i) {
                if (this.treeOutline.section.skipProto && properties[i].name === "__proto__")
                    continue;
                properties[i].parentObject = this.property.value;
                this.appendChild(new this.treeOutline.section.treeElementConstructor(properties[i]));
            }
        };
        this.property.value.getOwnProperties(callback.bind(this));
    },

    ondblclick: function(event)
    {
        if (this.property.writable)
            this.startEditing(event);
    },

    onattach: function()
    {
        this.update();
    },

    update: function()
    {
        this.nameElement = document.createElement("span");
        this.nameElement.className = "name";
        this.nameElement.textContent = this.property.name;
        if (!this.property.enumerable)
            this.nameElement.addStyleClass("dimmed");

        var separatorElement = document.createElement("span");
        separatorElement.className = "separator";
        separatorElement.textContent = ": ";

        this.valueElement = document.createElement("span");
        this.valueElement.className = "value";

        var description = this.property.value.description;
        // Render \n as a nice unicode cr symbol.
        if (this.property.wasThrown)
            this.valueElement.textContent = "[Exception: " + description + "]";
        else if (this.property.value.type === "string" && typeof description === "string") {
            this.valueElement.textContent = "\"" + description.replace(/\n/g, "\u21B5") + "\"";
            this.valueElement._originalTextContent = "\"" + description + "\"";
        } else if (this.property.value.type === "function" && typeof description === "string") {
            this.valueElement.textContent = /.*/.exec(description)[0].replace(/ +$/g, "");
            this.valueElement._originalTextContent = description;
        } else
            this.valueElement.textContent = description;

        if (this.property.value.type === "function")
            this.valueElement.addEventListener("contextmenu", this._functionContextMenuEventFired.bind(this), false);

        if (this.property.wasThrown)
            this.valueElement.addStyleClass("error");
        if (this.property.value.subtype)
            this.valueElement.addStyleClass("console-formatted-" + this.property.value.subtype);
        else if (this.property.value.type)
            this.valueElement.addStyleClass("console-formatted-" + this.property.value.type);
        if (this.property.value.subtype === "node")
            this.valueElement.addEventListener("contextmenu", this._contextMenuEventFired.bind(this), false);

        this.listItemElement.removeChildren();

        this.listItemElement.appendChild(this.nameElement);
        this.listItemElement.appendChild(separatorElement);
        this.listItemElement.appendChild(this.valueElement);
        this.hasChildren = this.property.value.hasChildren && !this.property.wasThrown;
    },

    _contextMenuEventFired: function(event)
    {
        function selectNode(nodeId)
        {
            if (nodeId)
                WebInspector.domAgent.inspectElement(nodeId);
        }

        function revealElement()
        {
            this.property.value.pushNodeToFrontend(selectNode);
        }

        var contextMenu = new WebInspector.ContextMenu();
        contextMenu.appendItem(WebInspector.UIString("Reveal in Elements Panel"), revealElement.bind(this));
        contextMenu.show(event);
    },

    _functionContextMenuEventFired: function(event)
    {
        function didGetDetails(error, response)
        {
            if (error) {
                console.error(error);
                return;
            }
            WebInspector.panels.scripts.showFunctionDefinition(response.location);
        }

        function revealFunction()
        {
            DebuggerAgent.getFunctionDetails(this.property.value.objectId, didGetDetails.bind(this));
        }

        var contextMenu = new WebInspector.ContextMenu();
        contextMenu.appendItem(WebInspector.UIString("Show function definition"), revealFunction.bind(this));
        contextMenu.show(event);
    },

    updateSiblings: function()
    {
        if (this.parent.root)
            this.treeOutline.section.update();
        else
            this.parent.shouldRefreshChildren = true;
    },

    renderPromptAsBlock: function()
    {
        return false;
    },

    /**
     * @param {Event=} event
     */
    elementAndValueToEdit: function(event)
    {
        return [this.valueElement, (typeof this.valueElement._originalTextContent === "string") ? this.valueElement._originalTextContent : undefined];
    },

    startEditing: function(event)
    {
        var elementAndValueToEdit = this.elementAndValueToEdit(event);
        var elementToEdit = elementAndValueToEdit[0];
        var valueToEdit = elementAndValueToEdit[1];

        if (WebInspector.isBeingEdited(elementToEdit) || !this.treeOutline.section.editable || this._readOnly)
            return;

        // Edit original source.
        if (typeof valueToEdit !== "undefined")
            elementToEdit.textContent = valueToEdit;

        var context = { expanded: this.expanded, elementToEdit: elementToEdit, previousContent: elementToEdit.textContent };

        // Lie about our children to prevent expanding on double click and to collapse subproperties.
        this.hasChildren = false;

        this.listItemElement.addStyleClass("editing-sub-part");

        this._prompt = new WebInspector.ObjectPropertyPrompt(this.editingCommitted.bind(this, null, elementToEdit.textContent, context.previousContent, context), this.editingCancelled.bind(this, null, context), this.renderPromptAsBlock());

        function blurListener()
        {
            this.editingCommitted(null, elementToEdit.textContent, context.previousContent, context);
        }

        var proxyElement = this._prompt.attachAndStartEditing(elementToEdit, blurListener.bind(this));
        window.getSelection().setBaseAndExtent(elementToEdit, 0, elementToEdit, 1);
        proxyElement.addEventListener("keydown", this._promptKeyDown.bind(this, context), false);
    },

    editingEnded: function(context)
    {
        this._prompt.detach();
        delete this._prompt;

        this.listItemElement.scrollLeft = 0;
        this.listItemElement.removeStyleClass("editing-sub-part");
        if (context.expanded)
            this.expand();
    },

    editingCancelled: function(element, context)
    {
        this.editingEnded(context);
        this.update();
    },

    editingCommitted: function(element, userInput, previousContent, context)
    {
        if (userInput === previousContent)
            return this.editingCancelled(element, context); // nothing changed, so cancel

        this.editingEnded(context);
        this.applyExpression(userInput, true);
    },

    _promptKeyDown: function(context, event)
    {
        if (isEnterKey(event)) {
            event.stopPropagation();
            event.preventDefault();
            return this.editingCommitted(null, context.elementToEdit.textContent, context.previousContent, context);
        }
        if (event.keyIdentifier === "U+001B") { // Esc
            event.stopPropagation();
            return this.editingCancelled(null, context);
        }
    },

    applyExpression: function(expression, updateInterface)
    {
        expression = expression.trim();
        var expressionLength = expression.length;
        function callback(error)
        {
            if (!updateInterface)
                return;

            if (error)
                this.update();

            if (!expressionLength) {
                // The property was deleted, so remove this tree element.
                this.parent.removeChild(this);
            } else {
                // Call updateSiblings since their value might be based on the value that just changed.
                this.updateSiblings();
            }
        };
        this.property.parentObject.setPropertyValue(this.property.name, expression.trim(), callback.bind(this));
    }
}

WebInspector.ObjectPropertyTreeElement.prototype.__proto__ = TreeElement.prototype;

/**
 * @constructor
 * @extends {TreeElement}
 */
WebInspector.ArrayGroupingTreeElement = function(treeElementConstructor, properties, fromIndex, toIndex)
{
    this._properties = properties;
    this._fromIndex = fromIndex;
    this._toIndex = toIndex;
    this._treeElementConstructor = treeElementConstructor;

    TreeElement.call(this, "[" + this._properties[fromIndex].name + " \u2026 " + this._properties[toIndex - 1].name + "]", null, true);
}

WebInspector.ArrayGroupingTreeElement._bucketThreshold = 20;

WebInspector.ArrayGroupingTreeElement.populateAsArray = function(parentTreeElement, treeElementConstructor, properties)
{
    var nonIndexProperties = [];

    for (var i = properties.length - 1; i >= 0; --i) {
        if (!isNaN(properties[i].name)) {
            // We've reached index properties, trim properties and break.
            properties.length = i + 1;
            break;
        }

        nonIndexProperties.push(properties[i]);
    }

    WebInspector.ArrayGroupingTreeElement._populate(parentTreeElement, treeElementConstructor, properties, 0, properties.length);

    nonIndexProperties.sort(WebInspector.ObjectPropertiesSection.CompareProperties);
    for (var i = 0; i < nonIndexProperties.length; ++i) {
        var treeElement = new treeElementConstructor(nonIndexProperties[i]);
        treeElement._readOnly = true; 
        parentTreeElement.appendChild(treeElement);
    }
}

WebInspector.ArrayGroupingTreeElement._populate = function(parentTreeElement, treeElementConstructor, properties, fromIndex, toIndex)
{
    parentTreeElement.removeChildren();
    const bucketThreshold = WebInspector.ArrayGroupingTreeElement._bucketThreshold;
    var range = toIndex - fromIndex;

    function appendElement(i)
    {
        var treeElement = new treeElementConstructor(properties[i]);
        treeElement._readOnly = true;
        parentTreeElement.appendChild(treeElement);
    }

    if (range <= bucketThreshold) {
        for (var i = fromIndex; i < toIndex; ++i)
            appendElement(i);
        return;
    }

    var bucketSize = Math.ceil(range / bucketThreshold);
    if (bucketSize < bucketThreshold)
        bucketSize = Math.floor(Math.sqrt(range));

    for (var i = fromIndex; i < toIndex; i += bucketSize) {
        var to = Math.min(i + bucketSize, toIndex);
        if (to - i > 1) {
            var group = new WebInspector.ArrayGroupingTreeElement(treeElementConstructor, properties, i, to);
            parentTreeElement.appendChild(group);
        } else
            appendElement(i);
    }
}

WebInspector.ArrayGroupingTreeElement.prototype = {
    onpopulate: function()
    {
        WebInspector.ArrayGroupingTreeElement._populate(this, this._treeElementConstructor, this._properties, this._fromIndex, this._toIndex);
    }
}

WebInspector.ArrayGroupingTreeElement.prototype.__proto__ = TreeElement.prototype;

/**
 * @constructor
 * @extends {WebInspector.TextPrompt}
 * @param {boolean=} renderAsBlock
 */
WebInspector.ObjectPropertyPrompt = function(commitHandler, cancelHandler, renderAsBlock)
{
    const ExpressionStopCharacters = " =:[({;,!+-*/&|^<>."; // Same as in ConsoleView.js + "."
    WebInspector.TextPrompt.call(this, WebInspector.consoleView.completionsForTextPrompt.bind(WebInspector.consoleView), ExpressionStopCharacters);
    this.setSuggestBoxEnabled("generic-suggest");
    if (renderAsBlock)
        this.renderAsBlock();
}

WebInspector.ObjectPropertyPrompt.prototype.__proto__ = WebInspector.TextPrompt.prototype;
