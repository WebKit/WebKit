/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.ComputedStyleDetailsPanel = class ComputedStyleDetailsPanel extends WI.StyleDetailsPanel
{
    constructor(delegate)
    {
        super(delegate, WI.ComputedStyleDetailsPanel.StyleClassName, "computed", WI.UIString("Styles \u2014 Computed"));

        this._computedStyleShowAllSetting = new WI.Setting("computed-style-show-all", false);

        this._filterText = null;
    }

    // Public

    refresh(significantChange)
    {
        // We only need to do a rebuild on significant changes. Other changes are handled
        // by the sections and text editors themselves.
        if (!significantChange) {
            super.refresh();
            return;
        }

        this._propertiesTextEditor.style = this.nodeStyles.computedStyle;
        this._propertiesSection.element.classList.toggle("hidden", !this._propertiesTextEditor.propertiesToRender.length);

        this._variablesTextEditor.style = this.nodeStyles.computedStyle;
        this._variablesSection.element.classList.toggle("hidden", !this._variablesTextEditor.propertiesToRender.length);

        this._boxModelDiagramRow.nodeStyles = this.nodeStyles;

        if (this._filterText)
            this.applyFilter(this._filterText);

        super.refresh();
    }

    applyFilter(filterText)
    {
        this._filterText = filterText;

        if (!this.didInitialLayout)
            return;

        this._propertiesTextEditor.applyFilter(filterText);
        this._variablesTextEditor.applyFilter(filterText);
    }

    // SpreadsheetCSSStyleDeclarationEditor delegate

    spreadsheetCSSStyleDeclarationEditorShowProperty(editor, property)
    {
        if (this._delegate.computedStyleDetailsPanelShowProperty)
            this._delegate.computedStyleDetailsPanelShowProperty(property);
    }

    focusFirstSection()
    {
        this._propertiesTextEditor.focus();
    }

    focusLastSection()
    {
        this._variablesTextEditor.focus();
    }

    // CSSStyleDeclarationTextEditor delegate

    cssStyleDeclarationTextEditorStartEditingAdjacentRule(cssStyleDeclarationTextEditor, backward)
    {
        if (cssStyleDeclarationTextEditor === this._propertiesTextEditor) {
            if (backward && this._delegate && this._delegate.styleDetailsPanelFocusLastPseudoClassCheckbox) {
                this._delegate.styleDetailsPanelFocusLastPseudoClassCheckbox(this);
                return;
            }

            this._variablesTextEditor.focus();
        } else if (cssStyleDeclarationTextEditor === this._variablesTextEditor) {
            if (!backward && this._delegate && this._delegate.styleDetailsPanelFocusFilterBar) {
                this._delegate.styleDetailsPanelFocusFilterBar(this);
                return;
            }

            this._propertiesTextEditor.focus();
        }
    }

    // Protected

    initialLayout()
    {
        let computedStyleShowAllLabel = document.createElement("label");
        computedStyleShowAllLabel.textContent = WI.UIString("Show All");

        this._computedStyleShowAllCheckbox = document.createElement("input");
        this._computedStyleShowAllCheckbox.type = "checkbox";
        this._computedStyleShowAllCheckbox.checked = this._computedStyleShowAllSetting.value;
        this._computedStyleShowAllCheckbox.addEventListener("change", this._computedStyleShowAllCheckboxValueChanged.bind(this));
        computedStyleShowAllLabel.appendChild(this._computedStyleShowAllCheckbox);

        this._propertiesTextEditor = new WI.SpreadsheetCSSStyleDeclarationEditor(this);
        this._propertiesTextEditor.propertyVisibilityMode = WI.SpreadsheetCSSStyleDeclarationEditor.PropertyVisibilityMode.HideVariables;
        this._propertiesTextEditor.showsImplicitProperties = this._computedStyleShowAllSetting.value;
        this._propertiesTextEditor.alwaysShowPropertyNames = ["display", "width", "height"];
        this._propertiesTextEditor.hideFilterNonMatchingProperties = true;
        this._propertiesTextEditor.sortPropertiesByName = true;
        this._propertiesTextEditor.addEventListener(WI.SpreadsheetCSSStyleDeclarationEditor.Event.FilterApplied, this._handleEditorFilterApplied, this);

        let propertiesRow = new WI.DetailsSectionRow;
        let propertiesGroup = new WI.DetailsSectionGroup([propertiesRow]);
        this._propertiesSection = new WI.DetailsSection("computed-style-properties", WI.UIString("Properties"), [propertiesGroup], computedStyleShowAllLabel);
        this._propertiesSection.addEventListener(WI.DetailsSection.Event.CollapsedStateChanged, this._handlePropertiesSectionCollapsedStateChanged, this);

        this.addSubview(this._propertiesTextEditor);

        propertiesRow.element.appendChild(this._propertiesTextEditor.element);

        this._variablesTextEditor = new WI.SpreadsheetCSSStyleDeclarationEditor(this);
        this._variablesTextEditor.propertyVisibilityMode = WI.SpreadsheetCSSStyleDeclarationEditor.PropertyVisibilityMode.HideNonVariables;
        this._variablesTextEditor.hideFilterNonMatchingProperties = true;
        this._variablesTextEditor.sortPropertiesByName = true;
        this._variablesTextEditor.addEventListener(WI.SpreadsheetCSSStyleDeclarationEditor.Event.FilterApplied, this._handleEditorFilterApplied, this);

        let variablesRow = new WI.DetailsSectionRow;
        let variablesGroup = new WI.DetailsSectionGroup([variablesRow]);
        this._variablesSection = new WI.DetailsSection("computed-style-properties", WI.UIString("Variables"), [variablesGroup]);
        this._variablesSection.addEventListener(WI.DetailsSection.Event.CollapsedStateChanged, this._handleVariablesSectionCollapsedStateChanged, this);

        this.addSubview(this._variablesTextEditor);

        variablesRow.element.appendChild(this._variablesTextEditor.element);

        this.element.appendChild(this._propertiesSection.element);
        this.element.appendChild(this._variablesSection.element);

        this._boxModelDiagramRow = new WI.BoxModelDetailsSectionRow;

        let boxModelGroup = new WI.DetailsSectionGroup([this._boxModelDiagramRow]);
        let boxModelSection = new WI.DetailsSection("computed-style-box-model", WI.UIString("Box Model"), [boxModelGroup]);

        this.element.appendChild(boxModelSection.element);
    }

    filterDidChange(filterBar)
    {
        super.filterDidChange(filterBar);

        this.applyFilter(filterBar.filters.text);
    }

    // Private

    _computedStyleShowAllCheckboxValueChanged(event)
    {
        let checked = this._computedStyleShowAllCheckbox.checked;
        this._computedStyleShowAllSetting.value = checked;
        this._propertiesTextEditor.showsImplicitProperties = checked;
    }

    _handlePropertiesSectionCollapsedStateChanged(event)
    {
        if (event && event.data && !event.data.collapsed)
            this._propertiesTextEditor.needsLayout();
    }

    _handleVariablesSectionCollapsedStateChanged(event)
    {
        if (event && event.data && !event.data.collapsed)
            this._variablesTextEditor.needsLayout();
    }

    _handleEditorFilterApplied(event)
    {
        let section = null;
        if (event.target === this._propertiesTextEditor)
            section = this._propertiesSection;
        else if (event.target === this._variablesTextEditor)
            section = this._variablesSection;

        if (section)
            section.element.classList.toggle("hidden", !event.data.matches);
    }
};

WI.ComputedStyleDetailsPanel.StyleClassName = "computed";
