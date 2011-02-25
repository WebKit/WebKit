/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.TextEditorHighlighter = function(textModel, damageCallback)
{
    this._textModel = textModel;
    this._tokenizer = WebInspector.SourceTokenizer.Registry.getInstance().getTokenizer("text/html");
    this._damageCallback = damageCallback;
    this._highlightChunkLimit = 1000;
    this.reset();
}

WebInspector.TextEditorHighlighter.prototype = {
    set mimeType(mimeType)
    {
        var tokenizer = WebInspector.SourceTokenizer.Registry.getInstance().getTokenizer(mimeType);
        if (tokenizer) {
            this._tokenizer = tokenizer;
            this._tokenizerConditionStringified = JSON.stringify(this._tokenizer.initialCondition);
        }
    },

    set highlightChunkLimit(highlightChunkLimit)
    {
        this._highlightChunkLimit = highlightChunkLimit;
    },

    reset: function()
    {
        this._lastHighlightedLine = 0;
        this._lastHighlightedColumn = 0;
        this._tokenizerConditionStringified = JSON.stringify(this._tokenizer.initialCondition);
    },

    highlight: function(endLine, opt_forceRun)
    {
        // First check if we have work to do.
        if (endLine <= this._lastHighlightedLine)
            return;

        this._requestedEndLine = endLine;

        if (this._highlightTimer && !opt_forceRun) {
            // There is a timer scheduled, it will catch the new job based on the new endLine set.
            return;
        }

        // Do small highlight synchronously. This will provide instant highlight on PageUp / PageDown, gentle scrolling.
        this._highlightInChunks(endLine);

        // Schedule tail highlight if necessary.
        if (this._lastHighlightedLine < endLine)
            this._highlightTimer = setTimeout(this._highlightInChunks.bind(this, endLine), 100);
    },

    updateHighlight: function(startLine, endLine)
    {
        if (this._lastHighlightedLine < startLine) {
            // Highlighter did not reach this point yet, nothing to update. It will reach it on subsequent timer tick and do the job.
            return false;
        }

        var savedLastHighlightedLine = this._lastHighlightedLine;
        var savedLastHighlightedColumn = this._lastHighlightedColumn;
        var savedTokenizerCondition = this._tokenizerConditionStringified;

        this._lastHighlightedLine = startLine;
        this._lastHighlightedColumn = 0;

        // Restore highlighter context taken from the previous line.
        var attributes = this._textModel.getAttribute(startLine - 1, "highlight") || {};
        this._tokenizerConditionStringified = attributes.postConditionStringified || JSON.stringify(this._tokenizer.initialCondition);

        // Try to update highlight synchronously.
        this._highlightLines(endLine);

        if (this._lastHighlightedLine >= this._textModel.linesCount) {
            // All is done up to the end.
            return true;
        }

        var attributes1 = this._textModel.getAttribute(this._lastHighlightedLine - 1, "highlight") || {};
        var attributes2 = this._textModel.getAttribute(this._lastHighlightedLine, "highlight") || {};
        if (this._lastHighlightedColumn === 0 && attributes2.preConditionStringified && attributes1.postConditionStringified === attributes2.preConditionStringified) {
            // Highlighting ended ahead of time. Restore previously saved state, unless we have done more than it was before.
            if (savedLastHighlightedLine >= this._lastHighlightedLine) {
                this._lastHighlightedLine = savedLastHighlightedLine;
                this._lastHighlightedColumn = savedLastHighlightedColumn;
                this._tokenizerConditionStringified = savedTokenizerCondition;
            }
            return true;
        } else {
            // If failed to update highlight synchronously, invalidate highlight data for the subsequent lines.
            if (this._lastHighlightedColumn === 0)
                this._textModel.removeAttribute(this._lastHighlightedLine, "highlight");
            for (var i = this._lastHighlightedLine + 1; i < this._textModel.linesCount; ++i)
                this._textModel.removeAttribute(i, "highlight");

            // Continue highlighting on subsequent timer ticks.
            this._requestedEndLine = endLine;
            if (!this._highlightTimer)
                this._highlightTimer = setTimeout(this._highlightInChunks.bind(this, endLine), 100);

            return false;
        }
    },

    _highlightInChunks: function(endLine)
    {
        delete this._highlightTimer;

        // First we always check if we have work to do. Could be that user scrolled back and we can quit.
        if (this._requestedEndLine <= this._lastHighlightedLine)
            return;

        if (this._requestedEndLine !== endLine) {
            // User keeps updating the job in between of our timer ticks. Just reschedule self, don't eat CPU (they must be scrolling).
            this._highlightTimer = setTimeout(this._highlightInChunks.bind(this, this._requestedEndLine), 100);
            return;
        }

        // The textModel may have been already updated.
        if (this._requestedEndLine > this._textModel.linesCount)
            this._requestedEndLine = this._textModel.linesCount;

        this._highlightLines(this._requestedEndLine);

        // Schedule tail highlight if necessary.
        if (this._lastHighlightedLine < this._requestedEndLine)
            this._highlightTimer = setTimeout(this._highlightInChunks.bind(this, this._requestedEndLine), 10);
    },

    _highlightLines: function(endLine)
    {
        // Tokenizer is stateless and reused across viewers, restore its condition before highlight and save it after.
        this._tokenizer.condition = JSON.parse(this._tokenizerConditionStringified);
        var tokensCount = 0;
        for (var lineNumber = this._lastHighlightedLine; lineNumber < endLine; ++lineNumber) {
            var line = this._textModel.line(lineNumber);
            this._tokenizer.line = line;

            if (this._lastHighlightedColumn === 0) {
                var attributes = {};
                attributes.preConditionStringified = JSON.stringify(this._tokenizer.condition);
                this._textModel.setAttribute(lineNumber, "highlight", attributes);
            } else
                var attributes = this._textModel.getAttribute(lineNumber, "highlight");

            // Highlight line.
            do {
                var newColumn = this._tokenizer.nextToken(this._lastHighlightedColumn);
                var tokenType = this._tokenizer.tokenType;
                if (tokenType)
                    attributes[this._lastHighlightedColumn] = { length: newColumn - this._lastHighlightedColumn, tokenType: tokenType };
                this._lastHighlightedColumn = newColumn;
                if (++tokensCount > this._highlightChunkLimit)
                    break;
            } while (this._lastHighlightedColumn < line.length);

            if (this._lastHighlightedColumn < line.length) {
                // Too much work for single chunk - exit.
                break;
            } else {
                this._lastHighlightedColumn = 0;
                attributes.postConditionStringified = JSON.stringify(this._tokenizer.condition);

                var nextAttributes = this._textModel.getAttribute(lineNumber + 1, "highlight") || {};
                if (nextAttributes.preConditionStringified === attributes.postConditionStringified) {
                    // Following lines are up to date, no need to re-highlight.
                    ++lineNumber;
                    break;
                }
            }
        }

        this._damageCallback(this._lastHighlightedLine, lineNumber);
        this._tokenizerConditionStringified = JSON.stringify(this._tokenizer.condition);
        this._lastHighlightedLine = lineNumber;
    }
}
