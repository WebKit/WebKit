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

WebInspector.SourceCodeTextEditor = class SourceCodeTextEditor extends WebInspector.TextEditor
{
    constructor(sourceCode)
    {
        console.assert(sourceCode instanceof WebInspector.SourceCode);

        super();

        this.delegate = this;

        this._sourceCode = sourceCode;
        this._breakpointMap = {};
        this._issuesLineNumberMap = new Map;
        this._widgetMap = new Map;
        this._contentPopulated = false;
        this._invalidLineNumbers = {0: true};
        this._ignoreContentDidChange = 0;

        this._typeTokenScrollHandler = null;
        this._typeTokenAnnotator = null;
        this._basicBlockAnnotator = null;

        this._isProbablyMinified = false;

        // FIXME: Currently this just jumps between resources and related source map resources. It doesn't "jump to symbol" yet.
        this._updateTokenTrackingControllerState();

        this.element.classList.add("source-code");

        if (this._supportsDebugging) {
            WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.DisabledStateDidChange, this._breakpointStatusDidChange, this);
            WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.AutoContinueDidChange, this._breakpointStatusDidChange, this);
            WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.ResolvedStateDidChange, this._breakpointStatusDidChange, this);
            WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.LocationDidChange, this._updateBreakpointLocation, this);

            WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointsEnabledDidChange, this._breakpointsEnabledDidChange, this);
            WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointAdded, this._breakpointAdded, this);
            WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointRemoved, this._breakpointRemoved, this);
            WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ActiveCallFrameDidChange, this._activeCallFrameDidChange, this);

            WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Paused, this._debuggerDidPause, this);
            WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Resumed, this._debuggerDidResume, this);
            if (WebInspector.debuggerManager.activeCallFrame)
                this._debuggerDidPause();

            this._activeCallFrameDidChange();
        }

        WebInspector.issueManager.addEventListener(WebInspector.IssueManager.Event.IssueWasAdded, this._issueWasAdded, this);

        if (this._sourceCode instanceof WebInspector.SourceMapResource || this._sourceCode.sourceMaps.length > 0)
            WebInspector.notifications.addEventListener(WebInspector.Notification.GlobalModifierKeysDidChange, this._updateTokenTrackingControllerState, this);
        else
            this._sourceCode.addEventListener(WebInspector.SourceCode.Event.SourceMapAdded, this._sourceCodeSourceMapAdded, this);

        sourceCode.requestContent().then(this._contentAvailable.bind(this));

        // FIXME: Cmd+L shorcut doesn't actually work.
        new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Command, "L", this.showGoToLineDialog.bind(this), this.element);
        new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Control, "G", this.showGoToLineDialog.bind(this), this.element);
    }

    // Public

    get sourceCode()
    {
        return this._sourceCode;
    }

    shown()
    {
        super.shown();

        if (WebInspector.showJavaScriptTypeInformationSetting.value) {
            if (this._typeTokenAnnotator)
                this._typeTokenAnnotator.resume();
            if (this._basicBlockAnnotator)
                this._basicBlockAnnotator.resume();
            if (!this._typeTokenScrollHandler && (this._typeTokenAnnotator || this._basicBlockAnnotator))
                this._enableScrollEventsForTypeTokenAnnotator();
        } else {
            if (this._typeTokenAnnotator || this._basicBlockAnnotator)
                this._setTypeTokenAnnotatorEnabledState(false);
        }
    }

    hidden()
    {
        super.hidden();

        this.tokenTrackingController.removeHighlightedRange();

        this._dismissPopover();

        this._dismissEditingController(true);

        if (this._typeTokenAnnotator)
            this._typeTokenAnnotator.pause();
        if (this._basicBlockAnnotator)
            this._basicBlockAnnotator.pause();
    }

    close()
    {
        if (this._supportsDebugging) {
            WebInspector.Breakpoint.removeEventListener(null, null, this);
            WebInspector.debuggerManager.removeEventListener(null, null, this);

            if (this._activeCallFrameSourceCodeLocation) {
                this._activeCallFrameSourceCodeLocation.removeEventListener(WebInspector.SourceCodeLocation.Event.LocationChanged, this._activeCallFrameSourceCodeLocationChanged, this);
                delete this._activeCallFrameSourceCodeLocation;
            }
        }

        WebInspector.issueManager.removeEventListener(WebInspector.IssueManager.Event.IssueWasAdded, this._issueWasAdded, this);

        WebInspector.notifications.removeEventListener(WebInspector.Notification.GlobalModifierKeysDidChange, this._updateTokenTrackingControllerState, this);
        this._sourceCode.removeEventListener(WebInspector.SourceCode.Event.SourceMapAdded, this._sourceCodeSourceMapAdded, this);
    }

    canBeFormatted()
    {
        // Currently we assume that source map resources are formatted how the author wants it.
        // We could allow source map resources to be formatted, we would then need to make
        // SourceCodeLocation watch listen for mappedResource's formatting changes, and keep
        // a formatted location alongside the regular mapped location.
        if (this._sourceCode instanceof WebInspector.SourceMapResource)
            return false;

        return super.canBeFormatted();
    }

    canShowTypeAnnotations()
    {
        return !!this._typeTokenAnnotator;
    }

    customPerformSearch(query)
    {
        function searchResultCallback(error, matches)
        {
            // Bail if the query changed since we started.
            if (this.currentSearchQuery !== query)
                return;

            if (error || !matches || !matches.length) {
                // Report zero matches.
                this.dispatchEventToListeners(WebInspector.TextEditor.Event.NumberOfSearchResultsDidChange);
                return;
            }

            var queryRegex = new RegExp(query.escapeForRegExp(), "gi");
            var searchResults = [];

            for (var i = 0; i < matches.length; ++i) {
                var matchLineNumber = matches[i].lineNumber;
                var line = this.line(matchLineNumber);
                if (!line)
                    return;

                // Reset the last index to reuse the regex on a new line.
                queryRegex.lastIndex = 0;

                // Search the line and mark the ranges.
                var lineMatch = null;
                while (queryRegex.lastIndex + query.length <= line.length && (lineMatch = queryRegex.exec(line))) {
                    var resultTextRange = new WebInspector.TextRange(matchLineNumber, lineMatch.index, matchLineNumber, queryRegex.lastIndex);
                    searchResults.push(resultTextRange);
                }
            }

            this.addSearchResults(searchResults);

            this.dispatchEventToListeners(WebInspector.TextEditor.Event.NumberOfSearchResultsDidChange);
        }

        if (this.hasEdits())
            return false;

        if (this._sourceCode instanceof WebInspector.SourceMapResource)
            return false;

        if (this._sourceCode instanceof WebInspector.Resource)
            PageAgent.searchInResource(this._sourceCode.parentFrame.id, this._sourceCode.url, query, false, false, searchResultCallback.bind(this));
        else if (this._sourceCode instanceof WebInspector.Script)
            DebuggerAgent.searchInContent(this._sourceCode.id, query, false, false, searchResultCallback.bind(this));
        return true;
    }

    showGoToLineDialog()
    {
        if (!this._goToLineDialog) {
            this._goToLineDialog = new WebInspector.GoToLineDialog;
            this._goToLineDialog.delegate = this;
        }

        this._goToLineDialog.present(this.element);
    }

    isGoToLineDialogValueValid(goToLineDialog, lineNumber)
    {
        return !isNaN(lineNumber) && lineNumber > 0 && lineNumber <= this.lineCount;
    }

    goToLineDialogValueWasValidated(goToLineDialog, lineNumber)
    {
        var position = new WebInspector.SourceCodePosition(lineNumber - 1, 0);
        var range = new WebInspector.TextRange(lineNumber - 1, 0, lineNumber, 0);
        this.revealPosition(position, range, false, true);
    }

    goToLineDialogWasDismissed()
    {
        this.focus();
    }

    contentDidChange(replacedRanges, newRanges)
    {
        super.contentDidChange(replacedRanges, newRanges);

        if (this._ignoreContentDidChange > 0)
            return;

        for (var range of newRanges)
            this._updateEditableMarkers(range);

        if (this._typeTokenAnnotator || this._basicBlockAnnotator) {
            this._setTypeTokenAnnotatorEnabledState(false);
            this._typeTokenAnnotator = null;
            this._basicBlockAnnotator = null;
        }
    }

    toggleTypeAnnotations()
    {
        if (!this._typeTokenAnnotator)
            return false;

        var newActivatedState = !this._typeTokenAnnotator.isActive();
        if (newActivatedState && this._isProbablyMinified && !this.formatted)
            this.formatted = true;

        this._setTypeTokenAnnotatorEnabledState(newActivatedState);
        return newActivatedState;
    }

    showPopoverForTypes(typeDescription, bounds, title)
    {
        var content = document.createElement("div");
        content.className = "object expandable";

        var titleElement = document.createElement("div");
        titleElement.className = "title";
        titleElement.textContent = title;
        content.appendChild(titleElement);

        var bodyElement = content.appendChild(document.createElement("div"));
        bodyElement.className = "body";

        var typeTreeView = new WebInspector.TypeTreeView(typeDescription);
        bodyElement.appendChild(typeTreeView.element);

        this._showPopover(content, bounds);
    }

    // Protected

    prettyPrint(pretty)
    {
        // The annotators must be cleared before pretty printing takes place and resumed
        // after so that they clear their annotations in a known state and insert new annotations
        // in the new state.
        var shouldResumeTypeTokenAnnotator = this._typeTokenAnnotator && this._typeTokenAnnotator.isActive();
        var shouldResumeBasicBlockAnnotator = this._basicBlockAnnotator && this._basicBlockAnnotator.isActive();
        if (shouldResumeTypeTokenAnnotator || shouldResumeBasicBlockAnnotator)
            this._setTypeTokenAnnotatorEnabledState(false);

        super.prettyPrint(pretty);

        if (pretty || !this._isProbablyMinified) {
            if (shouldResumeTypeTokenAnnotator || shouldResumeBasicBlockAnnotator)
                this._setTypeTokenAnnotatorEnabledState(true);
        } else {
            console.assert(!pretty && this._isProbablyMinified);
            if (this._typeTokenAnnotator || this._basicBlockAnnotator)
                this._setTypeTokenAnnotatorEnabledState(false);
        }
    }

    // Private

    _unformattedLineInfoForEditorLineInfo(lineInfo)
    {
        if (this.formatterSourceMap)
            return this.formatterSourceMap.formattedToOriginal(lineInfo.lineNumber, lineInfo.columnNumber);
        return lineInfo;
    }

    _sourceCodeLocationForEditorPosition(position)
    {
        var lineInfo = {lineNumber: position.line, columnNumber: position.ch};
        var unformattedLineInfo = this._unformattedLineInfoForEditorLineInfo(lineInfo);
        return this.sourceCode.createSourceCodeLocation(unformattedLineInfo.lineNumber, unformattedLineInfo.columnNumber);
    }

    _editorLineInfoForSourceCodeLocation(sourceCodeLocation)
    {
        if (this._sourceCode instanceof WebInspector.SourceMapResource)
            return {lineNumber: sourceCodeLocation.displayLineNumber, columnNumber: sourceCodeLocation.displayColumnNumber};
        return {lineNumber: sourceCodeLocation.formattedLineNumber, columnNumber: sourceCodeLocation.formattedColumnNumber};
    }

    _breakpointForEditorLineInfo(lineInfo)
    {
        if (!this._breakpointMap[lineInfo.lineNumber])
            return null;
        return this._breakpointMap[lineInfo.lineNumber][lineInfo.columnNumber];
    }

    _addBreakpointWithEditorLineInfo(breakpoint, lineInfo)
    {
        if (!this._breakpointMap[lineInfo.lineNumber])
            this._breakpointMap[lineInfo.lineNumber] = {};

        this._breakpointMap[lineInfo.lineNumber][lineInfo.columnNumber] = breakpoint;
    }

    _removeBreakpointWithEditorLineInfo(breakpoint, lineInfo)
    {
        console.assert(breakpoint === this._breakpointMap[lineInfo.lineNumber][lineInfo.columnNumber]);

        delete this._breakpointMap[lineInfo.lineNumber][lineInfo.columnNumber];

        if (isEmptyObject(this._breakpointMap[lineInfo.lineNumber]))
            delete this._breakpointMap[lineInfo.lineNumber];
    }

    _contentWillPopulate(content)
    {
        this.dispatchEventToListeners(WebInspector.SourceCodeTextEditor.Event.ContentWillPopulate);

        // We only do the rest of this work before the first populate.
        if (this._contentPopulated)
            return;

        if (this._supportsDebugging) {
            this._breakpointMap = {};

            var breakpoints = WebInspector.debuggerManager.breakpointsForSourceCode(this._sourceCode);
            for (var i = 0; i < breakpoints.length; ++i) {
                var breakpoint = breakpoints[i];
                console.assert(this._matchesBreakpoint(breakpoint));
                var lineInfo = this._editorLineInfoForSourceCodeLocation(breakpoint.sourceCodeLocation);
                this._addBreakpointWithEditorLineInfo(breakpoint, lineInfo);
                this.setBreakpointInfoForLineAndColumn(lineInfo.lineNumber, lineInfo.columnNumber, this._breakpointInfoForBreakpoint(breakpoint));
            }
        }

        if (this._sourceCode instanceof WebInspector.Resource)
            this.mimeType = this._sourceCode.syntheticMIMEType;
        else if (this._sourceCode instanceof WebInspector.Script)
            this.mimeType = "text/javascript";

        // Automatically format the content if it looks minified and it can be formatted.
        console.assert(!this.formatted);
        if (this.canBeFormatted() && this._isLikelyMinified(content)) {
            this.autoFormat = true;
            this._isProbablyMinified = true;
        }
    }

    _isLikelyMinified(content)
    {
        let whiteSpaceCount = 0;
        let ratio = 0;

        for (let i = 0, size = Math.min(5000, content.length); i < size; i++) {
            let char = content[i];
            if (char === " " || char === "\n" || char === "\t")
                whiteSpaceCount++;

            if (i >= 500) {
                ratio = whiteSpaceCount / i;
                if (ratio < 0.05)
                    return true;
            }
        }

        return ratio < 0.1;
    }

    _contentDidPopulate()
    {
        this._contentPopulated = true;

        this.dispatchEventToListeners(WebInspector.SourceCodeTextEditor.Event.ContentDidPopulate);

        // We add the issues each time content is populated. This is needed because lines might not exist
        // if we tried added them before when the full content wasn't available. (When populating with
        // partial script content this can be called multiple times.)

        this._reinsertAllIssues();

        this._updateEditableMarkers();
    }

    _populateWithContent(content)
    {
        content = content || "";

        this._contentWillPopulate(content);
        this.string = content;

        this._makeTypeTokenAnnotator();
        this._makeBasicBlockAnnotator();

        if (WebInspector.showJavaScriptTypeInformationSetting.value) {
            if (this._basicBlockAnnotator || this._typeTokenAnnotator)
                this._setTypeTokenAnnotatorEnabledState(true);
        }

        this._contentDidPopulate();
    }

    _contentAvailable(parameters)
    {
        // Return if resource is not available.
        if (parameters.error)
            return;

        var sourceCode = parameters.sourceCode;
        var content = sourceCode.content;
        var base64Encoded = parameters.base64Encoded;

        console.assert(sourceCode === this._sourceCode);
        console.assert(!base64Encoded);

        // Abort if the full content populated while waiting for this async callback.
        if (this._fullContentPopulated)
            return;

        this._fullContentPopulated = true;
        this._invalidLineNumbers = {};

        this._populateWithContent(content);
    }

    _breakpointStatusDidChange(event)
    {
        this._updateBreakpointStatus(event.target);
    }

    _breakpointsEnabledDidChange()
    {
        console.assert(this._supportsDebugging);

        var breakpoints = WebInspector.debuggerManager.breakpointsForSourceCode(this._sourceCode);
        for (var breakpoint of breakpoints)
            this._updateBreakpointStatus(breakpoint);
    }

    _updateBreakpointStatus(breakpoint)
    {
        console.assert(this._supportsDebugging);

        if (!this._contentPopulated)
            return;

        if (!this._matchesBreakpoint(breakpoint))
            return;

        var lineInfo = this._editorLineInfoForSourceCodeLocation(breakpoint.sourceCodeLocation);
        this.setBreakpointInfoForLineAndColumn(lineInfo.lineNumber, lineInfo.columnNumber, this._breakpointInfoForBreakpoint(breakpoint));
    }

    _updateBreakpointLocation(event)
    {
        console.assert(this._supportsDebugging);

        if (!this._contentPopulated)
            return;

        var breakpoint = event.target;
        if (!this._matchesBreakpoint(breakpoint))
            return;

        if (this._ignoreAllBreakpointLocationUpdates)
            return;

        if (breakpoint === this._ignoreLocationUpdateBreakpoint)
            return;

        var sourceCodeLocation = breakpoint.sourceCodeLocation;

        if (this._sourceCode instanceof WebInspector.SourceMapResource) {
            // Update our breakpoint location if the display location changed.
            if (sourceCodeLocation.displaySourceCode !== this._sourceCode)
                return;
            var oldLineInfo = {lineNumber: event.data.oldDisplayLineNumber, columnNumber: event.data.oldDisplayColumnNumber};
            var newLineInfo = {lineNumber: sourceCodeLocation.displayLineNumber, columnNumber: sourceCodeLocation.displayColumnNumber};
        } else {
            // Update our breakpoint location if the original location changed.
            if (sourceCodeLocation.sourceCode !== this._sourceCode)
                return;
            var oldLineInfo = {lineNumber: event.data.oldFormattedLineNumber, columnNumber: event.data.oldFormattedColumnNumber};
            var newLineInfo = {lineNumber: sourceCodeLocation.formattedLineNumber, columnNumber: sourceCodeLocation.formattedColumnNumber};
        }

        var existingBreakpoint = this._breakpointForEditorLineInfo(oldLineInfo);
        if (!existingBreakpoint)
            return;

        console.assert(breakpoint === existingBreakpoint);

        this.setBreakpointInfoForLineAndColumn(oldLineInfo.lineNumber, oldLineInfo.columnNumber, null);
        this.setBreakpointInfoForLineAndColumn(newLineInfo.lineNumber, newLineInfo.columnNumber, this._breakpointInfoForBreakpoint(breakpoint));

        this._removeBreakpointWithEditorLineInfo(breakpoint, oldLineInfo);
        this._addBreakpointWithEditorLineInfo(breakpoint, newLineInfo);
    }

    _breakpointAdded(event)
    {
        console.assert(this._supportsDebugging);

        if (!this._contentPopulated)
            return;

        var breakpoint = event.data.breakpoint;
        if (!this._matchesBreakpoint(breakpoint))
            return;

        if (breakpoint === this._ignoreBreakpointAddedBreakpoint)
            return;

        var lineInfo = this._editorLineInfoForSourceCodeLocation(breakpoint.sourceCodeLocation);
        this._addBreakpointWithEditorLineInfo(breakpoint, lineInfo);
        this.setBreakpointInfoForLineAndColumn(lineInfo.lineNumber, lineInfo.columnNumber, this._breakpointInfoForBreakpoint(breakpoint));
    }

    _breakpointRemoved(event)
    {
        console.assert(this._supportsDebugging);

        if (!this._contentPopulated)
            return;

        var breakpoint = event.data.breakpoint;
        if (!this._matchesBreakpoint(breakpoint))
            return;

        if (breakpoint === this._ignoreBreakpointRemovedBreakpoint)
            return;

        var lineInfo = this._editorLineInfoForSourceCodeLocation(breakpoint.sourceCodeLocation);
        this._removeBreakpointWithEditorLineInfo(breakpoint, lineInfo);
        this.setBreakpointInfoForLineAndColumn(lineInfo.lineNumber, lineInfo.columnNumber, null);
    }

    _activeCallFrameDidChange()
    {
        console.assert(this._supportsDebugging);

        if (this._activeCallFrameSourceCodeLocation) {
            this._activeCallFrameSourceCodeLocation.removeEventListener(WebInspector.SourceCodeLocation.Event.LocationChanged, this._activeCallFrameSourceCodeLocationChanged, this);
            delete this._activeCallFrameSourceCodeLocation;
        }

        var activeCallFrame = WebInspector.debuggerManager.activeCallFrame;
        if (!activeCallFrame || !this._matchesSourceCodeLocation(activeCallFrame.sourceCodeLocation)) {
            this.executionLineNumber = NaN;
            this.executionColumnNumber = NaN;
            return;
        }

        this._dismissPopover();

        this._activeCallFrameSourceCodeLocation = activeCallFrame.sourceCodeLocation;
        this._activeCallFrameSourceCodeLocation.addEventListener(WebInspector.SourceCodeLocation.Event.LocationChanged, this._activeCallFrameSourceCodeLocationChanged, this);

        // Don't return early if the line number didn't change. The execution state still
        // could have changed (e.g. continuing in a loop with a breakpoint inside).

        var lineInfo = this._editorLineInfoForSourceCodeLocation(activeCallFrame.sourceCodeLocation);
        this.executionLineNumber = lineInfo.lineNumber;
        this.executionColumnNumber = lineInfo.columnNumber;

        // If we have full content or this source code isn't a Resource we can return early.
        // Script source code populates from the request started in the constructor.
        if (this._fullContentPopulated || !(this._sourceCode instanceof WebInspector.Resource) || this._requestingScriptContent)
            return;

        // Since we are paused in the debugger we need to show some content, and since the Resource
        // content hasn't populated yet we need to populate with content from the Scripts by URL.
        // Document resources will attempt to populate the scripts as inline (in <script> tags.)
        // Other resources are assumed to be full scripts (JavaScript resources).
        if (this._sourceCode.type === WebInspector.Resource.Type.Document)
            this._populateWithInlineScriptContent();
        else
            this._populateWithScriptContent();
    }

    _activeCallFrameSourceCodeLocationChanged(event)
    {
        console.assert(!isNaN(this.executionLineNumber));
        if (isNaN(this.executionLineNumber))
            return;

        console.assert(WebInspector.debuggerManager.activeCallFrame);
        console.assert(this._activeCallFrameSourceCodeLocation === WebInspector.debuggerManager.activeCallFrame.sourceCodeLocation);

        var lineInfo = this._editorLineInfoForSourceCodeLocation(this._activeCallFrameSourceCodeLocation);
        this.executionLineNumber = lineInfo.lineNumber;
        this.executionColumnNumber = lineInfo.columnNumber;
    }

    _populateWithInlineScriptContent()
    {
        console.assert(this._sourceCode instanceof WebInspector.Resource);
        console.assert(!this._fullContentPopulated);
        console.assert(!this._requestingScriptContent);

        var scripts = this._sourceCode.scripts;
        console.assert(scripts.length);
        if (!scripts.length)
            return;

        var pendingRequestCount = scripts.length;

        // If the number of scripts hasn't change since the last populate, then there is nothing to do.
        if (this._inlineScriptContentPopulated === pendingRequestCount)
            return;

        this._inlineScriptContentPopulated = pendingRequestCount;

        function scriptContentAvailable(parameters)
        {
            // Return early if we are still waiting for content from other scripts.
            if (--pendingRequestCount)
                return;

            delete this._requestingScriptContent;

            // Abort if the full content populated while waiting for these async callbacks.
            if (this._fullContentPopulated)
                return;

            var scriptOpenTag = "<script>";
            var scriptCloseTag = "</script>";

            var content = "";
            var lineNumber = 0;
            var columnNumber = 0;

            this._invalidLineNumbers = {};

            for (var i = 0; i < scripts.length; ++i) {
                // Fill the line gap with newline characters.
                for (var newLinesCount = scripts[i].range.startLine - lineNumber; newLinesCount > 0; --newLinesCount) {
                    if (!columnNumber)
                        this._invalidLineNumbers[scripts[i].range.startLine - newLinesCount] = true;
                    columnNumber = 0;
                    content += "\n";
                }

                // Fill the column gap with space characters.
                for (var spacesCount = scripts[i].range.startColumn - columnNumber - scriptOpenTag.length; spacesCount > 0; --spacesCount)
                    content += " ";

                // Add script tags and content.
                content += scriptOpenTag;
                content += scripts[i].content;
                content += scriptCloseTag;

                lineNumber = scripts[i].range.endLine;
                columnNumber = scripts[i].range.endColumn + scriptCloseTag.length;
            }

            this._populateWithContent(content);
        }

        this._requestingScriptContent = true;

        var boundScriptContentAvailable = scriptContentAvailable.bind(this);
        for (var i = 0; i < scripts.length; ++i)
            scripts[i].requestContent().then(boundScriptContentAvailable);
    }

    _populateWithScriptContent()
    {
        console.assert(this._sourceCode instanceof WebInspector.Resource);
        console.assert(!this._fullContentPopulated);
        console.assert(!this._requestingScriptContent);

        // We can assume this resource only has one script that starts at line/column 0.
        var scripts = this._sourceCode.scripts;
        console.assert(scripts.length === 1);
        if (!scripts.length)
            return;

        console.assert(scripts[0].range.startLine === 0);
        console.assert(scripts[0].range.startColumn === 0);

        function scriptContentAvailable(parameters)
        {
            var content = parameters.content;
            delete this._requestingScriptContent;

            // Abort if the full content populated while waiting for this async callback.
            if (this._fullContentPopulated)
                return;

            // This is the full content.
            this._fullContentPopulated = true;

            this._populateWithContent(content);
        }

        this._requestingScriptContent = true;

        scripts[0].requestContent().then(scriptContentAvailable.bind(this));
    }

    _matchesSourceCodeLocation(sourceCodeLocation)
    {
        if (this._sourceCode instanceof WebInspector.SourceMapResource)
            return sourceCodeLocation.displaySourceCode === this._sourceCode;
        if (this._sourceCode instanceof WebInspector.Resource)
            return sourceCodeLocation.sourceCode.url === this._sourceCode.url;
        if (this._sourceCode instanceof WebInspector.Script)
            return sourceCodeLocation.sourceCode === this._sourceCode;
        return false;
    }

    _matchesBreakpoint(breakpoint)
    {
        console.assert(this._supportsDebugging);
        if (this._sourceCode instanceof WebInspector.SourceMapResource)
            return breakpoint.sourceCodeLocation.displaySourceCode === this._sourceCode;
        if (this._sourceCode instanceof WebInspector.Resource)
            return breakpoint.url === this._sourceCode.url;
        if (this._sourceCode instanceof WebInspector.Script)
            return breakpoint.url === this._sourceCode.url || breakpoint.scriptIdentifier === this._sourceCode.id;
        return false;
    }

    _matchesIssue(issue)
    {
        if (this._sourceCode instanceof WebInspector.Resource)
            return issue.url === this._sourceCode.url;
        // FIXME: Support issues for Scripts based on id, not only by URL.
        if (this._sourceCode instanceof WebInspector.Script)
            return issue.url === this._sourceCode.url;
        return false;
    }

    _issueWasAdded(event)
    {
        var issue = event.data.issue;
        if (!this._matchesIssue(issue))
            return;

        this._addIssue(issue);
    }

    _addIssue(issue)
    {
        // FIXME: Issue should have a SourceCodeLocation.
        var sourceCodeLocation = this._sourceCode.createSourceCodeLocation(issue.lineNumber, issue.columnNumber);
        var lineNumber = sourceCodeLocation.formattedLineNumber;

        var lineNumberIssues = this._issuesLineNumberMap.get(lineNumber);
        if (!lineNumberIssues) {
            lineNumberIssues = [];
            this._issuesLineNumberMap.set(lineNumber, lineNumberIssues);
        }

        // Avoid displaying duplicate issues on the same line.
        for (var existingIssue of lineNumberIssues) {
            if (existingIssue.columnNumber === issue.columnNumber && existingIssue.text === issue.text)
                return;
        }

        lineNumberIssues.push(issue);

        if (issue.level === WebInspector.IssueMessage.Level.Error)
            this.addStyleClassToLine(lineNumber, WebInspector.SourceCodeTextEditor.LineErrorStyleClassName);
        else if (issue.level === WebInspector.IssueMessage.Level.Warning)
            this.addStyleClassToLine(lineNumber, WebInspector.SourceCodeTextEditor.LineWarningStyleClassName);
        else
            console.error("Unknown issue level");

        var widget = this._issueWidgetForLine(lineNumber);
        if (widget) {
            if (issue.level === WebInspector.IssueMessage.Level.Error)
                widget.widgetElement.classList.add(WebInspector.SourceCodeTextEditor.LineErrorStyleClassName);
            else if (issue.level === WebInspector.IssueMessage.Level.Warning)
                widget.widgetElement.classList.add(WebInspector.SourceCodeTextEditor.LineWarningStyleClassName);

            this._updateIssueWidgetForIssues(widget, lineNumberIssues);
        }
    }

    _issueWidgetForLine(lineNumber)
    {
        var widget = this._widgetMap.get(lineNumber);
        if (widget)
            return widget;

        widget = this.createWidgetForLine(lineNumber);
        if (!widget)
            return null;

        var widgetElement = widget.widgetElement;
        widgetElement.classList.add("issue-widget", "inline");
        widgetElement.addEventListener("click", this._handleWidgetClick.bind(this, widget, lineNumber));

        this._widgetMap.set(lineNumber, widget);

        return widget;
    }

    _iconClassNameForIssueLevel(level)
    {
        if (level === WebInspector.IssueMessage.Level.Warning)
            return "icon-warning";

        console.assert(level === WebInspector.IssueMessage.Level.Error);
        return "icon-error";
    }

    _updateIssueWidgetForIssues(widget, issues)
    {
        var widgetElement = widget.widgetElement;
        widgetElement.removeChildren();

        if (widgetElement.classList.contains("inline") || issues.length === 1) {
            var arrowElement = widgetElement.appendChild(document.createElement("span"));
            arrowElement.className = "arrow";

            var iconElement = widgetElement.appendChild(document.createElement("span"));
            iconElement.className = "icon";

            var textElement = widgetElement.appendChild(document.createElement("span"));
            textElement.className = "text";

            if (issues.length === 1) {
                iconElement.classList.add(this._iconClassNameForIssueLevel(issues[0].level));
                textElement.textContent = issues[0].text;
            } else {
                var errorsCount = 0;
                var warningsCount = 0;
                for (var issue of issues) {
                    if (issue.level === WebInspector.IssueMessage.Level.Error)
                        ++errorsCount;
                    else if (issue.level === WebInspector.IssueMessage.Level.Warning)
                        ++warningsCount;
                }

                if (warningsCount && errorsCount) {
                    iconElement.classList.add(this._iconClassNameForIssueLevel(issue.level));
                    textElement.textContent = WebInspector.UIString("%d Errors, %d Warnings").format(errorsCount, warningsCount);
                } else if (errorsCount) {
                    iconElement.classList.add(this._iconClassNameForIssueLevel(issue.level));
                    textElement.textContent = WebInspector.UIString("%d Errors").format(errorsCount);
                } else if (warningsCount) {
                    iconElement.classList.add(this._iconClassNameForIssueLevel(issue.level));
                    textElement.textContent = WebInspector.UIString("%d Warnings").format(warningsCount);
                }

                widget[WebInspector.SourceCodeTextEditor.WidgetContainsMultipleIssuesSymbol] = true;
            }
        } else {
            for (var issue of issues) {
                var iconElement = widgetElement.appendChild(document.createElement("span"));
                iconElement.className = "icon";
                iconElement.classList.add(this._iconClassNameForIssueLevel(issue.level));

                var textElement = widgetElement.appendChild(document.createElement("span"));
                textElement.className = "text";
                textElement.textContent = issue.text;

                widgetElement.appendChild(document.createElement("br"));
            }
        }

        widget.update();
    }

    _isWidgetToggleable(widget)
    {
        if (widget[WebInspector.SourceCodeTextEditor.WidgetContainsMultipleIssuesSymbol])
            return true;

        if (!widget.widgetElement.classList.contains("inline"))
            return true;

        var textElement = widget.widgetElement.lastChild;
        if (textElement.offsetWidth !== textElement.scrollWidth)
            return true;

        return false;
    }

    _handleWidgetClick(widget, lineNumber, event)
    {
        if (!this._isWidgetToggleable(widget))
            return;

        widget.widgetElement.classList.toggle("inline");

        var lineNumberIssues = this._issuesLineNumberMap.get(lineNumber);
        this._updateIssueWidgetForIssues(widget, lineNumberIssues);
    }

    _breakpointInfoForBreakpoint(breakpoint)
    {
        return {resolved: breakpoint.resolved, disabled: breakpoint.disabled, autoContinue: breakpoint.autoContinue};
    }

    get _supportsDebugging()
    {
        if (this._sourceCode instanceof WebInspector.Resource)
            return this._sourceCode.type === WebInspector.Resource.Type.Document || this._sourceCode.type === WebInspector.Resource.Type.Script;
        if (this._sourceCode instanceof WebInspector.Script)
            return true;
        return false;
    }

    // TextEditor Delegate

    textEditorBaseURL(textEditor)
    {
        return this._sourceCode.url;
    }

    textEditorShouldHideLineNumber(textEditor, lineNumber)
    {
        return lineNumber in this._invalidLineNumbers;
    }

    textEditorGutterContextMenu(textEditor, lineNumber, columnNumber, editorBreakpoints, event)
    {
        if (!this._supportsDebugging)
            return;

        event.preventDefault();

        let addBreakpoint = () => {
            let data = this.textEditorBreakpointAdded(this, lineNumber, columnNumber);
            this.setBreakpointInfoForLineAndColumn(data.lineNumber, data.columnNumber, data.breakpointInfo);
        };

        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);

        // Paused. Add Continue to Here option only if we have a script identifier for the location.
        if (WebInspector.debuggerManager.paused) {
            let editorLineInfo = {lineNumber, columnNumber};
            let unformattedLineInfo = this._unformattedLineInfoForEditorLineInfo(editorLineInfo);
            let sourceCodeLocation = this._sourceCode.createSourceCodeLocation(unformattedLineInfo.lineNumber, unformattedLineInfo.columnNumber);

            let script;
            if (sourceCodeLocation.sourceCode instanceof WebInspector.Script)
                script = sourceCodeLocation.sourceCode;
            else if (sourceCodeLocation.sourceCode instanceof WebInspector.Resource)
                script = sourceCodeLocation.sourceCode.scriptForLocation(sourceCodeLocation);

            if (script) {
                contextMenu.appendItem(WebInspector.UIString("Continue to Here"), () => {
                    WebInspector.debuggerManager.continueToLocation(script.id, sourceCodeLocation.lineNumber, sourceCodeLocation.columnNumber);
                });
                contextMenu.appendSeparator();
            }
        }

        let breakpoints = [];
        for (let lineInfo of editorBreakpoints) {
            let breakpoint = this._breakpointForEditorLineInfo(lineInfo);
            console.assert(breakpoint);
            if (breakpoint)
                breakpoints.push(breakpoint);
        }

        // No breakpoints.
        if (!breakpoints.length) {
            contextMenu.appendItem(WebInspector.UIString("Add Breakpoint"), addBreakpoint.bind(this));
            return;
        }

        // Single breakpoint.
        if (breakpoints.length === 1) {
            WebInspector.breakpointPopoverController.appendContextMenuItems(contextMenu, breakpoints[0], event.target);

            if (!WebInspector.isShowingDebuggerTab()) {
                contextMenu.appendSeparator();
                contextMenu.appendItem(WebInspector.UIString("Reveal in Debugger Tab"), () => {
                    WebInspector.showDebuggerTab(breakpoint);
                });
            }

            return;
        }

        // Multiple breakpoints.
        let removeBreakpoints = () => {
            for (let breakpoint of breakpoints) {
                if (WebInspector.debuggerManager.isBreakpointRemovable(breakpoint))
                    WebInspector.debuggerManager.removeBreakpoint(breakpoint);
            }
        };

        let shouldDisable = breakpoints.some((breakpoint) => !breakpoint.disabled);
        let toggleBreakpoints = (shouldDisable) => {
            for (let breakpoint of breakpoints)
                breakpoint.disabled = shouldDisable;
        };

        if (shouldDisable)
            contextMenu.appendItem(WebInspector.UIString("Disable Breakpoints"), toggleBreakpoints);
        else
            contextMenu.appendItem(WebInspector.UIString("Enable Breakpoints"), toggleBreakpoints);
        contextMenu.appendItem(WebInspector.UIString("Delete Breakpoints"), removeBreakpoints);
    }

    textEditorBreakpointAdded(textEditor, lineNumber, columnNumber)
    {
        if (!this._supportsDebugging)
            return null;

        var editorLineInfo = {lineNumber, columnNumber};
        var unformattedLineInfo = this._unformattedLineInfoForEditorLineInfo(editorLineInfo);
        var sourceCodeLocation = this._sourceCode.createSourceCodeLocation(unformattedLineInfo.lineNumber, unformattedLineInfo.columnNumber);
        var breakpoint = new WebInspector.Breakpoint(sourceCodeLocation);

        var lineInfo = this._editorLineInfoForSourceCodeLocation(breakpoint.sourceCodeLocation);
        this._addBreakpointWithEditorLineInfo(breakpoint, lineInfo);

        this._ignoreBreakpointAddedBreakpoint = breakpoint;

        var shouldSkipEventDispatch = false;
        var shouldSpeculativelyResolveBreakpoint = true;
        WebInspector.debuggerManager.addBreakpoint(breakpoint, shouldSkipEventDispatch, shouldSpeculativelyResolveBreakpoint);
        delete this._ignoreBreakpointAddedBreakpoint;

        // Return the more accurate location and breakpoint info.
        return {
            breakpointInfo: this._breakpointInfoForBreakpoint(breakpoint),
            lineNumber: lineInfo.lineNumber,
            columnNumber: lineInfo.columnNumber
        };
    }

    textEditorBreakpointRemoved(textEditor, lineNumber, columnNumber)
    {
        console.assert(this._supportsDebugging);
        if (!this._supportsDebugging)
            return;

        var lineInfo = {lineNumber, columnNumber};
        var breakpoint = this._breakpointForEditorLineInfo(lineInfo);
        console.assert(breakpoint);
        if (!breakpoint)
            return;

        this._removeBreakpointWithEditorLineInfo(breakpoint, lineInfo);

        this._ignoreBreakpointRemovedBreakpoint = breakpoint;
        WebInspector.debuggerManager.removeBreakpoint(breakpoint);
        delete this._ignoreBreakpointAddedBreakpoint;
    }

    textEditorBreakpointMoved(textEditor, oldLineNumber, oldColumnNumber, newLineNumber, newColumnNumber)
    {
        console.assert(this._supportsDebugging);
        if (!this._supportsDebugging)
            return;

        var oldLineInfo = {lineNumber: oldLineNumber, columnNumber: oldColumnNumber};
        var breakpoint = this._breakpointForEditorLineInfo(oldLineInfo);
        console.assert(breakpoint);
        if (!breakpoint)
            return;

        this._removeBreakpointWithEditorLineInfo(breakpoint, oldLineInfo);

        var newLineInfo = {lineNumber: newLineNumber, columnNumber: newColumnNumber};
        var unformattedNewLineInfo = this._unformattedLineInfoForEditorLineInfo(newLineInfo);
        this._ignoreLocationUpdateBreakpoint = breakpoint;
        breakpoint.sourceCodeLocation.update(this._sourceCode, unformattedNewLineInfo.lineNumber, unformattedNewLineInfo.columnNumber);
        delete this._ignoreLocationUpdateBreakpoint;

        var accurateNewLineInfo = this._editorLineInfoForSourceCodeLocation(breakpoint.sourceCodeLocation);
        this._addBreakpointWithEditorLineInfo(breakpoint, accurateNewLineInfo);

        if (accurateNewLineInfo.lineNumber !== newLineInfo.lineNumber || accurateNewLineInfo.columnNumber !== newLineInfo.columnNumber)
            this.updateBreakpointLineAndColumn(newLineInfo.lineNumber, newLineInfo.columnNumber, accurateNewLineInfo.lineNumber, accurateNewLineInfo.columnNumber);
    }

    textEditorBreakpointClicked(textEditor, lineNumber, columnNumber)
    {
        console.assert(this._supportsDebugging);
        if (!this._supportsDebugging)
            return;

        var breakpoint = this._breakpointForEditorLineInfo({lineNumber, columnNumber});
        console.assert(breakpoint);
        if (!breakpoint)
            return;

        breakpoint.cycleToNextMode();
    }

    textEditorUpdatedFormatting(textEditor)
    {
        this._ignoreAllBreakpointLocationUpdates = true;
        this._sourceCode.formatterSourceMap = this.formatterSourceMap;
        delete this._ignoreAllBreakpointLocationUpdates;

        // Always put the source map on both the Script and Resource if both exist. For example,
        // if this SourceCode is a Resource, then there might also be a Script. In the debugger,
        // the backend identifies call frames with Script line and column information, and the
        // Script needs the formatter source map to produce the proper display line and column.
        if (this._sourceCode instanceof WebInspector.Resource && !(this._sourceCode instanceof WebInspector.SourceMapResource)) {
            var scripts = this._sourceCode.scripts;
            for (var i = 0; i < scripts.length; ++i)
                scripts[i].formatterSourceMap = this.formatterSourceMap;
        } else if (this._sourceCode instanceof WebInspector.Script) {
            if (this._sourceCode.resource)
                this._sourceCode.resource.formatterSourceMap = this.formatterSourceMap;
        }

        // Some breakpoints / issues may have moved, some might not have. Just go through
        // and remove and reinsert all the breakpoints / issues.

        var oldBreakpointMap = this._breakpointMap;
        this._breakpointMap = {};

        for (var lineNumber in oldBreakpointMap) {
            for (var columnNumber in oldBreakpointMap[lineNumber]) {
                var breakpoint = oldBreakpointMap[lineNumber][columnNumber];
                var newLineInfo = this._editorLineInfoForSourceCodeLocation(breakpoint.sourceCodeLocation);
                this._addBreakpointWithEditorLineInfo(breakpoint, newLineInfo);
                this.setBreakpointInfoForLineAndColumn(lineNumber, columnNumber, null);
                this.setBreakpointInfoForLineAndColumn(newLineInfo.lineNumber, newLineInfo.columnNumber, this._breakpointInfoForBreakpoint(breakpoint));
            }
        }

        this._reinsertAllIssues();
    }

    _clearWidgets()
    {
        for (var widget of this._widgetMap.values())
            widget.clear();

        this._widgetMap.clear();
    }

    _reinsertAllIssues()
    {
        this._issuesLineNumberMap.clear();
        this._clearWidgets();

        var issues = WebInspector.issueManager.issuesForSourceCode(this._sourceCode);
        for (var issue of issues) {
            console.assert(this._matchesIssue(issue));
            this._addIssue(issue);
        }
    }

    _debuggerDidPause(event)
    {
        this._updateTokenTrackingControllerState();
        if (this._typeTokenAnnotator && this._typeTokenAnnotator.isActive())
            this._typeTokenAnnotator.refresh();
        if (this._basicBlockAnnotator && this._basicBlockAnnotator.isActive())
            this._basicBlockAnnotator.refresh();
    }

    _debuggerDidResume(event)
    {
        this._updateTokenTrackingControllerState();
        this._dismissPopover();
        if (this._typeTokenAnnotator && this._typeTokenAnnotator.isActive())
            this._typeTokenAnnotator.refresh();
        if (this._basicBlockAnnotator && this._basicBlockAnnotator.isActive())
            this._basicBlockAnnotator.refresh();
    }

    _sourceCodeSourceMapAdded(event)
    {
        WebInspector.notifications.addEventListener(WebInspector.Notification.GlobalModifierKeysDidChange, this._updateTokenTrackingControllerState, this);
        this._sourceCode.removeEventListener(WebInspector.SourceCode.Event.SourceMapAdded, this._sourceCodeSourceMapAdded, this);

        this._updateTokenTrackingControllerState();
    }

    _updateTokenTrackingControllerState()
    {
        var mode = WebInspector.CodeMirrorTokenTrackingController.Mode.None;
        if (WebInspector.debuggerManager.paused)
            mode = WebInspector.CodeMirrorTokenTrackingController.Mode.JavaScriptExpression;
        else if (this._typeTokenAnnotator && this._typeTokenAnnotator.isActive())
            mode = WebInspector.CodeMirrorTokenTrackingController.Mode.JavaScriptTypeInformation;
        else if (this._hasColorMarkers())
            mode = WebInspector.CodeMirrorTokenTrackingController.Mode.MarkedTokens;
        else if ((this._sourceCode instanceof WebInspector.SourceMapResource || this._sourceCode.sourceMaps.length !== 0) && WebInspector.modifierKeys.metaKey && !WebInspector.modifierKeys.altKey && !WebInspector.modifierKeys.shiftKey)
            mode = WebInspector.CodeMirrorTokenTrackingController.Mode.NonSymbolTokens;

        this.tokenTrackingController.enabled = mode !== WebInspector.CodeMirrorTokenTrackingController.Mode.None;

        if (mode === this.tokenTrackingController.mode)
            return;

        switch (mode) {
        case WebInspector.CodeMirrorTokenTrackingController.Mode.MarkedTokens:
            this.tokenTrackingController.mouseOverDelayDuration = 0;
            this.tokenTrackingController.mouseOutReleaseDelayDuration = 0;
            break;
        case WebInspector.CodeMirrorTokenTrackingController.Mode.NonSymbolTokens:
            this.tokenTrackingController.mouseOverDelayDuration = 0;
            this.tokenTrackingController.mouseOutReleaseDelayDuration = 0;
            this.tokenTrackingController.classNameForHighlightedRange = WebInspector.CodeMirrorTokenTrackingController.JumpToSymbolHighlightStyleClassName;
            this._dismissPopover();
            break;
        case WebInspector.CodeMirrorTokenTrackingController.Mode.JavaScriptExpression:
        case WebInspector.CodeMirrorTokenTrackingController.Mode.JavaScriptTypeInformation:
            this.tokenTrackingController.mouseOverDelayDuration = WebInspector.SourceCodeTextEditor.DurationToMouseOverTokenToMakeHoveredToken;
            this.tokenTrackingController.mouseOutReleaseDelayDuration = WebInspector.SourceCodeTextEditor.DurationToMouseOutOfHoveredTokenToRelease;
            this.tokenTrackingController.classNameForHighlightedRange = WebInspector.SourceCodeTextEditor.HoveredExpressionHighlightStyleClassName;
            break;
        }

        this.tokenTrackingController.mode = mode;
    }

    _hasColorMarkers()
    {
        for (var marker of this.markers) {
            if (marker.type === WebInspector.TextMarker.Type.Color)
                return true;
        }
        return false;
    }

    // CodeMirrorTokenTrackingController Delegate

    tokenTrackingControllerCanReleaseHighlightedRange(tokenTrackingController, element)
    {
        if (!this._popover)
            return true;

        if (!window.getSelection().isCollapsed && this._popover.element.contains(window.getSelection().anchorNode))
            return false;

        return true;
    }

    tokenTrackingControllerHighlightedRangeReleased(tokenTrackingController, forceHide = false)
    {
        if (forceHide || !this._mouseIsOverPopover)
            this._dismissPopover();
    }

    tokenTrackingControllerHighlightedRangeWasClicked(tokenTrackingController)
    {
        if (this.tokenTrackingController.mode !== WebInspector.CodeMirrorTokenTrackingController.Mode.NonSymbolTokens)
            return;

        // Links are handled by TextEditor.
        if (/\blink\b/.test(this.tokenTrackingController.candidate.hoveredToken.type))
            return;

        var sourceCodeLocation = this._sourceCodeLocationForEditorPosition(this.tokenTrackingController.candidate.hoveredTokenRange.start);
        if (this.sourceCode instanceof WebInspector.SourceMapResource)
            WebInspector.showOriginalOrFormattedSourceCodeLocation(sourceCodeLocation);
        else
            WebInspector.showSourceCodeLocation(sourceCodeLocation);
    }

    tokenTrackingControllerNewHighlightCandidate(tokenTrackingController, candidate)
    {
        if (this.tokenTrackingController.mode === WebInspector.CodeMirrorTokenTrackingController.Mode.NonSymbolTokens) {
            this.tokenTrackingController.highlightRange(candidate.hoveredTokenRange);
            return;
        }

        if (this.tokenTrackingController.mode === WebInspector.CodeMirrorTokenTrackingController.Mode.JavaScriptExpression) {
            this._tokenTrackingControllerHighlightedJavaScriptExpression(candidate);
            return;
        }

        if (this.tokenTrackingController.mode === WebInspector.CodeMirrorTokenTrackingController.Mode.JavaScriptTypeInformation) {
            this._tokenTrackingControllerHighlightedJavaScriptTypeInformation(candidate);
            return;
        }

        if (this.tokenTrackingController.mode === WebInspector.CodeMirrorTokenTrackingController.Mode.MarkedTokens) {
            var markers = this.markersAtPosition(candidate.hoveredTokenRange.start);
            if (markers.length > 0)
                this._tokenTrackingControllerHighlightedMarkedExpression(candidate, markers);
            else
                this._dismissEditingController();
        }
    }

    tokenTrackingControllerMouseOutOfHoveredMarker(tokenTrackingController, hoveredMarker)
    {
        this._dismissEditingController();
    }

    _tokenTrackingControllerHighlightedJavaScriptExpression(candidate)
    {
        console.assert(candidate.expression);

        function populate(error, result, wasThrown)
        {
            if (error || wasThrown)
                return;

            if (candidate !== this.tokenTrackingController.candidate)
                return;

            var data = WebInspector.RemoteObject.fromPayload(result);
            switch (data.type) {
            case "function":
                this._showPopoverForFunction(data);
                break;
            case "object":
                if (data.subtype === "null" || data.subtype === "regexp")
                    this._showPopoverWithFormattedValue(data);
                else
                    this._showPopoverForObject(data);
                break;
            case "string":
            case "number":
            case "boolean":
            case "undefined":
            case "symbol":
                this._showPopoverWithFormattedValue(data);
                break;
            }
        }

        var expression = appendWebInspectorSourceURL(candidate.expression);

        if (WebInspector.debuggerManager.activeCallFrame) {
            DebuggerAgent.evaluateOnCallFrame.invoke({callFrameId: WebInspector.debuggerManager.activeCallFrame.id, expression, objectGroup: "popover", doNotPauseOnExceptionsAndMuteConsole: true}, populate.bind(this));
            return;
        }

        // No call frame available. Use the main page's context.
        RuntimeAgent.evaluate.invoke({expression, objectGroup: "popover", doNotPauseOnExceptionsAndMuteConsole: true}, populate.bind(this));
    }

    _tokenTrackingControllerHighlightedJavaScriptTypeInformation(candidate)
    {
        console.assert(candidate.expression);

        var sourceCode = this._sourceCode;
        var sourceID = sourceCode instanceof WebInspector.Script ? sourceCode.id : sourceCode.scripts[0].id;
        var range = candidate.hoveredTokenRange;
        var offset = this.currentPositionToOriginalOffset({line: range.start.line, ch: range.start.ch});

        var allRequests = [{
            typeInformationDescriptor: WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor.NormalExpression,
            sourceID,
            divot: offset
        }];

        function handler(error, allTypes) {
            if (error)
                return;

            if (candidate !== this.tokenTrackingController.candidate)
                return;

            console.assert(allTypes.length === 1);
            if (!allTypes.length)
                return;

            var typeDescription = WebInspector.TypeDescription.fromPayload(allTypes[0]);
            if (typeDescription.valid) {
                var popoverTitle = WebInspector.TypeTokenView.titleForPopover(WebInspector.TypeTokenView.TitleType.Variable, candidate.expression);
                this.showPopoverForTypes(typeDescription, null, popoverTitle);
            }
        }

        RuntimeAgent.getRuntimeTypesForVariablesAtOffsets(allRequests, handler.bind(this));
    }

    _showPopover(content, bounds)
    {
        console.assert(this.tokenTrackingController.candidate || bounds);

        var shouldHighlightRange = false;
        var candidate = this.tokenTrackingController.candidate;
        // If bounds is falsey, this is a popover introduced from a hover event.
        // Otherwise, this is called from TypeTokenAnnotator.
        if (!bounds) {
            if (!candidate)
                return;

            var rects = this.rectsForRange(candidate.hoveredTokenRange);
            bounds = WebInspector.Rect.unionOfRects(rects);

            shouldHighlightRange = true;
        }

        content.classList.add(WebInspector.SourceCodeTextEditor.PopoverDebuggerContentStyleClassName);

        this._popover = this._popover || new WebInspector.Popover(this);
        this._popover.presentNewContentWithFrame(content, bounds.pad(5), [WebInspector.RectEdge.MIN_Y, WebInspector.RectEdge.MAX_Y, WebInspector.RectEdge.MAX_X]);
        if (shouldHighlightRange)
            this.tokenTrackingController.highlightRange(candidate.expressionRange);

        this._trackPopoverEvents();
    }

    _showPopoverForFunction(data)
    {
        let candidate = this.tokenTrackingController.candidate;

        function didGetDetails(error, response)
        {
            if (error) {
                console.error(error);
                this._dismissPopover();
                return;
            }

            // Nothing to do if the token has changed since the time we
            // asked for the function details from the backend.
            if (candidate !== this.tokenTrackingController.candidate)
                return;

            let wrapper = document.createElement("div");
            wrapper.classList.add("body", "formatted-function");
            wrapper.textContent = data.description;

            let content = document.createElement("div");
            content.classList.add("function");

            let location = response.location;
            let sourceCode = WebInspector.debuggerManager.scriptForIdentifier(location.scriptId);
            let sourceCodeLocation = sourceCode.createSourceCodeLocation(location.lineNumber, location.columnNumber);
            let functionSourceCodeLink = WebInspector.createSourceCodeLocationLink(sourceCodeLocation);

            let title = content.appendChild(document.createElement("div"));
            title.classList.add("title");
            title.textContent = response.name || response.inferredName || response.displayName || WebInspector.UIString("(anonymous function)");
            title.appendChild(functionSourceCodeLink);

            content.appendChild(wrapper);

            this._showPopover(content);
        }
        DebuggerAgent.getFunctionDetails(data.objectId, didGetDetails.bind(this));
    }

    _showPopoverForObject(data)
    {
        var content = document.createElement("div");
        content.className = "object expandable";

        var titleElement = document.createElement("div");
        titleElement.className = "title";
        titleElement.textContent = data.description;
        content.appendChild(titleElement);

        if (data.subtype === "node") {
            data.pushNodeToFrontend(function(nodeId) {
                if (!nodeId)
                    return;

                var domNode = WebInspector.domTreeManager.nodeForId(nodeId);
                if (!domNode.ownerDocument)
                    return;

                var goToButton = titleElement.appendChild(WebInspector.createGoToArrowButton());
                goToButton.addEventListener("click", function() {
                    WebInspector.domTreeManager.inspectElement(nodeId);
                });
            });
        }

        // FIXME: If this is a variable, it would be nice to put the variable name in the PropertyPath.
        var objectTree = new WebInspector.ObjectTreeView(data);
        objectTree.showOnlyProperties();
        objectTree.expand();

        var bodyElement = content.appendChild(document.createElement("div"));
        bodyElement.className = "body";
        bodyElement.appendChild(objectTree.element);

        // Show the popover once we have the first set of properties for the object.
        var candidate = this.tokenTrackingController.candidate;
        objectTree.addEventListener(WebInspector.ObjectTreeView.Event.Updated, function() {
            if (candidate === this.tokenTrackingController.candidate)
                this._showPopover(content);
            objectTree.removeEventListener(null, null, this);
        }, this);
    }

    _showPopoverWithFormattedValue(remoteObject)
    {
        var content = WebInspector.FormattedValue.createElementForRemoteObject(remoteObject);
        this._showPopover(content);
    }

    willDismissPopover(popover)
    {
        this.tokenTrackingController.removeHighlightedRange();

        RuntimeAgent.releaseObjectGroup("popover");
    }

    _dismissPopover()
    {
        if (!this._popover)
            return;

        this._popover.dismiss();

        if (this._popoverEventListeners && this._popoverEventListenersAreRegistered) {
            this._popoverEventListenersAreRegistered = false;
            this._popoverEventListeners.unregister();
        }
    }

    _trackPopoverEvents()
    {
        if (!this._popoverEventListeners)
            this._popoverEventListeners = new WebInspector.EventListenerSet(this, "Popover listeners");
        if (!this._popoverEventListenersAreRegistered) {
            this._popoverEventListenersAreRegistered = true;
            this._popoverEventListeners.register(this._popover.element, "mouseover", this._popoverMouseover);
            this._popoverEventListeners.register(this._popover.element, "mouseout", this._popoverMouseout);
            this._popoverEventListeners.install();
        }
    }

    _popoverMouseover(event)
    {
        this._mouseIsOverPopover = true;
    }

    _popoverMouseout(event)
    {
        this._mouseIsOverPopover = this._popover.element.contains(event.relatedTarget);
    }

    _updateEditableMarkers(range)
    {
        this.createColorMarkers(range);
        this.createGradientMarkers(range);
        this.createCubicBezierMarkers(range);

        this._updateTokenTrackingControllerState();
    }

    _tokenTrackingControllerHighlightedMarkedExpression(candidate, markers)
    {
        // Look for the outermost editable marker.
        var editableMarker;
        for (var marker of markers) {
            if (!marker.range || (marker.type !== WebInspector.TextMarker.Type.Color && marker.type !== WebInspector.TextMarker.Type.Gradient && marker.type !== WebInspector.TextMarker.Type.CubicBezier))
                continue;

            if (!editableMarker || (marker.range.startLine < editableMarker.range.startLine || (marker.range.startLine === editableMarker.range.startLine && marker.range.startColumn < editableMarker.range.startColumn)))
                editableMarker = marker;
        }

        if (!editableMarker) {
            this.tokenTrackingController.hoveredMarker = null;
            return;
        }

        if (this.tokenTrackingController.hoveredMarker === editableMarker)
            return;

        this._dismissEditingController();

        this.tokenTrackingController.hoveredMarker = editableMarker;

        this._editingController = this.editingControllerForMarker(editableMarker);

        if (marker.type === WebInspector.TextMarker.Type.Color) {
            var color = this._editingController.value;
            if (!color || !color.valid) {
                editableMarker.clear();
                delete this._editingController;
                return;
            }
        }

        this._editingController.delegate = this;
        this._editingController.presentHoverMenu();
    }

    _dismissEditingController(discrete)
    {
        if (this._editingController)
            this._editingController.dismissHoverMenu(discrete);

        this.tokenTrackingController.hoveredMarker = null;
        delete this._editingController;
    }

    // CodeMirrorEditingController Delegate

    editingControllerDidStartEditing(editingController)
    {
        // We can pause the token tracking controller during editing, it will be reset
        // to the expected state by calling _updateEditableMarkers() in the
        // editingControllerDidFinishEditing delegate.
        this.tokenTrackingController.enabled = false;

        // We clear the marker since we'll reset it after editing.
        editingController.marker.clear();

        // We ignore content changes made as a result of color editing.
        this._ignoreContentDidChange++;
    }

    editingControllerDidFinishEditing(editingController)
    {
        this._updateEditableMarkers(editingController.range);

        this._ignoreContentDidChange--;

        delete this._editingController;
    }

    _setTypeTokenAnnotatorEnabledState(shouldActivate)
    {
        console.assert(this._typeTokenAnnotator);
        if (!this._typeTokenAnnotator)
            return;

        if (shouldActivate) {
            console.assert(this.visible, "Annotators should not be enabled if the TextEditor is not visible");

            RuntimeAgent.enableTypeProfiler();

            this._typeTokenAnnotator.reset();
            if (this._basicBlockAnnotator) {
                console.assert(!this._basicBlockAnnotator.isActive());
                this._basicBlockAnnotator.reset();
            }

            if (!this._typeTokenScrollHandler)
                this._enableScrollEventsForTypeTokenAnnotator();
        } else {
            // Because we disable type profiling when exiting the inspector, there is no need to call
            // RuntimeAgent.disableTypeProfiler() here.  If we were to call it here, JavaScriptCore would
            // compile out all the necessary type profiling information, so if a user were to quickly press then
            // unpress the type profiling button, we wouldn't be able to re-show type information which would
            // provide a confusing user experience.

            this._typeTokenAnnotator.clear();
            if (this._basicBlockAnnotator)
                this._basicBlockAnnotator.clear();

            if (this._typeTokenScrollHandler)
                this._disableScrollEventsForTypeTokenAnnotator();
        }

        WebInspector.showJavaScriptTypeInformationSetting.value = shouldActivate;

        this._updateTokenTrackingControllerState();
    }

    _getAssociatedScript()
    {
        var script = null;
        // FIXME: This needs to me modified to work with HTML files with inline script tags.
        if (this._sourceCode instanceof WebInspector.Script)
            script = this._sourceCode;
        else if (this._sourceCode instanceof WebInspector.Resource && this._sourceCode.type === WebInspector.Resource.Type.Script && this._sourceCode.scripts.length)
            script = this._sourceCode.scripts[0];
        return script;
    }

    _makeTypeTokenAnnotator()
    {
        // COMPATIBILITY (iOS 8): Runtime.getRuntimeTypesForVariablesAtOffsets did not exist yet.
        if (!RuntimeAgent.getRuntimeTypesForVariablesAtOffsets)
            return;

        var script = this._getAssociatedScript();
        if (!script)
            return;

        this._typeTokenAnnotator = new WebInspector.TypeTokenAnnotator(this, script);
    }

    _makeBasicBlockAnnotator()
    {
        // COMPATIBILITY (iOS 8): Runtime.getBasicBlocks did not exist yet.
        if (!RuntimeAgent.getBasicBlocks)
            return;

        var script = this._getAssociatedScript();
        if (!script)
            return;

        this._basicBlockAnnotator = new WebInspector.BasicBlockAnnotator(this, script);
    }

    _enableScrollEventsForTypeTokenAnnotator()
    {
        // Pause updating type tokens while scrolling to prevent frame loss.
        console.assert(!this._typeTokenScrollHandler);
        this._typeTokenScrollHandler = this._makeTypeTokenScrollEventHandler();
        this.addScrollHandler(this._typeTokenScrollHandler);
    }

    _disableScrollEventsForTypeTokenAnnotator()
    {
        console.assert(this._typeTokenScrollHandler);
        this.removeScrollHandler(this._typeTokenScrollHandler);
        this._typeTokenScrollHandler = null;
    }

    _makeTypeTokenScrollEventHandler()
    {
        var timeoutIdentifier = null;
        function scrollHandler()
        {
            if (timeoutIdentifier)
                clearTimeout(timeoutIdentifier);
            else {
                if (this._typeTokenAnnotator)
                    this._typeTokenAnnotator.pause();
                if (this._basicBlockAnnotator)
                    this._basicBlockAnnotator.pause();
            }

            timeoutIdentifier = setTimeout(function() {
                timeoutIdentifier = null;
                if (this._typeTokenAnnotator)
                    this._typeTokenAnnotator.resume();
                if (this._basicBlockAnnotator)
                    this._basicBlockAnnotator.resume();
            }.bind(this), WebInspector.SourceCodeTextEditor.DurationToUpdateTypeTokensAfterScrolling);
        }

        return scrollHandler.bind(this);
    }
};

WebInspector.SourceCodeTextEditor.LineErrorStyleClassName = "error";
WebInspector.SourceCodeTextEditor.LineWarningStyleClassName = "warning";
WebInspector.SourceCodeTextEditor.PopoverDebuggerContentStyleClassName = "debugger-popover-content";
WebInspector.SourceCodeTextEditor.HoveredExpressionHighlightStyleClassName = "hovered-expression-highlight";
WebInspector.SourceCodeTextEditor.DurationToMouseOverTokenToMakeHoveredToken = 500;
WebInspector.SourceCodeTextEditor.DurationToMouseOutOfHoveredTokenToRelease = 1000;
WebInspector.SourceCodeTextEditor.DurationToUpdateTypeTokensAfterScrolling = 100;
WebInspector.SourceCodeTextEditor.WidgetContainsMultipleIssuesSymbol = Symbol("source-code-widget-contains-multiple-issues");

WebInspector.SourceCodeTextEditor.Event = {
    ContentWillPopulate: "source-code-text-editor-content-will-populate",
    ContentDidPopulate: "source-code-text-editor-content-did-populate"
};
