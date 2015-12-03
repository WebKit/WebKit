/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.VisualStyleSelectorTreeItem = class VisualStyleSelectorTreeItem extends WebInspector.GeneralTreeElement
{
    constructor(style, title, subtitle)
    {
        var iconClassName;
        switch (style.type) {
        case WebInspector.CSSStyleDeclaration.Type.Rule:
            console.assert(style.ownerRule instanceof WebInspector.CSSRule, style.ownerRule);

            if (style.inherited)
                iconClassName = WebInspector.CSSStyleDeclarationSection.InheritedStyleRuleIconStyleClassName;
            else if (style.ownerRule.type === WebInspector.CSSStyleSheet.Type.Author)
                iconClassName = WebInspector.CSSStyleDeclarationSection.AuthorStyleRuleIconStyleClassName;
            else if (style.ownerRule.type === WebInspector.CSSStyleSheet.Type.User)
                iconClassName = WebInspector.CSSStyleDeclarationSection.UserStyleRuleIconStyleClassName;
            else if (style.ownerRule.type === WebInspector.CSSStyleSheet.Type.UserAgent)
                iconClassName = WebInspector.CSSStyleDeclarationSection.UserAgentStyleRuleIconStyleClassName;
            else if (style.ownerRule.type === WebInspector.CSSStyleSheet.Type.Inspector)
                iconClassName = WebInspector.CSSStyleDeclarationSection.InspectorStyleRuleIconStyleClassName;
            break;

        case WebInspector.CSSStyleDeclaration.Type.Inline:
        case WebInspector.CSSStyleDeclaration.Type.Attribute:
            if (style.inherited)
                iconClassName = WebInspector.CSSStyleDeclarationSection.InheritedElementStyleRuleIconStyleClassName;
            else
                iconClassName = WebInspector.DOMTreeElementPathComponent.DOMElementIconStyleClassName;
            break;
        }

        title = title.trim();

        super(["visual-style-selector-item", iconClassName], title, subtitle, style);

        this._iconClassName = iconClassName;
        this._lastValue = title;
        this._enableEditing = true;
    }

    // Public

    get iconClassName()
    {
        return this._iconClassName;
    }

    get selectorText()
    {
        var titleText = this._mainTitleElement.textContent;
        if (!titleText || !titleText.length)
            titleText = this._mainTitle;

        return titleText.trim();
    }

    // Protected

    onattach()
    {
        super.onattach();

        this._listItemNode.addEventListener("mouseover", this._highlightNodesWithSelector.bind(this));
        this._listItemNode.addEventListener("mouseout", this._hideDOMNodeHighlight.bind(this));
        this._listItemNode.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this));

        this._checkboxElement = document.createElement("input");
        this._checkboxElement.type = "checkbox";
        this._checkboxElement.checked = !this.representedObject[WebInspector.VisualStyleDetailsPanel.StyleDisabledSymbol];
        this._updateCheckboxTitle();
        this._checkboxElement.addEventListener("change", this._handleCheckboxChanged.bind(this));
        this._listItemNode.insertBefore(this._checkboxElement, this._iconElement);

        this._mainTitleElement.spellcheck = false;
        this._mainTitleElement.addEventListener("mousedown", this._handleMainTitleMouseDown.bind(this));
        this._mainTitleElement.addEventListener("keydown", this._handleMainTitleKeyDown.bind(this));
        this._mainTitleElement.addEventListener("keyup", this._highlightNodesWithSelector.bind(this));
        this._mainTitleElement.addEventListener("blur", this._commitSelector.bind(this));

        this.representedObject.addEventListener(WebInspector.CSSStyleDeclaration.Event.InitialTextModified, this._styleTextModified, this);
        if (this.representedObject.ownerRule)
            this.representedObject.ownerRule.addEventListener(WebInspector.CSSRule.Event.SelectorChanged, this._selectorChanged, this);

        this._styleTextModified();
    }

    ondeselect()
    {
        this._listItemNode.classList.remove("editable");
    }

    // Private

    _highlightNodesWithSelector()
    {
        if (!this.representedObject.ownerRule) {
            WebInspector.domTreeManager.highlightDOMNode(this.representedObject.node.id);
            return;
        }

        WebInspector.domTreeManager.highlightSelector(this.selectorText, this.representedObject.node.ownerDocument.frameIdentifier);
    }

    _hideDOMNodeHighlight()
    {
        WebInspector.domTreeManager.hideDOMNodeHighlight();
    }

    _handleContextMenuEvent(event)
    {
        var contextMenu = new WebInspector.ContextMenu(event);

        if (this.representedObject.ownerRule) {
            contextMenu.appendItem(WebInspector.UIString("Show Source"), function() {
                if (event.metaKey)
                    WebInspector.showOriginalUnformattedSourceCodeLocation(this.representedObject.ownerRule.sourceCodeLocation);
                else
                    WebInspector.showSourceCodeLocation(this.representedObject.ownerRule.sourceCodeLocation);
            }.bind(this));
        }

        contextMenu.appendItem(WebInspector.UIString("Copy Rule"), function() {
            InspectorFrontendHost.copyText(this.representedObject.generateCSSRuleString());
        }.bind(this));

        contextMenu.appendItem(WebInspector.UIString("Reset"), function() {
            this.representedObject.resetText();
            this.dispatchEventToListeners(WebInspector.VisualStyleSelectorTreeItem.Event.StyleTextReset);
        }.bind(this));

        contextMenu.show();
    }

    _handleCheckboxChanged(event)
    {
        this._updateCheckboxTitle();
        this.dispatchEventToListeners(WebInspector.VisualStyleSelectorTreeItem.Event.CheckboxChanged, {enabled: this._checkboxElement.checked});
    }

    _updateCheckboxTitle()
    {
        if (this._checkboxElement.checked)
            this._checkboxElement.title = WebInspector.UIString("Click to disable the selected rule");
        else
            this._checkboxElement.title = WebInspector.UIString("Click to enable the selected rule");
    }

    _handleMainTitleMouseDown(event)
    {
        if (event.button !== 0 || event.ctrlKey)
            return;

        this._listItemNode.classList.toggle("editable", this.selected);
    }

    _handleMainTitleKeyDown(event)
    {
        this._highlightNodesWithSelector();

        var enterKeyCode = WebInspector.KeyboardShortcut.Key.Enter.keyCode;
        if (event.keyCode === enterKeyCode)
            this._mainTitleElement.blur();
    }

    _commitSelector()
    {
        this._hideDOMNodeHighlight();
        this._listItemNode.classList.remove("editable");
        this._updateTitleTooltip();

        var value = this.selectorText;
        if (value === this._lastValue && this._valid)
            return;

        this.representedObject.ownerRule.selectorText = value;
    }

    _styleTextModified()
    {
        this._listItemNode.classList.toggle("modified", this.representedObject.modified);
    }

    _selectorChanged(event)
    {
        this._valid = event && event.data && event.data.valid;
        this._listItemNode.classList.toggle("selector-invalid", !this._valid);
        var invalidTitle = WebInspector.UIString("The selector '%s' is invalid.").format(this.selectorText);
        this._iconElement.title = !this._valid ? invalidTitle : null;
    }
};

WebInspector.VisualStyleSelectorTreeItem.Event = {
    StyleTextReset: "visual-style-selector-item-style-text-reset",
    CheckboxChanged: "visual-style-selector-item-checkbox-changed"
};
