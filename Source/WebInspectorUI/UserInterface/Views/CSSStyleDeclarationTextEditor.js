/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Tobias Reiss <tobi+webkit@basecode.de>
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

WebInspector.CSSStyleDeclarationTextEditor = class CSSStyleDeclarationTextEditor extends WebInspector.View
{
    constructor(delegate, style)
    {
        super();

        this.element.classList.add(WebInspector.CSSStyleDeclarationTextEditor.StyleClassName);
        this.element.classList.add(WebInspector.SyntaxHighlightedStyleClassName);
        this.element.addEventListener("mousedown", this._handleMouseDown.bind(this));
        this.element.addEventListener("mouseup", this._handleMouseUp.bind(this));

        this._mouseDownCursorPosition = null;

        this._showsImplicitProperties = true;
        this._alwaysShowPropertyNames = {};
        this._filterResultPropertyNames = null;
        this._sortProperties = false;

        this._linePrefixWhitespace = "";

        this._delegate = delegate || null;

        this._codeMirror = WebInspector.CodeMirrorEditor.create(this.element, {
            readOnly: true,
            lineWrapping: true,
            mode: "css-rule",
            electricChars: false,
            indentWithTabs: false,
            indentUnit: 4,
            smartIndent: false,
            matchBrackets: true,
            autoCloseBrackets: true
        });

        this._codeMirror.addKeyMap({
            "Enter": this._handleEnterKey.bind(this),
            "Shift-Enter": this._insertNewlineAfterCurrentLine.bind(this),
            "Shift-Tab": this._handleShiftTabKey.bind(this),
            "Tab": this._handleTabKey.bind(this)
        });

        this._completionController = new WebInspector.CodeMirrorCompletionController(this._codeMirror, this);
        this._tokenTrackingController = new WebInspector.CodeMirrorTokenTrackingController(this._codeMirror, this);

        this._completionController.noEndingSemicolon = true;

        this._jumpToSymbolTrackingModeEnabled = false;
        this._tokenTrackingController.classNameForHighlightedRange = WebInspector.CodeMirrorTokenTrackingController.JumpToSymbolHighlightStyleClassName;
        this._tokenTrackingController.mouseOverDelayDuration = 0;
        this._tokenTrackingController.mouseOutReleaseDelayDuration = 0;
        this._tokenTrackingController.mode = WebInspector.CodeMirrorTokenTrackingController.Mode.NonSymbolTokens;

        // Make sure CompletionController adds event listeners first.
        // Otherwise we end up in race conditions during complete or delete-complete phases.
        this._codeMirror.on("change", this._contentChanged.bind(this));
        this._codeMirror.on("blur", this._editorBlured.bind(this));
        this._codeMirror.on("beforeChange", this._handleBeforeChange.bind(this));

        if (typeof this._delegate.cssStyleDeclarationTextEditorFocused === "function")
            this._codeMirror.on("focus", this._editorFocused.bind(this));

        this.style = style;
    }

    // Public

    get delegate()
    {
        return this._delegate;
    }

    set delegate(delegate)
    {
        this._delegate = delegate || null;
    }

    get style()
    {
        return this._style;
    }

    set style(style)
    {
        if (this._style === style)
            return;

        if (this._style) {
            this._style.removeEventListener(WebInspector.CSSStyleDeclaration.Event.PropertiesChanged, this._propertiesChanged, this);
            if (this._style.ownerRule && this._style.ownerRule.sourceCodeLocation)
                WebInspector.notifications.removeEventListener(WebInspector.Notification.GlobalModifierKeysDidChange, this._updateJumpToSymbolTrackingMode, this);
        }

        this._style = style || null;

        if (this._style) {
            this._style.addEventListener(WebInspector.CSSStyleDeclaration.Event.PropertiesChanged, this._propertiesChanged, this);
            if (this._style.ownerRule && this._style.ownerRule.sourceCodeLocation)
                WebInspector.notifications.addEventListener(WebInspector.Notification.GlobalModifierKeysDidChange, this._updateJumpToSymbolTrackingMode, this);
        }

        this._updateJumpToSymbolTrackingMode();

        this._resetContent();
    }

    get focused()
    {
        return this._codeMirror.getWrapperElement().classList.contains("CodeMirror-focused");
    }

    get alwaysShowPropertyNames()
    {
        return Object.keys(this._alwaysShowPropertyNames);
    }

    set alwaysShowPropertyNames(alwaysShowPropertyNames)
    {
        this._alwaysShowPropertyNames = (alwaysShowPropertyNames || []).keySet();

        this._resetContent();
    }

    get showsImplicitProperties()
    {
        return this._showsImplicitProperties;
    }

    set showsImplicitProperties(showsImplicitProperties)
    {
        if (this._showsImplicitProperties === showsImplicitProperties)
            return;

        this._showsImplicitProperties = showsImplicitProperties;

        this._resetContent();
    }

    get sortProperties()
    {
        return this._sortProperties;
    }

    set sortProperties(sortProperties)
    {
        if (this._sortProperties === sortProperties)
            return;

        this._sortProperties = sortProperties;

        this._resetContent();
    }

    focus()
    {
        this._codeMirror.focus();
    }

    refresh()
    {
        this._resetContent();
    }

    highlightProperty(property)
    {
        function propertiesMatch(cssProperty)
        {
            if (cssProperty.enabled && !cssProperty.overridden) {
                if (cssProperty.canonicalName === property.canonicalName || hasMatchingLonghandProperty(cssProperty))
                    return true;
            }

            return false;
        }

        function hasMatchingLonghandProperty(cssProperty)
        {
            var cssProperties = cssProperty.relatedLonghandProperties;

            if (!cssProperties.length)
                return false;

            for (var property of cssProperties) {
                if (propertiesMatch(property))
                    return true;
            }

            return false;
        }

        for (var cssProperty of this.style.properties) {
            if (propertiesMatch(cssProperty)) {
                var selection = cssProperty.__propertyTextMarker.find();
                this._codeMirror.setSelection(selection.from, selection.to);
                this.focus();

                return true;
            }
        }

        return false;
    }

    clearSelection()
    {
        this._codeMirror.setCursor({line: 0, ch: 0});
    }

    findMatchingProperties(needle)
    {
        if (!needle) {
            this.resetFilteredProperties();
            return false;
        }

        var propertiesList = this._style.visibleProperties.length ? this._style.visibleProperties : this._style.properties;
        var matchingProperties = [];

        for (var property of propertiesList)
            matchingProperties.push(property.text.includes(needle));

        if (!matchingProperties.includes(true)) {
            this.resetFilteredProperties();
            return false;
        }

        for (var i = 0; i < matchingProperties.length; ++i) {
            var property = propertiesList[i];

            if (matchingProperties[i])
                property.__filterResultClassName = WebInspector.CSSStyleDetailsSidebarPanel.FilterMatchSectionClassName;
            else
                property.__filterResultClassName = WebInspector.CSSStyleDetailsSidebarPanel.NoFilterMatchInPropertyClassName;

            this._updateTextMarkerForPropertyIfNeeded(property);
        }

        return true;
    }

    resetFilteredProperties()
    {
        var propertiesList = this._style.visibleProperties.length ? this._style.visibleProperties : this._style.properties;

        for (var property of propertiesList) {
            if (property.__filterResultClassName) {
                property.__filterResultClassName = null;
                this._updateTextMarkerForPropertyIfNeeded(property)
            }
        }
    }

    removeNonMatchingProperties(needle)
    {
        this._filterResultPropertyNames = null;

        if (!needle) {
            this._resetContent();
            return false;
        }

        var matchingPropertyNames = [];

        for (var property of this._style.properties) {
            var indexesOfNeedle = property.text.getMatchingIndexes(needle);

            if (indexesOfNeedle.length) {
                matchingPropertyNames.push(property.name);
                property.__filterResultClassName = WebInspector.CSSStyleDetailsSidebarPanel.FilterMatchSectionClassName;
                property.__filterResultNeedlePosition = {start: indexesOfNeedle, length: needle.length};
            }
        }

        this._filterResultPropertyNames = matchingPropertyNames.length ? matchingPropertyNames.keySet() : {};

        this._resetContent();

        return matchingPropertyNames.length > 0;
    }

    uncommentAllProperties()
    {
        function uncommentProperties(properties)
        {
            if (!properties.length)
                return false;

            for (var property of properties) {
                if (property._commentRange) {
                    this._uncommentRange(property._commentRange);
                    property._commentRange = null;
                }
            }

            return true;
        }

        return uncommentProperties.call(this, this._style.pendingProperties) || uncommentProperties.call(this, this._style.properties);
    }

    commentAllProperties()
    {
        if (!this._style.hasProperties())
            return false;

        for (var property of this._style.properties) {
            if (property.__propertyTextMarker)
                this._commentProperty(property);
        }

        return true;
    }

    selectFirstProperty()
    {
        var line = this._codeMirror.getLine(0);
        var trimmedLine = line.trimRight();

        if (!line || !trimmedLine.trimLeft().length)
            this.clearSelection();

        var index = line.indexOf(":");
        var cursor = {line: 0, ch: 0};

        this._codeMirror.setSelection(cursor, {line: 0, ch: index < 0 || this._textAtCursorIsComment(this._codeMirror, cursor) ? trimmedLine.length : index});
    }

    selectLastProperty()
    {
        var line = this._codeMirror.lineCount() - 1;
        var lineText = this._codeMirror.getLine(line);
        var trimmedLine = lineText.trimRight();

        var lastAnchor;
        var lastHead;

        if (this._textAtCursorIsComment(this._codeMirror, {line, ch: line.length})) {
            lastAnchor = 0;
            lastHead = line.length;
        } else {
            var colon = /(?::\s*)/.exec(lineText);
            lastAnchor = colon ? colon.index + colon[0].length : 0;
            lastHead = trimmedLine.length - trimmedLine.endsWith(";");
        }

        this._codeMirror.setSelection({line, ch: lastAnchor}, {line, ch: lastHead});
    }

    // Protected

    completionControllerCompletionsHidden(completionController)
    {
        var styleText = this._style.text;
        var currentText = this._formattedContent();

        // If the style text and the current editor text differ then we need to commit.
        // Otherwise we can just update the properties that got skipped because a completion
        // was pending the last time _propertiesChanged was called.
        if (styleText !== currentText)
            this._commitChanges();
        else
            this._propertiesChanged();
    }

    layout()
    {
        this._codeMirror.refresh();
    }

    // Private

    _textAtCursorIsComment(codeMirror, cursor)
    {
        var token = codeMirror.getTokenTypeAt(cursor);
        return token && token.includes("comment");
    }

    _highlightNextNameOrValue(codeMirror, cursor, text)
    {
        var nextAnchor;
        var nextHead;

        if (this._textAtCursorIsComment(codeMirror, cursor)) {
            nextAnchor = 0;
            nextHead = text.length;
        } else {
            var colonIndex = text.indexOf(":");
            var substringIndex = colonIndex >= 0 && cursor.ch >= colonIndex ? colonIndex : 0;

            var regExp = /(?:[^:;\s]\s*)+/g;
            regExp.lastIndex = substringIndex;
            var match = regExp.exec(text);

            nextAnchor = match.index;
            nextHead = nextAnchor + match[0].length;
        }

        codeMirror.setSelection({line: cursor.line, ch: nextAnchor}, {line: cursor.line, ch: nextHead});
    }

    _handleMouseDown(event)
    {
        if (this._codeMirror.options.readOnly)
            return;

        let cursor = this._codeMirror.coordsChar({left: event.x, top: event.y});
        let line = this._codeMirror.getLine(cursor.line);
        let trimmedLine = line.trimRight();
        if (!trimmedLine.trimLeft().length || cursor.ch !== trimmedLine.length)
            return;

        this._mouseDownCursorPosition = cursor;
    }

    _handleMouseUp(event)
    {
        if (this._codeMirror.options.readOnly || !this._mouseDownCursorPosition)
            return;

        let cursor = this._codeMirror.coordsChar({left: event.x, top: event.y});
        if (this._mouseDownCursorPosition.line === cursor.line && this._mouseDownCursorPosition.ch === cursor.ch) {
            let nextLine = this._codeMirror.getLine(cursor.line + 1);
            if (cursor.line < this._codeMirror.lineCount() - 1 && (!nextLine || !nextLine.trim().length)) {
                this._codeMirror.setCursor({line: cursor.line + 1, ch: 0});
            } else {
                let line = this._codeMirror.getLine(cursor.line);
                let replacement = "\n";
                if (!line.trimRight().endsWith(";") && !this._textAtCursorIsComment(this._codeMirror, cursor))
                    replacement = ";" + replacement;

                this._codeMirror.replaceRange(replacement, cursor);
            }
        }

        this._mouseDownCursorPosition = null;
    }

    _handleBeforeChange(codeMirror, change)
    {
        if (change.origin !== "+delete" || this._completionController.isShowingCompletions())
            return CodeMirror.Pass;

        if (!change.to.line && !change.to.ch) {
            if (codeMirror.lineCount() === 1)
                return CodeMirror.Pass;

            var line = codeMirror.getLine(change.to.line);
            if (line && line.trim().length)
                return CodeMirror.Pass;

            codeMirror.execCommand("deleteLine");
            return;
        }

        var marks = codeMirror.findMarksAt(change.to);
        if (!marks.length)
            return CodeMirror.Pass;

        for (var mark of marks)
            mark.clear();
    }

    _handleEnterKey(codeMirror)
    {
        var cursor = codeMirror.getCursor();
        var line = codeMirror.getLine(cursor.line);
        var trimmedLine = line.trimRight();
        var hasEndingSemicolon = trimmedLine.endsWith(";");

        if (!trimmedLine.trimLeft().length)
            return CodeMirror.Pass;

        if (hasEndingSemicolon && cursor.ch === trimmedLine.length - 1)
            ++cursor.ch;

        if (cursor.ch === trimmedLine.length) {
            var replacement = "\n";

            if (!hasEndingSemicolon && !this._textAtCursorIsComment(this._codeMirror, cursor))
                replacement = ";" + replacement;

            this._codeMirror.replaceRange(replacement, cursor);
            return;
        }

        return CodeMirror.Pass;
    }

    _insertNewlineAfterCurrentLine(codeMirror)
    {
        var cursor = codeMirror.getCursor();
        var line = codeMirror.getLine(cursor.line);
        var trimmedLine = line.trimRight();

        cursor.ch = trimmedLine.length;

        if (cursor.ch) {
            var replacement = "\n";

            if (!trimmedLine.endsWith(";") && !this._textAtCursorIsComment(this._codeMirror, cursor))
                replacement = ";" + replacement;

            this._codeMirror.replaceRange(replacement, cursor);
            return;
        }

        return CodeMirror.Pass;
    }

    _handleShiftTabKey(codeMirror)
    {
        function switchRule()
        {
            if (this._delegate && typeof this._delegate.cssStyleDeclarationTextEditorSwitchRule === "function") {
                this._delegate.cssStyleDeclarationTextEditorSwitchRule(true);
                return;
            }

            return CodeMirror.Pass;
        }

        let cursor = codeMirror.getCursor();
        let line = codeMirror.getLine(cursor.line);
        let previousLine = codeMirror.getLine(cursor.line - 1);

        if (!line && !previousLine && !cursor.line)
            return switchRule.call(this);

        let trimmedPreviousLine = previousLine ? previousLine.trimRight() : "";
        let previousAnchor = 0;
        let previousHead = line.length;
        let isComment = this._textAtCursorIsComment(codeMirror, cursor);

        if (cursor.ch === line.indexOf(":") || line.indexOf(":") < 0 || isComment) {
            if (previousLine) {
                --cursor.line;
                previousHead = trimmedPreviousLine.length;

                if (!this._textAtCursorIsComment(codeMirror, cursor)) {
                    let colon = /(?::\s*)/.exec(previousLine);
                    previousAnchor = colon ? colon.index + colon[0].length : 0;
                    if (trimmedPreviousLine.includes(";"))
                        previousHead = trimmedPreviousLine.lastIndexOf(";");
                }
                
                codeMirror.setSelection({line: cursor.line, ch: previousAnchor}, {line: cursor.line, ch: previousHead});
                return;
            }

            if (cursor.line) {
                codeMirror.setCursor(cursor.line - 1, 0);
                return;
            }

            return switchRule.call(this);
        }

        if (!isComment) {
            let match = /(?:[^:;\s]\s*)+/.exec(line);
            previousAnchor = match.index;
            previousHead = previousAnchor + match[0].length;
        }

        codeMirror.setSelection({line: cursor.line, ch: previousAnchor}, {line: cursor.line, ch: previousHead});
    }

    _handleTabKey(codeMirror)
    {
        function switchRule() {
            if (this._delegate && typeof this._delegate.cssStyleDeclarationTextEditorSwitchRule === "function") {
                this._delegate.cssStyleDeclarationTextEditorSwitchRule();
                return;
            }

            return CodeMirror.Pass;
        }

        let cursor = codeMirror.getCursor();
        let line = codeMirror.getLine(cursor.line);
        let trimmedLine = line.trimRight();
        let lastLine = cursor.line === codeMirror.lineCount() - 1;
        let nextLine = codeMirror.getLine(cursor.line + 1);
        let trimmedNextLine = nextLine ? nextLine.trimRight() : "";

        if (!trimmedLine.trimLeft().length) {
            if (lastLine)
                return switchRule.call(this);

            if (!trimmedNextLine.trimLeft().length) {
                codeMirror.setCursor(cursor.line + 1, 0);
                return;
            }

            ++cursor.line;
            this._highlightNextNameOrValue(codeMirror, cursor, nextLine);
            return;
        }

        if (trimmedLine.endsWith(":")) {
            codeMirror.setCursor(cursor.line, line.length);
            this._completionController._completeAtCurrentPosition(true);
            return;
        }

        let hasEndingSemicolon = trimmedLine.endsWith(";");
        let pastLastSemicolon = line.includes(";") && cursor.ch >= line.lastIndexOf(";");

        if (cursor.ch >= line.trimRight().length - hasEndingSemicolon || pastLastSemicolon) {
            this._completionController.completeAtCurrentPositionIfNeeded().then(function(result) {
                if (result !== WebInspector.CodeMirrorCompletionController.UpdatePromise.NoCompletionsFound)
                    return;

                let replacement = "";

                if (!hasEndingSemicolon && !pastLastSemicolon && !this._textAtCursorIsComment(codeMirror, cursor))
                    replacement += ";";

                if (lastLine)
                    replacement += "\n";

                if (replacement.length)
                    codeMirror.replaceRange(replacement, {line: cursor.line, ch: trimmedLine.length});

                if (!nextLine) {
                    codeMirror.setCursor(cursor.line + 1, 0);
                    return;
                }

                this._highlightNextNameOrValue(codeMirror, {line: cursor.line + 1, ch: 0}, nextLine);
            }.bind(this));

            return;
        }

        this._highlightNextNameOrValue(codeMirror, cursor, line);
    }

    _clearRemoveEditingLineClassesTimeout()
    {
        if (!this._removeEditingLineClassesTimeout)
            return;

        clearTimeout(this._removeEditingLineClassesTimeout);
        delete this._removeEditingLineClassesTimeout;
    }

    _removeEditingLineClasses()
    {
        this._clearRemoveEditingLineClassesTimeout();

        function removeEditingLineClasses()
        {
            var lineCount = this._codeMirror.lineCount();
            for (var i = 0; i < lineCount; ++i)
                this._codeMirror.removeLineClass(i, "wrap", WebInspector.CSSStyleDeclarationTextEditor.EditingLineStyleClassName);
        }

        this._codeMirror.operation(removeEditingLineClasses.bind(this));
    }

    _removeEditingLineClassesSoon()
    {
        if (this._removeEditingLineClassesTimeout)
            return;
        this._removeEditingLineClassesTimeout = setTimeout(this._removeEditingLineClasses.bind(this), WebInspector.CSSStyleDeclarationTextEditor.RemoveEditingLineClassesDelay);
    }

    _formattedContent()
    {
        // Start with the prefix whitespace we stripped.
        var content = WebInspector.CSSStyleDeclarationTextEditor.PrefixWhitespace;

        // Get each line and add the line prefix whitespace and newlines.
        var lineCount = this._codeMirror.lineCount();
        for (var i = 0; i < lineCount; ++i) {
            var lineContent = this._codeMirror.getLine(i);
            content += this._linePrefixWhitespace + lineContent;
            if (i !== lineCount - 1)
                content += "\n";
        }

        // Add the suffix whitespace we stripped.
        content += WebInspector.CSSStyleDeclarationTextEditor.SuffixWhitespace;

        // This regular expression replacement removes extra newlines
        // in between properties while preserving leading whitespace
        return content.replace(/\s*\n\s*\n(\s*)/g, "\n$1");
    }

    _commitChanges()
    {
        if (this._commitChangesTimeout) {
            clearTimeout(this._commitChangesTimeout);
            delete this._commitChangesTimeout;
        }

        this._style.text = this._formattedContent();
    }

    _editorBlured(codeMirror)
    {
        // Clicking a suggestion causes the editor to blur. We don't want to reset content in this case.
        if (this._completionController.isHandlingClickEvent())
            return;

        // Reset the content on blur since we stop accepting external changes while the the editor is focused.
        // This causes us to pick up any change that was suppressed while the editor was focused.
        this._resetContent();
        this.dispatchEventToListeners(WebInspector.CSSStyleDeclarationTextEditor.Event.Blurred);
    }

    _editorFocused(codeMirror)
    {
        if (typeof this._delegate.cssStyleDeclarationTextEditorFocused === "function")
            this._delegate.cssStyleDeclarationTextEditorFocused();
    }

    _contentChanged(codeMirror, change)
    {
        // Return early if the style isn't editable. This still can be called when readOnly is set because
        // clicking on a color swatch modifies the text.
        if (!this._style || !this._style.editable || this._ignoreCodeMirrorContentDidChangeEvent)
            return;

        this._markLinesWithCheckboxPlaceholder();

        this._clearRemoveEditingLineClassesTimeout();
        this._codeMirror.addLineClass(change.from.line, "wrap", WebInspector.CSSStyleDeclarationTextEditor.EditingLineStyleClassName);

        // When the change is a completion change, create color swatches now since the changes
        // will not go through _propertiesChanged until completionControllerCompletionsHidden happens.
        // This way any auto completed colors get swatches right away.
        if (this._completionController.isCompletionChange(change))
            this._createInlineSwatches(false, change.from.line);

        // Use a short delay for user input to coalesce more changes before committing. Other actions like
        // undo, redo and paste are atomic and work better with a zero delay. CodeMirror identifies changes that
        // get coalesced in the undo stack with a "+" prefix on the origin. Use that to set the delay for our coalescing.
        var delay = change.origin && change.origin.charAt(0) === "+" ? WebInspector.CSSStyleDeclarationTextEditor.CommitCoalesceDelay : 0;

        // Reset the timeout so rapid changes coalesce after a short delay.
        if (this._commitChangesTimeout)
            clearTimeout(this._commitChangesTimeout);
        this._commitChangesTimeout = setTimeout(this._commitChanges.bind(this), delay);

        this.dispatchEventToListeners(WebInspector.CSSStyleDeclarationTextEditor.Event.ContentChanged);
    }

    _updateTextMarkers(nonatomic)
    {
        function update()
        {
            this._clearTextMarkers(true);

            this._iterateOverProperties(true, function(property) {
                var styleTextRange = property.styleDeclarationTextRange;
                console.assert(styleTextRange);
                if (!styleTextRange)
                    return;

                var from = {line: styleTextRange.startLine, ch: styleTextRange.startColumn};
                var to = {line: styleTextRange.endLine, ch: styleTextRange.endColumn};

                // Adjust the line position for the missing prefix line.
                from.line--;
                to.line--;

                // Adjust the column for the stripped line prefix whitespace.
                from.ch -= this._linePrefixWhitespace.length;
                to.ch -= this._linePrefixWhitespace.length;

                this._createTextMarkerForPropertyIfNeeded(from, to, property);
            });

            if (!this._codeMirror.getOption("readOnly")) {
                // Look for comments that look like properties and add checkboxes in front of them.
                this._codeMirror.eachLine((lineHandler) => {
                    this._createCommentedCheckboxMarker(lineHandler);
                });
            }

            // Look for swatchable values and make inline swatches.
            this._createInlineSwatches(true);

            this._markLinesWithCheckboxPlaceholder();
        }

        if (nonatomic)
            update.call(this);
        else
            this._codeMirror.operation(update.bind(this));
    }

    _createCommentedCheckboxMarker(lineHandle)
    {
        var lineNumber = lineHandle.lineNo();

        // Since lineNumber can be 0, it is also necessary to check if it is a number before returning.
        if (!lineNumber && isNaN(lineNumber))
            return;

        // Matches a comment like: /* -webkit-foo: bar; */
        let commentedPropertyRegex = /\/\*\s*[-\w]+\s*\:\s*(?:(?:\".*\"|url\(.+\)|[^;])\s*)+;?\s*\*\//g;

        var match = commentedPropertyRegex.exec(lineHandle.text);
        if (!match)
            return;

        while (match) {
            var checkboxElement = document.createElement("input");
            checkboxElement.type = "checkbox";
            checkboxElement.checked = false;
            checkboxElement.addEventListener("change", this._propertyCommentCheckboxChanged.bind(this));

            var from = {line: lineNumber, ch: match.index};
            var to = {line: lineNumber, ch: match.index + match[0].length};

            var checkboxMarker = this._codeMirror.setUniqueBookmark(from, checkboxElement);
            checkboxMarker.__propertyCheckbox = true;

            var commentTextMarker = this._codeMirror.markText(from, to);

            checkboxElement.__commentTextMarker = commentTextMarker;

            match = commentedPropertyRegex.exec(lineHandle.text);
        }
    }

    _createInlineSwatches(nonatomic, lineNumber)
    {
        function createSwatch(swatch, marker, valueObject, valueString)
        {
            swatch.addEventListener(WebInspector.InlineSwatch.Event.ValueChanged, this._inlineSwatchValueChanged, this);

            let codeMirrorTextMarker = marker.codeMirrorTextMarker;
            let codeMirrorTextMarkerRange = codeMirrorTextMarker.find();
            this._codeMirror.setUniqueBookmark(codeMirrorTextMarkerRange.from, swatch.element);

            swatch.__textMarker = codeMirrorTextMarker;
            swatch.__textMarkerRange = codeMirrorTextMarkerRange;
        }

        function update()
        {
            let range = typeof lineNumber === "number" ? new WebInspector.TextRange(lineNumber, 0, lineNumber + 1, 0) : null;

            // Look for color strings and add swatches in front of them.
            createCodeMirrorColorTextMarkers(this._codeMirror, range, (marker, color, colorString) => {
                let swatch = new WebInspector.InlineSwatch(WebInspector.InlineSwatch.Type.Color, color, this._codeMirror.getOption("readOnly"));
                createSwatch.call(this, swatch, marker, color, colorString);
            });

            // Look for gradient strings and add swatches in front of them.
            createCodeMirrorGradientTextMarkers(this._codeMirror, range, (marker, gradient, gradientString) => {
                let swatch = new WebInspector.InlineSwatch(WebInspector.InlineSwatch.Type.Gradient, gradient, this._codeMirror.getOption("readOnly"));
                createSwatch.call(this, swatch, marker, gradient, gradientString);
            });

            // Look for cubic-bezier strings and add swatches in front of them.
            createCodeMirrorCubicBezierTextMarkers(this._codeMirror, range, (marker, bezier, bezierString) => {
                let swatch = new WebInspector.InlineSwatch(WebInspector.InlineSwatch.Type.Bezier, bezier, this._codeMirror.getOption("readOnly"));
                createSwatch.call(this, swatch, marker, bezier, bezierString);
            });

            // Look for spring strings and add swatches in front of them.
            createCodeMirrorSpringTextMarkers(this._codeMirror, range, (marker, spring, springString) => {
                let swatch = new WebInspector.InlineSwatch(WebInspector.InlineSwatch.Type.Spring, spring, this._codeMirror.getOption("readOnly"));
                createSwatch.call(this, swatch, marker, spring, springString);
            });
        }

        if (nonatomic)
            update.call(this);
        else
            this._codeMirror.operation(update.bind(this));
    }

    _updateTextMarkerForPropertyIfNeeded(property)
    {
        var textMarker = property.__propertyTextMarker;
        console.assert(textMarker);
        if (!textMarker)
            return;

        var range = textMarker.find();
        console.assert(range);
        if (!range)
            return;

        this._createTextMarkerForPropertyIfNeeded(range.from, range.to, property);
    }

    _createTextMarkerForPropertyIfNeeded(from, to, property)
    {
        if (!this._codeMirror.getOption("readOnly")) {
            // Create a new checkbox element and marker.

            console.assert(property.enabled);

            var checkboxElement = document.createElement("input");
            checkboxElement.type = "checkbox";
            checkboxElement.checked = true;
            checkboxElement.addEventListener("change", this._propertyCheckboxChanged.bind(this));
            checkboxElement.__cssProperty = property;

            var checkboxMarker = this._codeMirror.setUniqueBookmark(from, checkboxElement);
            checkboxMarker.__propertyCheckbox = true;
        } else if (this._delegate.cssStyleDeclarationTextEditorShouldAddPropertyGoToArrows
                && !property.implicit && typeof this._delegate.cssStyleDeclarationTextEditorShowProperty === "function") {

            let arrowElement = WebInspector.createGoToArrowButton();
            arrowElement.title = WebInspector.UIString("Option-click to show source");

            let delegate = this._delegate;
            arrowElement.addEventListener("click", function(event) {
                delegate.cssStyleDeclarationTextEditorShowProperty(property, event.altKey);
            });

            this._codeMirror.setUniqueBookmark(to, arrowElement);
        }

        function duplicatePropertyExistsBelow(cssProperty)
        {
            var propertyFound = false;

            for (var property of this._style.properties) {
                if (property === cssProperty)
                    propertyFound = true;
                else if (property.name === cssProperty.name && propertyFound)
                    return true;
            }

            return false;
        }

        var propertyNameIsValid = false;
        if (WebInspector.CSSCompletions.cssNameCompletions)
            propertyNameIsValid = WebInspector.CSSCompletions.cssNameCompletions.isValidPropertyName(property.name);

        var classNames = ["css-style-declaration-property"];

        if (property.overridden)
            classNames.push("overridden");

        if (property.implicit)
            classNames.push("implicit");

        if (this._style.inherited && !property.inherited)
            classNames.push("not-inherited");

        if (!property.valid && property.hasOtherVendorNameOrKeyword())
            classNames.push("other-vendor");
        else if (!property.valid && (!propertyNameIsValid || duplicatePropertyExistsBelow.call(this, property)))
            classNames.push("invalid");

        if (!property.enabled)
            classNames.push("disabled");

        if (property.__filterResultClassName && !property.__filterResultNeedlePosition)
            classNames.push(property.__filterResultClassName);

        var classNamesString = classNames.join(" ");

        // If there is already a text marker and it's in the same document, then try to avoid recreating it.
        // FIXME: If there are multiple CSSStyleDeclarationTextEditors for the same style then this will cause
        // both editors to fight and always recreate their text markers. This isn't really common.
        if (property.__propertyTextMarker && property.__propertyTextMarker.doc.cm === this._codeMirror && property.__propertyTextMarker.find()) {
            // If the class name is the same then we don't need to make a new marker.
            if (property.__propertyTextMarker.className === classNamesString)
                return;

            property.__propertyTextMarker.clear();
        }

        var propertyTextMarker = this._codeMirror.markText(from, to, {className: classNamesString});

        propertyTextMarker.__cssProperty = property;
        property.__propertyTextMarker = propertyTextMarker;

        property.addEventListener(WebInspector.CSSProperty.Event.OverriddenStatusChanged, this._propertyOverriddenStatusChanged, this);

        this._removeCheckboxPlaceholder(from.line);

        if (property.__filterResultClassName && property.__filterResultNeedlePosition) {
            for (var needlePosition of property.__filterResultNeedlePosition.start) {
                var start = {line: from.line, ch: needlePosition};
                var end = {line: to.line, ch: start.ch + property.__filterResultNeedlePosition.length};

                this._codeMirror.markText(start, end, {className: property.__filterResultClassName});
            }
        }

        if (this._codeMirror.getOption("readOnly") || property.hasOtherVendorNameOrKeyword() || property.text.trim().endsWith(":"))
            return;

        var propertyHasUnnecessaryPrefix = property.name.startsWith("-webkit-") && WebInspector.CSSCompletions.cssNameCompletions.isValidPropertyName(property.canonicalName);

        function generateInvalidMarker(options)
        {
            var invalidMarker = document.createElement("button");
            invalidMarker.className = "invalid-warning-marker";
            invalidMarker.title = options.title;

            if (typeof options.correction === "string") {
                // Allow for blank strings
                invalidMarker.classList.add("clickable");
                invalidMarker.addEventListener("click", function() {
                    this._codeMirror.replaceRange(options.correction, from, to);

                    if (options.autocomplete) {
                        this._codeMirror.setCursor(to);
                        this.focus();
                        this._completionController._completeAtCurrentPosition(true);
                    }
                }.bind(this));
            }

            this._codeMirror.setBookmark(options.position, invalidMarker);
        }

        function instancesOfProperty(propertyName)
        {
            var count = 0;

            for (var property of this._style.properties) {
                if (property.name === propertyName)
                    ++count;
            }

            return count;
        }

        // Number of times this property name is listed in the rule.
        var instances = instancesOfProperty.call(this, property.name);
        var invalidMarkerInfo;

        if (propertyHasUnnecessaryPrefix && !instancesOfProperty.call(this, property.canonicalName)) {
            // This property has a prefix and is valid without the prefix and the rule containing this property does not have the unprefixed version of the property.
            generateInvalidMarker.call(this, {
                position: from,
                title: WebInspector.UIString("The “webkit” prefix is not necessary.\nClick to insert a duplicate without the prefix."),
                correction: property.text + "\n" + property.text.replace("-webkit-", ""),
                autocomplete: false
            });
        } else if (instances > 1) {
            invalidMarkerInfo = {
                position: from,
                title: WebInspector.UIString("Duplicate property “%s”.\nClick to delete this property.").format(property.name),
                correction: "",
                autocomplete: false
            };
        }

        if (property.valid) {
            if (invalidMarkerInfo)
                generateInvalidMarker.call(this, invalidMarkerInfo);

            return;
        }

        if (propertyNameIsValid) {
            // The property's name is valid but its value is not (either it is not supported for this property or there is no value).
            var semicolon = /:\s*/.exec(property.text);
            var start = {line: from.line, ch: semicolon.index + semicolon[0].length};
            var end = {line: to.line, ch: start.ch + property.value.length};

            this._codeMirror.markText(start, end, {className: "invalid"});

            if (/^(?:\d+)$/.test(property.value)) {
                invalidMarkerInfo = {
                    position: start,
                    title: WebInspector.UIString("The value “%s” needs units.\nClick to add “px” to the value.").format(property.value),
                    correction: property.name + ": " + property.value + "px;",
                    autocomplete: false
                };
            } else {
                var valueReplacement = property.value.length ? WebInspector.UIString("The value “%s” is not supported for this property.\nClick to delete and open autocomplete.").format(property.value) : WebInspector.UIString("This property needs a value.\nClick to open autocomplete.");

                invalidMarkerInfo = {
                    position: start,
                    title: valueReplacement,
                    correction: property.name + ": ",
                    autocomplete: true
                };
            }
        } else if (!instancesOfProperty.call(this, "-webkit-" + property.name) && WebInspector.CSSCompletions.cssNameCompletions.propertyRequiresWebkitPrefix(property.name)) {
            // The property is valid and exists in the rule while its prefixed version does not.
            invalidMarkerInfo = {
                position: from,
                title: WebInspector.UIString("The “webkit” prefix is needed for this property.\nClick to insert a duplicate with the prefix."),
                correction: "-webkit-" + property.text + "\n" + property.text,
                autocomplete: false
            };
        } else if (!propertyHasUnnecessaryPrefix && !WebInspector.CSSCompletions.cssNameCompletions.isValidPropertyName("-webkit-" + property.name)) {
            // The property either has no prefix and is invalid with a prefix or is invalid without a prefix.
            var closestPropertyName = WebInspector.CSSCompletions.cssNameCompletions.getClosestPropertyName(property.name);

            if (closestPropertyName) {
                // The property name has less than 3 other properties that have the same Levenshtein distance.
                invalidMarkerInfo = {
                    position: from,
                    title: WebInspector.UIString("Did you mean “%s”?\nClick to replace.").format(closestPropertyName),
                    correction: property.text.replace(property.name, closestPropertyName),
                    autocomplete: true
                };
            } else if (property.name.startsWith("-webkit-") && (closestPropertyName = WebInspector.CSSCompletions.cssNameCompletions.getClosestPropertyName(property.canonicalName))) {
                // The unprefixed property name has less than 3 other properties that have the same Levenshtein distance.
                invalidMarkerInfo = {
                    position: from,
                    title: WebInspector.UIString("Did you mean “%s”?\nClick to replace.").format("-webkit-" + closestPropertyName),
                    correction: property.text.replace(property.canonicalName, closestPropertyName),
                    autocomplete: true
                };
            } else {
                // The property name is so vague or nonsensical that there are more than 3 other properties that have the same Levenshtein value.
                invalidMarkerInfo = {
                    position: from,
                    title: WebInspector.UIString("The property “%s” is not supported.").format(property.name),
                    correction: false,
                    autocomplete: false
                };
            }
        }

        if (!invalidMarkerInfo)
            return;

        generateInvalidMarker.call(this, invalidMarkerInfo);
    }

    _clearTextMarkers(nonatomic, all)
    {
        function clear()
        {
            var markers = this._codeMirror.getAllMarks();
            for (var i = 0; i < markers.length; ++i) {
                var textMarker = markers[i];

                if (!all && textMarker.__checkboxPlaceholder) {
                    var position = textMarker.find();

                    // Only keep checkbox placeholders if they are in the first column.
                    if (position && !position.ch)
                        continue;
                }

                if (textMarker.__cssProperty) {
                    textMarker.__cssProperty.removeEventListener(null, null, this);

                    delete textMarker.__cssProperty.__propertyTextMarker;
                    delete textMarker.__cssProperty;
                }

                textMarker.clear();
            }
        }

        if (nonatomic)
            clear.call(this);
        else
            this._codeMirror.operation(clear.bind(this));
    }

    _iterateOverProperties(onlyVisibleProperties, callback)
    {
        var properties = onlyVisibleProperties ? this._style.visibleProperties : this._style.properties;

        if (this._filterResultPropertyNames) {
            properties = properties.filter(function(property) {
                return (!property.implicit || this._showsImplicitProperties) && property.name in this._filterResultPropertyNames;
            }, this);

            if (this._sortProperties)
                properties.sort(function(a, b) { return a.name.localeCompare(b.name); });
        } else if (!onlyVisibleProperties) {
            // Filter based on options only when all properties are used.
            properties = properties.filter(function(property) {
                return !property.implicit || this._showsImplicitProperties || property.canonicalName in this._alwaysShowPropertyNames;
            }, this);

            if (this._sortProperties)
                properties.sort(function(a, b) { return a.name.localeCompare(b.name); });
        }

        for (var i = 0; i < properties.length; ++i) {
            if (callback.call(this, properties[i], i === properties.length - 1))
                break;
        }
    }

    _propertyCheckboxChanged(event)
    {
        var property = event.target.__cssProperty;
        console.assert(property);
        if (!property)
            return;

        this._commentProperty(property);
    }

    _commentProperty(property)
    {
        var textMarker = property.__propertyTextMarker;
        console.assert(textMarker);
        if (!textMarker)
            return;

        // Check if the property has been removed already, like from double-clicking
        // the checkbox and calling this event listener multiple times.
        var range = textMarker.find();
        if (!range)
            return;

        property._commentRange = range;
        property._commentRange.to.ch += 6; // Number of characters added by comments.

        var text = this._codeMirror.getRange(range.from, range.to);

        function update()
        {
            // Replace the text with a commented version.
            this._codeMirror.replaceRange("/* " + text + " */", range.from, range.to);

            // Update the line for any inline swatches that got removed.
            this._createInlineSwatches(true, range.from.line);
        }

        this._codeMirror.operation(update.bind(this));
    }

    _propertyCommentCheckboxChanged(event)
    {
        var commentTextMarker = event.target.__commentTextMarker;
        console.assert(commentTextMarker);
        if (!commentTextMarker)
            return;

        // Check if the comment has been removed already, like from double-clicking
        // the checkbox and calling event listener multiple times.
        var range = commentTextMarker.find();
        if (!range)
            return;

        this._uncommentRange(range);
    }

    _uncommentRange(range)
    {
        var text = this._codeMirror.getRange(range.from, range.to);

        // Remove the comment prefix and suffix.
        text = text.replace(/^\/\*\s*/, "").replace(/\s*\*\/$/, "");

        // Add a semicolon if there isn't one already.
        if (text.length && text.charAt(text.length - 1) !== ";")
            text += ";";

        function update()
        {
            this._codeMirror.addLineClass(range.from.line, "wrap", WebInspector.CSSStyleDeclarationTextEditor.EditingLineStyleClassName);
            this._codeMirror.replaceRange(text, range.from, range.to);

            // Update the line for any inline swatches that got removed.
            this._createInlineSwatches(true, range.from.line);
        }

        this._codeMirror.operation(update.bind(this));
    }

    _inlineSwatchValueChanged(event)
    {
        let swatch = event && event.target;
        console.assert(swatch);
        if (!swatch)
            return;

        let value = event.data && event.data.value && event.data.value.toString();
        console.assert(value);
        if (!value)
            return;

        let textMarker = swatch.__textMarker;
        let range = swatch.__textMarkerRange;
        console.assert(range);
        if (!range)
            return;

        function update()
        {
            // The original text marker might have been cleared by a style update,
            // in this case we need to find the new text marker so we know the
            // right range for the new style text.
            if (!textMarker || !textMarker.find()) {
                textMarker = null;

                let marks = this._codeMirror.findMarksAt(range.from);
                if (!marks.length)
                    return;

                for (let mark of marks) {
                    let type = WebInspector.TextMarker.textMarkerForCodeMirrorTextMarker(mark).type;
                    if (Object.values(WebInspector.TextMarker.Type).includes(type))
                        continue;

                    textMarker = mark;
                    break;
                }
            }

            if (!textMarker)
                return;

            // Sometimes we still might find a stale text marker with findMarksAt.
            range = textMarker.find();
            if (!range)
                return;

            textMarker.clear();

            this._codeMirror.replaceRange(value, range.from, range.to);

            // The value's text could have changed, so we need to update the "range"
            // variable to anticipate a different "range.to" property.
            range.to.ch = range.from.ch + value.length;

            textMarker = this._codeMirror.markText(range.from, range.to);

            swatch.__textMarker = textMarker;
        }

        this._codeMirror.operation(update.bind(this));
    }

    _propertyOverriddenStatusChanged(event)
    {
        this._updateTextMarkerForPropertyIfNeeded(event.target);
    }

    _propertiesChanged(event)
    {
        // Don't try to update the document while completions are showing. Doing so will clear
        // the completion hint and prevent further interaction with the completion.
        if (this._completionController.isShowingCompletions())
            return;

        // Reset the content if the text is different and we are not focused.
        if (!this.focused && (!this._style.text || this._style.text !== this._formattedContent())) {
            this._resetContent();
            return;
        }

        this._removeEditingLineClassesSoon();

        this._updateTextMarkers();
    }

    _markLinesWithCheckboxPlaceholder()
    {
        if (this._codeMirror.getOption("readOnly"))
            return;

        var linesWithPropertyCheckboxes = {};
        var linesWithCheckboxPlaceholders = {};

        var markers = this._codeMirror.getAllMarks();
        for (var i = 0; i < markers.length; ++i) {
            var textMarker = markers[i];
            if (textMarker.__propertyCheckbox) {
                var position = textMarker.find();
                if (position)
                    linesWithPropertyCheckboxes[position.line] = true;
            } else if (textMarker.__checkboxPlaceholder) {
                var position = textMarker.find();
                if (position)
                    linesWithCheckboxPlaceholders[position.line] = true;
            }
        }

        var lineCount = this._codeMirror.lineCount();

        for (var i = 0; i < lineCount; ++i) {
            if (i in linesWithPropertyCheckboxes || i in linesWithCheckboxPlaceholders)
                continue;

            var position = {line: i, ch: 0};

            var placeholderElement = document.createElement("div");
            placeholderElement.className = WebInspector.CSSStyleDeclarationTextEditor.CheckboxPlaceholderElementStyleClassName;

            var placeholderMark = this._codeMirror.setUniqueBookmark(position, placeholderElement);
            placeholderMark.__checkboxPlaceholder = true;
        }
    }

    _removeCheckboxPlaceholder(lineNumber)
    {
        var marks = this._codeMirror.findMarksAt({line: lineNumber, ch: 0});
        for (var i = 0; i < marks.length; ++i) {
            var mark = marks[i];
            if (!mark.__checkboxPlaceholder)
                continue;

            mark.clear();
            return;
        }
    }

    _formattedContentFromEditor()
    {
        // FIXME: <rdar://problem/10593948> Provide a way to change the tab width in the Web Inspector
        var indentString = "    ";
        var builder = new FormatterContentBuilder(indentString);
        var formatter = new WebInspector.Formatter(this._codeMirror, builder);
        var start = {line: 0, ch: 0};
        var end = {line: this._codeMirror.lineCount() - 1};
        formatter.format(start, end);

        return builder.formattedContent.trim();
    }

    _resetContent()
    {
        if (this._commitChangesTimeout) {
            clearTimeout(this._commitChangesTimeout);
            this._commitChangesTimeout = null;
        }

        this._removeEditingLineClasses();

        // Only allow editing if we have a style, it is editable and we have text range in the stylesheet.
        const readOnly = !this._style || !this._style.editable || !this._style.styleSheetTextRange;
        this._codeMirror.setOption("readOnly", readOnly);

        if (readOnly) {
            this.element.classList.add(WebInspector.CSSStyleDeclarationTextEditor.ReadOnlyStyleClassName);
            this._codeMirror.setOption("placeholder", WebInspector.UIString("No Properties"));
        } else {
            this.element.classList.remove(WebInspector.CSSStyleDeclarationTextEditor.ReadOnlyStyleClassName);
            this._codeMirror.setOption("placeholder", WebInspector.UIString("No Properties \u2014 Click to Edit"));
        }

        if (!this._style) {
            this._ignoreCodeMirrorContentDidChangeEvent = true;

            this._clearTextMarkers(false, true);
            this._codeMirror.setValue("");
            this._codeMirror.clearHistory();
            this._codeMirror.markClean();

            this._ignoreCodeMirrorContentDidChangeEvent = false;
            return;
        }

        function update()
        {
            // Remember the cursor position/selection.
            let isEditorReadOnly = this._codeMirror.getOption("readOnly");
            let styleText = this._style.text;
            let trimmedStyleText = styleText.trim();

            // We only need to format non-empty styles, but prepare checkbox placeholders
            // in any case because that will indent the cursor when the User starts typing.
            if (!trimmedStyleText && !isEditorReadOnly) {
                this._markLinesWithCheckboxPlaceholder();
                return;
            }

            // Generate formatted content for readonly editors by iterating properties.
            if (isEditorReadOnly) {
                this._codeMirror.setValue("");
                let lineNumber = 0;
                this._iterateOverProperties(false, function(property) {
                    let from = {line: lineNumber, ch: 0};
                    let to = {line: lineNumber};
                    // Readonly properties are pretty printed by `synthesizedText` and not the Formatter.
                    this._codeMirror.replaceRange((lineNumber ? "\n" : "") + property.synthesizedText, from);
                    this._createTextMarkerForPropertyIfNeeded(from, to, property);
                    lineNumber++;
                });
                return;
            }

            let selectionAnchor = this._codeMirror.getCursor("anchor");
            let selectionHead = this._codeMirror.getCursor("head");
            let whitespaceRegex = /\s+/g;

            // FIXME: <rdar://problem/10593948> Provide a way to change the tab width in the Web Inspector
            this._linePrefixWhitespace = "    ";
            let styleTextPrefixWhitespace = styleText.match(/^\s*/);

            // If there is a match and the style text contains a newline, attempt to pull out the prefix whitespace
            // in front of the first line of CSS to use for every line.  If  there is no newline, we want to avoid
            // adding multiple spaces to a single line CSS rule and instead format it on multiple lines.
            if (styleTextPrefixWhitespace && trimmedStyleText.includes("\n")) {
                let linePrefixWhitespaceMatch = styleTextPrefixWhitespace[0].match(/[^\S\n]+$/);
                if (linePrefixWhitespaceMatch)
                    this._linePrefixWhitespace = linePrefixWhitespaceMatch[0];
            }

            // Set non-optimized, valid and invalid styles in preparation for the Formatter.
            this._codeMirror.setValue(trimmedStyleText);

            // Now the Formatter pretty prints the styles.
            this._codeMirror.setValue(this._formattedContentFromEditor());

            // We need to workaround the fact that...
            // 1) `this._style.properties` only holds valid CSSProperty instances but not
            // comments and invalid properties like `color;`.
            // 2) `_createTextMarkerForPropertyIfNeeded` relies on CSSProperty instances.
            let cssPropertiesMap = new Map();
            this._iterateOverProperties(false, function(cssProperty) {
                cssProperty.__refreshedAfterBlur = false;

                let propertyTextSansWhitespace = cssProperty.text.replace(whitespaceRegex, "");
                let existingProperties = cssPropertiesMap.get(propertyTextSansWhitespace) || [];
                existingProperties.push(cssProperty);

                cssPropertiesMap.set(propertyTextSansWhitespace, existingProperties);
            });

            // Go through the Editor line by line and create TextMarker when a
            // CSSProperty instance for that property exists. If not, then don't create a TextMarker.
            this._codeMirror.eachLine(function(lineHandler) {
                let lineNumber = lineHandler.lineNo();
                let lineContentSansWhitespace = lineHandler.text.replace(whitespaceRegex, "");
                let properties = cssPropertiesMap.get(lineContentSansWhitespace);
                if (!properties) {
                    this._createCommentedCheckboxMarker(lineHandler);
                    return;
                }

                for (let property of properties) {
                    if (property.__refreshedAfterBlur)
                        continue;

                    let from = {line: lineNumber, ch: 0};
                    let to = {line: lineNumber};
                    this._createTextMarkerForPropertyIfNeeded(from, to, property);
                    property.__refreshedAfterBlur = true;
                    break;
                }
            }.bind(this));

            // Look for swatchable values and make inline swatches.
            this._createInlineSwatches(true);

            // Restore the cursor position/selection.
            this._codeMirror.setSelection(selectionAnchor, selectionHead);

            // Reset undo history since undo past the reset is wrong when the content was empty before
            // or the content was representing a previous style object.
            this._codeMirror.clearHistory();

            // Mark the editor as clean (unedited state).
            this._codeMirror.markClean();

            this._markLinesWithCheckboxPlaceholder();
        }

        // This needs to be done first and as a separate operation to avoid an exception in CodeMirror.
        this._clearTextMarkers(false, true);

        this._ignoreCodeMirrorContentDidChangeEvent = true;
        this._codeMirror.operation(update.bind(this));
        this._ignoreCodeMirrorContentDidChangeEvent = false;
    }

    _updateJumpToSymbolTrackingMode()
    {
        var oldJumpToSymbolTrackingModeEnabled = this._jumpToSymbolTrackingModeEnabled;

        if (!this._style || !this._style.ownerRule || !this._style.ownerRule.sourceCodeLocation)
            this._jumpToSymbolTrackingModeEnabled = false;
        else
            this._jumpToSymbolTrackingModeEnabled = WebInspector.modifierKeys.altKey && !WebInspector.modifierKeys.metaKey && !WebInspector.modifierKeys.shiftKey;

        if (oldJumpToSymbolTrackingModeEnabled !== this._jumpToSymbolTrackingModeEnabled) {
            if (this._jumpToSymbolTrackingModeEnabled) {
                this._tokenTrackingController.highlightLastHoveredRange();
                this._tokenTrackingController.enabled = !this._codeMirror.getOption("readOnly");
            } else {
                this._tokenTrackingController.removeHighlightedRange();
                this._tokenTrackingController.enabled = false;
            }
        }
    }

    tokenTrackingControllerHighlightedRangeWasClicked(tokenTrackingController)
    {
        let sourceCodeLocation = this._style.ownerRule.sourceCodeLocation;
        console.assert(sourceCodeLocation);
        if (!sourceCodeLocation)
            return;

        let candidate = tokenTrackingController.candidate;
        console.assert(candidate);
        if (!candidate)
            return;

        let token = candidate.hoveredToken;

        // Special case command clicking url(...) links.
        if (token && /\blink\b/.test(token.type)) {
            let url = token.string;
            let baseURL = sourceCodeLocation.sourceCode.url;
            WebInspector.openURL(absoluteURL(url, baseURL));
            return;
        }

        function showRangeInSourceCode(sourceCode, range)
        {
            if (!sourceCode || !range)
                return false;

            WebInspector.showSourceCodeLocation(sourceCode.createSourceCodeLocation(range.startLine, range.startColumn));
            return true;
        }

        // Special case option clicking CSS variables.
        if (token && /\bvariable-2\b/.test(token.type)) {
            let property = this._style.nodeStyles.effectivePropertyForName(token.string);
            if (property && showRangeInSourceCode(property.ownerStyle.ownerRule.sourceCodeLocation.sourceCode, property.styleSheetTextRange))
                return;
        }

        // Jump to the rule if we can't find a property.
        // Find a better source code location from the property that was clicked.
        let marks = this._codeMirror.findMarksAt(candidate.hoveredTokenRange.start);
        for (let mark of marks) {
            let property = mark.__cssProperty;
            if (property && showRangeInSourceCode(sourceCodeLocation.sourceCode, property.styleSheetTextRange))
                return;
        }
    }

    tokenTrackingControllerNewHighlightCandidate(tokenTrackingController, candidate)
    {
        this._tokenTrackingController.highlightRange(candidate.hoveredTokenRange);
    }
};

WebInspector.CSSStyleDeclarationTextEditor.Event = {
    ContentChanged: "css-style-declaration-text-editor-content-changed",
    Blurred: "css-style-declaration-text-editor-blurred"
};

WebInspector.CSSStyleDeclarationTextEditor.PrefixWhitespace = "\n";
WebInspector.CSSStyleDeclarationTextEditor.SuffixWhitespace = "\n";
WebInspector.CSSStyleDeclarationTextEditor.StyleClassName = "css-style-text-editor";
WebInspector.CSSStyleDeclarationTextEditor.ReadOnlyStyleClassName = "read-only";
WebInspector.CSSStyleDeclarationTextEditor.CheckboxPlaceholderElementStyleClassName = "checkbox-placeholder";
WebInspector.CSSStyleDeclarationTextEditor.EditingLineStyleClassName = "editing-line";
WebInspector.CSSStyleDeclarationTextEditor.CommitCoalesceDelay = 250;
WebInspector.CSSStyleDeclarationTextEditor.RemoveEditingLineClassesDelay = 2000;
