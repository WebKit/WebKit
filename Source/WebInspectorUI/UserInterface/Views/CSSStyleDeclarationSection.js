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

WI.CSSStyleDeclarationSection = class CSSStyleDeclarationSection extends WI.Object
{
    constructor(delegate, style)
    {
        console.assert(style instanceof WI.CSSStyleDeclaration, style);

        super();

        this._delegate = delegate || null;

        this._style = style || null;
        this._selectorElements = [];
        this._ruleDisabled = false;
        this._hasInvalidSelector = false;

        this._element = document.createElement("div");
        this._element.classList.add("style-declaration-section");

        new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "S", this._save.bind(this), this._element);
        new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "S", this._save.bind(this), this._element);

        this._headerElement = document.createElement("div");
        this._headerElement.classList.add("header");

        if (!style.editable) {
            let lockedIconElement = this._headerElement.createChild("img", "locked-icon");

            let styleLabel;
            if (style.ownerRule && style.ownerRule.type === WI.CSSStyleSheet.Type.UserAgent)
                styleLabel = WI.UIString("User Agent Stylesheet");
            else
                styleLabel = WI.UIString("Style rule");

            lockedIconElement.title = WI.UIString("%s cannot be modified").format(styleLabel);
        }

        this._iconElement = this._headerElement.createChild("img", "icon");

        if (this.selectorEditable) {
            this._selectorInput = this._headerElement.createChild("textarea");
            this._selectorInput.spellcheck = false;
            this._selectorInput.dir = "ltr";
            this._selectorInput.tabIndex = -1;
            this._selectorInput.addEventListener("mouseover", this._handleMouseOver.bind(this));
            this._selectorInput.addEventListener("mousemove", this._handleMouseMove.bind(this));
            this._selectorInput.addEventListener("mouseout", this._handleMouseOut.bind(this));
            this._selectorInput.addEventListener("keydown", this._handleKeyDown.bind(this));
            this._selectorInput.addEventListener("keypress", this._handleKeyPress.bind(this));
            this._selectorInput.addEventListener("input", this._handleInput.bind(this));
            this._selectorInput.addEventListener("paste", this._handleSelectorPaste.bind(this));
            this._selectorInput.addEventListener("blur", this._handleBlur.bind(this));
        }

        this._selectorElement = this._headerElement.createChild("span", "selector");
        if (!this.selectorEditable) {
            this._selectorElement.addEventListener("mouseover", this._handleMouseOver.bind(this));
            this._selectorElement.addEventListener("mouseout", this._handleMouseOut.bind(this));
        }

        this._originElement = this._headerElement.createChild("span", "origin");

        this._propertiesElement = document.createElement("div");
        this._propertiesElement.classList.add("properties");

        this._editorActive = false;
        this._propertiesTextEditor = new WI.CSSStyleDeclarationTextEditor(this, style);
        this._propertiesTextEditor.addEventListener(WI.CSSStyleDeclarationTextEditor.Event.ContentChanged, this._editorContentChanged.bind(this));
        this._propertiesTextEditor.addEventListener(WI.CSSStyleDeclarationTextEditor.Event.Blurred, this._editorBlurred.bind(this));
        this._propertiesElement.appendChild(this._propertiesTextEditor.element);

        this._element.appendChild(this._headerElement);
        this._element.appendChild(this._propertiesElement);

        let iconClassName = null;
        switch (style.type) {
        case WI.CSSStyleDeclaration.Type.Rule:
            console.assert(style.ownerRule);

            if (style.inherited)
                iconClassName = WI.CSSStyleDeclarationSection.InheritedStyleRuleIconStyleClassName;
            else if (style.ownerRule.type === WI.CSSStyleSheet.Type.Author)
                iconClassName = WI.CSSStyleDeclarationSection.AuthorStyleRuleIconStyleClassName;
            else if (style.ownerRule.type === WI.CSSStyleSheet.Type.User)
                iconClassName = WI.CSSStyleDeclarationSection.UserStyleRuleIconStyleClassName;
            else if (style.ownerRule.type === WI.CSSStyleSheet.Type.UserAgent)
                iconClassName = WI.CSSStyleDeclarationSection.UserAgentStyleRuleIconStyleClassName;
            else if (style.ownerRule.type === WI.CSSStyleSheet.Type.Inspector)
                iconClassName = WI.CSSStyleDeclarationSection.InspectorStyleRuleIconStyleClassName;
            break;

        case WI.CSSStyleDeclaration.Type.Inline:
        case WI.CSSStyleDeclaration.Type.Attribute:
            if (style.inherited)
                iconClassName = WI.CSSStyleDeclarationSection.InheritedElementStyleRuleIconStyleClassName;
            else
                iconClassName = WI.DOMTreeElementPathComponent.DOMElementIconStyleClassName;
            break;
        }

        if (style.editable) {
            this._iconElement.classList.add("toggle-able");
            this._iconElement.title = WI.UIString("Comment All Properties");
            this._iconElement.addEventListener("click", this._handleIconElementClicked.bind(this));
        }

        console.assert(iconClassName);
        this._element.classList.add(iconClassName);

        if (!style.editable)
            this._element.classList.add(WI.CSSStyleDeclarationSection.LockedStyleClassName);
        else if (style.ownerRule)
            this._style.ownerRule.addEventListener(WI.CSSRule.Event.SelectorChanged, this._updateSelectorIcon.bind(this));
        else
            this._element.classList.add(WI.CSSStyleDeclarationSection.SelectorLockedStyleClassName);

        this.refresh();

        this._headerElement.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this));
    }

    // Public

    get element()
    {
        return this._element;
    }

    get style()
    {
        return this._style;
    }

    get lastInGroup()
    {
        return this._element.classList.contains(WI.CSSStyleDeclarationSection.LastInGroupStyleClassName);
    }

    set lastInGroup(last)
    {
        if (last)
            this._element.classList.add(WI.CSSStyleDeclarationSection.LastInGroupStyleClassName);
        else
            this._element.classList.remove(WI.CSSStyleDeclarationSection.LastInGroupStyleClassName);
    }

    get focused()
    {
        return this._propertiesTextEditor.focused;
    }

    focus()
    {
        this._propertiesTextEditor.focus();
    }

    refresh()
    {
        this._selectorElement.removeChildren();
        this._originElement.removeChildren();
        this._selectorElements = [];

        this._originElement.append(` ${emDash} `);

        function appendSelector(selector, matched)
        {
            console.assert(selector instanceof WI.CSSSelector);

            let selectorElement = document.createElement("span");
            selectorElement.textContent = selector.text;

            if (matched)
                selectorElement.classList.add(WI.CSSStyleDeclarationSection.MatchedSelectorElementStyleClassName);

            let specificity = selector.specificity;
            if (specificity) {
                let tooltip = WI.UIString("Specificity: (%d, %d, %d)").format(specificity[0], specificity[1], specificity[2]);
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

            this._selectorElement.appendChild(selectorElement);
            this._selectorElements.push(selectorElement);
        }

        function appendSelectorTextKnownToMatch(selectorText)
        {
            let selectorElement = document.createElement("span");
            selectorElement.textContent = selectorText;
            selectorElement.classList.add(WI.CSSStyleDeclarationSection.MatchedSelectorElementStyleClassName);
            this._selectorElement.appendChild(selectorElement);
        }

        switch (this._style.type) {
        case WI.CSSStyleDeclaration.Type.Rule:
            console.assert(this._style.ownerRule);

            var selectors = this._style.ownerRule.selectors;
            var matchedSelectorIndices = this._style.ownerRule.matchedSelectorIndices;
            var alwaysMatch = !matchedSelectorIndices.length;
            if (selectors.length) {
                let hasMatchingPseudoElementSelector = false;
                for (let i = 0; i < selectors.length; ++i) {
                    appendSelector.call(this, selectors[i], alwaysMatch || matchedSelectorIndices.includes(i));
                    if (i < selectors.length - 1)
                        this._selectorElement.append(", ");

                    if (matchedSelectorIndices.includes(i) && selectors[i].isPseudoElementSelector())
                        hasMatchingPseudoElementSelector = true;
                }
                this._element.classList.toggle(WI.CSSStyleDeclarationSection.PseudoElementSelectorStyleClassName, hasMatchingPseudoElementSelector);
            } else
                appendSelectorTextKnownToMatch.call(this, this._style.ownerRule.selectorText);

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
                let originString;
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
            }

            break;

        case WI.CSSStyleDeclaration.Type.Inline:
            appendSelectorTextKnownToMatch.call(this, this._style.node.displayName);
            this._originElement.append(WI.UIString("Style Attribute"));
            break;

        case WI.CSSStyleDeclaration.Type.Attribute:
            appendSelectorTextKnownToMatch.call(this, this._style.node.displayName);
            this._originElement.append(WI.UIString("HTML Attributes"));
            break;
        }

        this._updateSelectorIcon();
        if (this._selectorInput)
            this._selectorInput.value = this._selectorElement.textContent;
    }

    refreshEditor()
    {
        this._propertiesTextEditor.refresh();
    }

    highlightProperty(property)
    {
        if (this._propertiesTextEditor.highlightProperty(property)) {
            this._element.scrollIntoView();
            return true;
        }

        return false;
    }

    findMatchingPropertiesAndSelectors(needle)
    {
        this._element.classList.remove(WI.GeneralStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName, WI.GeneralStyleDetailsSidebarPanel.FilterMatchingSectionHasLabelClassName);

        var hasMatchingSelector = false;

        for (var selectorElement of this._selectorElements) {
            selectorElement.classList.remove(WI.GeneralStyleDetailsSidebarPanel.FilterMatchSectionClassName);

            if (needle && selectorElement.textContent.includes(needle)) {
                selectorElement.classList.add(WI.GeneralStyleDetailsSidebarPanel.FilterMatchSectionClassName);
                hasMatchingSelector = true;
            }
        }

        if (!needle) {
            this._propertiesTextEditor.resetFilteredProperties();
            return false;
        }

        var hasMatchingProperty = this._propertiesTextEditor.findMatchingProperties(needle);

        if (!hasMatchingProperty && !hasMatchingSelector) {
            this._element.classList.add(WI.GeneralStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName);
            return false;
        }

        return true;
    }

    updateLayout()
    {
        this._propertiesTextEditor.updateLayout();
    }

    clearSelection()
    {
        this._propertiesTextEditor.clearSelection();
    }

    cssStyleDeclarationTextEditorFocused()
    {
        if (typeof this._delegate.cssStyleDeclarationSectionEditorFocused === "function")
            this._delegate.cssStyleDeclarationSectionEditorFocused(this);
    }

    cssStyleDeclarationTextEditorSwitchRule(reverse)
    {
        if (!this._delegate)
            return;

        if (reverse && typeof this._delegate.cssStyleDeclarationSectionEditorPreviousRule === "function")
            this._delegate.cssStyleDeclarationSectionEditorPreviousRule(this);
        else if (!reverse && typeof this._delegate.cssStyleDeclarationSectionEditorNextRule === "function")
            this._delegate.cssStyleDeclarationSectionEditorNextRule(this);
    }

    focusRuleSelector(reverse)
    {
        if (!this.selectorEditable && !this.locked) {
            this.focus();
            return;
        }

        if (this.locked) {
            this.cssStyleDeclarationTextEditorSwitchRule(reverse);
            return;
        }

        let selection = window.getSelection();
        selection.removeAllRanges();

        this._element.scrollIntoViewIfNeeded();

        if (this._selectorInput) {
            this._selectorInput.focus();
            this._selectorInput.selectionStart = 0;
            this._selectorInput.selectionEnd = this._selectorInput.value.length;
        } else {
            let range = document.createRange();
            range.selectNodeContents(this._selectorElement);
            selection.addRange(range);
        }
    }

    selectLastProperty()
    {
        this._propertiesTextEditor.selectLastProperty();
    }

    get selectorEditable()
    {
        return !this.locked && this._style.ownerRule;
    }

    get locked()
    {
        return !this._style.editable;
    }

    get editorActive()
    {
        return this._editorActive;
    }

    // Private

    get _currentSelectorText()
    {
        let selectorText = this.selectorEditable ? this._selectorInput.value : this._selectorElement.textContent;
        if (!selectorText || !selectorText.length) {
            if (!this._style.ownerRule)
                return "";

            selectorText = this._style.ownerRule.selectorText;
        }

        return selectorText.trim();
    }

    _handleSelectorPaste(event)
    {
        if (this._style.type === WI.CSSStyleDeclaration.Type.Inline || !this._style.ownerRule)
            return;

        if (!event || !event.clipboardData)
            return;

        let data = event.clipboardData.getData("text/plain");
        if (!data)
            return;

        function parseTextForRule(text)
        {
            let containsBraces = /[{}]/;
            if (!containsBraces.test(text))
                return [];

            let match = text.match(/([^{]+){([\s\S]*)}/);
            if (!match)
                return [];

            // If the match "body" contains braces, parse that body as if it were a rule.
            // This will usually happen if the user includes a media query in the copied text.
            return containsBraces.test(match[2]) ? parseTextForRule(match[2]) : match;
        }

        let [selector, value] = parseTextForRule(data);
        if (!selector || !value)
            return;

        this._style.nodeStyles.changeRule(this._style.ownerRule, selector.trim(), value);
        event.preventDefault();
    }

    _handleContextMenuEvent(event)
    {
        if (window.getSelection().toString().length)
            return;

        let contextMenu = WI.ContextMenu.createFromEvent(event);

        contextMenu.appendItem(WI.UIString("Copy Rule"), () => {
            InspectorFrontendHost.copyText(this._style.generateCSSRuleString());
        });

        if (this._style.inherited)
            return;

        contextMenu.appendItem(WI.UIString("Duplicate Selector"), () => {
            if (this._delegate && typeof this._delegate.focusEmptySectionWithStyle === "function") {
                let existingRules = this._style.nodeStyles.rulesForSelector(this._currentSelectorText);
                for (let rule of existingRules) {
                    if (this._delegate.focusEmptySectionWithStyle(rule.style))
                        return;
                }
            }

            this._style.nodeStyles.addRule(this._currentSelectorText);
        });

        // Only used one colon temporarily since single-colon pseudo elements are valid CSS.
        if (WI.CSSStyleManager.PseudoElementNames.some((className) => this._style.selectorText.includes(":" + className)))
            return;

        if (WI.CSSStyleManager.ForceablePseudoClasses.every((className) => !this._style.selectorText.includes(":" + className))) {
            contextMenu.appendSeparator();

            for (let pseudoClass of WI.CSSStyleManager.ForceablePseudoClasses) {
                if (pseudoClass === "visited" && this._style.node.nodeName() !== "A")
                    continue;

                let pseudoClassSelector = ":" + pseudoClass;

                contextMenu.appendItem(WI.UIString("Add %s Rule").format(pseudoClassSelector), () => {
                    this._style.node.setPseudoClassEnabled(pseudoClass, true);

                    let selector;
                    if (this._style.ownerRule)
                        selector = this._style.ownerRule.selectors.map((selector) => selector.text + pseudoClassSelector).join(", ");
                    else
                        selector = this._currentSelectorText + pseudoClassSelector;

                    this._style.nodeStyles.addRule(selector);
                });
            }
        }

        contextMenu.appendSeparator();

        for (let pseudoElement of WI.CSSStyleManager.PseudoElementNames) {
            let pseudoElementSelector = "::" + pseudoElement;
            const styleText = "content: \"\";";

            let existingSection = null;
            if (this._delegate && typeof this._delegate.sectionForStyle === "function") {
                let existingRules = this._style.nodeStyles.rulesForSelector(this._currentSelectorText + pseudoElementSelector);
                if (existingRules.length) {
                    // There shouldn't really ever be more than one pseudo-element rule
                    // that is not in a media query. As such, just focus the first rule
                    // on the assumption that it is the only one necessary.
                    existingSection = this._delegate.sectionForStyle(existingRules[0].style);
                }
            }

            let title = existingSection ? WI.UIString("Focus %s Rule") : WI.UIString("Create %s Rule");
            contextMenu.appendItem(title.format(pseudoElementSelector), () => {
                if (existingSection) {
                    existingSection.focus();
                    return;
                }

                let selector;
                if (this._style.ownerRule)
                    selector = this._style.ownerRule.selectors.map((selector) => selector.text + pseudoElementSelector).join(", ");
                else
                    selector = this._currentSelectorText + pseudoElementSelector;

                this._style.nodeStyles.addRule(selector, styleText);
            });
        }
    }

    _handleIconElementClicked()
    {
        if (this._hasInvalidSelector) {
            // This will revert the selector text to the original valid value.
            this.refresh();
            return;
        }

        this._ruleDisabled = this._ruleDisabled ? !this._propertiesTextEditor.uncommentAllProperties() : this._propertiesTextEditor.commentAllProperties();
        this._iconElement.title = this._ruleDisabled ? WI.UIString("Uncomment All Properties") : WI.UIString("Comment All Properties");
        this._element.classList.toggle("rule-disabled", this._ruleDisabled);
    }

    _highlightNodesWithSelector()
    {
        if (!this._style.ownerRule) {
            WI.domTreeManager.highlightDOMNode(this._style.node.id);
            return;
        }

        WI.domTreeManager.highlightSelector(this._currentSelectorText, this._style.node.ownerDocument.frameIdentifier);
    }

    _hideDOMNodeHighlight()
    {
        WI.domTreeManager.hideDOMNodeHighlight();
    }

    _handleMouseOver(event)
    {
        this._highlightNodesWithSelector();
    }

    _handleMouseMove(event)
    {
        if (this._hasInvalidSelector)
            return;

        // Attempts to find a selector element under the mouse so that the title (which contains the
        // specificity information) can be applied to _selectorInput, which will then display the
        // title if the user hovers long enough.
        for (let element of this._selectorElements) {
            let {top, right, bottom, left} = element.getBoundingClientRect();
            if (event.clientX >= left && event.clientX <= right && event.clientY >= top && event.clientY <= bottom) {
                this._selectorInput.title = element.title;
                return;
            }
        }

        this._selectorInput.title = "";
    }

    _handleMouseOut(event)
    {
        this._hideDOMNodeHighlight();
    }

    _save(event)
    {
        event.preventDefault();
        event.stopPropagation();

        if (this._style.type !== WI.CSSStyleDeclaration.Type.Rule) {
            // FIXME: Can't save CSS inside <style></style> <https://webkit.org/b/150357>
            InspectorFrontendHost.beep();
            return;
        }

        console.assert(this._style.ownerRule instanceof WI.CSSRule);
        console.assert(this._style.ownerRule.sourceCodeLocation instanceof WI.SourceCodeLocation);

        let sourceCode = this._style.ownerRule.sourceCodeLocation.sourceCode;
        if (sourceCode.type !== WI.Resource.Type.Stylesheet) {
            // FIXME: Can't save CSS inside style="" <https://webkit.org/b/150357>
            InspectorFrontendHost.beep();
            return;
        }

        var url;
        if (sourceCode.urlComponents.scheme === "data") {
            let mainResource = WI.frameResourceManager.mainFrame.mainResource;
            let pathDirectory = mainResource.url.slice(0, -mainResource.urlComponents.lastPathComponent.length);
            url = pathDirectory + "base64.css";
        } else
            url = sourceCode.url;

        const saveAs = event.shiftKey;
        WI.saveDataToFile({url: url, content: sourceCode.content}, saveAs);
    }

    _handleKeyDown(event)
    {
        if (event.keyCode === WI.KeyboardShortcut.Key.Enter.keyCode) {
            event.preventDefault();
            this.focus();
            return;
        }

        if (event.keyCode !== WI.KeyboardShortcut.Key.Tab.keyCode) {
            this._highlightNodesWithSelector();
            return;
        }

        if (event.shiftKey && this._delegate && typeof this._delegate.cssStyleDeclarationSectionEditorPreviousRule === "function") {
            event.preventDefault();
            this._delegate.cssStyleDeclarationSectionEditorPreviousRule(this, true);
            return;
        }

        if (!event.metaKey) {
            event.preventDefault();
            this.focus();
            this._propertiesTextEditor.selectFirstProperty();
            return;
        }
    }

    _handleKeyPress(event)
    {
        if (!event.altGraphKey && !event.altKey && !event.ctrlKey && !event.metaKey) {
            // Ensures that <textarea> does not scroll with added characters.  Since a
            // <textarea> does not expand to fit its content, appending the pressed character to the
            // end of the original (non-editable) selector element will ensure that the <textarea>
            // will be large enough to fit the selector without scrolling.
            this._selectorElement.append(String.fromCharCode(event.keyCode));
        }
    }

    _handleInput(event)
    {
        this._selectorElement.textContent = this._selectorInput.value;

        this._highlightNodesWithSelector();
    }

    _handleBlur(event)
    {
        this._hideDOMNodeHighlight();

        let newSelectorText = this._currentSelectorText.trim();
        if (!newSelectorText) {
            // Revert to the current selector (by doing a refresh) since the new selector is empty.
            this.refresh();
            return;
        }

        if (event.relatedTarget && event.relatedTarget.isDescendant(this.element)) {
            this._editorActive = true;
            this.focus();
        }

        this._style.ownerRule.selectorText = newSelectorText;
    }

    _updateSelectorIcon(event)
    {
        if (!this._style.ownerRule || !this._style.editable)
            return;

        this._hasInvalidSelector = event && event.data && !event.data.valid;
        this._element.classList.toggle("invalid-selector", !!this._hasInvalidSelector);
        if (this._hasInvalidSelector) {
            this._iconElement.title = WI.UIString("The selector “%s” is invalid.\nClick to revert to the previous selector.").format(this._selectorElement.textContent.trim());
            this._selectorInput.title = WI.UIString("Using previous selector “%s”").format(this._style.ownerRule.selectorText);
            return;
        }

        this._iconElement.title = this._ruleDisabled ? WI.UIString("Uncomment All Properties") : WI.UIString("Comment All Properties");
        this._selectorInput.title = "";
    }

    _editorContentChanged(event)
    {
        this._editorActive = true;
    }

    _editorBlurred(event)
    {
        this._editorActive = false;
        this.dispatchEventToListeners(WI.CSSStyleDeclarationSection.Event.Blurred);
    }
};

