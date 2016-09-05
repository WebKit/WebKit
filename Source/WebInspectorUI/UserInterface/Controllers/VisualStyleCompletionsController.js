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

WebInspector.VisualStyleCompletionsController = class VisualStyleCompletionsController extends WebInspector.Object
{
    constructor(delegate)
    {
        super();

        this._delegate = delegate || null;
        this._suggestionsView = new WebInspector.CompletionSuggestionsView(this);
        this._completions = null;
        this._currentCompletions = [];
        this._selectedCompletionIndex = 0;
    }

    // Public

    get visible()
    {
        return this._completions && this._currentCompletions.length && this._suggestionsView.visible;
    }

    get hasCompletions()
    {
        return !!this._completions;
    }

    get currentCompletion()
    {
        if (!this.hasCompletions)
            return null;

        return this._currentCompletions[this._selectedCompletionIndex] || null;
    }

    set completions(completions)
    {
        this._completions = completions || null;
    }

    completionSuggestionsViewCustomizeCompletionElement(suggestionsView, element, item)
    {
        if (this._delegate && typeof this._delegate.visualStyleCompletionsControllerCustomizeCompletionElement === "function")
            this._delegate.visualStyleCompletionsControllerCustomizeCompletionElement(suggestionsView, element, item);
    }

    completionSuggestionsClickedCompletion(suggestionsView, text)
    {
        suggestionsView.hide();
        this.dispatchEventToListeners(WebInspector.VisualStyleCompletionsController.Event.CompletionSelected, {text});
    }

    previous()
    {
        this._suggestionsView.selectPrevious();
        this._selectedCompletionIndex = this._suggestionsView.selectedIndex;
    }

    next()
    {
        this._suggestionsView.selectNext();
        this._selectedCompletionIndex = this._suggestionsView.selectedIndex;
    }

    update(value)
    {
        if (!this.hasCompletions)
            return false;

        this._currentCompletions = this._completions.startsWith(value);

        var currentCompletionsLength = this._currentCompletions.length;
        if (currentCompletionsLength) {
            if (currentCompletionsLength === 1 && this._currentCompletions[0] === value) {
                this.hide();
                return false;
            }

            if (this._selectedCompletionIndex >= currentCompletionsLength)
                this._selectedCompletionIndex = 0;

            this._suggestionsView.update(this._currentCompletions, this._selectedCompletionIndex);
            return true;
        }

        this.hide();
        return false;
    }

    show(bounds, padding)
    {
        if (!bounds)
            return;

        this._suggestionsView.show(bounds.pad(padding || 0));
    }

    hide()
    {
        if (this._suggestionsView.isHandlingClickEvent())
            return;

        this._suggestionsView.hide();
    }
};

WebInspector.VisualStyleCompletionsController.Event = {
    CompletionSelected: "visual-style-completions-controller-completion-selected"
};
