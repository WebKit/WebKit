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

WI.CSSStyleDeclaration = class CSSStyleDeclaration extends WI.Object
{
    constructor(nodeStyles, ownerStyleSheet, id, type, node, inherited, text, properties, styleSheetTextRange)
    {
        super();

        console.assert(nodeStyles);
        this._nodeStyles = nodeStyles;

        this._ownerRule = null;

        this._ownerStyleSheet = ownerStyleSheet || null;
        this._id = id || null;
        this._type = type || null;
        this._node = node || null;
        this._inherited = inherited || false;

        this._initialState = null;
        this._updatesInProgressCount = 0;
        this._locked = false;
        this._pendingProperties = [];
        this._propertyNameMap = {};

        this._properties = [];
        this._enabledProperties = null;
        this._visibleProperties = null;

        this.update(text, properties, styleSheetTextRange, {dontFireEvents: true});
    }

    // Public

    get initialState() { return this._initialState; }

    get id()
    {
        return this._id;
    }

    get ownerStyleSheet()
    {
        return this._ownerStyleSheet;
    }

    get type()
    {
        return this._type;
    }

    get inherited()
    {
        return this._inherited;
    }

    get node()
    {
        return this._node;
    }

    get editable()
    {
        if (!this._id)
            return false;

        if (this._type === WI.CSSStyleDeclaration.Type.Rule)
            return this._ownerRule && this._ownerRule.editable;

        if (this._type === WI.CSSStyleDeclaration.Type.Inline)
            return !this._node.isInUserAgentShadowTree();

        return false;
    }

    get selectorEditable()
    {
        return this._ownerRule && this._ownerRule.editable;
    }

    get locked() { return this._locked; }
    set locked(value) { this._locked = value; }

    update(text, properties, styleSheetTextRange, options = {})
    {
        let dontFireEvents = options.dontFireEvents || false;

        // When two consequent setText calls happen (A and B), only update when the last call (B) is finished.
        //               Front-end:   A B
        //                Back-end:       A B
        // _updatesInProgressCount: 0 1 2 1 0
        //                                  ^
        //                                  update only happens here
        if (this._updatesInProgressCount > 0 && !options.forceUpdate) {
            if (WI.settings.enableStyleEditingDebugMode.value && text !== this._text)
                console.warn("Style modified while editing:", text);

            return;
        }

        // Allow updates from the backend when text matches because `properties` may contain warnings that need to be shown.
        if (this._locked && !options.forceUpdate && text !== this._text)
            return;

        text = text || "";
        properties = properties || [];

        let oldProperties = this._properties || [];
        let oldText = this._text;

        this._text = text;
        this._properties = properties;

        this._styleSheetTextRange = styleSheetTextRange;
        this._propertyNameMap = {};

        this._enabledProperties = null;
        this._visibleProperties = null;

        let editable = this.editable;

        for (let property of this._properties) {
            property.ownerStyle = this;

            // Store the property in a map if we aren't editable. This
            // allows for quick lookup for computed style. Editable
            // styles don't use the map since they need to account for
            // overridden properties.
            if (!editable)
                this._propertyNameMap[property.name] = property;
            else {
                // Remove from pendingProperties (if it was pending).
                this._pendingProperties.remove(property);
            }
        }

        for (let oldProperty of oldProperties) {
            if (this.enabledProperties.includes(oldProperty))
                continue;

            // Clear the index, since it is no longer valid.
            oldProperty.index = NaN;

            // Keep around old properties in pending in case they
            // are needed again during editing.
            if (editable)
                this._pendingProperties.push(oldProperty);
        }

        if (dontFireEvents)
            return;

        // Don't fire the event if text hasn't changed. However, it should still fire for Computed style declarations
        // because it never has text.
        if (oldText === this._text && this._type !== WI.CSSStyleDeclaration.Type.Computed)
            return;

        function delayed()
        {
            this.dispatchEventToListeners(WI.CSSStyleDeclaration.Event.PropertiesChanged);
        }

        // Delay firing the PropertiesChanged event so DOMNodeStyles has a chance to mark overridden and associated properties.
        setTimeout(delayed.bind(this), 0);
    }

    get ownerRule()
    {
        return this._ownerRule;
    }

    set ownerRule(rule)
    {
        this._ownerRule = rule || null;
    }

    get text()
    {
        return this._text;
    }

    set text(text)
    {
        if (this._text === text)
            return;

        let trimmedText = text.trim();
        if (this._text === trimmedText)
            return;

        if (!trimmedText.length || this._type === WI.CSSStyleDeclaration.Type.Inline)
            text = trimmedText;

        this._text = text;
        ++this._updatesInProgressCount;

        let timeoutId = setTimeout(() => {
            console.error("Timed out when setting style text:", text);
            styleTextDidChange();
        }, 2000);

        let styleTextDidChange = () => {
            if (!timeoutId)
                return;

            clearTimeout(timeoutId);
            timeoutId = null;
            this._updatesInProgressCount = Math.max(0, this._updatesInProgressCount - 1);
        };

        this._nodeStyles.changeStyleText(this, text, styleTextDidChange);
    }

    get enabledProperties()
    {
        if (!this._enabledProperties)
            this._enabledProperties = this._properties.filter((property) => property.enabled);

        return this._enabledProperties;
    }

    get properties()
    {
        return this._properties;
    }

    set properties(properties)
    {
        if (properties === this._properties)
            return;

        this._properties = properties;
        this._enabledProperties = null;
        this._visibleProperties = null;
    }

    get visibleProperties()
    {
        if (!this._visibleProperties)
            this._visibleProperties = this._properties.filter((property) => !!property.styleDeclarationTextRange);

        return this._visibleProperties;
    }

    get pendingProperties()
    {
        return this._pendingProperties;
    }

    get styleSheetTextRange()
    {
        return this._styleSheetTextRange;
    }

    get mediaList()
    {
        if (this._ownerRule)
            return this._ownerRule.mediaList;
        return [];
    }

    get selectorText()
    {
        if (this._ownerRule)
            return this._ownerRule.selectorText;
        return this._node.appropriateSelectorFor(true);
    }

    propertyForName(name, dontCreateIfMissing)
    {
        console.assert(name);
        if (!name)
            return null;

        if (!this.editable)
            return this._propertyNameMap[name] || null;

        // Editable styles don't use the map since they need to
        // account for overridden properties.

        function findMatch(properties)
        {
            for (var i = 0; i < properties.length; ++i) {
                var property = properties[i];
                if (property.canonicalName !== name && property.name !== name)
                    continue;
                if (bestMatchProperty && !bestMatchProperty.overridden && property.overridden)
                    continue;
                bestMatchProperty = property;
            }
        }

        var bestMatchProperty = null;

        findMatch(this.enabledProperties);

        if (bestMatchProperty)
            return bestMatchProperty;

        if (dontCreateIfMissing || !this.editable)
            return null;

        findMatch(this._pendingProperties, true);

        if (bestMatchProperty)
            return bestMatchProperty;

        var newProperty = new WI.CSSProperty(NaN, null, name);
        newProperty.ownerStyle = this;

        this._pendingProperties.push(newProperty);

        return newProperty;
    }

    newBlankProperty(propertyIndex)
    {
        let text, name, value, priority, overridden, implicit, anonymous;
        let enabled = true;
        let valid = false;
        let styleSheetTextRange = this._rangeAfterPropertyAtIndex(propertyIndex - 1);

        this.markModified();
        let property = new WI.CSSProperty(propertyIndex, text, name, value, priority, enabled, overridden, implicit, anonymous, valid, styleSheetTextRange);

        this._properties.insertAtIndex(property, propertyIndex);
        for (let index = propertyIndex + 1; index < this._properties.length; index++)
            this._properties[index].index = index;

        this.update(this._text, this._properties, this._styleSheetTextRange, {dontFireEvents: true, forceUpdate: true});

        return property;
    }

    markModified()
    {
        let properties = this._initialState ? this._initialState.properties : this._properties;

        if (!this._initialState) {
            this._initialState = new WI.CSSStyleDeclaration(
                this._nodeStyles,
                this._ownerStyleSheet,
                this._id,
                this._type,
                this._node,
                this._inherited,
                this._text,
                [], // Passing CSS properties here would change their ownerStyle.
                this._styleSheetTextRange);
        }

        this._initialState.properties = properties.map((property) => { return property.initialState || property });

        if (this._ownerRule)
            this._ownerRule.markModified();
    }

    shiftPropertiesAfter(cssProperty, lineDelta, columnDelta, propertyWasRemoved)
    {
        // cssProperty.index could be set to NaN by WI.CSSStyleDeclaration.prototype.update.
        let realIndex = this._properties.indexOf(cssProperty);
        if (realIndex === -1)
            return;

        let endLine = cssProperty.styleSheetTextRange.endLine;

        for (let i = realIndex + 1; i < this._properties.length; i++) {
            let property = this._properties[i];

            if (property._styleSheetTextRange) {
                if (property.styleSheetTextRange.startLine === endLine) {
                    // Only update column data if it's on the same line.
                    property._styleSheetTextRange = property._styleSheetTextRange.cloneAndModify(lineDelta, columnDelta, lineDelta, columnDelta);
                } else
                    property._styleSheetTextRange = property._styleSheetTextRange.cloneAndModify(lineDelta, 0, lineDelta, 0);
            }

            if (propertyWasRemoved && !isNaN(property._index))
                property._index--;
        }

        if (propertyWasRemoved)
            this._properties.splice(realIndex, 1);

        // Invalidate cached properties.
        this._visibleProperties = null;
    }

    // Protected

    get nodeStyles()
    {
        return this._nodeStyles;
    }

    // Private

    _rangeAfterPropertyAtIndex(index)
    {
        if (index < 0)
            return this._styleSheetTextRange.collapseToStart();

        if (index >= this.visibleProperties.length)
            return this._styleSheetTextRange.collapseToEnd();

        let property = this.visibleProperties[index];
        return property.styleSheetTextRange.collapseToEnd();
    }
};

WI.CSSStyleDeclaration.Event = {
    PropertiesChanged: "css-style-declaration-properties-changed",
};

WI.CSSStyleDeclaration.Type = {
    Rule: "css-style-declaration-type-rule",
    Inline: "css-style-declaration-type-inline",
    Attribute: "css-style-declaration-type-attribute",
    Computed: "css-style-declaration-type-computed"
};