WI.CSSStyleDeclarationSection.Event = {
    Blurred: "css-style-declaration-sections-blurred"
};

WI.CSSStyleDeclarationSection.LockedStyleClassName = "locked";
WI.CSSStyleDeclarationSection.SelectorLockedStyleClassName = "selector-locked";
WI.CSSStyleDeclarationSection.LastInGroupStyleClassName = "last-in-group";
WI.CSSStyleDeclarationSection.MatchedSelectorElementStyleClassName = "matched";
WI.CSSStyleDeclarationSection.PseudoElementSelectorStyleClassName = "pseudo-element-selector";

WI.CSSStyleDeclarationSection.AuthorStyleRuleIconStyleClassName = "author-style-rule-icon";
WI.CSSStyleDeclarationSection.UserStyleRuleIconStyleClassName = "user-style-rule-icon";
WI.CSSStyleDeclarationSection.UserAgentStyleRuleIconStyleClassName = "user-agent-style-rule-icon";
WI.CSSStyleDeclarationSection.InspectorStyleRuleIconStyleClassName = "inspector-style-rule-icon";
WI.CSSStyleDeclarationSection.InheritedStyleRuleIconStyleClassName = "inherited-style-rule-icon";
WI.CSSStyleDeclarationSection.InheritedElementStyleRuleIconStyleClassName = "inherited-element-style-rule-icon";
