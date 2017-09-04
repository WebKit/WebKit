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
        this._selectorElements = [];
    }

    // Public

    get style() { return this._style; }

    initialLayout()
    {
        super.initialLayout();

        this._headerElement = document.createElement("span");
        this._headerElement.classList.add("header");

        this._originElement = document.createElement("span");
        this._originElement.classList.add("origin");
        this._headerElement.append(this._originElement);

        this._selectorElement = document.createElement("span");
        this._selectorElement.classList.add("selector");
        this._headerElement.append(this._selectorElement);

        this._propertiesEditor = new WI.SpreadsheetCSSStyleDeclarationEditor(this, this._style);
        this._propertiesEditor.element.classList.add("properties");

        let openBrace = document.createElement("span");
        openBrace.classList.add("open-brace");
        openBrace.textContent = " {";

        let closeBrace = document.createElement("span");
        closeBrace.classList.add("close-brace");
        closeBrace.textContent = "}";

        this._element.append(this._headerElement, openBrace);
        this.addSubview(this._propertiesEditor);
        this._propertiesEditor.needsLayout(WI.View.LayoutReason.Dirty);
        this._element.append(closeBrace);

        if (!this._style.editable)
            this._element.classList.add("locked");
        else if (!this._style.ownerRule)
            this._element.classList.add("selector-locked");
    }

    layout()
    {
        super.layout();

        this._selectorElement.removeChildren();
        this._originElement.removeChildren();

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

            let selectors = this._style.ownerRule.selectors;
            let matchedSelectorIndices = this._style.ownerRule.matchedSelectorIndices;
            let alwaysMatch = !matchedSelectorIndices.length;
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

        case WI.CSSStyleDeclaration.Type.Inline:
            this._selectorElement.textContent = WI.UIString("Style Attribute");
            this._selectorElement.classList.add("style-attribute");
            break;

        case WI.CSSStyleDeclaration.Type.Attribute:
            appendSelectorTextKnownToMatch(this._style.node.displayName);
            this._originElement.append(WI.UIString("HTML Attributes"));
            break;
        }
    }

    cssStyleDeclarationTextEditorFocused()
    {
        if (this._delegate && typeof this._delegate.cssStyleDeclarationSectionEditorFocused === "function")
            this._delegate.cssStyleDeclarationSectionEditorFocused(this);
    }

    get locked()
    {
        return !this._style.editable;
    }

    get selectorEditable()
    {
        return this._style.editable && this._style.ownerRule;
    }
};

WI.SpreadsheetCSSStyleDeclarationSection.MatchedSelectorElementStyleClassName = "matched";
