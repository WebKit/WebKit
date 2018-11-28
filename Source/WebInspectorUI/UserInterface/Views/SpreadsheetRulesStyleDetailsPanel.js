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

WI.SpreadsheetRulesStyleDetailsPanel = class SpreadsheetRulesStyleDetailsPanel extends WI.StyleDetailsPanel
{
    constructor(delegate)
    {
        const className = "rules";
        const identifier = "rules";
        const label = WI.UIString("Styles \u2014 Rules");
        super(delegate, className, identifier, label);

        // Make the styles sidebar always left-to-right since CSS is strictly an LTR language.
        this.element.dir = "ltr";

        this._headerMap = new Map;
        this._sections = [];
        this._newRuleSelector = null;
        this._ruleMediaAndInherticanceList = [];
        this._propertyToSelectAndHighlight = null;
        this._filterText = null;
        this._shouldRefreshSubviews = false;

        this._emptyFilterResultsElement = WI.createMessageTextView(WI.UIString("No Results Found"));
    }

    // Public

    refresh(significantChange)
    {
        // We only need to do a rebuild on significant changes. Other changes are handled
        // by the sections and text editors themselves.
        if (significantChange) {
            this._shouldRefreshSubviews = true;
            this.needsLayout();
        }

        super.refresh(significantChange);
    }

    hidden()
    {
        for (let section of this._sections)
            section.hidden();

        super.hidden();
    }

    scrollToSectionAndHighlightProperty(property)
    {
        if (!this._visible) {
            this._propertyToSelectAndHighlight = property;
            return;
        }

        for (let section of this._sections) {
            if (section.highlightProperty(property))
                return;
        }
    }

    nodeStylesRefreshed(event)
    {
        super.nodeStylesRefreshed(event);

        if (this._propertyToSelectAndHighlight) {
            this.scrollToSectionAndHighlightProperty(this._propertyToSelectAndHighlight);
            this._propertyToSelectAndHighlight = null;
        }
    }

    newRuleButtonClicked()
    {
        if (this.nodeStyles.node.isInUserAgentShadowTree())
            return;

        this._addNewRule();
    }

    newRuleButtonContextMenu(event)
    {
        if (this.nodeStyles.node.isInUserAgentShadowTree())
            return;

        let styleSheets = WI.cssManager.styleSheets.filter(styleSheet => styleSheet.hasInfo() && !styleSheet.isInlineStyleTag() && !styleSheet.isInlineStyleAttributeStyleSheet());
        if (!styleSheets.length)
            return;

        let contextMenu = WI.ContextMenu.createFromEvent(event);

        const handler = null;
        const disabled = true;
        contextMenu.appendItem(WI.UIString("Available Style Sheets"), handler, disabled);

        let [inspectorStyleSheets, regularStyleSheets] = styleSheets.partition(styleSheet => styleSheet.isInspectorStyleSheet());
        console.assert(inspectorStyleSheets.length <= 1, "There should never be more than one inspector style sheet");

        contextMenu.appendItem(WI.UIString("Inspector Style Sheet"), () => {
            this._addNewRule(inspectorStyleSheets.length ? inspectorStyleSheets[0].id : null);
        });

        for (let styleSheet of regularStyleSheets) {
            contextMenu.appendItem(styleSheet.displayName, () => {
                this._addNewRule(styleSheet.id);
            });
        }
    }

    applyFilter(filterText)
    {
        this._filterText = filterText;

        if (!this.didInitialLayout)
            return;

        if (this._filterText)
            this.element.classList.add("filter-non-matching");

        for (let header of this._headerMap.values())
            header.classList.add(WI.GeneralStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName);

        for (let section of this._sections)
            section.applyFilter(this._filterText);
    }

    focusFirstSection()
    {
        this.spreadsheetCSSStyleDeclarationSectionStartEditingAdjacentRule(null, 1);
    }

    focusLastSection()
    {
        this.spreadsheetCSSStyleDeclarationSectionStartEditingAdjacentRule(null, -1);
    }

    // SpreadsheetCSSStyleDeclarationSection delegate

    spreadsheetCSSStyleDeclarationSectionStartEditingAdjacentRule(currentSection, delta)
    {
        console.assert(delta !== 0);

        let index = this._sections.indexOf(currentSection);
        if (index < 0) {
            if (delta < 0)
                index = this._sections.length;
            else if (delta > 0)
                index = -1;
        }
        index += delta;

        while (this._sections[index] !== currentSection) {
            if (index < 0) {
                if (this._delegate && this._delegate.styleDetailsPanelFocusLastPseudoClassCheckbox) {
                    this._delegate.styleDetailsPanelFocusLastPseudoClassCheckbox(this);
                    break;
                }

                index = this._sections.length - 1;
            } else if (index >= this._sections.length) {
                if (this._delegate && this._delegate.styleDetailsPanelFocusFilterBar) {
                    this._delegate.styleDetailsPanelFocusFilterBar(this);
                    break;
                }

                index = 0;
            }

            let section = this._sections[index];
            if (section.editable) {
                if (delta < 0)
                    section._propertiesEditor.startEditingLastProperty();
                else
                    section.startEditingRuleSelector();
                break;
            }

            index += delta;
        }
    }

    // Protected

    layout()
    {
        if (!this._shouldRefreshSubviews)
            return;

        this._shouldRefreshSubviews = false;

        this.removeAllSubviews();

        let previousStyle = null;
        this._headerMap.clear();
        this._sections = [];

        let createHeader = (text, node) => {
            let header = this.element.appendChild(document.createElement("h2"));
            header.classList.add("section-header");
            header.append(text);
            header.append(" ", WI.linkifyNodeReference(node, {
                maxLength: 100,
                excludeRevealElement: true,
            }));

            this._headerMap.set(node, header);
        };

        let createSection = (style) => {
            let section = style[WI.SpreadsheetRulesStyleDetailsPanel.RuleSection];
            if (!section) {
                section = new WI.SpreadsheetCSSStyleDeclarationSection(this, style);
                style[WI.SpreadsheetRulesStyleDetailsPanel.RuleSection] = section;
            }

            section.addEventListener(WI.SpreadsheetCSSStyleDeclarationSection.Event.FilterApplied, this._handleSectionFilterApplied, this);

            if (this._newRuleSelector === style.selectorText && !style.hasProperties())
                section.startEditingRuleSelector();

            this.addSubview(section);
            section.needsLayout();
            this._sections.push(section);

            previousStyle = style;
        };

        for (let style of this.nodeStyles.uniqueOrderedStyles) {
            if (style.inherited && (!previousStyle || previousStyle.node !== style.node))
                createHeader(WI.UIString("Inherited From"), style.node);

            createSection(style);
        }

        let pseudoElements = Array.from(this.nodeStyles.node.pseudoElements().values());
        Promise.all(pseudoElements.map((pseudoElement) => WI.cssManager.stylesForNode(pseudoElement).refreshIfNeeded()))
        .then((pseudoNodeStyles) => {
            for (let pseudoNodeStyle of pseudoNodeStyles) {
                createHeader(WI.UIString("Pseudo Element"), pseudoNodeStyle.node);

                for (let style of pseudoNodeStyle.uniqueOrderedStyles)
                    createSection(style);
            }
        });

        this._newRuleSelector = null;

        this.element.append(this._emptyFilterResultsElement);

        if (this._filterText)
            this.applyFilter(this._filterText);
    }

    filterDidChange(filterBar)
    {
        super.filterDidChange(filterBar);

        this.applyFilter(filterBar.filters.text);
    }

    // Private

    _handleSectionFilterApplied(event)
    {
        if (!event.data.matches)
            return;

        this.element.classList.remove("filter-non-matching");

        let header = this._headerMap.get(event.target.style.node);
        if (header)
            header.classList.remove(WI.GeneralStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName);
    }

    // Private

    _addNewRule(stylesheetId)
    {
        const justSelector = true;
        this._newRuleSelector = this.nodeStyles.node.appropriateSelectorFor(justSelector);

        const text = "";
        this.nodeStyles.addRule(this._newRuleSelector, text, stylesheetId);
    }
};

WI.SpreadsheetRulesStyleDetailsPanel.RuleSection = Symbol("rule-section");
