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

WebInspector.BasicBlockAnnotator = class BasicBlockAnnotator extends WebInspector.Annotator
{
    constructor(sourceCodeTextEditor, script)
    {
        super(sourceCodeTextEditor);

        this._script = script;
        this._basicBlockMarkers = new Map; // Only contains unexecuted basic blocks.
    }

    // Protected

    clearAnnotations()
    {
        for (var key of this._basicBlockMarkers.keys())
            this._clearRangeForBasicBlockMarker(key);
    }

    insertAnnotations()
    {
        if (!this.isActive())
            return;
        this._annotateBasicBlockExecutionRanges();
    }

    // Private

    _annotateBasicBlockExecutionRanges()
    {
        var sourceID = this._script.id;
        var startTime = Date.now();

        this._script.target.RuntimeAgent.getBasicBlocks(sourceID, function(error, basicBlocks) {
            if (error) {
                console.error("Error in getting basic block locations: " + error);
                return;
            }

            if (!this.isActive())
                return;

            var {startOffset, endOffset} = this.sourceCodeTextEditor.visibleRangeOffsets();
            basicBlocks = basicBlocks.filter(function(block) {
                // Viewport: [--]
                // Block:         [--]
                if (block.startOffset > endOffset)
                    return false;

                // Viewport:      [--]
                // Block:    [--]
                if (block.endOffset < startOffset)
                    return false;

                return true;
            });

            for (var block of basicBlocks) {
                var key = block.startOffset + ":" + block.endOffset;
                var hasKey = this._basicBlockMarkers.has(key);
                var hasExecuted = block.hasExecuted;
                if (hasKey && hasExecuted)
                    this._clearRangeForBasicBlockMarker(key);
                else if (!hasKey && !hasExecuted) {
                    var marker = this._highlightTextForBasicBlock(block);
                    this._basicBlockMarkers.set(key, marker);
                }
            }

            var totalTime = Date.now() - startTime;
            var timeoutTime = Number.constrain(30 * totalTime, 500, 5000);
            this._timeoutIdentifier = setTimeout(this.insertAnnotations.bind(this), timeoutTime);
        }.bind(this));
    }

    _highlightTextForBasicBlock(basicBlock)
    {
        console.assert(basicBlock.startOffset <= basicBlock.endOffset && basicBlock.startOffset >= 0 && basicBlock.endOffset >= 0, "" + basicBlock.startOffset + ":" + basicBlock.endOffset);
        console.assert(!basicBlock.hasExecuted);

        var startPosition = this.sourceCodeTextEditor.originalOffsetToCurrentPosition(basicBlock.startOffset);
        var endPosition = this.sourceCodeTextEditor.originalOffsetToCurrentPosition(basicBlock.endOffset);
        if (this._isTextRangeOnlyClosingBrace(startPosition, endPosition))
            return null;

        var marker = this.sourceCodeTextEditor.addStyleToTextRange(startPosition, endPosition, WebInspector.BasicBlockAnnotator.HasNotExecutedClassName);
        return marker;
    }

    _isTextRangeOnlyClosingBrace(startPosition, endPosition)
    {
        var isOnlyClosingBrace = /^\s*\}$/;
        return isOnlyClosingBrace.test(this.sourceCodeTextEditor.getTextInRange(startPosition, endPosition));
    }

    _clearRangeForBasicBlockMarker(key)
    {
        console.assert(this._basicBlockMarkers.has(key));
        var marker = this._basicBlockMarkers.get(key);
        if (marker)
            marker.clear();
        this._basicBlockMarkers.delete(key);
    }
};

WebInspector.BasicBlockAnnotator.HasNotExecutedClassName = "basic-block-has-not-executed";
