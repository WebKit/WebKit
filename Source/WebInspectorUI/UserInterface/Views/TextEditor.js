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

WebInspector.TextEditor = class TextEditor extends WebInspector.View
{
    constructor(element, mimeType, delegate)
    {
        super(element);

        this.element.classList.add("text-editor", WebInspector.SyntaxHighlightedStyleClassName);

        this._codeMirror = WebInspector.CodeMirrorEditor.create(this.element, {
            readOnly: true,
            indentWithTabs: WebInspector.settings.indentWithTabs.value,
            indentUnit: WebInspector.settings.indentUnit.value,
            tabSize: WebInspector.settings.tabSize.value,
            lineNumbers: true,
            lineWrapping: WebInspector.settings.enableLineWrapping.value,
            matchBrackets: true,
            autoCloseBrackets: true,
            showWhitespaceCharacters: WebInspector.settings.showWhitespaceCharacters.value,
            styleSelectedText: true,
        });

        WebInspector.settings.indentWithTabs.addEventListener(WebInspector.Setting.Event.Changed, (event) => {
            this._codeMirror.setOption("indentWithTabs", WebInspector.settings.indentWithTabs.value);
        });

        WebInspector.settings.indentUnit.addEventListener(WebInspector.Setting.Event.Changed, (event) => {
            this._codeMirror.setOption("indentUnit", WebInspector.settings.indentUnit.value);
        });

        WebInspector.settings.tabSize.addEventListener(WebInspector.Setting.Event.Changed, (event) => {
            this._codeMirror.setOption("tabSize", WebInspector.settings.tabSize.value);
        });

        WebInspector.settings.enableLineWrapping.addEventListener(WebInspector.Setting.Event.Changed, (event) => {
            this._codeMirror.setOption("lineWrapping", WebInspector.settings.enableLineWrapping.value);
        });

        WebInspector.settings.showWhitespaceCharacters.addEventListener(WebInspector.Setting.Event.Changed, (event) => {
            this._codeMirror.setOption("showWhitespaceCharacters", WebInspector.settings.showWhitespaceCharacters.value);
        });

        this._codeMirror.on("change", this._contentChanged.bind(this));
        this._codeMirror.on("gutterClick", this._gutterMouseDown.bind(this));
        this._codeMirror.on("gutterContextMenu", this._gutterContextMenu.bind(this));
        this._codeMirror.getScrollerElement().addEventListener("click", this._openClickedLinks.bind(this), true);

        this._completionController = new WebInspector.CodeMirrorCompletionController(this._codeMirror, this);
        this._tokenTrackingController = new WebInspector.CodeMirrorTokenTrackingController(this._codeMirror, this);

        this._initialStringNotSet = true;

        this.mimeType = mimeType;

        this._breakpoints = {};
        this._executionLineNumber = NaN;
        this._executionColumnNumber = NaN;

        this._executionLineHandle = null;
        this._executionMultilineHandles = [];
        this._executionRangeHighlightMarker = null;

        this._searchQuery = null;
        this._searchResults = [];
        this._currentSearchResultIndex = -1;
        this._ignoreCodeMirrorContentDidChangeEvent = 0;

        this._formatted = false;
        this._formattingPromise = null;
        this._formatterSourceMap = null;
        this._deferReveal = false;

        this._delegate = delegate || null;
    }

    // Public

    get visible()
    {
        return this._visible;
    }

    get string()
    {
        return this._codeMirror.getValue();
    }

    set string(newString)
    {
        function update()
        {
            // Clear any styles that may have been set on the empty line before content loaded.
            if (this._initialStringNotSet)
                this._codeMirror.removeLineClass(0, "wrap");

            if (this._codeMirror.getValue() !== newString) {
                this._codeMirror.setValue(newString);
                console.assert(this.string.length === newString.length, "A lot of our code depends on precise text offsets, so the string should remain the same.");
            } else {
                // Ensure we at display content even if the value did not change. This often happens when auto formatting.
                this.layout();
            }

            if (this._initialStringNotSet) {
                this._codeMirror.clearHistory();
                this._codeMirror.markClean();
                this._initialStringNotSet = false;
            }

            // Update the execution line now that we might have content for that line.
            this._updateExecutionLine();
            this._updateExecutionRangeHighlight();

            // Set the breakpoint styles now that we might have content for those lines.
            for (var lineNumber in this._breakpoints)
                this._setBreakpointStylesOnLine(lineNumber);

            // Try revealing the pending line now that we might have content with enough lines.
            this._revealPendingPositionIfPossible();
        }

        this._ignoreCodeMirrorContentDidChangeEvent++;
        this._codeMirror.operation(update.bind(this));
        this._ignoreCodeMirrorContentDidChangeEvent--;
        console.assert(this._ignoreCodeMirrorContentDidChangeEvent >= 0);
    }

    get readOnly()
    {
        return this._codeMirror.getOption("readOnly") || false;
    }

    set readOnly(readOnly)
    {
        this._codeMirror.setOption("readOnly", readOnly);
    }

    get formatted()
    {
        return this._formatted;
    }

    get hasModified()
    {
        let historySize = this._codeMirror.historySize().undo;

        // Formatting code creates a history item.
        if (this._formatted)
            historySize--;

        return historySize > 0;
    }

    updateFormattedState(formatted)
    {
        return this._format(formatted).catch(handlePromiseException);
    }

    hasFormatter()
    {
        let mode = this._codeMirror.getMode().name;
        return mode === "javascript" || mode === "css";
    }

    canBeFormatted()
    {
        // Can be overriden by subclasses.
        return this.hasFormatter();
    }

    canShowTypeAnnotations()
    {
        return false;
    }

    canShowCoverageHints()
    {
        return false;
    }

    get selectedTextRange()
    {
        var start = this._codeMirror.getCursor(true);
        var end = this._codeMirror.getCursor(false);
        return this._textRangeFromCodeMirrorPosition(start, end);
    }

    set selectedTextRange(textRange)
    {
        var position = this._codeMirrorPositionFromTextRange(textRange);
        this._codeMirror.setSelection(position.start, position.end);
    }

    get mimeType()
    {
        return this._mimeType;
    }

    set mimeType(newMIMEType)
    {
        newMIMEType = parseMIMEType(newMIMEType).type;

        this._mimeType = newMIMEType;
        this._codeMirror.setOption("mode", {name: newMIMEType, globalVars: true});
    }

    get executionLineNumber()
    {
        return this._executionLineNumber;
    }

    get executionColumnNumber()
    {
        return this._executionColumnNumber;
    }

    get formatterSourceMap()
    {
        return this._formatterSourceMap;
    }

    get tokenTrackingController()
    {
        return this._tokenTrackingController;
    }

    get delegate()
    {
        return this._delegate;
    }

    set delegate(newDelegate)
    {
        this._delegate = newDelegate || null;
    }

    get numberOfSearchResults()
    {
        return this._searchResults.length;
    }

    get currentSearchQuery()
    {
        return this._searchQuery;
    }

    set automaticallyRevealFirstSearchResult(reveal)
    {
        this._automaticallyRevealFirstSearchResult = reveal;

        // If we haven't shown a search result yet, reveal one now.
        if (this._automaticallyRevealFirstSearchResult && this._searchResults.length > 0) {
            if (this._currentSearchResultIndex === -1)
                this._revealFirstSearchResultAfterCursor();
        }
    }

    set deferReveal(defer)
    {
        this._deferReveal = defer;
    }

    performSearch(query)
    {
        if (this._searchQuery === query)
            return;

        this.searchCleared();

        this._searchQuery = query;

        // Allow subclasses to handle the searching if they have a better way.
        // If we are formatted, just use CodeMirror's search.
        if (typeof this.customPerformSearch === "function" && !this.formatted) {
            if (this.customPerformSearch(query))
                return;
        }

        // Go down the slow patch for all other text content.
        var queryRegex = new RegExp(query.escapeForRegExp(), "gi");
        var searchCursor = this._codeMirror.getSearchCursor(queryRegex, {line: 0, ch: 0}, false);
        var boundBatchSearch = batchSearch.bind(this);
        var numberOfSearchResultsDidChangeTimeout = null;

        function reportNumberOfSearchResultsDidChange()
        {
            if (numberOfSearchResultsDidChangeTimeout) {
                clearTimeout(numberOfSearchResultsDidChangeTimeout);
                numberOfSearchResultsDidChangeTimeout = null;
            }

            this.dispatchEventToListeners(WebInspector.TextEditor.Event.NumberOfSearchResultsDidChange);
        }

        function batchSearch()
        {
            // Bail if the query changed since we started.
            if (this._searchQuery !== query)
                return;

            var newSearchResults = [];
            var foundResult = false;
            for (var i = 0; i < WebInspector.TextEditor.NumberOfFindsPerSearchBatch && (foundResult = searchCursor.findNext()); ++i) {
                var textRange = this._textRangeFromCodeMirrorPosition(searchCursor.from(), searchCursor.to());
                newSearchResults.push(textRange);
            }

            this.addSearchResults(newSearchResults);

            // Don't report immediately, coalesce updates so they come in no faster than half a second.
            if (!numberOfSearchResultsDidChangeTimeout)
                numberOfSearchResultsDidChangeTimeout = setTimeout(reportNumberOfSearchResultsDidChange.bind(this), 500);

            if (foundResult) {
                // More lines to search, set a timeout so we don't block the UI long.
                setTimeout(boundBatchSearch, 50);
            } else {
                // Report immediately now that we are finished, canceling any pending update.
                reportNumberOfSearchResultsDidChange.call(this);
            }
        }

        // Start the search.
        boundBatchSearch();
    }

    setExecutionLineAndColumn(lineNumber, columnNumber)
    {
        // Only return early if there isn't a line handle and that isn't changing.
        if (!this._executionLineHandle && isNaN(lineNumber))
            return;

        this._executionLineNumber = lineNumber;
        this._executionColumnNumber = columnNumber;

        if (!this._initialStringNotSet) {
            this._updateExecutionLine();
            this._updateExecutionRangeHighlight();
        }

        // Still dispatch the event even if the number didn't change. The execution state still
        // could have changed (e.g. continuing in a loop with a breakpoint inside).
        this.dispatchEventToListeners(WebInspector.TextEditor.Event.ExecutionLineNumberDidChange);
    }

    addSearchResults(textRanges)
    {
        console.assert(textRanges);
        if (!textRanges || !textRanges.length)
            return;

        function markRanges()
        {
            for (var i = 0; i < textRanges.length; ++i) {
                var position = this._codeMirrorPositionFromTextRange(textRanges[i]);
                var mark = this._codeMirror.markText(position.start, position.end, {className: WebInspector.TextEditor.SearchResultStyleClassName});
                this._searchResults.push(mark);
            }

            // If we haven't shown a search result yet, reveal one now.
            if (this._automaticallyRevealFirstSearchResult) {
                if (this._currentSearchResultIndex === -1)
                    this._revealFirstSearchResultAfterCursor();
            }
        }

        this._codeMirror.operation(markRanges.bind(this));
    }

    searchCleared()
    {
        function clearResults() {
            for (var i = 0; i < this._searchResults.length; ++i)
                this._searchResults[i].clear();
        }

        this._codeMirror.operation(clearResults.bind(this));

        this._searchQuery = null;
        this._searchResults = [];
        this._currentSearchResultIndex = -1;
    }

    searchQueryWithSelection()
    {
        if (!this._codeMirror.somethingSelected())
            return null;

        return this._codeMirror.getSelection();
    }

    revealPreviousSearchResult(changeFocus)
    {
        if (!this._searchResults.length)
            return;

        if (this._currentSearchResultIndex === -1 || this._cursorDoesNotMatchLastRevealedSearchResult()) {
            this._revealFirstSearchResultBeforeCursor(changeFocus);
            return;
        }

        if (this._currentSearchResultIndex > 0)
            --this._currentSearchResultIndex;
        else
            this._currentSearchResultIndex = this._searchResults.length - 1;

        this._revealSearchResult(this._searchResults[this._currentSearchResultIndex], changeFocus, -1);
    }

    revealNextSearchResult(changeFocus)
    {
        if (!this._searchResults.length)
            return;

        if (this._currentSearchResultIndex === -1 || this._cursorDoesNotMatchLastRevealedSearchResult()) {
            this._revealFirstSearchResultAfterCursor(changeFocus);
            return;
        }

        if (this._currentSearchResultIndex + 1 < this._searchResults.length)
            ++this._currentSearchResultIndex;
        else
            this._currentSearchResultIndex = 0;

        this._revealSearchResult(this._searchResults[this._currentSearchResultIndex], changeFocus, 1);
    }

    line(lineNumber)
    {
        return this._codeMirror.getLine(lineNumber);
    }

    getTextInRange(startPosition, endPosition)
    {
        return this._codeMirror.getRange(startPosition, endPosition);
    }

    addStyleToTextRange(startPosition, endPosition, styleClassName)
    {
        endPosition.ch += 1;
        return this._codeMirror.getDoc().markText(startPosition, endPosition, {className: styleClassName, inclusiveLeft: true, inclusiveRight: true});
    }

    revealPosition(position, textRangeToSelect, forceUnformatted, noHighlight)
    {
        console.assert(position === undefined || position instanceof WebInspector.SourceCodePosition, "revealPosition called without a SourceCodePosition");
        if (!(position instanceof WebInspector.SourceCodePosition))
            return;

        if (!this._visible || this._initialStringNotSet || this._deferReveal) {
            // If we can't get a line handle or are not visible then we wait to do the reveal.
            this._positionToReveal = position;
            this._textRangeToSelect = textRangeToSelect;
            this._forceUnformatted = forceUnformatted;
            return;
        }

        // Delete now that the reveal is happening.
        delete this._positionToReveal;
        delete this._textRangeToSelect;
        delete this._forceUnformatted;

        // If we need to unformat, reveal the line after a wait.
        // Otherwise the line highlight doesn't work properly.
        if (this._formatted && forceUnformatted) {
            this.updateFormattedState(false).then(() => {
                setTimeout(this.revealPosition.bind(this), 0, position, textRangeToSelect);
            });
            return;
        }

        let line = Number.constrain(position.lineNumber, 0, this._codeMirror.lineCount() - 1);
        let lineHandle = this._codeMirror.getLineHandle(line);

        if (!textRangeToSelect) {
            let column = Number.constrain(position.columnNumber, 0, this._codeMirror.getLine(line).length - 1);
            textRangeToSelect = new WebInspector.TextRange(line, column, line, column);
        }

        function removeStyleClass()
        {
            this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.HighlightedStyleClassName);
        }

        function revealAndHighlightLine()
        {
            // If the line is not visible, reveal it as the center line in the editor.
            var position = this._codeMirrorPositionFromTextRange(textRangeToSelect);
            if (!this._isPositionVisible(position.start))
                this._scrollIntoViewCentered(position.start);

            this.selectedTextRange = textRangeToSelect;

            if (noHighlight)
                return;

            // Avoid highlighting the execution line while debugging.
            if (WebInspector.debuggerManager.paused && (!this._executionLineNumber || line === this._executionLineNumber))
                return;

            this._codeMirror.addLineClass(lineHandle, "wrap", WebInspector.TextEditor.HighlightedStyleClassName);

            // Use a timeout instead of a animationEnd event listener because the line element might
            // be removed if the user scrolls during the animation. In that case animationEnd isn't
            // fired, and the line would highlight again the next time it scrolls into view.
            setTimeout(removeStyleClass.bind(this), WebInspector.TextEditor.HighlightAnimationDuration);
        }

        this._codeMirror.operation(revealAndHighlightLine.bind(this));
    }

    shown()
    {
        this._visible = true;

        // Refresh since our size might have changed.
        this._codeMirror.refresh();

        // Try revealing the pending line now that we are visible.
        // This needs to be done as a separate operation from the refresh
        // so that the scrollInfo coordinates are correct.
        this._revealPendingPositionIfPossible();
    }

    hidden()
    {
        this._visible = false;
    }

    close()
    {
        WebInspector.settings.indentWithTabs.removeEventListener(null, null, this);
        WebInspector.settings.indentUnit.removeEventListener(null, null, this);
        WebInspector.settings.tabSize.removeEventListener(null, null, this);
        WebInspector.settings.enableLineWrapping.removeEventListener(null, null, this);
        WebInspector.settings.showWhitespaceCharacters.removeEventListener(null, null, this);
    }

    setBreakpointInfoForLineAndColumn(lineNumber, columnNumber, breakpointInfo)
    {
        if (this._ignoreSetBreakpointInfoCalls)
            return;

        if (breakpointInfo)
            this._addBreakpointToLineAndColumnWithInfo(lineNumber, columnNumber, breakpointInfo);
        else
            this._removeBreakpointFromLineAndColumn(lineNumber, columnNumber);
    }

    updateBreakpointLineAndColumn(oldLineNumber, oldColumnNumber, newLineNumber, newColumnNumber)
    {
        console.assert(this._breakpoints[oldLineNumber]);
        if (!this._breakpoints[oldLineNumber])
            return;

        console.assert(this._breakpoints[oldLineNumber][oldColumnNumber]);
        if (!this._breakpoints[oldLineNumber][oldColumnNumber])
            return;

        var breakpointInfo = this._breakpoints[oldLineNumber][oldColumnNumber];
        this._removeBreakpointFromLineAndColumn(oldLineNumber, oldColumnNumber);
        this._addBreakpointToLineAndColumnWithInfo(newLineNumber, newColumnNumber, breakpointInfo);
    }

    addStyleClassToLine(lineNumber, styleClassName)
    {
        var lineHandle = this._codeMirror.getLineHandle(lineNumber);
        if (!lineHandle)
            return null;

        return this._codeMirror.addLineClass(lineHandle, "wrap", styleClassName);
    }

    removeStyleClassFromLine(lineNumber, styleClassName)
    {
        var lineHandle = this._codeMirror.getLineHandle(lineNumber);
        console.assert(lineHandle);
        if (!lineHandle)
            return null;

        return this._codeMirror.removeLineClass(lineHandle, "wrap", styleClassName);
    }

    toggleStyleClassForLine(lineNumber, styleClassName)
    {
        var lineHandle = this._codeMirror.getLineHandle(lineNumber);
        console.assert(lineHandle);
        if (!lineHandle)
            return false;

        return this._codeMirror.toggleLineClass(lineHandle, "wrap", styleClassName);
    }

    createWidgetForLine(lineNumber)
    {
        var lineHandle = this._codeMirror.getLineHandle(lineNumber);
        if (!lineHandle)
            return null;

        var widgetElement = document.createElement("div");
        var lineWidget = this._codeMirror.addLineWidget(lineHandle, widgetElement, {coverGutter: false, noHScroll: true});
        return new WebInspector.LineWidget(lineWidget, widgetElement);
    }

    get lineCount()
    {
        return this._codeMirror.lineCount();
    }

    focus()
    {
        this._codeMirror.focus();
    }

    contentDidChange(replacedRanges, newRanges)
    {
        // Implemented by subclasses.
    }

    rectsForRange(range)
    {
        return this._codeMirror.rectsForRange(range);
    }

    get markers()
    {
        return this._codeMirror.getAllMarks().map(WebInspector.TextMarker.textMarkerForCodeMirrorTextMarker);
    }

    markersAtPosition(position)
    {
        return this._codeMirror.findMarksAt(position).map(WebInspector.TextMarker.textMarkerForCodeMirrorTextMarker);
    }

    createColorMarkers(range)
    {
        return createCodeMirrorColorTextMarkers(this._codeMirror, range);
    }

    createGradientMarkers(range)
    {
        return createCodeMirrorGradientTextMarkers(this._codeMirror, range);
    }

    createCubicBezierMarkers(range)
    {
        return createCodeMirrorCubicBezierTextMarkers(this._codeMirror, range);
    }

    createSpringMarkers(range)
    {
        return createCodeMirrorSpringTextMarkers(this._codeMirror, range);
    }

    editingControllerForMarker(editableMarker)
    {
        switch (editableMarker.type) {
        case WebInspector.TextMarker.Type.Color:
            return new WebInspector.CodeMirrorColorEditingController(this._codeMirror, editableMarker);
        case WebInspector.TextMarker.Type.Gradient:
            return new WebInspector.CodeMirrorGradientEditingController(this._codeMirror, editableMarker);
        case WebInspector.TextMarker.Type.CubicBezier:
            return new WebInspector.CodeMirrorBezierEditingController(this._codeMirror, editableMarker);
        case WebInspector.TextMarker.Type.Spring:
            return new WebInspector.CodeMirrorSpringEditingController(this._codeMirror, editableMarker);
        default:
            return new WebInspector.CodeMirrorEditingController(this._codeMirror, editableMarker);
        }
    }

    visibleRangeOffsets()
    {
        var startOffset = null;
        var endOffset = null;
        var visibleRange = this._codeMirror.getViewport();

        if (this._formatterSourceMap) {
            startOffset = this._formatterSourceMap.formattedToOriginalOffset(Math.max(visibleRange.from - 1, 0), 0);
            endOffset = this._formatterSourceMap.formattedToOriginalOffset(visibleRange.to - 1, 0);
        } else {
            startOffset = this._codeMirror.getDoc().indexFromPos({line: visibleRange.from, ch: 0});
            endOffset = this._codeMirror.getDoc().indexFromPos({line: visibleRange.to, ch: 0});
        }

        return {startOffset, endOffset};
    }

    originalOffsetToCurrentPosition(offset)
    {
        var position = null;
        if (this._formatterSourceMap) {
            var location = this._formatterSourceMap.originalPositionToFormatted(offset);
            position = {line: location.lineNumber, ch: location.columnNumber};
        } else
            position = this._codeMirror.getDoc().posFromIndex(offset);

        return position;
    }

    currentOffsetToCurrentPosition(offset)
    {
        return this._codeMirror.getDoc().posFromIndex(offset);
    }

    currentPositionToOriginalOffset(position)
    {
        let offset = null;

        if (this._formatterSourceMap)
            offset = this._formatterSourceMap.formattedToOriginalOffset(position.line, position.ch);
        else
            offset = this._codeMirror.getDoc().indexFromPos(position);

        return offset;
    }

    currentPositionToOriginalPosition(position)
    {
        if (!this._formatterSourceMap)
            return position;

        let location = this._formatterSourceMap.formattedToOriginal(position.line, position.ch);
        return {line: location.lineNumber, ch: location.columnNumber};
    }

    currentPositionToCurrentOffset(position)
    {
        return this._codeMirror.getDoc().indexFromPos(position);
    }

    setInlineWidget(position, inlineElement)
    {
        return this._codeMirror.setUniqueBookmark(position, {widget: inlineElement});
    }

    addScrollHandler(handler)
    {
        this._codeMirror.on("scroll", handler);
    }

    removeScrollHandler(handler)
    {
        this._codeMirror.off("scroll", handler);
    }

    // Protected

    layout()
    {
        // FIXME: <https://webkit.org/b/146256> Web Inspector: Nested ContentBrowsers / ContentViewContainers cause too many ContentView updates
        // Ideally we would not get an updateLayout call if we are not visible. We should restructure ContentView
        // show/hide restoration to reduce duplicated work and solve this in the process.

        // FIXME: visible check can be removed once <https://webkit.org/b/150741> is fixed.
        if (this._visible)
            this._codeMirror.refresh();
    }

    _format(formatted)
    {
        if (this._formatted === formatted)
            return Promise.resolve(this._formatted);

        console.assert(!formatted || this.canBeFormatted());
        if (formatted && !this.canBeFormatted())
            return Promise.resolve(this._formatted);

        if (this._formattingPromise)
            return this._formattingPromise;

        this._ignoreCodeMirrorContentDidChangeEvent++;
        this._formattingPromise = this.prettyPrint(formatted).then(() => {
            this._ignoreCodeMirrorContentDidChangeEvent--;
            console.assert(this._ignoreCodeMirrorContentDidChangeEvent >= 0);

            this._formattingPromise = null;

            let originalFormatted = this._formatted;
            this._formatted = !!this._formatterSourceMap;

            if (this._formatted !== originalFormatted)
                this.dispatchEventToListeners(WebInspector.TextEditor.Event.FormattingDidChange);

            return this._formatted;
        });

        return this._formattingPromise;
    }

    prettyPrint(pretty)
    {
        return new Promise((resolve, reject) => {
            let beforePrettyPrintState = {
                selectionAnchor: this._codeMirror.getCursor("anchor"),
                selectionHead: this._codeMirror.getCursor("head"),
            };

            if (!pretty)
                this._undoFormatting(beforePrettyPrintState, resolve);
            else if (this._canUseFormatterWorker())
                this._startWorkerPrettyPrint(beforePrettyPrintState, resolve);
            else
                this._startCodeMirrorPrettyPrint(beforePrettyPrintState, resolve);
        });
    }

    _canUseFormatterWorker()
    {
        return this._codeMirror.getMode().name === "javascript";
    }

    _startWorkerPrettyPrint(beforePrettyPrintState, callback)
    {
        let sourceText = this._codeMirror.getValue();
        let indentString = WebInspector.indentString();
        const includeSourceMapData = true;

        let sourceType = this._delegate.textEditorScriptSourceType(this);
        const isModule = sourceType === WebInspector.Script.SourceType.Module;

        let workerProxy = WebInspector.FormatterWorkerProxy.singleton();
        workerProxy.formatJavaScript(sourceText, isModule, indentString, includeSourceMapData, ({formattedText, sourceMapData}) => {
            // Handle if formatting failed, which is possible for invalid programs.
            if (formattedText === null) {
                callback();
                return;
            }
            this._finishPrettyPrint(beforePrettyPrintState, formattedText, sourceMapData, callback);
        });
    }

    _startCodeMirrorPrettyPrint(beforePrettyPrintState, callback)
    {
        let indentString = WebInspector.indentString();
        let start = {line: 0, ch: 0};
        let end = {line: this._codeMirror.lineCount() - 1};
        let builder = new FormatterContentBuilder(indentString);
        let formatter = new WebInspector.Formatter(this._codeMirror, builder);
        formatter.format(start, end);

        let formattedText = builder.formattedContent;
        let sourceMapData = builder.sourceMapData;
        this._finishPrettyPrint(beforePrettyPrintState, formattedText, sourceMapData, callback);
    }

    _finishPrettyPrint(beforePrettyPrintState, formattedText, sourceMapData, callback)
    {
        this._codeMirror.operation(() => {
            this._formatterSourceMap = WebInspector.FormatterSourceMap.fromSourceMapData(sourceMapData);
            this._codeMirror.setValue(formattedText);
            this._updateAfterFormatting(true, beforePrettyPrintState);
        });

        callback();
    }

    _undoFormatting(beforePrettyPrintState, callback)
    {
        this._codeMirror.operation(() => {
            this._codeMirror.undo();
            this._updateAfterFormatting(false, beforePrettyPrintState);
        });

        callback();
    }

    _updateAfterFormatting(pretty, beforePrettyPrintState)
    {
        let oldSelectionAnchor = beforePrettyPrintState.selectionAnchor;
        let oldSelectionHead = beforePrettyPrintState.selectionHead;
        let newSelectionAnchor, newSelectionHead;
        let newExecutionLocation = null;

        if (pretty) {
            if (this._positionToReveal) {
                let newRevealPosition = this._formatterSourceMap.originalToFormatted(this._positionToReveal.lineNumber, this._positionToReveal.columnNumber);
                this._positionToReveal = new WebInspector.SourceCodePosition(newRevealPosition.lineNumber, newRevealPosition.columnNumber);
            }

            if (this._textRangeToSelect) {
                let mappedRevealSelectionStart = this._formatterSourceMap.originalToFormatted(this._textRangeToSelect.startLine, this._textRangeToSelect.startColumn);
                let mappedRevealSelectionEnd = this._formatterSourceMap.originalToFormatted(this._textRangeToSelect.endLine, this._textRangeToSelect.endColumn);
                this._textRangeToSelect = new WebInspector.TextRange(mappedRevealSelectionStart.lineNumber, mappedRevealSelectionStart.columnNumber, mappedRevealSelectionEnd.lineNumber, mappedRevealSelectionEnd.columnNumber);
            }

            if (!isNaN(this._executionLineNumber)) {
                console.assert(!isNaN(this._executionColumnNumber));
                newExecutionLocation = this._formatterSourceMap.originalToFormatted(this._executionLineNumber, this._executionColumnNumber);
            }

            let mappedAnchorLocation = this._formatterSourceMap.originalToFormatted(oldSelectionAnchor.line, oldSelectionAnchor.ch);
            let mappedHeadLocation = this._formatterSourceMap.originalToFormatted(oldSelectionHead.line, oldSelectionHead.ch);
            newSelectionAnchor = {line: mappedAnchorLocation.lineNumber, ch: mappedAnchorLocation.columnNumber};
            newSelectionHead = {line: mappedHeadLocation.lineNumber, ch: mappedHeadLocation.columnNumber};
        } else {
            if (this._positionToReveal) {
                let newRevealPosition = this._formatterSourceMap.formattedToOriginal(this._positionToReveal.lineNumber, this._positionToReveal.columnNumber);
                this._positionToReveal = new WebInspector.SourceCodePosition(newRevealPosition.lineNumber, newRevealPosition.columnNumber);
            }

            if (this._textRangeToSelect) {
                let mappedRevealSelectionStart = this._formatterSourceMap.formattedToOriginal(this._textRangeToSelect.startLine, this._textRangeToSelect.startColumn);
                let mappedRevealSelectionEnd = this._formatterSourceMap.formattedToOriginal(this._textRangeToSelect.endLine, this._textRangeToSelect.endColumn);
                this._textRangeToSelect = new WebInspector.TextRange(mappedRevealSelectionStart.lineNumber, mappedRevealSelectionStart.columnNumber, mappedRevealSelectionEnd.lineNumber, mappedRevealSelectionEnd.columnNumber);
            }

            if (!isNaN(this._executionLineNumber)) {
                console.assert(!isNaN(this._executionColumnNumber));
                newExecutionLocation = this._formatterSourceMap.formattedToOriginal(this._executionLineNumber, this._executionColumnNumber);
            }

            let mappedAnchorLocation = this._formatterSourceMap.formattedToOriginal(oldSelectionAnchor.line, oldSelectionAnchor.ch);
            let mappedHeadLocation = this._formatterSourceMap.formattedToOriginal(oldSelectionHead.line, oldSelectionHead.ch);
            newSelectionAnchor = {line: mappedAnchorLocation.lineNumber, ch: mappedAnchorLocation.columnNumber};
            newSelectionHead = {line: mappedHeadLocation.lineNumber, ch: mappedHeadLocation.columnNumber};

            this._formatterSourceMap = null;
        }

        this._scrollIntoViewCentered(newSelectionAnchor);
        this._codeMirror.setSelection(newSelectionAnchor, newSelectionHead);

        if (newExecutionLocation) {
            this._executionLineHandle = null;
            this._executionMultilineHandles = [];
            this.setExecutionLineAndColumn(newExecutionLocation.lineNumber, newExecutionLocation.columnNumber);
        }

        // FIXME: <rdar://problem/13129955> FindBanner: New searches should not lose search position (start from current selection/caret)
        if (this.currentSearchQuery) {
            let searchQuery = this.currentSearchQuery;
            this.searchCleared();
            // Set timeout so that this happens after the current CodeMirror operation.
            // The editor has to update for the value and selection changes.
            setTimeout(() => { this.performSearch(searchQuery); }, 0);
        }

        if (this._delegate && typeof this._delegate.textEditorUpdatedFormatting === "function")
            this._delegate.textEditorUpdatedFormatting(this);
    }

    // Private

    hasEdits()
    {
        return !this._codeMirror.isClean();
    }

    _contentChanged(codeMirror, change)
    {
        if (this._ignoreCodeMirrorContentDidChangeEvent > 0)
            return;

        var replacedRanges = [];
        var newRanges = [];
        while (change) {
            replacedRanges.push(new WebInspector.TextRange(
                change.from.line,
                change.from.ch,
                change.to.line,
                change.to.ch
            ));
            newRanges.push(new WebInspector.TextRange(
                change.from.line,
                change.from.ch,
                change.from.line + change.text.length - 1,
                change.text.length === 1 ? change.from.ch + change.text[0].length : change.text.lastValue.length
            ));
            change = change.next;
        }
        this.contentDidChange(replacedRanges, newRanges);

        if (this._formatted) {
            this._formatterSourceMap = null;
            this._formatted = false;

            if (this._delegate && typeof this._delegate.textEditorUpdatedFormatting === "function")
                this._delegate.textEditorUpdatedFormatting(this);

            this.dispatchEventToListeners(WebInspector.TextEditor.Event.FormattingDidChange);
        }

        this.dispatchEventToListeners(WebInspector.TextEditor.Event.ContentDidChange);
    }

    _textRangeFromCodeMirrorPosition(start, end)
    {
        console.assert(start);
        console.assert(end);

        return new WebInspector.TextRange(start.line, start.ch, end.line, end.ch);
    }

    _codeMirrorPositionFromTextRange(textRange)
    {
        console.assert(textRange);

        var start = {line: textRange.startLine, ch: textRange.startColumn};
        var end = {line: textRange.endLine, ch: textRange.endColumn};
        return {start, end};
    }

    _revealPendingPositionIfPossible()
    {
        // Nothing to do if we don't have a pending position.
        if (!this._positionToReveal)
            return;

        // Don't try to reveal unless we are visible.
        if (!this._visible)
            return;

        this.revealPosition(this._positionToReveal, this._textRangeToSelect, this._forceUnformatted);
    }

    _revealSearchResult(result, changeFocus, directionInCaseOfRevalidation)
    {
        var position = result.find();

        // Check for a valid position, it might have been removed from editing by the user.
        // If the position is invalide, revalidate all positions reveal as needed.
        if (!position) {
            this._revalidateSearchResults(directionInCaseOfRevalidation);
            return;
        }

        // If the line is not visible, reveal it as the center line in the editor.
        if (!this._isPositionVisible(position.from))
            this._scrollIntoViewCentered(position.from);

        // Update the text selection to select the search result.
        this.selectedTextRange = this._textRangeFromCodeMirrorPosition(position.from, position.to);

        // Remove the automatically reveal state now that we have revealed a search result.
        this._automaticallyRevealFirstSearchResult = false;

        // Focus the editor if requested.
        if (changeFocus)
            this._codeMirror.focus();

        // Remove the bouncy highlight if it is still around. The animation will not
        // start unless we remove it and add it back to the document.
        if (this._bouncyHighlightElement)
            this._bouncyHighlightElement.remove();

        // Create the bouncy highlight.
        this._bouncyHighlightElement = document.createElement("div");
        this._bouncyHighlightElement.className = WebInspector.TextEditor.BouncyHighlightStyleClassName;

        // Collect info for the bouncy highlight.
        var textContent = this._codeMirror.getSelection();
        var coordinates = this._codeMirror.cursorCoords(true, "page");

        // Adjust the coordinates to be based in the text editor's space.
        let textEditorRect = this.element.getBoundingClientRect();
        coordinates.top -= textEditorRect.top;
        coordinates.left -= textEditorRect.left;

        // Position and show the bouncy highlight.
        this._bouncyHighlightElement.textContent = textContent;
        this._bouncyHighlightElement.style.top = coordinates.top + "px";
        this._bouncyHighlightElement.style.left = coordinates.left + "px";
        this.element.appendChild(this._bouncyHighlightElement);

        let scrollHandler = () => {
            if (this._bouncyHighlightElement)
                this._bouncyHighlightElement.remove();
        };

        this.addScrollHandler(scrollHandler);

        function animationEnded()
        {
            if (!this._bouncyHighlightElement)
                return;

            this._bouncyHighlightElement.remove();
            delete this._bouncyHighlightElement;

            this.removeScrollHandler(scrollHandler);
        }

        // Listen for the end of the animation so we can remove the element.
        this._bouncyHighlightElement.addEventListener("animationend", animationEnded.bind(this));
    }

    _binarySearchInsertionIndexInSearchResults(object, comparator)
    {
        // It is possible that markers in the search results array may have been deleted.
        // In those cases the comparator will return "null" and we immediately stop
        // the binary search and return null. The search results list needs to be updated.
        var array = this._searchResults;

        var first = 0;
        var last = array.length - 1;

        while (first <= last) {
            var mid = (first + last) >> 1;
            var c = comparator(object, array[mid]);
            if (c === null)
                return null;
            if (c > 0)
                first = mid + 1;
            else if (c < 0)
                last = mid - 1;
            else
                return mid;
        }

        return first - 1;
    }

    _revealFirstSearchResultBeforeCursor(changeFocus)
    {
        console.assert(this._searchResults.length);

        var currentCursorPosition = this._codeMirror.getCursor("start");
        if (currentCursorPosition.line === 0 && currentCursorPosition.ch === 0) {
            this._currentSearchResultIndex = this._searchResults.length - 1;
            this._revealSearchResult(this._searchResults[this._currentSearchResultIndex], changeFocus, -1);
            return;
        }

        var index = this._binarySearchInsertionIndexInSearchResults(currentCursorPosition, function(current, searchResult) {
            var searchResultMarker = searchResult.find();
            if (!searchResultMarker)
                return null;
            return WebInspector.compareCodeMirrorPositions(current, searchResultMarker.from);
        });

        if (index === null) {
            this._revalidateSearchResults(-1);
            return;
        }

        this._currentSearchResultIndex = index;
        this._revealSearchResult(this._searchResults[this._currentSearchResultIndex], changeFocus);
    }

    _revealFirstSearchResultAfterCursor(changeFocus)
    {
        console.assert(this._searchResults.length);

        var currentCursorPosition = this._codeMirror.getCursor("start");
        if (currentCursorPosition.line === 0 && currentCursorPosition.ch === 0) {
            this._currentSearchResultIndex = 0;
            this._revealSearchResult(this._searchResults[this._currentSearchResultIndex], changeFocus, 1);
            return;
        }

        var index = this._binarySearchInsertionIndexInSearchResults(currentCursorPosition, function(current, searchResult) {
            var searchResultMarker = searchResult.find();
            if (!searchResultMarker)
                return null;
            return WebInspector.compareCodeMirrorPositions(current, searchResultMarker.from);
        });

        if (index === null) {
            this._revalidateSearchResults(1);
            return;
        }

        if (index + 1 < this._searchResults.length)
            ++index;
        else
            index = 0;

        this._currentSearchResultIndex = index;
        this._revealSearchResult(this._searchResults[this._currentSearchResultIndex], changeFocus);
    }

    _cursorDoesNotMatchLastRevealedSearchResult()
    {
        console.assert(this._currentSearchResultIndex !== -1);
        console.assert(this._searchResults.length);

        var lastRevealedSearchResultMarker = this._searchResults[this._currentSearchResultIndex].find();
        if (!lastRevealedSearchResultMarker)
            return true;

        var currentCursorPosition = this._codeMirror.getCursor("start");
        var lastRevealedSearchResultPosition = lastRevealedSearchResultMarker.from;

        return WebInspector.compareCodeMirrorPositions(currentCursorPosition, lastRevealedSearchResultPosition) !== 0;
    }

    _revalidateSearchResults(direction)
    {
        console.assert(direction !== undefined);

        this._currentSearchResultIndex = -1;

        var updatedSearchResults = [];
        for (var i = 0; i < this._searchResults.length; ++i) {
            if (this._searchResults[i].find())
                updatedSearchResults.push(this._searchResults[i]);
        }

        console.assert(updatedSearchResults.length !== this._searchResults.length);

        this._searchResults = updatedSearchResults;
        this.dispatchEventToListeners(WebInspector.TextEditor.Event.NumberOfSearchResultsDidChange);

        if (this._searchResults.length) {
            if (direction > 0)
                this._revealFirstSearchResultAfterCursor();
            else
                this._revealFirstSearchResultBeforeCursor();
        }
    }

    _clearMultilineExecutionLineHighlights()
    {
        if (this._executionMultilineHandles.length) {
            for (let lineHandle of this._executionMultilineHandles)
                this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.ExecutionLineStyleClassName);
            this._executionMultilineHandles = [];
        }
    }

    _updateExecutionLine()
    {
        this._codeMirror.operation(() => {
            if (this._executionLineHandle) {
                this._codeMirror.removeLineClass(this._executionLineHandle, "wrap", WebInspector.TextEditor.ExecutionLineStyleClassName);
                this._codeMirror.removeLineClass(this._executionLineHandle, "wrap", "primary");
            }

            this._clearMultilineExecutionLineHighlights();

            this._executionLineHandle = !isNaN(this._executionLineNumber) ? this._codeMirror.getLineHandle(this._executionLineNumber) : null;

            if (this._executionLineHandle) {
                this._codeMirror.addLineClass(this._executionLineHandle, "wrap", WebInspector.TextEditor.ExecutionLineStyleClassName);
                this._codeMirror.addLineClass(this._executionLineHandle, "wrap", "primary");
                this._codeMirror.removeLineClass(this._executionLineHandle, "wrap", WebInspector.TextEditor.HighlightedStyleClassName);
            }
        });
    }

    _updateExecutionRangeHighlight()
    {
        if (this._executionRangeHighlightMarker) {
            this._executionRangeHighlightMarker.clear();
            this._executionRangeHighlightMarker = null;
        }

        if (isNaN(this._executionLineNumber))
            return;

        let currentPosition = {line: this._executionLineNumber, ch: this._executionColumnNumber};
        let originalOffset = this.currentPositionToOriginalOffset(currentPosition);
        let originalCodeMirrorPosition = this.currentPositionToOriginalPosition(currentPosition);
        let originalPosition = new WebInspector.SourceCodePosition(originalCodeMirrorPosition.line, originalCodeMirrorPosition.ch);
        let characterAtOffset = this._codeMirror.getRange(currentPosition, {line: this._executionLineNumber, ch: this._executionColumnNumber + 1});

        this._delegate.textEditorExecutionHighlightRange(originalOffset, originalPosition, characterAtOffset, (range) => {
            let start, end;
            if (!range) {
                // Highlight the rest of the line.
                start = {line: this._executionLineNumber, ch: this._executionColumnNumber};
                end = {line: this._executionLineNumber};
            } else {
                // Highlight the range.
                start = this.originalOffsetToCurrentPosition(range[0]);
                end = this.originalOffsetToCurrentPosition(range[1]);
            }

            // Ensure the marker is cleared in case there were multiple updates very quickly.
            if (this._executionRangeHighlightMarker) {
                this._executionRangeHighlightMarker.clear();
                this._executionRangeHighlightMarker = null;
            }

            // Avoid highlighting trailing whitespace.
            let text = this._codeMirror.getRange(start, end);
            let trailingWhitespace = text.match(/\s+$/);
            if (trailingWhitespace)
                end.ch = Math.max(0, end.ch - trailingWhitespace[0].length);

            // Give each line containing part of the range the full line style.
            this._clearMultilineExecutionLineHighlights();
            if (start.line !== end.line) {
                for (let line = start.line; line < end.line; ++line) {
                    let lineHandle = this._codeMirror.getLineHandle(line);
                    this._codeMirror.addLineClass(lineHandle, "wrap", WebInspector.TextEditor.ExecutionLineStyleClassName);
                    this._executionMultilineHandles.push(lineHandle);
                }
            }

            this._executionRangeHighlightMarker = this._codeMirror.markText(start, end, {className: "execution-range-highlight"});
        });
    }

    _setBreakpointStylesOnLine(lineNumber)
    {
        var columnBreakpoints = this._breakpoints[lineNumber];
        console.assert(columnBreakpoints);
        if (!columnBreakpoints)
            return;

        var allDisabled = true;
        var allResolved = true;
        var allAutoContinue = true;
        var multiple = Object.keys(columnBreakpoints).length > 1;
        for (var columnNumber in columnBreakpoints) {
            var breakpointInfo = columnBreakpoints[columnNumber];
            if (!breakpointInfo.disabled)
                allDisabled = false;
            if (!breakpointInfo.resolved)
                allResolved = false;
            if (!breakpointInfo.autoContinue)
                allAutoContinue = false;
        }

        allResolved = allResolved && WebInspector.debuggerManager.breakpointsEnabled;

        function updateStyles()
        {
            // We might not have a line if the content isn't fully populated yet.
            // This will be called again when the content is available.
            var lineHandle = this._codeMirror.getLineHandle(lineNumber);
            if (!lineHandle)
                return;

            this._codeMirror.addLineClass(lineHandle, "wrap", WebInspector.TextEditor.HasBreakpointStyleClassName);

            if (allResolved)
                this._codeMirror.addLineClass(lineHandle, "wrap", WebInspector.TextEditor.BreakpointResolvedStyleClassName);
            else
                this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.BreakpointResolvedStyleClassName);

            if (allDisabled)
                this._codeMirror.addLineClass(lineHandle, "wrap", WebInspector.TextEditor.BreakpointDisabledStyleClassName);
            else
                this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.BreakpointDisabledStyleClassName);

            if (allAutoContinue)
                this._codeMirror.addLineClass(lineHandle, "wrap", WebInspector.TextEditor.BreakpointAutoContinueStyleClassName);
            else
                this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.BreakpointAutoContinueStyleClassName);

            if (multiple)
                this._codeMirror.addLineClass(lineHandle, "wrap", WebInspector.TextEditor.MultipleBreakpointsStyleClassName);
            else
                this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.MultipleBreakpointsStyleClassName);
        }

        this._codeMirror.operation(updateStyles.bind(this));
    }

    _addBreakpointToLineAndColumnWithInfo(lineNumber, columnNumber, breakpointInfo)
    {
        if (!this._breakpoints[lineNumber])
            this._breakpoints[lineNumber] = {};
        this._breakpoints[lineNumber][columnNumber] = breakpointInfo;

        this._setBreakpointStylesOnLine(lineNumber);
    }

    _removeBreakpointFromLineAndColumn(lineNumber, columnNumber)
    {
        console.assert(columnNumber in this._breakpoints[lineNumber]);
        delete this._breakpoints[lineNumber][columnNumber];

        // There are still breakpoints on the line. Update the breakpoint style.
        if (!isEmptyObject(this._breakpoints[lineNumber])) {
            this._setBreakpointStylesOnLine(lineNumber);
            return;
        }

        delete this._breakpoints[lineNumber];

        function updateStyles()
        {
            var lineHandle = this._codeMirror.getLineHandle(lineNumber);
            if (!lineHandle)
                return;

            this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.HasBreakpointStyleClassName);
            this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.BreakpointResolvedStyleClassName);
            this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.BreakpointDisabledStyleClassName);
            this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.BreakpointAutoContinueStyleClassName);
            this._codeMirror.removeLineClass(lineHandle, "wrap", WebInspector.TextEditor.MultipleBreakpointsStyleClassName);
        }

        this._codeMirror.operation(updateStyles.bind(this));
    }

    _allColumnBreakpointInfoForLine(lineNumber)
    {
        return this._breakpoints[lineNumber];
    }

    _setColumnBreakpointInfoForLine(lineNumber, columnBreakpointInfo)
    {
        console.assert(columnBreakpointInfo);
        this._breakpoints[lineNumber] = columnBreakpointInfo;
        this._setBreakpointStylesOnLine(lineNumber);
    }

    _gutterMouseDown(codeMirror, lineNumber, gutterElement, event)
    {
        if (event.button !== 0 || event.ctrlKey)
            return;

        if (!this._codeMirror.hasLineClass(lineNumber, "wrap", WebInspector.TextEditor.HasBreakpointStyleClassName)) {
            console.assert(!(lineNumber in this._breakpoints));

            // No breakpoint, add a new one.
            if (this._delegate && typeof this._delegate.textEditorBreakpointAdded === "function") {
                var data = this._delegate.textEditorBreakpointAdded(this, lineNumber, 0);
                if (data) {
                    var breakpointInfo = data.breakpointInfo;
                    if (breakpointInfo)
                        this._addBreakpointToLineAndColumnWithInfo(data.lineNumber, data.columnNumber, breakpointInfo);
                }
            }

            return;
        }

        console.assert(lineNumber in this._breakpoints);

        if (this._codeMirror.hasLineClass(lineNumber, "wrap", WebInspector.TextEditor.MultipleBreakpointsStyleClassName)) {
            console.assert(!isEmptyObject(this._breakpoints[lineNumber]));
            return;
        }

        // Single existing breakpoint, start tracking it for dragging.
        console.assert(Object.keys(this._breakpoints[lineNumber]).length === 1);
        var columnNumber = Object.keys(this._breakpoints[lineNumber])[0];
        this._draggingBreakpointInfo = this._breakpoints[lineNumber][columnNumber];
        this._lineNumberWithMousedDownBreakpoint = lineNumber;
        this._lineNumberWithDraggedBreakpoint = lineNumber;
        this._columnNumberWithMousedDownBreakpoint = columnNumber;
        this._columnNumberWithDraggedBreakpoint = columnNumber;

        this._documentMouseMovedEventListener = this._documentMouseMoved.bind(this);
        this._documentMouseUpEventListener = this._documentMouseUp.bind(this);

        // Register these listeners on the document so we can track the mouse if it leaves the gutter.
        document.addEventListener("mousemove", this._documentMouseMovedEventListener, true);
        document.addEventListener("mouseup", this._documentMouseUpEventListener, true);
    }

    _gutterContextMenu(codeMirror, lineNumber, gutterElement, event)
    {
        if (this._delegate && typeof this._delegate.textEditorGutterContextMenu === "function") {
            var breakpoints = [];
            for (var columnNumber in this._breakpoints[lineNumber])
                breakpoints.push({lineNumber, columnNumber});

            this._delegate.textEditorGutterContextMenu(this, lineNumber, 0, breakpoints, event);
        }
    }

    _documentMouseMoved(event)
    {
        console.assert("_lineNumberWithMousedDownBreakpoint" in this);
        if (!("_lineNumberWithMousedDownBreakpoint" in this))
            return;

        event.preventDefault();

        var lineNumber;
        var position = this._codeMirror.coordsChar({left: event.pageX, top: event.pageY});

        // CodeMirror's coordsChar returns a position even if it is outside the bounds. Nullify the position
        // if the event is outside the bounds of the gutter so we will remove the breakpoint.
        var gutterBounds = this._codeMirror.getGutterElement().getBoundingClientRect();
        if (event.pageX < gutterBounds.left || event.pageX > gutterBounds.right || event.pageY < gutterBounds.top || event.pageY > gutterBounds.bottom)
            position = null;

        // If we have a position and it has a line then use it.
        if (position && "line" in position)
            lineNumber = position.line;

        // The _lineNumberWithDraggedBreakpoint property can be undefined if the user drags
        // outside of the gutter. The lineNumber variable can be undefined for the same reason.

        if (lineNumber === this._lineNumberWithDraggedBreakpoint)
            return;

        // Record that the mouse dragged some so when mouse up fires we know to do the
        // work of removing and moving the breakpoint.
        this._mouseDragged = true;

        if ("_lineNumberWithDraggedBreakpoint" in this) {
            // We have a line that is currently showing the dragged breakpoint. Remove that breakpoint
            // and restore the previous one (if any.)
            if (this._previousColumnBreakpointInfo)
                this._setColumnBreakpointInfoForLine(this._lineNumberWithDraggedBreakpoint, this._previousColumnBreakpointInfo);
            else
                this._removeBreakpointFromLineAndColumn(this._lineNumberWithDraggedBreakpoint, this._columnNumberWithDraggedBreakpoint);

            delete this._previousColumnBreakpointInfo;
            delete this._lineNumberWithDraggedBreakpoint;
            delete this._columnNumberWithDraggedBreakpoint;
        }

        if (lineNumber !== undefined) {
            // We have a new line that will now show the dragged breakpoint.
            var newColumnBreakpoints = {};
            var columnNumber = (lineNumber === this._lineNumberWithMousedDownBreakpoint ? this._columnNumberWithDraggedBreakpoint : 0);
            newColumnBreakpoints[columnNumber] = this._draggingBreakpointInfo;
            this._previousColumnBreakpointInfo = this._allColumnBreakpointInfoForLine(lineNumber);
            this._setColumnBreakpointInfoForLine(lineNumber, newColumnBreakpoints);
            this._lineNumberWithDraggedBreakpoint = lineNumber;
            this._columnNumberWithDraggedBreakpoint = columnNumber;
        }
    }

    _documentMouseUp(event)
    {
        console.assert("_lineNumberWithMousedDownBreakpoint" in this);
        if (!("_lineNumberWithMousedDownBreakpoint" in this))
            return;

        event.preventDefault();

        document.removeEventListener("mousemove", this._documentMouseMovedEventListener, true);
        document.removeEventListener("mouseup", this._documentMouseUpEventListener, true);

        var delegateImplementsBreakpointClicked = this._delegate && typeof this._delegate.textEditorBreakpointClicked === "function";
        var delegateImplementsBreakpointRemoved = this._delegate && typeof this._delegate.textEditorBreakpointRemoved === "function";
        var delegateImplementsBreakpointMoved = this._delegate && typeof this._delegate.textEditorBreakpointMoved === "function";

        if (this._mouseDragged) {
            if (!("_lineNumberWithDraggedBreakpoint" in this)) {
                // The breakpoint was dragged off the gutter, remove it.
                if (delegateImplementsBreakpointRemoved) {
                    this._ignoreSetBreakpointInfoCalls = true;
                    this._delegate.textEditorBreakpointRemoved(this, this._lineNumberWithMousedDownBreakpoint, this._columnNumberWithMousedDownBreakpoint);
                    delete this._ignoreSetBreakpointInfoCalls;
                }
            } else if (this._lineNumberWithMousedDownBreakpoint !== this._lineNumberWithDraggedBreakpoint) {
                // The dragged breakpoint was moved to a new line.

                // If there is are breakpoints already at the drop line, tell the delegate to remove them.
                // We have already updated the breakpoint info internally, so when the delegate removes the breakpoints
                // and tells us to clear the breakpoint info, we can ignore those calls.
                if (this._previousColumnBreakpointInfo && delegateImplementsBreakpointRemoved) {
                    this._ignoreSetBreakpointInfoCalls = true;
                    for (var columnNumber in this._previousColumnBreakpointInfo)
                        this._delegate.textEditorBreakpointRemoved(this, this._lineNumberWithDraggedBreakpoint, columnNumber);
                    delete this._ignoreSetBreakpointInfoCalls;
                }

                // Tell the delegate to move the breakpoint from one line to another.
                if (delegateImplementsBreakpointMoved) {
                    this._ignoreSetBreakpointInfoCalls = true;
                    this._delegate.textEditorBreakpointMoved(this, this._lineNumberWithMousedDownBreakpoint, this._columnNumberWithMousedDownBreakpoint, this._lineNumberWithDraggedBreakpoint, this._columnNumberWithDraggedBreakpoint);
                    delete this._ignoreSetBreakpointInfoCalls;
                }
            }
        } else {
            // Toggle the disabled state of the breakpoint.
            console.assert(this._lineNumberWithMousedDownBreakpoint in this._breakpoints);
            console.assert(this._columnNumberWithMousedDownBreakpoint in this._breakpoints[this._lineNumberWithMousedDownBreakpoint]);
            if (this._lineNumberWithMousedDownBreakpoint in this._breakpoints && this._columnNumberWithMousedDownBreakpoint in this._breakpoints[this._lineNumberWithMousedDownBreakpoint] && delegateImplementsBreakpointClicked)
                this._delegate.textEditorBreakpointClicked(this, this._lineNumberWithMousedDownBreakpoint, this._columnNumberWithMousedDownBreakpoint);
        }

        delete this._documentMouseMovedEventListener;
        delete this._documentMouseUpEventListener;
        delete this._lineNumberWithMousedDownBreakpoint;
        delete this._lineNumberWithDraggedBreakpoint;
        delete this._columnNumberWithMousedDownBreakpoint;
        delete this._columnNumberWithDraggedBreakpoint;
        delete this._previousColumnBreakpointInfo;
        delete this._mouseDragged;
    }

    _openClickedLinks(event)
    {
        // Get the position in the text and the token at that position.
        var position = this._codeMirror.coordsChar({left: event.pageX, top: event.pageY});
        var tokenInfo = this._codeMirror.getTokenAt(position);
        if (!tokenInfo || !tokenInfo.type || !tokenInfo.string)
            return;

        // If the token is not a link, then ignore it.
        if (!/\blink\b/.test(tokenInfo.type))
            return;

        // The token string is the URL we should open. It might be a relative URL.
        var url = tokenInfo.string;

        // Get the base URL.
        var baseURL = "";
        if (this._delegate && typeof this._delegate.textEditorBaseURL === "function")
            baseURL = this._delegate.textEditorBaseURL(this);

        // Open the link after resolving the absolute URL from the base URL.
        WebInspector.openURL(absoluteURL(url, baseURL));

        // Stop processing the event.
        event.preventDefault();
        event.stopPropagation();
    }

    _isPositionVisible(position)
    {
        var scrollInfo = this._codeMirror.getScrollInfo();
        var visibleRangeStart = scrollInfo.top;
        var visibleRangeEnd = visibleRangeStart + scrollInfo.clientHeight;
        var coords = this._codeMirror.charCoords(position, "local");

        return coords.top >= visibleRangeStart && coords.bottom <= visibleRangeEnd;
    }

    _scrollIntoViewCentered(position)
    {
        var scrollInfo = this._codeMirror.getScrollInfo();
        var lineHeight = Math.ceil(this._codeMirror.defaultTextHeight());
        var margin = Math.floor((scrollInfo.clientHeight - lineHeight) / 2);
        this._codeMirror.scrollIntoView(position, margin);
    }
};

