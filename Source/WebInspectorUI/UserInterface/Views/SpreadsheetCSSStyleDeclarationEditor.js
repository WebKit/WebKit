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

        this._inlineSwatchActive = false;
        this._focused = false;

        this._propertyPendingStartEditing = null;
        this._pendingAddBlankPropertyIndexOffset = NaN;
        this._filterText = null;
    }

    // Public

    initialLayout()
    {
        if (!this.style.editable)
            return;

        this.element.addEventListener("focus", () => { this.focused = true; }, true);
        this.element.addEventListener("blur", (event) => {
            let focusedElement = event.relatedTarget;
            if (focusedElement && focusedElement.isDescendant(this.element))
                return;

            this.focused = false;
        }, true);
    }

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
            let propertyView = new WI.SpreadsheetStyleProperty(this, property);
            propertyView.index = index;
            this.element.append(propertyView.element);
            this._propertyViews.push(propertyView);

            if (property === this._propertyPendingStartEditing)
                propertyViewPendingStartEditing = propertyView;
        }

        if (propertyViewPendingStartEditing) {
            propertyViewPendingStartEditing.nameTextField.startEditing();
            this._propertyPendingStartEditing = null;
        }

        if (this._filterText)
            this.applyFilter(this._filterText);

        if (!isNaN(this._pendingAddBlankPropertyIndexOffset))
            this.addBlankProperty(this._propertyViews.length - 1 - this._pendingAddBlankPropertyIndexOffset);
    }

    detached()
    {
        this._inlineSwatchActive = false;
        this.focused = false;

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

    get editing()
    {
        return this._focused || this._inlineSwatchActive;
    }

    set focused(value)
    {
        this._focused = value;
        this._updateStyleLock();
    }

    set inlineSwatchActive(value)
    {
        this._inlineSwatchActive = value;
        this._updateStyleLock();
    }

    startEditingFirstProperty()
    {
        let firstEditableProperty = this._editablePropertyAfter(-1);
        if (firstEditableProperty)
            firstEditableProperty.nameTextField.startEditing();
        else {
            const appendAfterLast = -1;
            this.addBlankProperty(appendAfterLast);
        }
    }

    startEditingLastProperty()
    {
        let lastEditableProperty = this._editablePropertyBefore(this._propertyViews.length);
        if (lastEditableProperty)
            lastEditableProperty.valueTextField.startEditing();
        else {
            const appendAfterLast = -1;
            this.addBlankProperty(appendAfterLast);
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

    addBlankProperty(index)
    {
        this._pendingAddBlankPropertyIndexOffset = NaN;

        if (index === -1) {
            // Append to the end.
            index = this._propertyViews.length;
        }

        this._propertyPendingStartEditing = this._style.newBlankProperty(index);
        this.needsLayout();
    }

    spreadsheetStylePropertyFocusMoved(propertyView, {direction, willRemoveProperty})
    {
        let movedFromIndex = this._propertyViews.indexOf(propertyView);
        console.assert(movedFromIndex !== -1, "Property doesn't exist, focusing on a selector as a fallback.");
        if (movedFromIndex === -1) {
            if (this._style.selectorEditable)
                this._delegate.cssStyleDeclarationTextEditorStartEditingRuleSelector();

            return;
        }

        if (direction === "forward") {
            // Move from the value to the next enabled property's name.
            let propertyView = this._editablePropertyAfter(movedFromIndex);
            if (propertyView)
                propertyView.nameTextField.startEditing();
            else {
                if (willRemoveProperty) {
                    // Move from the last value in the rule to the next rule's selector.
                    let reverse = false;
                    this._delegate.cssStyleDeclarationEditorStartEditingAdjacentRule(reverse);
                } else {
                    const appendAfterLast = -1;
                    this.addBlankProperty(appendAfterLast);
                }
            }
        } else {
            let propertyView = this._editablePropertyBefore(movedFromIndex);
            if (propertyView) {
                // Move from the property's name to the previous enabled property's value.
                propertyView.valueTextField.startEditing()
            } else {
                // Move from the first property's name to the rule's selector.
                if (this._style.selectorEditable)
                    this._delegate.cssStyleDeclarationTextEditorStartEditingRuleSelector();
            }
        }
    }

    // SpreadsheetStyleProperty delegate

    spreadsheetStylePropertyAddBlankPropertySoon(propertyView, {index})
    {
        if (isNaN(index))
            index = this._propertyViews.length;
        this._pendingAddBlankPropertyIndexOffset = this._propertyViews.length - index;
    }

    spreadsheetStylePropertyRemoved(propertyView)
    {
        this._propertyViews.remove(propertyView);

        for (let index = 0; index < this._propertyViews.length; index++)
            this._propertyViews[index].index = index;

        this._focused = false;
    }

    stylePropertyInlineSwatchActivated()
    {
        this.inlineSwatchActive = true;
    }

    stylePropertyInlineSwatchDeactivated()
    {
        this.inlineSwatchActive = false;
    }

    applyFilter(filterText)
    {
        this._filterText = filterText;

        if (!this.didInitialLayout)
            return;

        let matches = false;
        for (let propertyView of this._propertyViews) {
            if (propertyView.applyFilter(this._filterText))
                matches = true;
        }

        this.dispatchEventToListeners(WI.SpreadsheetCSSStyleDeclarationEditor.Event.FilterApplied, {matches});
    }

    // Private

    get _propertiesToRender()
    {
        if (this._style._styleSheetTextRange)
            return this._style.allVisibleProperties;

        return this._style.allProperties;
    }

    _editablePropertyAfter(propertyIndex)
    {
        for (let index = propertyIndex + 1; index < this._propertyViews.length; index++) {
            let property = this._propertyViews[index];
            if (property.enabled)
                return property;
        }

        return null;
    }

    _editablePropertyBefore(propertyIndex)
    {
        for (let index = propertyIndex - 1; index >= 0; index--) {
            let property = this._propertyViews[index];
            if (property.enabled)
                return property;
        }

        return null;
    }

    _propertiesChanged(event)
    {
        if (this.editing && isNaN(this._pendingAddBlankPropertyIndexOffset)) {
            for (let propertyView of this._propertyViews)
                propertyView.updateStatus();
        } else
            this.needsLayout();
    }

    _updateStyleLock()
    {
        this.style.locked = this._focused || this._inlineSwatchActive;
    }
};

WI.SpreadsheetCSSStyleDeclarationEditor.Event = {
    FilterApplied: "spreadsheet-css-style-declaration-editor-filter-applied",
};

WI.SpreadsheetCSSStyleDeclarationEditor.StyleClassName = "spreadsheet-style-declaration-editor";
