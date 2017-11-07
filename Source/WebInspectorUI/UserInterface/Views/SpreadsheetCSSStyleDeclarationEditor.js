/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.SpreadsheetCSSStyleDeclarationEditor = class SpreadsheetCSSStyleDeclarationEditor extends WI.View
{
    constructor(delegate, style)
    {
        super();

        this.element.classList.add(WI.SpreadsheetCSSStyleDeclarationEditor.StyleClassName);

        this._delegate = delegate;
        this.style = style;
        this._propertyViews = [];
        this._propertyPendingStartEditing = null;
    }

    // Public

    layout()
    {
        super.layout();

        this.element.removeChildren();

        let properties = this._propertiesToRender;
        this.element.classList.toggle("no-properties", !properties.length);

        // FIXME: Only re-layout properties that have been modified and preserve focus whenever possible.
        this._propertyViews = [];

        let propertyViewPendingStartEditing = null;
        for (let index = 0; index < properties.length; index++) {
            let property = properties[index];
            let propertyView = new WI.SpreadsheetStyleProperty(this, property, index);
            this.element.append(propertyView.element);
            this._propertyViews.push(propertyView);

            if (property === this._propertyPendingStartEditing)
                propertyViewPendingStartEditing = propertyView;
        }

        if (propertyViewPendingStartEditing) {
            propertyViewPendingStartEditing.nameTextField.startEditing();
            this._propertyPendingStartEditing = null;
        }
    }

    detached()
    {
        for (let propertyView of this._propertyViews)
            propertyView.detached();
    }

    get style()
    {
        return this._style;
    }

    set style(style)
    {
        if (this._style === style)
            return;

        if (this._style)
            this._style.removeEventListener(WI.CSSStyleDeclaration.Event.PropertiesChanged, this._propertiesChanged, this);

        this._style = style || null;

        if (this._style)
            this._style.addEventListener(WI.CSSStyleDeclaration.Event.PropertiesChanged, this._propertiesChanged, this);

        this.needsLayout();
    }

    startEditingFirstProperty()
    {
        if (this._propertyViews.length)
            this._propertyViews[0].nameTextField.startEditing();
        else {
            let index = 0;
            this.addBlankProperty(index);
        }
    }

    startEditingLastProperty()
    {
        let lastProperty = this._propertyViews.lastValue;
        if (lastProperty)
            lastProperty.valueTextField.startEditing();
        else {
            let index = 0;
            this.addBlankProperty(index);
        }
    }

    highlightProperty(property)
    {
        let propertiesMatch = (cssProperty) => {
            if (cssProperty.attached && !cssProperty.overridden) {
                if (cssProperty.canonicalName === property.canonicalName || hasMatchingLonghandProperty(cssProperty))
                    return true;
            }

            return false;
        };

        let hasMatchingLonghandProperty = (cssProperty) => {
            let cssProperties = cssProperty.relatedLonghandProperties;

            if (!cssProperties.length)
                return false;

            for (let property of cssProperties) {
                if (propertiesMatch(property))
                    return true;
            }

            return false;
        };

        for (let cssProperty of this.style.properties) {
            if (propertiesMatch(cssProperty)) {
                let propertyView = cssProperty.__propertyView;
                if (propertyView) {
                    propertyView.highlight();

                    if (cssProperty.editable)
                        propertyView.valueTextField.startEditing();
                }
                return true;
            }
        }

        return false;
    }

    isFocused()
    {
        let focusedElement = document.activeElement;

        if (!focusedElement || focusedElement.tagName === "BODY")
            return false;

        return focusedElement.isSelfOrDescendant(this.element);
    }

    addBlankProperty(index)
    {
        if (index === -1) {
            // Append to the end.
            index = this._propertyViews.length;
        }

        this._propertyPendingStartEditing = this._style.newBlankProperty(index);
        this.needsLayout();
    }

    spreadsheetCSSStyleDeclarationEditorFocusMoved({direction, movedFromProperty, willRemoveProperty})
    {
        let movedFromIndex = this._propertyViews.indexOf(movedFromProperty);
        console.assert(movedFromIndex !== -1, "Property doesn't exist, focusing on a selector as a fallback.");
        if (movedFromIndex === -1) {
            if (this._style.selectorEditable)
                this._delegate.cssStyleDeclarationTextEditorStartEditingRuleSelector();

            return;
        }

        if (direction === "forward") {
            // Move from the value to the next property's name.
            let index = movedFromIndex + 1;
            if (index < this._propertyViews.length)
                this._propertyViews[index].nameTextField.startEditing();
            else {
                if (willRemoveProperty) {
                    // Move from the last value in the rule to the next rule's selector.
                    let reverse = false;
                    this._delegate.cssStyleDeclarationEditorStartEditingAdjacentRule(reverse);
                } else
                    this.addBlankProperty(index);
            }
        } else {
            let index = movedFromIndex - 1;
            if (index < 0) {
                // Move from the first property's name to the rule's selector.
                if (this._style.selectorEditable)
                    this._delegate.cssStyleDeclarationTextEditorStartEditingRuleSelector();
            } else {
                // Move from the property's name to the previous property's value.
                let valueTextField = this._propertyViews[index].valueTextField;
                if (valueTextField)
                    valueTextField.startEditing();
            }
        }
    }

    spreadsheetStylePropertyRemoved(propertyView)
    {
        this._propertyViews.remove(propertyView);
    }

    // Private

    get _propertiesToRender()
    {
        if (this._style._styleSheetTextRange)
            return this._style.allVisibleProperties;

        return this._style.allProperties;
    }

    _propertiesChanged(event)
    {
        if (this.isFocused()) {
            for (let propertyView of this._propertyViews)
                propertyView.updateStatus();
        } else
            this.needsLayout();
    }
};

WI.SpreadsheetCSSStyleDeclarationEditor.StyleClassName = "spreadsheet-style-declaration-editor";
