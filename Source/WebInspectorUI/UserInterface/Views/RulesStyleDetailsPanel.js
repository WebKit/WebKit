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

WebInspector.RulesStyleDetailsPanel = class RulesStyleDetailsPanel extends WebInspector.StyleDetailsPanel
{
    constructor(delegate)
    {
        super(delegate, "rules", "rules", WebInspector.UIString("Rules"));

        this._sections = [];
        this._previousFocusedSection = null;
        this._ruleMediaAndInherticanceList = [];
        this._propertyToSelectAndHighlight = null;

        this._emptyFilterResultsElement = document.createElement("div");
        this._emptyFilterResultsElement.classList.add("no-filter-results");

        this._emptyFilterResultsMessage = document.createElement("div");
        this._emptyFilterResultsMessage.classList.add("no-filter-results-message");
        this._emptyFilterResultsMessage.textContent = WebInspector.UIString("No Results Found");
        this._emptyFilterResultsElement.appendChild(this._emptyFilterResultsMessage);
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

        var newSections = [];
        var newDOMFragment = document.createDocumentFragment();

        var previousMediaList = [];
        var previousSection = null;

        function mediaListsEqual(a, b)
        {
            a = a || [];
            b = b || [];

            if (a.length !== b.length)
                return false;

            for (var i = 0; i < a.length; ++i) {
                var aMedia = a[i];
                var bMedia = b[i];

                if (aMedia.type !== bMedia.type)
                    return false;

                if (aMedia.text !== bMedia.text)
                    return false;

                if (!aMedia.sourceCodeLocation && bMedia.sourceCodeLocation)
                    return false;

                if (aMedia.sourceCodeLocation && !aMedia.sourceCodeLocation.isEqual(bMedia.sourceCodeLocation))
                    return false;
            }

            return true;
        }

        function filteredMediaList(mediaList)
        {
            if (!mediaList)
                return [];

            // Exclude the basic "screen" query since it's very common and just clutters things.
            return mediaList.filter(function(media) {
                return media.text !== "screen";
            });
        }

        function uniqueOrderedStyles(orderedStyles)
        {
            var uniqueStyles = [];

            for (var style of orderedStyles) {
                var rule = style.ownerRule;
                if (!rule) {
                    uniqueStyles.push(style);
                    continue;
                }

                var found = false;
                for (var existingStyle of uniqueStyles) {
                    if (rule.isEqualTo(existingStyle.ownerRule)) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    uniqueStyles.push(style);
            }

            return uniqueStyles;
        }

        function appendStyleSection(style)
        {
            var section = style.__rulesSection;
            if (section && section.focused && !this._previousFocusedSection)
                this._previousFocusedSection = section;

            if (!section) {
                section = new WebInspector.CSSStyleDeclarationSection(this, style);
                style.__rulesSection = section;
            } else
                section.refresh();

            if (this._focusNextNewInspectorRule && style.ownerRule && style.ownerRule.type === WebInspector.CSSRule.Type.Inspector) {
                this._previousFocusedSection = section;
                delete this._focusNextNewInspectorRule;
            }

            // Reset lastInGroup in case the order/grouping changed.
            section.lastInGroup = false;

            newDOMFragment.appendChild(section.element);
            newSections.push(section);

            previousSection = section;
        }

        function addNewRuleButton()
        {
            if (previousSection)
                previousSection.lastInGroup = true;

            if (!this.nodeStyles.node.isInShadowTree()) {
                var newRuleButton = document.createElement("div");
                newRuleButton.className = "new-rule";
                newRuleButton.addEventListener("click", this._newRuleClicked.bind(this));

                newRuleButton.appendChild(document.createElement("img"));
                newRuleButton.appendChild(document.createTextNode(WebInspector.UIString("New Rule")));

                newDOMFragment.appendChild(newRuleButton);
            }

            addedNewRuleButton = true;
        }

        var pseudoElements = this.nodeStyles.pseudoElements;
        var pseudoElementsStyle = [];

        for (var pseudoIdentifier in pseudoElements)
            pseudoElementsStyle = pseudoElementsStyle.concat(pseudoElements[pseudoIdentifier].orderedStyles);

        var orderedPseudoStyles = uniqueOrderedStyles(pseudoElementsStyle);
        var pseudoElementSelectors = [];

        for (var style of orderedPseudoStyles)
            pseudoElementSelectors.push({ style, selectorText: style.ownerRule.selectorText.replace(/:{1,2}[\w-]+\s*/g, " ").trimRight() });

        // Reverse the array to allow ensure that splicing the array will not mess with the order.
        if (pseudoElementSelectors.length)
            pseudoElementSelectors.reverse();

        var addedNewRuleButton = false;
        this._ruleMediaAndInherticanceList = [];

        var orderedStyles = uniqueOrderedStyles(this.nodeStyles.orderedStyles);
        for (var i = 0; i < orderedStyles.length; ++i) {
            var style = orderedStyles[i];

            var hasMediaOrInherited = [];

            if (style.type === WebInspector.CSSStyleDeclaration.Type.Rule && !addedNewRuleButton)
                addNewRuleButton.call(this);

            if (previousSection && previousSection.style.node !== style.node) {
                previousSection.lastInGroup = true;

                var prefixElement = document.createElement("strong");
                prefixElement.textContent = WebInspector.UIString("Inherited From: ");

                var inheritedLabel = document.createElement("div");
                inheritedLabel.className = "label";
                inheritedLabel.appendChild(prefixElement);
                inheritedLabel.appendChild(WebInspector.linkifyNodeReference(style.node));
                newDOMFragment.appendChild(inheritedLabel);

                hasMediaOrInherited.push(inheritedLabel);
            }

            // Only include the media list if it is different from the previous media list shown.
            var currentMediaList = filteredMediaList(style.ownerRule && style.ownerRule.mediaList);
            if (!mediaListsEqual(previousMediaList, currentMediaList)) {
                previousMediaList = currentMediaList;

                // Break the section group even if the media list is empty. That way the user knows
                // the previous displayed media list does not apply to the next section.
                if (previousSection)
                    previousSection.lastInGroup = true;

                for (var j = 0; j < currentMediaList.length; ++j) {
                    var media = currentMediaList[j];

                    var prefixElement = document.createElement("strong");
                    prefixElement.textContent = WebInspector.UIString("Media: ");

                    var mediaLabel = document.createElement("div");
                    mediaLabel.className = "label";
                    mediaLabel.appendChild(prefixElement);
                    mediaLabel.appendChild(document.createTextNode(media.text));

                    if (media.sourceCodeLocation) {
                        mediaLabel.appendChild(document.createTextNode(" \u2014 "));
                        mediaLabel.appendChild(WebInspector.createSourceCodeLocationLink(media.sourceCodeLocation, true));
                    }

                    newDOMFragment.appendChild(mediaLabel);

                    hasMediaOrInherited.push(mediaLabel);
                }
            }

            if (!hasMediaOrInherited.length && style.type !== WebInspector.CSSStyleDeclaration.Type.Inline) {
                if (previousSection && !previousSection.lastInGroup)
                    hasMediaOrInherited = this._ruleMediaAndInherticanceList.lastValue;
                else {
                    var prefixElement = document.createElement("strong");
                    prefixElement.textContent = WebInspector.UIString("Media: ");

                    var mediaLabel = document.createElement("div");
                    mediaLabel.className = "label";
                    mediaLabel.appendChild(prefixElement);
                    mediaLabel.appendChild(document.createTextNode("all"));

                    newDOMFragment.appendChild(mediaLabel);
                    hasMediaOrInherited.push(mediaLabel);
                }
            }

            this._ruleMediaAndInherticanceList.push(hasMediaOrInherited);

            var ownerRule = style.ownerRule;

            if (pseudoElementSelectors.length && ownerRule) {
                for (var j = pseudoElementSelectors.length - 1; j >= 0; --j) {
                    var pseudoElement = pseudoElementSelectors[j];
                    var matchedSelectorText = ownerRule.matchedSelectorText;

                    if (ownerRule.type === WebInspector.CSSRule.Type.UserAgent || style.inerhited
                    || (pseudoElement.lastMatchingSelector && pseudoElement.lastMatchingSelector !== matchedSelectorText)) {
                        appendStyleSection.call(this, pseudoElement.style);
                        pseudoElementSelectors.splice(j, 1);
                        this._ruleMediaAndInherticanceList.push(hasMediaOrInherited);

                        continue;
                    }

                    if (matchedSelectorText.includes(pseudoElement.selectorText) || !ownerRule.selectorIsGreater(pseudoElement.style.ownerRule.selectors))
                        pseudoElement.lastMatchingSelector = matchedSelectorText;
                }
            }

            appendStyleSection.call(this, style);
        }

        // Just in case there are any pseudo-selectors left that haven't been added.
        if (pseudoElementSelectors.length) {
            for (var i = pseudoElementSelectors.length - 1; i >= 0; --i)
                appendStyleSection.call(this, pseudoElementSelectors[i].style);
        }

        if (!addedNewRuleButton)
            addNewRuleButton.call(this);

        if (previousSection)
            previousSection.lastInGroup = true;

        this.element.removeChildren();
        this.element.appendChild(newDOMFragment);
        this.element.appendChild(this._emptyFilterResultsElement);

        this._sections = newSections;

        for (var i = 0; i < this._sections.length; ++i)
            this._sections[i].updateLayout();

        super.refresh();
    }

    scrollToSectionAndHighlightProperty(property)
    {
        if (!this._visible) {
            this._propertyToSelectAndHighlight = property;
            return false;
        }

        for (var section of this._sections) {
            if (section.highlightProperty(property))
                return true;
        }

        return false;
    }

    cssStyleDeclarationSectionEditorFocused(ignoredSection)
    {
        for (var section of this._sections) {
            if (section !== ignoredSection)
                section.clearSelection();
        }
    }

    cssStyleDeclarationSectionEditorNextRule(currentSection)
    {
        currentSection.clearSelection();

        var index = this._sections.indexOf(currentSection);
        this._sections[index < this._sections.length - 1 ? index + 1 : 0].focusRuleSelector();
    }

    cssStyleDeclarationSectionEditorPreviousRule(currentSection, selectLastProperty) {
        currentSection.clearSelection();

        if (selectLastProperty || currentSection.selectorLocked) {
            var index = this._sections.indexOf(currentSection);
            index = index > 0 ? index - 1 : this._sections.length - 1;

            var section = this._sections[index];
            while (section.locked) {
                index = index > 0 ? index - 1 : this._sections.length - 1;
                section = this._sections[index];
            }

            section.focus();
            section.selectLastProperty();
            return;
        }

        currentSection.focusRuleSelector(true);
    }

    filterDidChange(filterBar)
    {
        for (var labels of this._ruleMediaAndInherticanceList) {
            for (var i = 0; i < labels.length; ++i) {
                labels[i].classList.toggle(WebInspector.CSSStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName, filterBar.hasActiveFilters());

                if (i === labels.length - 1)
                    labels[i].classList.toggle("filter-matching-label", filterBar.hasActiveFilters());
            }
        }

        var matchFound = !filterBar.hasActiveFilters();
        for (var i = 0; i < this._sections.length; ++i) {
            var section = this._sections[i];

            if (section.findMatchingPropertiesAndSelectors(filterBar.filters.text) && filterBar.hasActiveFilters()) {
                if (this._ruleMediaAndInherticanceList[i].length) {
                    for (var label of this._ruleMediaAndInherticanceList[i])
                        label.classList.remove(WebInspector.CSSStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName);
                } else
                    section.element.classList.add(WebInspector.CSSStyleDetailsSidebarPanel.FilterMatchingSectionHasLabelClassName);
                
                matchFound = true;
            }
        }

        this.element.classList.toggle("filter-non-matching", !matchFound);
    }

    // Protected

    shown()
    {
        super.shown();

        // Associate the style and section objects so they can be reused.
        // Also update the layout in case we changed widths while hidden.
        for (var i = 0; i < this._sections.length; ++i) {
            var section = this._sections[i];
            section.style.__rulesSection = section;
            section.updateLayout();
        }
    }

    hidden()
    {
        super.hidden();

        // Disconnect the style and section objects so they have a chance
        // to release their objects when this panel is not visible.
        for (var i = 0; i < this._sections.length; ++i)
            delete this._sections[i].style.__rulesSection;
    }

    widthDidChange()
    {
        for (var i = 0; i < this._sections.length; ++i)
            this._sections[i].updateLayout();
    }

    nodeStylesRefreshed(event)
    {
        super.nodeStylesRefreshed(event);
        
        if (this._propertyToSelectAndHighlight) {
            this.scrollToSectionAndHighlightProperty(this._propertyToSelectAndHighlight);
            this._propertyToSelectAndHighlight = null;
        }

        if (this._previousFocusedSection && this._visible) {
            this._previousFocusedSection.focus();
            this._previousFocusedSection = null;
        }
    }

    // Private

    _newRuleClicked(event)
    {
        this._focusNextNewInspectorRule = true;
        this.nodeStyles.addEmptyRule();
    }
};
