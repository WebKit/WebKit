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

WI.SpreadsheetCSSStyleDeclarationSection = class SpreadsheetCSSStyleDeclarationSection extends WI.View
{
    constructor(delegate, style)
    {
        console.assert(style instanceof WI.CSSStyleDeclaration, style);

        let element = document.createElement("section");
        element.classList.add("spreadsheet-css-declaration");
        super(element);

        this._delegate = delegate || null;
        this._style = style;
        this._propertiesEditor = null;
        this._selectorElements = [];
        this._wasFocused = false;
    }

    // Public

    get style() { return this._style; }

    get propertiesEditor() { return this._propertiesEditor; }

    get editable()
    {
        return this._style.editable;
    }

    initialLayout()
    {
        super.initialLayout();

        this._headerElement = document.createElement("div");
        this._headerElement.classList.add("header");

        this._originElement = document.createElement("span");
        this._originElement.classList.add("origin");
        this._headerElement.append(this._originElement);

        this._selectorElement = document.createElement("span");
        this._selectorElement.classList.add("selector");
        this._headerElement.append(this._selectorElement);

        let openBrace = document.createElement("span");
        openBrace.classList.add("open-brace");
        openBrace.textContent = " {";
        this._headerElement.append(openBrace);

        if (this._style.selectorEditable) {
            this._selectorTextField = new WI.SpreadsheetSelectorField(this, this._selectorElement);
            this._selectorElement.tabIndex = 0;
        }

        this._propertiesEditor = new WI.SpreadsheetCSSStyleDeclarationEditor(this, this._style);
        this._propertiesEditor.element.classList.add("properties");

        let closeBrace = document.createElement("span");
        closeBrace.classList.add("close-brace");
        closeBrace.textContent = "}";

        this._element.append(this._createMediaHeader(), this._headerElement);
        this.addSubview(this._propertiesEditor);
        this._propertiesEditor.needsLayout();
        this._element.append(closeBrace);

        if (!this._style.editable)
            this._element.classList.add("locked");
        else if (!this._style.ownerRule)
            this._element.classList.add("selector-locked");

        if (this._style.editable) {
            this.element.addEventListener("click", this._handleClick.bind(this));
            this.element.addEventListener("mousedown", this._handleMouseDown.bind(this));
        }
    }

    layout()
    {
        super.layout();

        this._renderOrigin();
        this._renderSelector();
    }

    startEditingRuleSelector()
    {
        this._selectorElement.focus();
    }

    highlightProperty(property)
    {
        // When navigating from the Computed panel to the Styles panel, the latter
        // could be empty. Layout all properties so they can be highlighted.
        if (!this.didInitialLayout)
            this.updateLayout();

        if (this._propertiesEditor.highlightProperty(property)) {
            this._element.scrollIntoView();
            return true;
        }

        return false;
    }

    cssStyleDeclarationTextEditorStartEditingRuleSelector()
    {
        this.startEditingRuleSelector();
    }

    spreadsheetSelectorFieldDidChange(direction)
    {
        let selectorText = this._selectorElement.textContent.trim();

        if (!selectorText || selectorText === this._style.ownerRule.selectorText)
            this._discardSelectorChange();
        else {
            this._style.ownerRule.singleFireEventListener(WI.CSSRule.Event.SelectorChanged, this._renderSelector, this);
            this._style.ownerRule.selectorText = selectorText;
        }

        if (!direction) {
            // Don't do anything when it's a blur event.
            return;
        }

        if (direction === "forward")
            this._propertiesEditor.startEditingFirstProperty();
        else if (direction === "backward") {
            if (typeof this._delegate.cssStyleDeclarationSectionStartEditingPreviousRule === "function")
                this._delegate.cssStyleDeclarationSectionStartEditingPreviousRule(this);
        }
    }

    spreadsheetSelectorFieldDidDiscard()
    {
        this._discardSelectorChange();
    }

    cssStyleDeclarationEditorStartEditingAdjacentRule(toPreviousRule)
    {
        if (!this._delegate)
            return;

        if (toPreviousRule && typeof this._delegate.cssStyleDeclarationSectionStartEditingPreviousRule === "function")
            this._delegate.cssStyleDeclarationSectionStartEditingPreviousRule(this);
        else if (!toPreviousRule && typeof this._delegate.cssStyleDeclarationSectionStartEditingNextRule === "function")
            this._delegate.cssStyleDeclarationSectionStartEditingNextRule(this);
    }

    // Private

    _discardSelectorChange()
    {
        // Re-render selector for syntax highlighting.
        this._renderSelector();
    }

    _renderSelector()
    {
        this._selectorElement.removeChildren();
        this._selectorElements = [];

        let appendSelector = (selector, matched) => {
            console.assert(selector instanceof WI.CSSSelector);

            let selectorElement = this._selectorElement.appendChild(document.createElement("span"));
            selectorElement.textContent = selector.text;

            if (matched)
                selectorElement.classList.add(WI.SpreadsheetCSSStyleDeclarationSection.MatchedSelectorElementStyleClassName);

            let specificity = selector.specificity.map((number) => number.toLocaleString());
            if (specificity) {
                let tooltip = WI.UIString("Specificity: (%d, %d, %d)").format(...specificity);
                if (selector.dynamic) {
                    tooltip += "\n";
                    if (this._style.inherited)
                        tooltip += WI.UIString("Dynamically calculated for the parent element");
                    else
                        tooltip += WI.UIString("Dynamically calculated for the selected element");
                }
                selectorElement.title = tooltip;
            } else if (selector.dynamic) {
                let tooltip = WI.UIString("Specificity: No value for selected element");
                tooltip += "\n";
                tooltip += WI.UIString("Dynamically calculated for the selected element and did not match");
                selectorElement.title = tooltip;
            }

            this._selectorElements.push(selectorElement);
        };

        let appendSelectorTextKnownToMatch = (selectorText) => {
            let selectorElement = this._selectorElement.appendChild(document.createElement("span"));
            selectorElement.textContent = selectorText;
            selectorElement.classList.add(WI.SpreadsheetCSSStyleDeclarationSection.MatchedSelectorElementStyleClassName);
        };

        switch (this._style.type) {
        case WI.CSSStyleDeclaration.Type.Rule:
            console.assert(this._style.ownerRule);

            var selectors = this._style.ownerRule.selectors;
            var matchedSelectorIndices = this._style.ownerRule.matchedSelectorIndices;
            var alwaysMatch = !matchedSelectorIndices.length;
            if (selectors.length) {
                let hasMatchingPseudoElementSelector = false;
                for (let i = 0; i < selectors.length; ++i) {
                    appendSelector(selectors[i], alwaysMatch || matchedSelectorIndices.includes(i));
                    if (i < selectors.length - 1)
                        this._selectorElement.append(", ");

                    if (matchedSelectorIndices.includes(i) && selectors[i].isPseudoElementSelector())
                        hasMatchingPseudoElementSelector = true;
                }
                this._element.classList.toggle("pseudo-element-selector", hasMatchingPseudoElementSelector);
            } else
                appendSelectorTextKnownToMatch(this._style.ownerRule.selectorText);

            break;

        case WI.CSSStyleDeclaration.Type.Inline:
            this._selectorElement.textContent = WI.UIString("Style Attribute");
            this._selectorElement.classList.add("style-attribute");
            break;

        case WI.CSSStyleDeclaration.Type.Attribute:
            appendSelectorTextKnownToMatch(this._style.node.displayName);
            break;
        }
    }

    _renderOrigin()
    {
        this._originElement.removeChildren();

        switch (this._style.type) {
        case WI.CSSStyleDeclaration.Type.Rule:
            console.assert(this._style.ownerRule);

            if (this._style.ownerRule.sourceCodeLocation) {
                let options = {
                    dontFloat: true,
                    ignoreNetworkTab: true,
                    ignoreSearchTab: true,
                };

                if (this._style.ownerStyleSheet.isInspectorStyleSheet()) {
                    options.nameStyle = WI.SourceCodeLocation.NameStyle.None;
                    options.prefix = WI.UIString("Inspector Style Sheet") + ":";
                }

                let sourceCodeLink = WI.createSourceCodeLocationLink(this._style.ownerRule.sourceCodeLocation, options);
                this._originElement.appendChild(sourceCodeLink);
            } else {
                let originString = "";

                switch (this._style.ownerRule.type) {
                case WI.CSSStyleSheet.Type.Author:
                    originString = WI.UIString("Author Stylesheet");
                    break;

                case WI.CSSStyleSheet.Type.User:
                    originString = WI.UIString("User Stylesheet");
                    break;

                case WI.CSSStyleSheet.Type.UserAgent:
                    originString = WI.UIString("User Agent Stylesheet");
                    break;

                case WI.CSSStyleSheet.Type.Inspector:
                    originString = WI.UIString("Web Inspector");
                    break;
                }

                console.assert(originString);
                if (originString)
                    this._originElement.append(originString);

                if (!this._style.editable) {
                    let styleTitle = "";
                    if (this._style.ownerRule && this._style.ownerRule.type === WI.CSSStyleSheet.Type.UserAgent)
                        styleTitle = WI.UIString("User Agent Stylesheet");
                    else
                        styleTitle = WI.UIString("Style rule");

                    this._originElement.title = WI.UIString("%s cannot be modified").format(styleTitle);
                }
            }
            break;

        case WI.CSSStyleDeclaration.Type.Attribute:
            this._originElement.append(WI.UIString("HTML Attributes"));
            break;
        }
    }

    _createMediaHeader()
    {
        if (!this._style.ownerRule)
            return "";

        console.assert(Array.isArray(this._style.ownerRule.mediaList));

        let mediaList = this._style.ownerRule.mediaList;
        let mediaText = mediaList.map((media) => media.text).join(", ");
        if (!mediaText || mediaText === "all" || mediaText === "screen")
            return "";

        let mediaElement = document.createElement("div");
        mediaElement.classList.add("header-media");

        let mediaLabel = mediaElement.appendChild(document.createElement("div"));
        mediaLabel.className = "media-label";
        mediaLabel.append("@media ", mediaText);
        mediaElement.append(mediaLabel);

        return mediaElement;
    }

    _handleMouseDown(event)
    {
        this._wasFocused = this._propertiesEditor.isFocused();
    }

    _handleClick(event)
    {
        if (this._wasFocused)
            return;

        event.stop();

        if (event.target.classList.contains(WI.SpreadsheetStyleProperty.StyleClassName)) {
            let propertyIndex = parseInt(event.target.dataset.propertyIndex);
            this._propertiesEditor.addBlankProperty(propertyIndex + 1);
            return;
        }

        if (event.target.isSelfOrDescendant(this._headerElement)) {
            this._propertiesEditor.addBlankProperty(0);
            return;
        }

        const appendAfterLast = -1;
        this._propertiesEditor.addBlankProperty(appendAfterLast);
    }
};

WI.SpreadsheetCSSStyleDeclarationSection.MatchedSelectorElementStyleClassName = "matched";
