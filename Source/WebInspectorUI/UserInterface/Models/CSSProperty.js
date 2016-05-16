/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.CSSProperty = class CSSProperty extends WebInspector.Object
{
    constructor(index, text, name, value, priority, enabled, overridden, implicit, anonymous, valid, styleSheetTextRange)
    {
        super();

        this._ownerStyle = null;
        this._index = index;

        this.update(text, name, value, priority, enabled, overridden, implicit, anonymous, valid, styleSheetTextRange, true);
    }

    // Static

    static isInheritedPropertyName(name)
    {
        console.assert(typeof name === "string");
        if (name in WebInspector.CSSKeywordCompletions.InheritedProperties)
            return true;
        // Check if the name is a CSS variable.
        return name.startsWith("--");
    }

    // Public

    get ownerStyle()
    {
        return this._ownerStyle;
    }

    set ownerStyle(ownerStyle)
    {
        this._ownerStyle = ownerStyle || null;
    }

    get index()
    {
        return this._index;
    }

    set index(index)
    {
        this._index = index;
    }

    update(text, name, value, priority, enabled, overridden, implicit, anonymous, valid, styleSheetTextRange, dontFireEvents)
    {
        text = text || "";
        name = name || "";
        value = value || "";
        priority = priority || "";
        enabled = enabled || false;
        overridden = overridden || false;
        implicit = implicit || false;
        anonymous = anonymous || false;
        valid = valid || false;

        var changed = false;

        if (!dontFireEvents) {
            changed = this._name !== name || this._value !== value || this._priority !== priority ||
                this._enabled !== enabled || this._implicit !== implicit || this._anonymous !== anonymous || this._valid !== valid;
        }

        // Use the setter for overridden if we want to fire events since the
        // OverriddenStatusChanged event coalesces changes before it fires.
        if (!dontFireEvents)
            this.overridden = overridden;
        else
            this._overridden = overridden;

        this._text = text;
        this._name = name;
        this._value = value;
        this._priority = priority;
        this._enabled = enabled;
        this._implicit = implicit;
        this._anonymous = anonymous;
        this._inherited = WebInspector.CSSProperty.isInheritedPropertyName(name);
        this._valid = valid;
        this._styleSheetTextRange = styleSheetTextRange || null;

        this._relatedShorthandProperty = null;
        this._relatedLonghandProperties = [];

        // Clear computed properties.
        delete this._styleDeclarationTextRange;
        delete this._canonicalName;
        delete this._hasOtherVendorNameOrKeyword;

        if (changed)
            this.dispatchEventToListeners(WebInspector.CSSProperty.Event.Changed);
    }

    get synthesizedText()
    {
        var name = this.name;
        if (!name)
            return "";

        var priority = this.priority;
        return name + ": " + this.value.trim() + (priority ? " !" + priority : "") + ";";
    }

    get text()
    {
        return this._text || this.synthesizedText;
    }

    get name()
    {
        return this._name;
    }

    get canonicalName()
    {
        if (this._canonicalName)
            return this._canonicalName;

        this._canonicalName = WebInspector.cssStyleManager.canonicalNameForPropertyName(this.name);

        return this._canonicalName;
    }

    get value()
    {
        return this._value;
    }

    get important()
    {
        return this.priority === "important";
    }

    get priority()
    {
        return this._priority;
    }

    get enabled()
    {
        return this._enabled && this._ownerStyle && (!isNaN(this._index) || this._ownerStyle.type === WebInspector.CSSStyleDeclaration.Type.Computed);
    }

    get overridden()
    {
        return this._overridden;
    }

    set overridden(overridden)
    {
        overridden = overridden || false;

        if (this._overridden === overridden)
            return;

        var previousOverridden = this._overridden;

        this._overridden = overridden;

        if (this._overriddenStatusChangedTimeout)
            return;

        function delayed()
        {
            delete this._overriddenStatusChangedTimeout;

            if (this._overridden === previousOverridden)
                return;

            this.dispatchEventToListeners(WebInspector.CSSProperty.Event.OverriddenStatusChanged);
        }

        this._overriddenStatusChangedTimeout = setTimeout(delayed.bind(this), 0);
    }

    get implicit() { return this._implicit; }
    set implicit(implicit) { this._implicit = implicit; }

    get anonymous()
    {
        return this._anonymous;
    }

    get inherited()
    {
        return this._inherited;
    }

    get valid()
    {
        return this._valid;
    }

    get styleSheetTextRange()
    {
        return this._styleSheetTextRange;
    }

    get styleDeclarationTextRange()
    {
        if ("_styleDeclarationTextRange" in this)
            return this._styleDeclarationTextRange;

        if (!this._ownerStyle || !this._styleSheetTextRange)
            return null;

        var styleTextRange = this._ownerStyle.styleSheetTextRange;
        if (!styleTextRange)
            return null;

        var startLine = this._styleSheetTextRange.startLine - styleTextRange.startLine;
        var endLine = this._styleSheetTextRange.endLine - styleTextRange.startLine;

        var startColumn = this._styleSheetTextRange.startColumn;
        if (!startLine)
            startColumn -= styleTextRange.startColumn;

        var endColumn = this._styleSheetTextRange.endColumn;
        if (!endLine)
            endColumn -= styleTextRange.startColumn;

        this._styleDeclarationTextRange = new WebInspector.TextRange(startLine, startColumn, endLine, endColumn);

        return this._styleDeclarationTextRange;
    }

    get relatedShorthandProperty()
    {
        return this._relatedShorthandProperty;
    }

    set relatedShorthandProperty(property)
    {
        this._relatedShorthandProperty = property || null;
    }

    get relatedLonghandProperties()
    {
        return this._relatedLonghandProperties;
    }

    addRelatedLonghandProperty(property)
    {
        this._relatedLonghandProperties.push(property);
    }

    clearRelatedLonghandProperties(property)
    {
        this._relatedLonghandProperties = [];
    }

    hasOtherVendorNameOrKeyword()
    {
        if ("_hasOtherVendorNameOrKeyword" in this)
            return this._hasOtherVendorNameOrKeyword;

        this._hasOtherVendorNameOrKeyword = WebInspector.cssStyleManager.propertyNameHasOtherVendorPrefix(this.name) || WebInspector.cssStyleManager.propertyValueHasOtherVendorKeyword(this.value);

        return this._hasOtherVendorNameOrKeyword;
    }
};

WebInspector.CSSProperty.Event = {
    Changed: "css-property-changed",
    OverriddenStatusChanged: "css-property-overridden-status-changed"
};
