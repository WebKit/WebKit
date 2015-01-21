/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Saam Barati <saambarati1@gmail.com>
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

WebInspector.BasicBlockAnnotator = function(sourceCodeTextEditor, script)
{
    WebInspector.Annotator.call(this, sourceCodeTextEditor);

    this._script = script;
    this._basicBlockMarkers = new Map; // Only contains unexecuted basic blocks.
};

WebInspector.BasicBlockAnnotator.HasNotExecutedClassName = "basic-block-has-not-executed";
WebInspector.BasicBlockAnnotator.HasNotExecutedPrependClassName = "basic-block-has-not-executed-prepend";

WebInspector.BasicBlockAnnotator.prototype = {
    constructor: WebInspector.BasicBlockAnnotator,
    __proto__: WebInspector.Annotator.prototype,

    // Protected

    clearAnnotations: function()
    {
        for (var key of this._basicBlockMarkers.keys())
            this._clearRangeForBasicBlockMarker(key);
    },

    insertAnnotations: function()
    {
        if (!this._isActive)
            return;
        this._annotateBasicBlockExecutionRanges();
    },

    // Private

    _annotateBasicBlockExecutionRanges: function()
    {
        var sourceID = this._script.id;
        var startTime = Date.now();

        RuntimeAgent.getBasicBlocks(sourceID, function(error, basicBlocks) {
            if (error) {
                console.error("Error in getting basic block locations: " + error);
                return;
            }

            if (!this._isActive)
                return;

            var {startOffset, endOffset} = this.sourceCodeTextEditor.visibleRangeOffsets();
            basicBlocks = basicBlocks.filter(function(block) {
                return block.startOffset >= startOffset && block.startOffset <= endOffset;
            });

            for (var block of basicBlocks) {
                var key = block.startOffset + ":" + block.endOffset;
                var hasKey = this._basicBlockMarkers.has(key);
                var hasExecuted = block.hasExecuted;
                if (hasKey && hasExecuted)
                    this._clearRangeForBasicBlockMarker(key);
                else if (!hasKey && !hasExecuted) {
                    var basicBlockMarker = this._highlightTextForBasicBlock(block);
                    if (basicBlockMarker)
                        this._basicBlockMarkers.set(key, basicBlockMarker);
                }
            }

            var totalTime = Date.now() - startTime;
            var timeoutTime = Math.min(Math.max(6500, totalTime), 15 * totalTime);
            this._timeoutIdentifier = setTimeout(this.insertAnnotations.bind(this), timeoutTime);
        }.bind(this));
    },

    _highlightTextForBasicBlock: function(basicBlock)
    {
        console.assert(basicBlock.startOffset <= basicBlock.endOffset && basicBlock.startOffset >= 0 && basicBlock.endOffset >= 0, "" + basicBlock.startOffset + ":" + basicBlock.endOffset);
        console.assert(!basicBlock.hasExecuted);

        var startPosition = this.sourceCodeTextEditor.originalOffsetToCurrentPosition(basicBlock.startOffset);
        var endPosition = this.sourceCodeTextEditor.originalOffsetToCurrentPosition(basicBlock.endOffset);

        if (this._isTextRangeOnlyWhitespace(startPosition, endPosition) || this._isTextRangeOnlyClosingBrace(startPosition, endPosition))
            return null;

        // Gray out the text range.
        var marker = this.sourceCodeTextEditor.addStyleToTextRange(startPosition, endPosition, WebInspector.BasicBlockAnnotator.HasNotExecutedClassName);

        // When adding a style to the code range in CodeMirror, such as a background color, it will only apply the style
        // to the area where text is, not to the style of an entire line. But, from a user interface perspective, it looks
        // cleaner to have entire lines highlighted. The following logic grays out entire lines when the line's range is
        // not across basic block boundaries.
        var lineStyles = [];
        if (endPosition.line > startPosition.line) {
            var startLineEndPosition = {line: startPosition.line, ch: this.sourceCodeTextEditor.line(startPosition.line).length};
            if (this._canGrayOutEntireLine(startPosition.line, startPosition, startLineEndPosition)) {
                this._grayOutLine(startPosition.line);
                lineStyles.push({line: startPosition.line, className: WebInspector.BasicBlockAnnotator.HasNotExecutedClassName});
            }

            var endLineStartPosition = {line: endPosition.line, ch: 0};
             
            if (this._canGrayOutEntireLine(endPosition.line, endLineStartPosition, endPosition)) {
                this._grayOutLine(endPosition.line);
                lineStyles.push({line: endPosition.line, className: WebInspector.BasicBlockAnnotator.HasNotExecutedClassName});
            } else {
                this.sourceCodeTextEditor.addStyleClassToLine(endPosition.line, WebInspector.BasicBlockAnnotator.HasNotExecutedPrependClassName);
                lineStyles.push({line: endPosition.line, className: WebInspector.BasicBlockAnnotator.HasNotExecutedPrependClassName});
            }
        } else {
            console.assert(endPosition.line === startPosition.line);
            if (this._canGrayOutEntireLine(startPosition.line, startPosition, endPosition)) {
                this._grayOutLine(startPosition.line);
                lineStyles.push({line: startPosition.line, className: WebInspector.BasicBlockAnnotator.HasNotExecutedClassName});
            } else if (startPosition.ch === 0) {
                this.sourceCodeTextEditor.addStyleClassToLine(endPosition.line, WebInspector.BasicBlockAnnotator.HasNotExecutedPrependClassName);
                lineStyles.push({line: startPosition.line, className: WebInspector.BasicBlockAnnotator.HasNotExecutedPrependClassName});
            }
        }

        if (endPosition.line - startPosition.line >= 2) {
            // There is at least one entire line to be grayed out between the start and end position of the basic block:
            // n  : ...
            // n+1: ... (This entire line can safely be grayed out).
            // n+2: ...
            for (var lineNumber = startPosition.line + 1; lineNumber < endPosition.line; lineNumber++) {
                this._grayOutLine(lineNumber);
                lineStyles.push({line: lineNumber, className: WebInspector.BasicBlockAnnotator.HasNotExecutedClassName});
            }
        }

        return {marker: marker, lineStyles: lineStyles};
    },

    _isTextRangeOnlyWhitespace: function(startPosition, endPosition)
    {
        var isOnlyWhitespace = /^\s+$/;
        return isOnlyWhitespace.test(this.sourceCodeTextEditor.getTextInRange(startPosition, endPosition));
    },

    _isTextRangeOnlyClosingBrace: function(startPosition, endPosition)
    {
        var isOnlyClosingBrace = /^\s*\}$/;
        return isOnlyClosingBrace.test(this.sourceCodeTextEditor.getTextInRange(startPosition, endPosition));
    },

    _canGrayOutEntireLine: function(lineNumber, startPosition, endPosition)
    {
        console.assert(startPosition.line === endPosition.line);
        var lineText = this.sourceCodeTextEditor.line(lineNumber);
        var blockText = this.sourceCodeTextEditor.getTextInRange(startPosition, endPosition);
        return lineText.trim() === blockText.trim();
    },

    _grayOutLine: function(lineNumber)
    {
        var className = WebInspector.BasicBlockAnnotator.HasNotExecutedClassName;
        this.sourceCodeTextEditor.addStyleClassToLine(lineNumber, className);
    },

    _clearRangeForBasicBlockMarker: function(key)
    {
        console.assert(this._basicBlockMarkers.has(key));
        var {marker, lineStyles} = this._basicBlockMarkers.get(key);
        marker.clear();
        for (var {line, className} of lineStyles)
            this.sourceCodeTextEditor.removeStyleClassFromLine(line, className);
        this._basicBlockMarkers.delete(key);
    }
};
