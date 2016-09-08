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

WebInspector.TextContentView = class TextContentView extends WebInspector.ContentView
{
    constructor(string, mimeType)
    {
        super(string);

        this.element.classList.add("text");

        this._textEditor = new WebInspector.TextEditor;
        this._textEditor.addEventListener(WebInspector.TextEditor.Event.NumberOfSearchResultsDidChange, this._numberOfSearchResultsDidChange, this);
        this._textEditor.addEventListener(WebInspector.TextEditor.Event.FormattingDidChange, this._textEditorFormattingDidChange, this);

        this.addSubview(this._textEditor);

        this._textEditor.readOnly = true;
        this._textEditor.mimeType = mimeType;
        this._textEditor.string = string;

        var toolTip = WebInspector.UIString("Pretty print");
        var activatedToolTip = WebInspector.UIString("Original formatting");
        this._prettyPrintButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("pretty-print", toolTip, activatedToolTip, "Images/NavigationItemCurleyBraces.svg", 13, 13);
        this._prettyPrintButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._togglePrettyPrint, this);
        this._prettyPrintButtonNavigationItem.enabled = this._textEditor.canBeFormatted();

        var toolTipTypes = WebInspector.UIString("Show type information");
        var activatedToolTipTypes = WebInspector.UIString("Hide type information");
        this._showTypesButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("show-types", toolTipTypes, activatedToolTipTypes, "Images/NavigationItemTypes.svg", 13, 14);
        this._showTypesButtonNavigationItem.enabled = false;

        let toolTipCodeCoverage = WebInspector.UIString("Fade unexecuted code");
        let activatedToolTipCodeCoverage = WebInspector.UIString("Do not fade unexecuted code");
        this._codeCoverageButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("code-coverage", toolTipCodeCoverage, activatedToolTipCodeCoverage, "Images/NavigationItemCodeCoverage.svg", 13, 14);
        this._codeCoverageButtonNavigationItem.enabled = false;
    }

    // Public

    get textEditor()
    {
        return this._textEditor;
    }

    get navigationItems()
    {
        return [this._prettyPrintButtonNavigationItem, this._showTypesButtonNavigationItem, this._codeCoverageButtonNavigationItem];
    }

    revealPosition(position, textRangeToSelect, forceUnformatted)
    {
        this._textEditor.revealPosition(position, textRangeToSelect, forceUnformatted);
    }

    shown()
    {
        super.shown();

        this._textEditor.shown();
    }

    hidden()
    {
        super.hidden();

        this._textEditor.hidden();
    }

    closed()
    {
        super.closed();

        this._textEditor.close();
    }

    get supportsSave()
    {
        return true;
    }

    get saveData()
    {
        var url = "web-inspector:///" + encodeURI(WebInspector.UIString("Untitled")) + ".txt";
        return {url, content: this._textEditor.string, forceSaveAs: true};
    }

    get supportsSearch()
    {
        return true;
    }

    get numberOfSearchResults()
    {
        return this._textEditor.numberOfSearchResults;
    }

    get hasPerformedSearch()
    {
        return this._textEditor.currentSearchQuery !== null;
    }

    set automaticallyRevealFirstSearchResult(reveal)
    {
        this._textEditor.automaticallyRevealFirstSearchResult = reveal;
    }

    performSearch(query)
    {
        this._textEditor.performSearch(query);
    }

    searchCleared()
    {
        this._textEditor.searchCleared();
    }

    searchQueryWithSelection()
    {
        return this._textEditor.searchQueryWithSelection();
    }

    revealPreviousSearchResult(changeFocus)
    {
        this._textEditor.revealPreviousSearchResult(changeFocus);
    }

    revealNextSearchResult(changeFocus)
    {
        this._textEditor.revealNextSearchResult(changeFocus);
    }

    // Private

    _togglePrettyPrint(event)
    {
        var activated = !this._prettyPrintButtonNavigationItem.activated;
        this._textEditor.updateFormattedState(activated);
    }

    _textEditorFormattingDidChange(event)
    {
        this._prettyPrintButtonNavigationItem.activated = this._textEditor.formatted;
    }

    _numberOfSearchResultsDidChange(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.NumberOfSearchResultsDidChange);
    }
};
