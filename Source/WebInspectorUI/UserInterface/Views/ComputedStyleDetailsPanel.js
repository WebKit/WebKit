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

        this.cssStyleDeclarationTextEditorShouldAddPropertyGoToArrows = true;
    }

    // Public

    cssStyleDeclarationTextEditorShowProperty(property, showSource)
    {
        function delegateShowProperty() {
            if (typeof this._delegate.computedStyleDetailsPanelShowProperty === "function")
                this._delegate.computedStyleDetailsPanelShowProperty(property);
        }

        if (!showSource) {
            delegateShowProperty.call(this);
            return;
        }

        let effectiveProperty = this._nodeStyles.effectivePropertyForName(property.name);
        if (!effectiveProperty || !effectiveProperty.styleSheetTextRange) {
            if (!effectiveProperty.relatedShorthandProperty) {
                delegateShowProperty.call(this);
                return;
            }
            effectiveProperty = effectiveProperty.relatedShorthandProperty;
        }

        let ownerRule = effectiveProperty.ownerStyle.ownerRule;
        if (!ownerRule) {
            delegateShowProperty.call(this);
            return;
        }

        let sourceCode = ownerRule.sourceCodeLocation.sourceCode;
        let {startLine, startColumn} = effectiveProperty.styleSheetTextRange;

        const options = {
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
        };
        WI.showSourceCodeLocation(sourceCode.createSourceCodeLocation(startLine, startColumn), options);
    }

    refresh(significantChange)
    {
        // We only need to do a rebuild on significant changes. Other changes are handled
        // by the sections and text editors themselves.
        if (!significantChange) {
            super.refresh();
            return;
        }

        this._propertiesTextEditor.style = this.nodeStyles.computedStyle;
        this._variablesTextEditor.style = this.nodeStyles.computedStyle;
        this._boxModelDiagramRow.nodeStyles = this.nodeStyles;

        super.refresh();

        this._variablesSection.element.classList.toggle("hidden", !this._variablesTextEditor.shownProperties.length);
    }

    filterDidChange(filterBar)
    {
        let filterText = filterBar.filters.text;
        this._propertiesTextEditor.removeNonMatchingProperties(filterText);
        this._variablesTextEditor.removeNonMatchingProperties(filterText);
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

        this._propertiesTextEditor = new WI.CSSStyleDeclarationTextEditor(this);
        this._propertiesTextEditor.propertyVisibilityMode = WI.CSSStyleDeclarationTextEditor.PropertyVisibilityMode.HideVariables;
        this._propertiesTextEditor.showsImplicitProperties = this._computedStyleShowAllSetting.value;
        this._propertiesTextEditor.alwaysShowPropertyNames = ["display", "width", "height"];
        this._propertiesTextEditor.sortProperties = true;

        let propertiesRow = new WI.DetailsSectionRow;
        let propertiesGroup = new WI.DetailsSectionGroup([propertiesRow]);
        let propertiesSection = new WI.DetailsSection("computed-style-properties", WI.UIString("Properties"), [propertiesGroup], computedStyleShowAllLabel);
        propertiesSection.addEventListener(WI.DetailsSection.Event.CollapsedStateChanged, this._handlePropertiesSectionCollapsedStateChanged, this);

        this.addSubview(this._propertiesTextEditor);

        propertiesRow.element.appendChild(this._propertiesTextEditor.element);

        this._variablesTextEditor = new WI.CSSStyleDeclarationTextEditor(this);
        this._variablesTextEditor.propertyVisibilityMode = WI.CSSStyleDeclarationTextEditor.PropertyVisibilityMode.HideNonVariables;
        this._variablesTextEditor.sortProperties = true;

        let variablesRow = new WI.DetailsSectionRow;
        let variablesGroup = new WI.DetailsSectionGroup([variablesRow]);
        this._variablesSection = new WI.DetailsSection("computed-style-properties", WI.UIString("Variables"), [variablesGroup]);
        this._variablesSection.addEventListener(WI.DetailsSection.Event.CollapsedStateChanged, this._handleVariablesSectionCollapsedStateChanged, this);

        this.addSubview(this._variablesTextEditor);

        variablesRow.element.appendChild(this._variablesTextEditor.element);

        this.element.appendChild(propertiesSection.element);
        this.element.appendChild(this._variablesSection.element);

        this._boxModelDiagramRow = new WI.BoxModelDetailsSectionRow;

        let boxModelGroup = new WI.DetailsSectionGroup([this._boxModelDiagramRow]);
        let boxModelSection = new WI.DetailsSection("style-box-model", WI.UIString("Box Model"), [boxModelGroup]);

        this.element.appendChild(boxModelSection.element);
    }

    // Private

    _computedStyleShowAllCheckboxValueChanged(event)
    {
        let checked = this._computedStyleShowAllCheckbox.checked;
        this._computedStyleShowAllSetting.value = checked;
        this._propertiesTextEditor.showsImplicitProperties = checked;
        this._propertiesTextEditor.updateLayout();
    }

    _handlePropertiesSectionCollapsedStateChanged(event)
    {
        if (event && event.data && !event.data.collapsed)
            this._propertiesTextEditor.refresh();
    }

    _handleVariablesSectionCollapsedStateChanged(event)
    {
        if (event && event.data && !event.data.collapsed)
            this._variablesTextEditor.refresh();
    }
};

WI.ComputedStyleDetailsPanel.StyleClassName = "computed";
