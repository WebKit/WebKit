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

WI.VisualStyleCommaSeparatedKeywordEditor = class VisualStyleCommaSeparatedKeywordEditor extends WI.VisualStylePropertyEditor
{
    constructor(propertyNames, text, longhandProperties, insertNewItemsBeforeSelected, layoutReversed)
    {
        super(propertyNames, text, null, null, "comma-separated-keyword-editor", layoutReversed);

        this._insertNewItemsBeforeSelected = insertNewItemsBeforeSelected || false;
        this._longhandProperties = longhandProperties || {};

        let listElement = document.createElement("ol");
        listElement.classList.add("visual-style-comma-separated-keyword-list");
        listElement.addEventListener("keydown", this._listElementKeyDown.bind(this));
        this.contentElement.appendChild(listElement);

        this._commaSeparatedKeywords = new WI.TreeOutline(listElement);
        this._commaSeparatedKeywords.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

        let controlContainer = document.createElement("div");
        controlContainer.classList.add("visual-style-comma-separated-keyword-controls");
        this.contentElement.appendChild(controlContainer);

        let addGlyphElement = WI.ImageUtilities.useSVGSymbol("Images/Plus13.svg", "visual-style-add-comma-separated-keyword");
        addGlyphElement.addEventListener("click", this._addEmptyCommaSeparatedKeyword.bind(this));
        controlContainer.appendChild(addGlyphElement);

        let removeGlyphElement = WI.ImageUtilities.useSVGSymbol("Images/Minus.svg", "visual-style-remove-comma-separated-keyword", WI.UIString("Remove selected item"));
        removeGlyphElement.addEventListener("click", this._removeSelectedCommaSeparatedKeyword.bind(this));
        controlContainer.appendChild(removeGlyphElement);
    }

    // Public

    set selectedTreeElementValue(text)
    {
        let selectedTreeElement = this._commaSeparatedKeywords.selectedTreeElement;
        if (!selectedTreeElement)
            return;

        selectedTreeElement.element.classList.toggle("no-value", !text || !text.length);
        selectedTreeElement.mainTitle = text;
        this._valueDidChange();
    }

    get value()
    {
        if (!this._commaSeparatedKeywords.hasChildren)
            return "";

        let value = "";
        for (let treeItem of this._commaSeparatedKeywords.children) {
            if (this._treeElementIsEmpty(treeItem))
                continue;

            if (value.length)
                value += ", ";

            let text = treeItem.mainTitle;
            if (typeof this._modifyCommaSeparatedKeyword === "function")
                text = this._modifyCommaSeparatedKeyword(text);

            value += text;
        }

        return value;
    }

    set value(commaSeparatedValue)
    {
        if (commaSeparatedValue && commaSeparatedValue === this.value)
            return;

        this._commaSeparatedKeywords.removeChildren();
        if (!commaSeparatedValue || !commaSeparatedValue.length) {
            this.dispatchEventToListeners(WI.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems);
            return;
        }

        // It is necessary to add the beginning \) to ensure inner parenthesis are not matched.
        let values = commaSeparatedValue.split(/\)\s*,\s*(?![^\(\)]*\))/);
        for (let i = 0; i < values.length; ++i) {
            if (!values[i].includes(","))
                continue;

            let openParentheses = values[i].getMatchingIndexes("(").length;
            let closedParenthesis = values[i].getMatchingIndexes(")").length;
            values[i] += (openParentheses - closedParenthesis === 1) ? ")" : "";
        }

        // Allow splitting with parenthesis if the parenthesis does not have any commas.
        let hasParenthesis = values[0] && (values[0].includes("(") || values[0].includes(")"));
        if (values.length === 1 && (!hasParenthesis || !/\([^\)]*,[^\)]*\)/.test(values[0])))
            values = values[0].split(/\s*,\s*/);

        for (let value of values)
            this._addCommaSeparatedKeyword(value);

        this._commaSeparatedKeywords.children[0].select(true);
    }

    get synthesizedValue()
    {
        return this.value || null;
    }

    recalculateWidth(value)
    {
        if (this._titleElement) {
            // 55px width and 4px margin on left and right for title element,
            // plus the 11px margin right on the content element
            value -= 74;
        } else {
            // 11px margin on left and right of the content element
            value -= 22;
        }

        this.contentElement.style.width = Math.max(value, 0) + "px";
    }

    // Private

    _generateTextFromLonghandProperties()
    {
        let text = "";
        if (!this._style)
            return text;

        function propertyValue(existingProperty, propertyName) {
            if (existingProperty)
                return existingProperty.value;

            if (propertyName)
                return this._longhandProperties[propertyName];

            return "";
        }

        let onePropertyExists = false;
        let valueLists = [];
        let valuesCount = 0;
        for (let propertyName in this._longhandProperties) {
            let existingProperty = this._style.propertyForName(propertyName, true);
            if (existingProperty)
                onePropertyExists = true;

            let matches = propertyValue.call(this, existingProperty, propertyName).split(/\s*,\s*(?![^\(]*\))/);
            valuesCount = Math.max(valuesCount, matches.length);
            valueLists.push(matches);
        }

        if (!onePropertyExists)
            return text;

        let count = 0;
        while (count < valuesCount) {
            if (count > 0)
                text += ", ";

            for (let valueList of valueLists)
                text += valueList[count > valueList.length - 1 ? valueList.length - 1 : count] + " ";

            ++count;
        }
        return text;
    }

    modifyPropertyText(text, value)
    {
        for (let property in this._longhandProperties) {
            let replacementRegExp = new RegExp(property + "\s*:\s*[^;]*(;|$)");
            text = text.replace(replacementRegExp, "");
        }
        return super.modifyPropertyText(text, value);
    }

    _listElementKeyDown(event)
    {
        let selectedTreeElement = this._commaSeparatedKeywords.selectedTreeElement;
        if (!selectedTreeElement)
            return;

        if (selectedTreeElement.currentlyEditing)
            return;

        let keyCode = event.keyCode;
        let backspaceKeyCode = WI.KeyboardShortcut.Key.Backspace.keyCode;
        let deleteKeyCode = WI.KeyboardShortcut.Key.Delete.keyCode;
        if (keyCode === backspaceKeyCode || keyCode === deleteKeyCode)
            this._removeSelectedCommaSeparatedKeyword();
    }

    _treeSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement)
            return;

        this._removeEmptyCommaSeparatedKeywords();
        this.dispatchEventToListeners(WI.VisualStyleCommaSeparatedKeywordEditor.Event.TreeItemSelected, {text: treeElement.mainTitle});
    }

    _treeElementIsEmpty(item)
    {
        return !item._mainTitle || !item._mainTitle.length;
    }

    _addEmptyCommaSeparatedKeyword()
    {
        let newTreeElement = this._addCommaSeparatedKeyword(null, this._commaSeparatedKeywords.selectedTreeElementIndex);
        newTreeElement.subtitle = WI.UIString("(modify the boxes below to add a value)");
        newTreeElement.element.classList.add("no-value");
        newTreeElement.select(true, true);
        return newTreeElement;
    }

    _removeSelectedCommaSeparatedKeyword()
    {
        let selectedTreeElement = this._commaSeparatedKeywords.selectedTreeElement;
        this._removeCommaSeparatedKeyword(selectedTreeElement);
    }

    _removeEmptyCommaSeparatedKeywords()
    {
        for (let treeElement of this._commaSeparatedKeywords.children) {
            if (!this._treeElementIsEmpty(treeElement) || treeElement.selected)
                continue;

            treeElement.deselect();
            this._removeCommaSeparatedKeyword(treeElement);
        }
    }

    _addCommaSeparatedKeyword(value, index)
    {
        let valueElement = this._createNewTreeElement(value);
        if (!isNaN(index))
            this._commaSeparatedKeywords.insertChild(valueElement, index + !this._insertNewItemsBeforeSelected);
        else
            this._commaSeparatedKeywords.appendChild(valueElement);

        return valueElement;
    }

    _removeCommaSeparatedKeyword(treeElement)
    {
        if (!treeElement)
            return;

        this._commaSeparatedKeywords.removeChild(treeElement);
        if (!this._commaSeparatedKeywords.children.length)
            this.dispatchEventToListeners(WI.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems);

        this._valueDidChange();
    }

    _createNewTreeElement(value)
    {
        return new WI.GeneralTreeElement(WI.VisualStyleCommaSeparatedKeywordEditor.ListItemClassName, value);
    }
};

WI.VisualStyleCommaSeparatedKeywordEditor.ListItemClassName = "visual-style-comma-separated-keyword-item";

WI.VisualStyleCommaSeparatedKeywordEditor.Event = {
    TreeItemSelected: "visual-style-comma-separated-keyword-editor-tree-item-selected",
    NoRemainingTreeItems: "visual-style-comma-separated-keyword-editor-no-remaining-tree-items"
};
