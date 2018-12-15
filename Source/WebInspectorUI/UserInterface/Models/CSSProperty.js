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

WI.CSSProperty = class CSSProperty extends WI.Object
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
        if (WI.CSSKeywordCompletions.InheritedProperties.has(name))
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
        // Locked CSSProperty can still be updated from the back-end when the text matches.
        // We need to do this to keep attributes such as valid and overridden up to date.
        if (this._ownerStyle && this._ownerStyle.locked && text !== this._text)
            return;

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
            changed = this._name !== name || this._rawValue !== value || this._priority !== priority ||
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
        this._rawValue = value;
        this._priority = priority;
        this._enabled = enabled;
        this._implicit = implicit;
        this._anonymous = anonymous;
        this._inherited = WI.CSSProperty.isInheritedPropertyName(name);
        this._valid = valid;
        this._variable = name.startsWith("--");
        this._styleSheetTextRange = styleSheetTextRange || null;

        this._relatedShorthandProperty = null;
        this._relatedLonghandProperties = [];

        // Clear computed properties.
        delete this._styleDeclarationTextRange;
        delete this._canonicalName;
        delete this._hasOtherVendorNameOrKeyword;

        if (changed)
            this.dispatchEventToListeners(WI.CSSProperty.Event.Changed);
    }

    remove()
    {
        // Setting name or value to an empty string removes the entire CSSProperty.
        this._name = "";
        const forceRemove = true;
        this._updateStyleText(forceRemove);
    }

    replaceWithText(text)
    {
        this._updateOwnerStyleText(this._text, text, true);
    }

    commentOut(disabled)
    {
        console.assert(this._enabled === disabled, "CSS property is already " + (disabled ? "disabled" : "enabled"));
        if (this._enabled === !disabled)
            return;

        this._enabled = !disabled;

        if (disabled)
            this.text = "/* " + this._text + " */";
        else
            this.text = this._text.slice(2, -2).trim();
    }

    get text()
    {
        return this._text;
    }

    set text(newText)
    {
        if (this._text === newText)
            return;

        this._updateOwnerStyleText(this._text, newText);
        this._text = newText;
    }

    get formattedText()
    {
        if (!this._name)
            return "";

        return `${this._name}: ${this._rawValue};`;
    }

    get name()
    {
        return this._name;
    }

    set name(name)
    {
        if (name === this._name)
            return;

        this._name = name;
        this._updateStyleText();
    }

    get canonicalName()
    {
        if (this._canonicalName)
            return this._canonicalName;

        this._canonicalName = WI.cssManager.canonicalNameForPropertyName(this.name);

        return this._canonicalName;
    }

    // FIXME: Remove current value getter and rename rawValue to value once the old styles sidebar is removed.
    get value()
    {
        if (!this._value)
            this._value = this._rawValue.replace(/\s*!important\s*$/, "");

        return this._value;
    }

    get rawValue()
    {
        return this._rawValue;
    }

    set rawValue(value)
    {
        if (value === this._rawValue)
            return;

        this._rawValue = value;
        this._value = undefined;
        this._updateStyleText();
    }

    get important()
    {
        return this.priority === "important";
    }

    get priority() { return this._priority; }

    get attached()
    {
        return this._enabled && this._ownerStyle && (!isNaN(this._index) || this._ownerStyle.type === WI.CSSStyleDeclaration.Type.Computed);
    }

    // Only commented out properties are disabled.
    get enabled() { return this._enabled; }

    get overridden() { return this._overridden; }
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

            this.dispatchEventToListeners(WI.CSSProperty.Event.OverriddenStatusChanged);
        }

        this._overriddenStatusChangedTimeout = setTimeout(delayed.bind(this), 0);
    }

    get implicit() { return this._implicit; }
    set implicit(implicit) { this._implicit = implicit; }

    get anonymous() { return this._anonymous; }
    get inherited() { return this._inherited; }
    get valid() { return this._valid; }
    get variable() { return this._variable; }
    get styleSheetTextRange() { return this._styleSheetTextRange; }

    get editable()
    {
        return !!(this._styleSheetTextRange && this._ownerStyle && this._ownerStyle.styleSheetTextRange);
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

        this._styleDeclarationTextRange = new WI.TextRange(startLine, startColumn, endLine, endColumn);

        return this._styleDeclarationTextRange;
    }

    get relatedShorthandProperty() { return this._relatedShorthandProperty; }
    set relatedShorthandProperty(property)
    {
        this._relatedShorthandProperty = property || null;
    }

    get relatedLonghandProperties() { return this._relatedLonghandProperties; }

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

        this._hasOtherVendorNameOrKeyword = WI.cssManager.propertyNameHasOtherVendorPrefix(this.name) || WI.cssManager.propertyValueHasOtherVendorKeyword(this.value);

        return this._hasOtherVendorNameOrKeyword;
    }

    // Private

    _updateStyleText(forceRemove = false)
    {
        let text = "";

        if (this._name && this._rawValue)
            text = this._name + ": " + this._rawValue + ";";

        let oldText = this._text;
        this._text = text;
        this._updateOwnerStyleText(oldText, this._text, forceRemove);
    }

    _updateOwnerStyleText(oldText, newText, forceRemove = false)
    {
        if (oldText === newText) {
            if (forceRemove) {
                const lineDelta = 0;
                const columnDelta = 0;
                this._ownerStyle.shiftPropertiesAfter(this, lineDelta, columnDelta, forceRemove);
            }
            return;
        }

        this._prependSemicolonIfNeeded();

        let styleText = this._ownerStyle.text || "";

        // _styleSheetTextRange is the position of the property within the stylesheet.
        // range is the position of the property within the rule.
        let range = this._styleSheetTextRange.relativeTo(this._ownerStyle.styleSheetTextRange.startLine, this._ownerStyle.styleSheetTextRange.startColumn);

        // Append a line break to count the last line of styleText towards endOffset.
        range.resolveOffsets(styleText + "\n");

        console.assert(oldText === styleText.slice(range.startOffset, range.endOffset), "_styleSheetTextRange data is invalid.");

        if (WI.settings.enableStyleEditingDebugMode.value) {
            let prefix = styleText.slice(0, range.startOffset);
            let postfix = styleText.slice(range.endOffset);
            console.info(`${prefix}%c${oldText}%c${newText}%c${postfix}`, `background: hsl(356, 100%, 90%); color: black`, `background: hsl(100, 100%, 91%); color: black`, `background: transparent`);
        }

        let newStyleText = styleText.slice(0, range.startOffset) + newText + styleText.slice(range.endOffset);

        let lineDelta = newText.lineCount - oldText.lineCount;
        let columnDelta = newText.lastLine.length - oldText.lastLine.length;
        this._styleSheetTextRange = this._styleSheetTextRange.cloneAndModify(0, 0, lineDelta, columnDelta);

        this._ownerStyle.text = newStyleText;

        let propertyWasRemoved = !newText;
        this._ownerStyle.shiftPropertiesAfter(this, lineDelta, columnDelta, propertyWasRemoved);
    }

    _prependSemicolonIfNeeded()
    {
        for (let i = this.index - 1; i >= 0; --i) {
            let property = this._ownerStyle.allProperties[i];
            if (!property.enabled)
                continue;

            let match = property.text.match(/[^;\s](\s*)$/);
            if (match)
                property.text = property.text.trimRight() + ";" + match[1];

            break;
        }
    }
};

WI.CSSProperty.Event = {
    Changed: "css-property-changed",
    OverriddenStatusChanged: "css-property-overridden-status-changed"
};
