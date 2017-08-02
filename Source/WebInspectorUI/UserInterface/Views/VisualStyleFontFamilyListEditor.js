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

WI.VisualStyleFontFamilyListEditor = class VisualStyleFontFamilyListEditor extends WI.VisualStyleCommaSeparatedKeywordEditor
{
    constructor(propertyNames, text, longhandProperties, layoutReversed)
    {
        super(propertyNames, text, longhandProperties, true, layoutReversed);

        this._commaSeparatedKeywords.element.addEventListener("scroll", this._hideCompletions.bind(this));

        this._completionController = new WI.VisualStyleCompletionsController(this);
        this._completionController.addEventListener(WI.VisualStyleCompletionsController.Event.CompletionSelected, this._completionClicked, this);

        this._wrapWithQuotesRegExp = /^[a-zA-Z\-]+$/;
        this._removeWrappedQuotesRegExp = /[\"\']/g;
    }

    // Public

    visualStyleCompletionsControllerCustomizeCompletionElement(suggestionsView, element, item)
    {
        element.style.fontFamily = item;
    }

    get hasCompletions()
    {
        return this._completionController.hasCompletions;
    }

    set completions(completions)
    {
        this._completionController.completions = completions;
    }

    // Private

    _modifyCommaSeparatedKeyword(text)
    {
        if (!this._wrapWithQuotesRegExp.test(text))
            text = "\"" + text + "\"";

        return text;
    }

    _addCommaSeparatedKeyword(value, index)
    {
        if (value)
            value = value.replace(this._removeWrappedQuotesRegExp, "");

        let valueElement = super._addCommaSeparatedKeyword(value, index);
        valueElement.addEventListener(WI.VisualStyleFontFamilyTreeElement.Event.EditorKeyDown, this._treeElementKeyDown, this);
        valueElement.addEventListener(WI.VisualStyleFontFamilyTreeElement.Event.KeywordChanged, this._treeElementKeywordChanged, this);
        valueElement.addEventListener(WI.VisualStyleFontFamilyTreeElement.Event.EditorBlurred, this._hideCompletions, this);
        return valueElement;
    }

    _addEmptyCommaSeparatedKeyword()
    {
        let newItem = super._addEmptyCommaSeparatedKeyword();
        newItem.showKeywordEditor();
    }

    _completionClicked(event)
    {
        this.value = event.data.text;
        this._valueDidChange();
    }

    _treeElementKeyDown(event)
    {
        if (!this._completionController.visible)
            return;

        let key = event && event.data && event.data.key;
        if (!key)
            return;

        if (key === "Enter" || key === "Tab") {
            let selectedTreeElement = this._commaSeparatedKeywords.selectedTreeElement;
            if (!selectedTreeElement)
                return;

            selectedTreeElement.updateMainTitle(this._completionController.currentCompletion);
            this._completionController.hide();
            return;
        }

        if (key === "Escape") {
            this._hideCompletions();
            return;
        }

        if (key === "Up") {
            this._completionController.previous();
            return;
        }

        if (key === "Down") {
            this._completionController.next();
            return;
        }
    }

    _treeElementKeywordChanged()
    {
        if (!this.hasCompletions)
            return;

        let result = this._valueDidChange();
        if (!result)
            return;

        let treeElement = this._commaSeparatedKeywords.selectedTreeElement;
        if (!treeElement)
            return;

        if (this._completionController.update(treeElement.mainTitle)) {
            let bounds = treeElement.editorBounds(2);
            if (!bounds)
                return;

            this._completionController.show(bounds);
        }
    }

    _hideCompletions()
    {
        this._completionController.hide();
    }

    _createNewTreeElement(value)
    {
        return new WI.VisualStyleFontFamilyTreeElement(value);
    }
};
