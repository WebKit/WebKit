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

WI.TextResourceContentView = class TextResourceContentView extends WI.ResourceContentView
{
    constructor(resource)
    {
        super(resource, "text");

        resource.addEventListener(WI.SourceCode.Event.ContentDidChange, this._sourceCodeContentDidChange, this);

        var toolTip = WI.UIString("Pretty print");
        var activatedToolTip = WI.UIString("Original formatting");
        this._prettyPrintButtonNavigationItem = new WI.ActivateButtonNavigationItem("pretty-print", toolTip, activatedToolTip, "Images/NavigationItemCurleyBraces.svg", 13, 13);
        this._prettyPrintButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._togglePrettyPrint, this);
        this._prettyPrintButtonNavigationItem.enabled = false; // Enabled when the text editor is populated with content.
        this._prettyPrintButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;

        var toolTipTypes = WI.UIString("Show type information");
        var activatedToolTipTypes = WI.UIString("Hide type information");
        this._showTypesButtonNavigationItem = new WI.ActivateButtonNavigationItem("show-types", toolTipTypes, activatedToolTipTypes, "Images/NavigationItemTypes.svg", 13, 14);
        this._showTypesButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleTypeAnnotations, this);
        this._showTypesButtonNavigationItem.enabled = false;
        this._showTypesButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        WI.settings.showJavaScriptTypeInformation.addEventListener(WI.Setting.Event.Changed, this._showJavaScriptTypeInformationSettingChanged, this);

        let toolTipCodeCoverage = WI.UIString("Fade unexecuted code");
        let activatedToolTipCodeCoverage = WI.UIString("Do not fade unexecuted code");
        this._codeCoverageButtonNavigationItem = new WI.ActivateButtonNavigationItem("code-coverage", toolTipCodeCoverage, activatedToolTipCodeCoverage, "Images/NavigationItemCodeCoverage.svg", 13, 14);
        this._codeCoverageButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleUnexecutedCodeHighlights, this);
        this._codeCoverageButtonNavigationItem.enabled = false;
        this._codeCoverageButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        WI.settings.enableControlFlowProfiler.addEventListener(WI.Setting.Event.Changed, this._enableControlFlowProfilerSettingChanged, this);

        this._textEditor = new WI.SourceCodeTextEditor(resource);
        this._textEditor.addEventListener(WI.TextEditor.Event.ExecutionLineNumberDidChange, this._executionLineNumberDidChange, this);
        this._textEditor.addEventListener(WI.TextEditor.Event.NumberOfSearchResultsDidChange, this._numberOfSearchResultsDidChange, this);
        this._textEditor.addEventListener(WI.TextEditor.Event.ContentDidChange, this._textEditorContentDidChange, this);
        this._textEditor.addEventListener(WI.TextEditor.Event.FormattingDidChange, this._textEditorFormattingDidChange, this);
        this._textEditor.addEventListener(WI.TextEditor.Event.MIMETypeChanged, this._handleTextEditorMIMETypeChanged, this);
        this._textEditor.addEventListener(WI.SourceCodeTextEditor.Event.ContentWillPopulate, this._contentWillPopulate, this);
        this._textEditor.addEventListener(WI.SourceCodeTextEditor.Event.ContentDidPopulate, this._contentDidPopulate, this);
        this._textEditor.readOnly = !this._shouldBeEditable();

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ProbeSetAdded, this._probeSetsChanged, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ProbeSetRemoved, this._probeSetsChanged, this);
    }

    // Public

    get navigationItems()
    {
        return [this._prettyPrintButtonNavigationItem, this._showTypesButtonNavigationItem, this._codeCoverageButtonNavigationItem];
    }

    get managesOwnIssues()
    {
        // SourceCodeTextEditor manages the issues, we don't need ResourceContentView doing it.
        return true;
    }

    get textEditor()
    {
        return this._textEditor;
    }

    get supplementalRepresentedObjects()
    {
        let objects = WI.debuggerManager.probeSets.filter(function(probeSet) {
            return this._resource.contentIdentifier === probeSet.breakpoint.contentIdentifier;
        }, this);

        // If the SourceCodeTextEditor has an executionLineNumber, we can assume
        // it is always the active call frame.
        if (!isNaN(this._textEditor.executionLineNumber))
            objects.push(WI.debuggerManager.activeCallFrame);

        return objects;
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

        this.resource.removeEventListener(null, null, this);
        WI.debuggerManager.removeEventListener(null, null, this);
        WI.settings.showJavaScriptTypeInformation.removeEventListener(null, null, this);
        WI.settings.enableControlFlowProfiler.removeEventListener(null, null, this);
    }

    contentAvailable(content, base64Encoded)
    {
        // Do nothing.
    }

    get supportsSave()
    {
        return super.supportsSave || this.resource instanceof WI.CSSStyleSheet;
    }

    get saveData()
    {
        if (this.resource instanceof WI.CSSStyleSheet) {
            let url = WI.FileUtilities.inspectorURLForFilename("InspectorStyleSheet.css");
            return {url, content: this._textEditor.string, forceSaveAs: true};
        }
        return {url: this.resource.url, content: this._textEditor.string};
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
        if (this._textEditor.parentView === this)
            return;

        this.removeLoadingIndicator();

        this.addSubview(this._textEditor);
    }

    _contentDidPopulate(event)
    {
        this._prettyPrintButtonNavigationItem.enabled = this._textEditor.canBeFormatted();

        this._showTypesButtonNavigationItem.enabled = this._textEditor.canShowTypeAnnotations();
        this._showTypesButtonNavigationItem.activated = WI.settings.showJavaScriptTypeInformation.value;

        this._codeCoverageButtonNavigationItem.enabled = this._textEditor.canShowCoverageHints();
        this._codeCoverageButtonNavigationItem.activated = WI.settings.enableControlFlowProfiler.value;
    }

    _togglePrettyPrint(event)
    {
        var activated = !this._prettyPrintButtonNavigationItem.activated;
        this._textEditor.updateFormattedState(activated);
    }

    _toggleTypeAnnotations(event)
    {
        this._showTypesButtonNavigationItem.enabled = false;
        this._textEditor.toggleTypeAnnotations().then(() => {
            this._showTypesButtonNavigationItem.enabled = true;
        });
    }

    _toggleUnexecutedCodeHighlights(event)
    {
        this._codeCoverageButtonNavigationItem.enabled = false;
        this._textEditor.toggleUnexecutedCodeHighlights().then(() => {
            this._codeCoverageButtonNavigationItem.enabled = true;
        });
    }

    _showJavaScriptTypeInformationSettingChanged(event)
    {
        this._showTypesButtonNavigationItem.activated = WI.settings.showJavaScriptTypeInformation.value;
    }

    _enableControlFlowProfilerSettingChanged(event)
    {
        this._codeCoverageButtonNavigationItem.activated = WI.settings.enableControlFlowProfiler.value;
    }

    _textEditorFormattingDidChange(event)
    {
        this._prettyPrintButtonNavigationItem.activated = this._textEditor.formatted;
    }

    _handleTextEditorMIMETypeChanged(event)
    {
        this._prettyPrintButtonNavigationItem.enabled = this._textEditor.canBeFormatted();
    }

    _sourceCodeContentDidChange(event)
    {
        if (this._ignoreSourceCodeContentDidChangeEvent)
            return;

        this._textEditor.string = this.resource.currentRevision.content;
    }

    _textEditorContentDidChange(event)
    {
        this._ignoreSourceCodeContentDidChangeEvent = true;
        WI.branchManager.currentBranch.revisionForRepresentedObject(this.resource).content = this._textEditor.string;
        delete this._ignoreSourceCodeContentDidChangeEvent;
    }

    _executionLineNumberDidChange(event)
    {
        this.dispatchEventToListeners(WI.ContentView.Event.SupplementalRepresentedObjectsDidChange);
    }

    _numberOfSearchResultsDidChange(event)
    {
        this.dispatchEventToListeners(WI.ContentView.Event.NumberOfSearchResultsDidChange);
    }

    _probeSetsChanged(event)
    {
        var breakpoint = event.data.probeSet.breakpoint;
        if (breakpoint.sourceCodeLocation.sourceCode === this.resource)
            this.dispatchEventToListeners(WI.ContentView.Event.SupplementalRepresentedObjectsDidChange);
    }

    _shouldBeEditable()
    {
        if (this.resource instanceof WI.CSSStyleSheet)
            return true;

        // Check the MIME-type for CSS since Resource.Type.StyleSheet also includes XSL, which we can't edit yet.
        if (this.resource.type === WI.Resource.Type.StyleSheet && this.resource.syntheticMIMEType === "text/css")
            return true;

        // Allow editing any local file since edits can be saved and reloaded right from the Inspector.
        if (this.resource.urlComponents.scheme === "file")
            return true;

        return false;
    }
};
