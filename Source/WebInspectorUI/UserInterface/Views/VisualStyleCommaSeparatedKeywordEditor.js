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

WebInspector.VisualStyleCommaSeparatedKeywordEditor = class VisualStyleCommaSeparatedKeywordEditor extends WebInspector.VisualStylePropertyEditor
{
    constructor(propertyNames, text, longhandProperties, insertNewItemsBeforeSelected, layoutReversed)
    {
        super(propertyNames, text, null, null, "comma-separated-keyword-editor", layoutReversed);

        this._insertNewItemsBeforeSelected = insertNewItemsBeforeSelected || false;
        this._longhandProperties = longhandProperties || {};

        var listElement = document.createElement("ol");
        listElement.classList.add("visual-style-comma-separated-keyword-list");
        listElement.addEventListener("keydown", this._listElementKeyDown.bind(this));
        this.contentElement.appendChild(listElement);

        this._commaSeparatedKeywords = new WebInspector.TreeOutline(listElement);
        this._commaSeparatedKeywords.onselect = this._treeElementSelected.bind(this);

        var controlContainer = document.createElement("div");
        controlContainer.classList.add("visual-style-comma-separated-keyword-controls");
        this.contentElement.appendChild(controlContainer);

        var addGlyphElement = useSVGSymbol("Images/Plus13.svg", "visual-style-add-comma-separated-keyword");
        addGlyphElement.addEventListener("click", this._addEmptyCommaSeparatedKeyword.bind(this));
        controlContainer.appendChild(addGlyphElement);

        var removeGlyphElement = useSVGSymbol("Images/Minus.svg", "visual-style-remove-comma-separated-keyword", WebInspector.UIString("Click to remove the selected item."));
        removeGlyphElement.addEventListener("click", this._removeSelectedCommaSeparatedKeyword.bind(this));
        controlContainer.appendChild(removeGlyphElement);
    }

    // Public

    set selectedTreeElementValue(text)
    {
        var selectedTreeElement = this._commaSeparatedKeywords.selectedTreeElement;
        if (!selectedTreeElement)
            return;

        selectedTreeElement.element.classList.toggle("no-value", !text || !text.length);
        selectedTreeElement.mainTitle = text;
        this._valueDidChange();
    }

    get value()
    {
        if (!this._commaSeparatedKeywords.hasChildren)
            return;

        var value = "";
        for (var treeItem of this._commaSeparatedKeywords.children) {
            if (this._treeElementIsEmpty(treeItem))
                continue;

            if (value.length)
                value += ", ";

            var text = treeItem.mainTitle;
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
            this.dispatchEventToListeners(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems);
            return;
        }

        // It is necessary to add the beginning \) to ensure inner parenthesis are not matched.
        var values = commaSeparatedValue.split(/\)\s*,\s*(?![^\(\)]*\))/);
        for (var value of values)
            this._addCommaSeparatedKeyword(value + (value.endsWith(")") ? "" : ")"));

        this._commaSeparatedKeywords.children[0].select(true);
    }

    get synthesizedValue()
    {
        return this.value || null;
    }

    // Private

    _generateTextFromLonghandProperties()
    {
        var text = "";
        if (!this._style)
            return text;

        function propertyValue(existingProperty, propertyName) {
            if (existingProperty)
                return existingProperty.value;

            if (propertyName)
                return this._longhandProperties[propertyName];

            return "";
        }

        var onePropertyExists = false;
        var valueLists = [];
        var valuesCount = 0;
        for (var propertyName in this._longhandProperties) {
            var existingProperty = this._style.propertyForName(propertyName, true);
            if (existingProperty)
                onePropertyExists = true;

            var matches = propertyValue.call(this, existingProperty, propertyName).split(/\s*,\s*(?![^\(]*\))/);
            valuesCount = Math.max(valuesCount, matches.length);
            valueLists.push(matches);
        }

        if (!onePropertyExists)
            return text;

        var count = 0;
        while (count < valuesCount) {
            if (count > 0)
                text += ",";

            for (var valueList of valueLists)
                text += valueList[count > valueList.length - 1 ? valueList.length - 1 : count] + " ";

            ++count;
        }
        return text;
    }

    modifyPropertyText(text, value)
    {
        for (var property in this._longhandProperties) {
            var replacementRegExp = new RegExp(property + "\s*:\s*[^;]*(;|$)");
            text = text.replace(replacementRegExp, "");
        }
        return super.modifyPropertyText(text, value);
    }

    _listElementKeyDown(event)
    {
        var selectedTreeElement = this._commaSeparatedKeywords.selectedTreeElement;
        if (!selectedTreeElement)
            return;

        if (selectedTreeElement.currentlyEditing)
            return;

        var keyCode = event.keyCode;
        var backspaceKeyCode = WebInspector.KeyboardShortcut.Key.Backspace.keyCode;
        var deleteKeyCode = WebInspector.KeyboardShortcut.Key.Delete.keyCode;
        if (keyCode === backspaceKeyCode || keyCode === deleteKeyCode)
            this._removeSelectedCommaSeparatedKeyword();
    }

    _treeElementSelected(item, selectedByUser)
    {
        this._removeEmptyCommaSeparatedKeywords();
        this.dispatchEventToListeners(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.TreeItemSelected, {text: item.mainTitle});
    }

    _treeElementIsEmpty(item)
    {
        return !item._mainTitle || !item._mainTitle.length;
    }

    _addEmptyCommaSeparatedKeyword()
    {
        var newTreeElement = this._addCommaSeparatedKeyword(null, this._commaSeparatedKeywords.selectedTreeElementIndex);
        newTreeElement.subtitle = WebInspector.UIString("(modify the boxes below to add a value)");
        newTreeElement.element.classList.add("no-value");
        newTreeElement.select(true, true);
        return newTreeElement;
    }

    _removeSelectedCommaSeparatedKeyword()
    {
        var selectedTreeElement = this._commaSeparatedKeywords.selectedTreeElement;
        this._removeCommaSeparatedKeyword(selectedTreeElement);
    }

    _removeEmptyCommaSeparatedKeywords()
    {
        for (var treeElement of this._commaSeparatedKeywords.children) {
            if (!this._treeElementIsEmpty(treeElement) || treeElement.selected)
                continue;

            treeElement.deselect();
            this._removeCommaSeparatedKeyword(treeElement);
        }
    }

    _addCommaSeparatedKeyword(value, index)
    {
        var valueElement = this._createNewTreeElement(value);
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
            this.dispatchEventToListeners(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems);

        this._valueDidChange();
    }

    _createNewTreeElement(value)
    {
        return new WebInspector.GeneralTreeElement(WebInspector.VisualStyleCommaSeparatedKeywordEditor.ListItemClassName, value);
    }
};

WebInspector.VisualStyleCommaSeparatedKeywordEditor.ListItemClassName = "visual-style-comma-separated-keyword-item";

WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event = {
    TreeItemSelected: "visual-style-comma-separated-keyword-editor-tree-item-selected",
    NoRemainingTreeItems: "visual-style-comma-separated-keyword-editor-no-remaining-tree-items"
}
