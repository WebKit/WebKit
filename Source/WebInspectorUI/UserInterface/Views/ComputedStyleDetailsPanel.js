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
        this._detailsSectionByStyleSectionMap = new Map;
        this._variablesStyleSectionForGroupTypeMap = new Map;
    }

    // Static

    static displayNameForVariablesGroupType(variablesGroupType)
    {
        switch (variablesGroupType) {
        case WI.DOMNodeStyles.VariablesGroupType.Colors:
            return WI.UIString("Colors", "Colors @ Computed Style variables section", "Section header for the group of CSS variables with colors as values");

        case WI.DOMNodeStyles.VariablesGroupType.Dimensions:
            return WI.UIString("Dimensions", "Dimensions @ Computed style variables section", "Section header for the group of CSS variables with dimensions as values");

        case WI.DOMNodeStyles.VariablesGroupType.Numbers:
            return WI.UIString("Numbers", "Numbers @ Computed Style variables section", "Section header for the group of CSS variables with numbers as values");

        case WI.DOMNodeStyles.VariablesGroupType.Other:
            return WI.UIString("Other", "Other @ Computed Style variables section", "Section header for the generic group of CSS variables");
        }

        console.assert(false, "Unknown group type", variablesGroupType);
        return "";
    }

    // Public

    get minimumWidth()
    {
        return this._boxModelDiagramRow?.minimumWidth ?? 0;
    }

    get variablesGroupingMode()
    {
        console.assert(this._variablesGroupingModeScopeBar.selectedItems[0], "No selected variables grouping mode", this._variablesGroupingModeScopeBar.selectedItems);
        return this._variablesGroupingModeScopeBar.selectedItems[0].id;
    }

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
        this._propertiesSection.element.classList.toggle("hidden", !this._computedStyleSection.propertiesToRender.length);
        this._boxModelDiagramRow.nodeStyles = this.nodeStyles;

        let styleGroups = new Map;

        switch (this.variablesGroupingMode) {
        case WI.ComputedStyleDetailsPanel.VariablesGroupingMode.Ungrouped:
            styleGroups.set(this._variablesStyleSectionForGroupTypeMap.get(WI.DOMNodeStyles.VariablesGroupType.Ungrouped), this.nodeStyles.computedStyle);
            break;

        case WI.ComputedStyleDetailsPanel.VariablesGroupingMode.ByType:
            styleGroups = this.nodeStyles.variableStylesByType;
            break;
        }

        for (let [type, style] of styleGroups) {
            let variablesStyleSection = this._variablesStyleSectionForGroupTypeMap.get(type);
            if (!variablesStyleSection) {
                this.needsLayout();
                break;
            }

            variablesStyleSection.style = style;

            let detailsSection = this._detailsSectionByStyleSectionMap.get(variablesStyleSection);
            detailsSection.element.classList.toggle("hidden", !variablesStyleSection.propertiesToRender.length);
        }

        if (this._filterText)
            this.applyFilter(this._filterText);

        super.refresh();
    }

    applyFilter(filterText)
    {
        this._filterText = filterText;

        if (!this.didInitialLayout)
            return;

        for (let styleSection of this._detailsSectionByStyleSectionMap.keys())
            styleSection.applyFilter(filterText);
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

        let propertyFiltersElement = WI.ImageUtilities.useSVGSymbol("Images/Filter.svg", "filter");
        WI.addMouseDownContextMenuHandlers(propertyFiltersElement, (contextMenu) => {
            contextMenu.appendCheckboxItem(WI.UIString("Show All"), () => {
                this._computedStyleShowAllSetting.value = !this._computedStyleShowAllSetting.value;

                propertyFiltersElement.classList.toggle("active", this._computedStyleShowAllSetting.value || this._computedStylePreferShorthandsSetting.value);
            }, this._computedStyleShowAllSetting.value);

            contextMenu.appendCheckboxItem(WI.UIString("Prefer Shorthands"), () => {
                this._computedStylePreferShorthandsSetting.value = !this._computedStylePreferShorthandsSetting.value;

                propertyFiltersElement.classList.toggle("active", this._computedStyleShowAllSetting.value || this._computedStylePreferShorthandsSetting.value);
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
        this._propertiesSection.addEventListener(WI.DetailsSection.Event.CollapsedStateChanged, this._handleDetailsSectionCollapsedStateChanged, this);

        this.addSubview(this._computedStyleSection);

        propertiesRow.element.appendChild(this._computedStyleSection.element);
        this._detailsSectionByStyleSectionMap.set(this._computedStyleSection, this._propertiesSection);

        let variablesGroupingModeScopeBarItems = [
            new WI.ScopeBarItem(WI.ComputedStyleDetailsPanel.VariablesGroupingMode.Ungrouped, WI.UIString("Ungrouped", "Ungrouped @ Computed Style variables grouping mode", "Label for button to show CSS variables ungrouped")),
            new WI.ScopeBarItem(WI.ComputedStyleDetailsPanel.VariablesGroupingMode.ByType, WI.UIString("By Type", "By Type @ Computed Style variables grouping mode", "Label for button to show CSS variables grouped by type"))
        ];

        const shouldGroupNonExclusiveItems = true;
        this._variablesGroupingModeScopeBar = new WI.ScopeBar("computed-style-variables-grouping-mode", variablesGroupingModeScopeBarItems, variablesGroupingModeScopeBarItems[0], shouldGroupNonExclusiveItems);
        this._variablesGroupingModeScopeBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._handleVariablesGroupingModeScopeBarSelectionChanged, this);

        this._variablesRow = new WI.DetailsSectionRow;
        let variablesGroup = new WI.DetailsSectionGroup([this._variablesRow]);
        this._variablesSection = new WI.DetailsSection("computed-style-variables", WI.UIString("Variables"), [variablesGroup], this._variablesGroupingModeScopeBar.element);
        this._variablesSection.addEventListener(WI.DetailsSection.Event.CollapsedStateChanged, this._handleDetailsSectionCollapsedStateChanged, this);

        this.element.appendChild(this._propertiesSection.element);
        this.element.appendChild(this._variablesSection.element);

        this._computedStyleShowAllSetting.addEventListener(WI.Setting.Event.Changed, this._handleShowAllSettingChanged, this);
        this._computedStylePreferShorthandsSetting.addEventListener(WI.Setting.Event.Changed, this._handleUseShorthandsSettingChanged, this);
    }

    layout()
    {
        super.layout();

        for (let [styleSection, detailsSection] of this._detailsSectionByStyleSectionMap) {
            // The details section for computed properties is updated in-place by WI.ComputedStyleDetailsPanel.refresh().
            if (styleSection === this._computedStyleSection)
                continue;

            // WI.ComputedStyleSection is a view but its element is attached to a WI.DetailsSection which isn't. Reparent it before removing.
            // FIXME: <https://webkit.org/b/152269> - Web Inspector: Convert DetailsSection classes to use View
            this.element.appendChild(styleSection.element);
            this.removeSubview(styleSection);
            styleSection.element.remove();

            // The top-level details section for variables needs to be preserved because it's the host of nested details sections for variables groups.
            if (detailsSection === this._variablesSection)
                continue;

            detailsSection.element.remove();
            this._detailsSectionByStyleSectionMap.delete(styleSection);
        }

        this._variablesStyleSectionForGroupTypeMap.clear();

        switch (this.variablesGroupingMode) {
        case WI.ComputedStyleDetailsPanel.VariablesGroupingMode.Ungrouped:
            this._renderVariablesStyleSectionGroup(this.nodeStyles.computedStyle, WI.DOMNodeStyles.VariablesGroupType.Ungrouped);
            break;

        case WI.ComputedStyleDetailsPanel.VariablesGroupingMode.ByType:
            for (let [type, style] of this.nodeStyles.variableStylesByType)
                this._renderVariablesStyleSectionGroup(style, type, WI.ComputedStyleDetailsPanel.displayNameForVariablesGroupType(type));
            break;
        }

        if (this._filterText)
            this.applyFilter(this._filterText);
    }

    filterDidChange(filterBar)
    {
        this.applyFilter(filterBar.filters.text);
    }

    // Private

    _renderVariablesStyleSectionGroup(style, groupType, label)
    {
        let variablesStyleSection = new WI.ComputedStyleSection(this);
        variablesStyleSection.propertyVisibilityMode = WI.ComputedStyleSection.PropertyVisibilityMode.HideNonVariables;
        variablesStyleSection.hideFilterNonMatchingProperties = true;
        variablesStyleSection.addEventListener(WI.ComputedStyleSection.Event.FilterApplied, this._handleEditorFilterApplied, this);
        variablesStyleSection.element.dir = "ltr";
        variablesStyleSection.style = style;

        this.addSubview(variablesStyleSection);

        let detailsSection;
        if (!label) {
            this._variablesRow.element.appendChild(variablesStyleSection.element);
            detailsSection = this._variablesSection;
        } else {
            let detailsSectionRow = new WI.DetailsSectionRow;
            let detailsSectionGroup = new WI.DetailsSectionGroup([detailsSectionRow]);
            detailsSection = new WI.DetailsSection(`computed-style-variables-group-${groupType}`, label, [detailsSectionGroup]);
            detailsSection.addEventListener(WI.DetailsSection.Event.CollapsedStateChanged, this._handleDetailsSectionCollapsedStateChanged, this);

            detailsSectionRow.element.appendChild(variablesStyleSection.element);
            this._variablesRow.element.appendChild(detailsSection.element);
        }

        this._detailsSectionByStyleSectionMap.set(variablesStyleSection, detailsSection);
        this._variablesStyleSectionForGroupTypeMap.set(groupType, variablesStyleSection);
    }

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

    _handleEditorFilterApplied(event)
    {
        let section = this._detailsSectionByStyleSectionMap.get(event.target);
        section?.element.classList.toggle("hidden", !event.data.matches);
    }

    _handleDetailsSectionCollapsedStateChanged(event)
    {
        if (event.data.collapsed)
            return;

        for (let [styleSection, detailsSection] of this._detailsSectionByStyleSectionMap) {
            if (event.target === detailsSection) {
                styleSection.needsLayout();
                return;
            }
        }
    }

    _handleShowAllSettingChanged(event)
    {
        this._computedStyleSection.showsImplicitProperties = this._computedStyleShowAllSetting.value;
    }

    _handleUseShorthandsSettingChanged(event)
    {
        this._computedStyleSection.showsShorthandsInsteadOfLonghands = this._computedStylePreferShorthandsSetting.value;
    }

    _handleVariablesGroupingModeScopeBarSelectionChanged(event)
    {
        this.needsLayout();
    }
};

WI.ComputedStyleDetailsPanel.StyleClassName = "computed";
WI.ComputedStyleDetailsPanel.VariablesGroupingMode = {
    Ungrouped: "ungrouped",
    ByType: "by-type",
};
