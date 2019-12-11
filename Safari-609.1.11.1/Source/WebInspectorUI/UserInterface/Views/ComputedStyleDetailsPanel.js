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
        this._computedStylePreferShorthandsSetting = new WI.Setting("computed-style-use-shorthands", false);

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

        this._computedStyleSection.styleTraces = this._computePropertyTraces(this.nodeStyles.uniqueOrderedStyles);
        this._computedStyleSection.style = this.nodeStyles.computedStyle;

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

        this._computedStyleSection.applyFilter(filterText);
        this._variablesTextEditor.applyFilter(filterText);
    }

    // SpreadsheetCSSStyleDeclarationEditor delegate

    spreadsheetCSSStyleDeclarationEditorShowProperty(editor, property)
    {
        if (this._delegate.computedStyleDetailsPanelShowProperty)
            this._delegate.computedStyleDetailsPanelShowProperty(property);
    }

    // Protected

    initialLayout()
    {
        this._boxModelDiagramRow = new WI.BoxModelDetailsSectionRow;

        let boxModelGroup = new WI.DetailsSectionGroup([this._boxModelDiagramRow]);
        let boxModelSection = new WI.DetailsSection("computed-style-box-model", WI.UIString("Box Model"), [boxModelGroup]);

        this.element.appendChild(boxModelSection.element);

        let propertyFiltersElement = WI.ImageUtilities.useSVGSymbol("Images/FilterFieldGlyph.svg", "filter");
        WI.addMouseDownContextMenuHandlers(propertyFiltersElement, (contextMenu) => {
            contextMenu.appendCheckboxItem(WI.UIString("Show All"), () => {
                this._computedStyleShowAllSetting.value = !this._computedStyleShowAllSetting.value;
            }, this._computedStyleShowAllSetting.value);

            contextMenu.appendCheckboxItem(WI.UIString("Prefer Shorthands"), () => {
                this._computedStylePreferShorthandsSetting.value = !this._computedStylePreferShorthandsSetting.value;
            }, this._computedStylePreferShorthandsSetting.value);
        });

        this._computedStyleSection = new WI.ComputedStyleSection(this);
        this._computedStyleSection.addEventListener(WI.ComputedStyleSection.Event.FilterApplied, this._handleEditorFilterApplied, this);
        this._computedStyleSection.showsImplicitProperties = this._computedStyleShowAllSetting.value;
        this._computedStyleSection.propertyVisibilityMode = WI.ComputedStyleSection.PropertyVisibilityMode.HideVariables;
        this._computedStyleSection.showsShorthandsInsteadOfLonghands = this._computedStylePreferShorthandsSetting.value;
        this._computedStyleSection.alwaysShowPropertyNames = ["display", "width", "height"];
        this._computedStyleSection.hideFilterNonMatchingProperties = true;

        let propertiesRow = new WI.DetailsSectionRow;
        let propertiesGroup = new WI.DetailsSectionGroup([propertiesRow]);
        this._propertiesSection = new WI.DetailsSection("computed-style-properties", WI.UIString("Properties"), [propertiesGroup], propertyFiltersElement);
        this._propertiesSection.addEventListener(WI.DetailsSection.Event.CollapsedStateChanged, this._handlePropertiesSectionCollapsedStateChanged, this);

        this.addSubview(this._computedStyleSection);

        propertiesRow.element.appendChild(this._computedStyleSection.element);

        this._variablesTextEditor = new WI.SpreadsheetCSSStyleDeclarationEditor(this);
        this._variablesTextEditor.propertyVisibilityMode = WI.SpreadsheetCSSStyleDeclarationEditor.PropertyVisibilityMode.HideNonVariables;
        this._variablesTextEditor.hideFilterNonMatchingProperties = true;
        this._variablesTextEditor.sortPropertiesByName = true;
        this._variablesTextEditor.addEventListener(WI.SpreadsheetCSSStyleDeclarationEditor.Event.FilterApplied, this._handleEditorFilterApplied, this);
        this._variablesTextEditor.element.dir = "ltr";

        let variablesRow = new WI.DetailsSectionRow;
        let variablesGroup = new WI.DetailsSectionGroup([variablesRow]);
        this._variablesSection = new WI.DetailsSection("computed-style-variables", WI.UIString("Variables"), [variablesGroup]);
        this._variablesSection.addEventListener(WI.DetailsSection.Event.CollapsedStateChanged, this._handleVariablesSectionCollapsedStateChanged, this);

        this.addSubview(this._variablesTextEditor);

        variablesRow.element.appendChild(this._variablesTextEditor.element);

        this.element.appendChild(this._propertiesSection.element);
        this.element.appendChild(this._variablesSection.element);

        this._computedStyleShowAllSetting.addEventListener(WI.Setting.Event.Changed, this._handleShowAllSettingChanged, this);
        this._computedStylePreferShorthandsSetting.addEventListener(WI.Setting.Event.Changed, this._handleUseShorthandsSettingChanged, this);
    }

    filterDidChange(filterBar)
    {
        super.filterDidChange(filterBar);

        this.applyFilter(filterBar.filters.text);
    }

    // Private

    _computePropertyTraces(orderedDeclarations)
    {
        let result = new Map();
        for (let rule of orderedDeclarations) {
            for (let property of rule.properties) {
                let properties = result.get(property.name);
                if (!properties) {
                    properties = [];
                    result.set(property.name, properties);
                }
                properties.push(property);
            }
        }

        return result;
    }

    _handlePropertiesSectionCollapsedStateChanged(event)
    {
        if (event && event.data && !event.data.collapsed)
            this._computedStyleSection.needsLayout();
    }

    _handleVariablesSectionCollapsedStateChanged(event)
    {
        if (event && event.data && !event.data.collapsed)
            this._variablesTextEditor.needsLayout();
    }

    _handleEditorFilterApplied(event)
    {
        let section = null;
        if (event.target === this._computedStyleSection)
            section = this._propertiesSection;
        else if (event.target === this._variablesTextEditor)
            section = this._variablesSection;

        if (section)
            section.element.classList.toggle("hidden", !event.data.matches);
    }

    _handleShowAllSettingChanged(event)
    {
        this._computedStyleSection.showsImplicitProperties = this._computedStyleShowAllSetting.value;
    }

    _handleUseShorthandsSettingChanged(event)
    {
        this._computedStyleSection.showsShorthandsInsteadOfLonghands = this._computedStylePreferShorthandsSetting.value;
    }
};

WI.ComputedStyleDetailsPanel.StyleClassName = "computed";