WebInspector.TextEditor.HighlightedStyleClassName = "highlighted";
WebInspector.TextEditor.SearchResultStyleClassName = "search-result";
WebInspector.TextEditor.HasBreakpointStyleClassName = "has-breakpoint";
WebInspector.TextEditor.BreakpointResolvedStyleClassName = "breakpoint-resolved";
WebInspector.TextEditor.BreakpointAutoContinueStyleClassName = "breakpoint-auto-continue";
WebInspector.TextEditor.BreakpointDisabledStyleClassName = "breakpoint-disabled";
WebInspector.TextEditor.MultipleBreakpointsStyleClassName = "multiple-breakpoints";
WebInspector.TextEditor.ExecutionLineStyleClassName = "execution-line";
WebInspector.TextEditor.BouncyHighlightStyleClassName = "bouncy-highlight";
WebInspector.TextEditor.NumberOfFindsPerSearchBatch = 10;
WebInspector.TextEditor.HighlightAnimationDuration = 2000;

WebInspector.TextEditor.Event = {
    ExecutionLineNumberDidChange: "text-editor-execution-line-number-did-change",
    NumberOfSearchResultsDidChange: "text-editor-number-of-search-results-did-change",
    ContentDidChange: "text-editor-content-did-change",
    FormattingDidChange: "text-editor-formatting-did-change"
};
