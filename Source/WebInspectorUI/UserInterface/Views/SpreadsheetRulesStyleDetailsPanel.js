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

        this.element.classList.add("spreadsheet-style-panel");

        this._sections = [];
        this._inspectorSection = null;
        this._isInspectorSectionPendingFocus = false;
        this._ruleMediaAndInherticanceList = [];
        this._propertyToSelectAndHighlight = null;

        this._emptyFilterResultsElement = WI.createMessageTextView(WI.UIString("No Results Found"));
    }

    // Public

    refresh(significantChange)
    {
        // We only need to do a rebuild on significant changes. Other changes are handled
        // by the sections and text editors themselves.
        if (!significantChange) {
            super.refresh(significantChange);
            return;
        }

        let uniqueOrderedStyles = (orderedStyles) => {
            let uniqueStyles = [];

            for (let style of orderedStyles) {
                let rule = style.ownerRule;
                if (!rule) {
                    uniqueStyles.push(style);
                    continue;
                }

                let found = false;
                for (let existingStyle of uniqueStyles) {
                    if (rule.isEqualTo(existingStyle.ownerRule)) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    uniqueStyles.push(style);
            }

            return uniqueStyles;
        };

        let createInheritedHeader = (style) => {
            let inheritedHeader = document.createElement("h2");
            inheritedHeader.classList.add("section-inherited");
            inheritedHeader.append(WI.UIString("Inherited From: "), WI.linkifyNodeReference(style.node, {
                maxLength: 100,
                excludeRevealElement: true,
            }));

            return inheritedHeader;
        };

        this.removeAllSubviews();

        let orderedStyles = uniqueOrderedStyles(this.nodeStyles.orderedStyles);
        let previousStyle = null;

        this._sections = [];

        for (let style of orderedStyles) {
            if (style.inherited && (!previousStyle || previousStyle.node !== style.node))
                this.element.append(createInheritedHeader(style));

            // FIXME: <https://webkit.org/b/176187> Display matching pseudo-styles
            // FIXME: <https://webkit.org/b/176289> Display at-rule section headers (@media, @keyframes, etc.)

            let section = style[WI.SpreadsheetRulesStyleDetailsPanel.RuleSection];
            if (!section) {
                section = new WI.SpreadsheetCSSStyleDeclarationSection(this, style);
                style[WI.SpreadsheetRulesStyleDetailsPanel.RuleSection] = section;
            }

            if (this._isInspectorSectionPendingFocus && style.isInspectorRule())
                this._inspectorSection = section;

            this.addSubview(section);
            section.needsLayout();
            this._sections.push(section);

            previousStyle = style;
        }

        this.element.append(this._emptyFilterResultsElement);

        super.refresh(significantChange);
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

    cssStyleDeclarationSectionStartEditingNextRule(currentSection)
    {
        let currentIndex = this._sections.indexOf(currentSection);
        let index = currentIndex < this._sections.length - 1 ? currentIndex + 1 : 0;
        this._sections[index].startEditingRuleSelector();
    }

    cssStyleDeclarationSectionStartEditingPreviousRule(currentSection)
    {
        let index = this._sections.indexOf(currentSection);
        console.assert(index > -1);

        while (true) {
            index--;
            if (index < 0)
                break;

            let section = this._sections[index];
            if (section.editable) {
                section._propertiesEditor.startEditingLastProperty();
                break;
            }
        }
    }
};

WI.SpreadsheetRulesStyleDetailsPanel.RuleSection = Symbol("rule-section");
