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

WebInspector.ScriptContentView = class ScriptContentView extends WebInspector.ContentView
{
    constructor(script)
    {
        console.assert(script instanceof WebInspector.Script, script);

        super(script);

        this.element.classList.add("script");

        // Append a spinner while waiting for _contentWillPopulate.
        var spinner = new WebInspector.IndeterminateProgressSpinner;
        this.element.appendChild(spinner.element);

        this._script = script;

        // This view is only for standalone Scripts with no corresponding Resource. All other Scripts
        // should be handled by TextResourceContentView via the Resource.
        console.assert(!script.resource);
        console.assert(script.range.startLine === 0);
        console.assert(script.range.startColumn === 0);

        this._textEditor = new WebInspector.SourceCodeTextEditor(script);
        this._textEditor.addEventListener(WebInspector.TextEditor.Event.ExecutionLineNumberDidChange, this._executionLineNumberDidChange, this);
        this._textEditor.addEventListener(WebInspector.TextEditor.Event.NumberOfSearchResultsDidChange, this._numberOfSearchResultsDidChange, this);
        this._textEditor.addEventListener(WebInspector.TextEditor.Event.FormattingDidChange, this._textEditorFormattingDidChange, this);
        this._textEditor.addEventListener(WebInspector.SourceCodeTextEditor.Event.ContentWillPopulate, this._contentWillPopulate, this);
        this._textEditor.addEventListener(WebInspector.SourceCodeTextEditor.Event.ContentDidPopulate, this._contentDidPopulate, this);

        var toolTip = WebInspector.UIString("Pretty print");
        var activatedToolTip = WebInspector.UIString("Original formatting");
        this._prettyPrintButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("pretty-print", toolTip, activatedToolTip, "Images/NavigationItemCurleyBraces.svg", 13, 13);
        this._prettyPrintButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._togglePrettyPrint, this);
        this._prettyPrintButtonNavigationItem.enabled = false; // Enabled when the text editor is populated with content.

        var toolTipTypes = WebInspector.UIString("Show type information");
        var activatedToolTipTypes = WebInspector.UIString("Hide type information");
        this._showTypesButtonNavigationItem = new WebInspector.ActivateButtonNavigationItem("show-types", toolTipTypes, activatedToolTipTypes, "Images/NavigationItemTypes.svg", 13, 14);
        this._showTypesButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._toggleTypeAnnotations, this);
        this._showTypesButtonNavigationItem.enabled = false;

        WebInspector.showJavaScriptTypeInformationSetting.addEventListener(WebInspector.Setting.Event.Changed, this._showJavaScriptTypeInformationSettingChanged, this);
    }

    // Public

    get navigationItems()
    {
        return [this._prettyPrintButtonNavigationItem, this._showTypesButtonNavigationItem];
    }

    get script()
    {
        return this._script;
    }

    get textEditor()
    {
        return this._textEditor;
    }

    get supplementalRepresentedObjects()
    {
        if (isNaN(this._textEditor.executionLineNumber))
            return [];

        // If the SourceCodeTextEditor has an executionLineNumber, we can assume
        // it is always the active call frame.
        return [WebInspector.debuggerManager.activeCallFrame];
    }

    revealPosition(position, textRangeToSelect, forceUnformatted)
    {
        this._textEditor.revealPosition(position, textRangeToSelect, forceUnformatted);
    }

    shown()
    {
        this._textEditor.shown();
    }

    hidden()
    {
        this._textEditor.hidden();
    }

    closed()
    {
        WebInspector.showJavaScriptTypeInformationSetting.removeEventListener(null, null, this);

        this._textEditor.close();
    }

    saveToCookie(cookie)
    {
        cookie.type = WebInspector.ContentViewCookieType.Resource;
        cookie.url = this.representedObject.url;
    }

    restoreFromCookie(cookie)
    {
        if ("lineNumber" in cookie && "columnNumber" in cookie)
            this.revealPosition(new WebInspector.SourceCodePosition(cookie.lineNumber, cookie.columnNumber));
    }

    get supportsSave()
    {
        return true;
    }

    get saveData()
    {
        var url = this._script.url || "web-inspector:///" + encodeURI(this._script.displayName) + ".js";
        return {url, content: this._textEditor.string};
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

    _contentWillPopulate(event)
    {
        if (this._textEditor.element.parentNode === this.element)
            return;

        // Allow editing any local file since edits can be saved and reloaded right from the Inspector.
        if (this._script.urlComponents.scheme === "file")
            this._textEditor.readOnly = false;

        this.element.removeChildren();
        this.addSubview(this._textEditor);
    }

    _contentDidPopulate(event)
    {
        this._prettyPrintButtonNavigationItem.enabled = this._textEditor.canBeFormatted();
        this._showTypesButtonNavigationItem.enabled = this._textEditor.canShowTypeAnnotations();
        this._showTypesButtonNavigationItem.activated = WebInspector.showJavaScriptTypeInformationSetting.value;
    }

    _togglePrettyPrint(event)
    {
        var activated = !this._prettyPrintButtonNavigationItem.activated;
        this._textEditor.updateFormattedState(activated);
    }

    _toggleTypeAnnotations(event)
    {
        this._textEditor.toggleTypeAnnotations();
    }

    _showJavaScriptTypeInformationSettingChanged(event)
    {
        this._showTypesButtonNavigationItem.activated = WebInspector.showJavaScriptTypeInformationSetting.value;
    }

    _textEditorFormattingDidChange(event)
    {
        this._prettyPrintButtonNavigationItem.activated = this._textEditor.formatted;
    }

    _executionLineNumberDidChange(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SupplementalRepresentedObjectsDidChange);
    }

    _numberOfSearchResultsDidChange(event)
    {
        this.dispatchEventToListeners(WebInspector.ContentView.Event.NumberOfSearchResultsDidChange);
    }
};
