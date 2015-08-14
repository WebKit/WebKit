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
    constructor(propertyNames, text, insertNewItemsBeforeSelected, layoutReversed)
    {
        super(propertyNames, text, null, null, "comma-separated-keyword-editor", layoutReversed);

        this._insertNewItemsBeforeSelected = insertNewItemsBeforeSelected || false;

        let listElement = document.createElement("ol");
        listElement.classList.add("visual-style-comma-separated-keyword-list");
        this.contentElement.appendChild(listElement);

        this._commaSeparatedKeywords = new WebInspector.TreeOutline(listElement);
        this._commaSeparatedKeywords.onselect = this._treeElementSelected.bind(this);

        let controlContainer = document.createElement("div");
        controlContainer.classList.add("visual-style-comma-separated-keyword-controls");
        this.contentElement.appendChild(controlContainer);

        wrappedSVGDocument("Images/Plus.svg", "visual-style-add-comma-separated-keyword", WebInspector.UIString("Click to add a new item."), function(wrapper) {
            wrapper.addEventListener("click", this._addEmptyCommaSeparatedKeyword.bind(this));
            controlContainer.appendChild(wrapper);
        }.bind(this));

        wrappedSVGDocument("Images/Minus.svg", "visual-style-remove-comma-separated-keyword", WebInspector.UIString("Click to remove the selected item."), function(wrapper) {
            wrapper.addEventListener("click", this._removeSelectedCommaSeparatedKeyword.bind(this));
            controlContainer.appendChild(wrapper);
        }.bind(this));
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
            return;

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
            this.dispatchEventToListeners(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems);
            return;
        }

        let values = commaSeparatedValue.split(/\s*,\s*(?![^\(]*\))/);
        for (let value of values)
            this._addCommaSeparatedKeyword(value);

        this._commaSeparatedKeywords.children[0].select(true);
    }

    get synthesizedValue()
    {
        return this.value || null;
    }

    // Private

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
        let newTreeElement = this._addCommaSeparatedKeyword(null, this._commaSeparatedKeywords.selectedTreeElementIndex);
        newTreeElement.subtitle = WebInspector.UIString("(modify the boxes below to add a value)");
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
        let indexIsSet = !isNaN(index);
        this._commaSeparatedKeywords.insertChild(valueElement, indexIsSet ? index + !this._insertNewItemsBeforeSelected : 0);
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
