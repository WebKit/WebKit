/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

WebInspector.SourceFrameContent = function(text, scripts)
{
    if (scripts.length && scripts[0].length < text.length) {
        // WebKit html lexer normalizes line endings and scripts are passed to VM with "\n" line endings.
        // However, resource content has original line endings, so we have to normalize line endings here.
        text = text.replace(/\r\n/g, "\n");
    }
    this._text = text;
    this._lineEndings = text.findAll("\n");
    this._lineEndings.push(text.length);

    this._scriptRanges = [];
    for (var i = 0; i < scripts.length; ++i) {
        var script = scripts[i];
        var offset = this.locationToPosition(script.lineOffset, script.columnOffset);
        this._scriptRanges.push({ start: offset, end: offset + script.length, script: script });
    }
    this._scriptRanges.sort(function(x, y) { return x.start - y.start; });
}

WebInspector.SourceFrameContent.prototype = {
    get text()
    {
        return this._text;
    },

    get scriptRanges()
    {
        return this._scriptRanges;
    },

    locationToPosition: function(lineNumber, columnNumber)
    {
        var position = lineNumber ? this._lineEndings[lineNumber - 1] + 1 : 0;
        return position + columnNumber;
    },

    positionToLocation: function(position)
    {
        var location = {};
        location.lineNumber = this._lineEndings.upperBound(position - 1);
        if (!location.lineNumber)
            location.columnNumber = position;
        else
            location.columnNumber = position - this._lineEndings[location.lineNumber - 1] - 1;
        return location;
    },

    scriptLocationForLineNumber: function(lineNumber)
    {
        var range = this.lineNumberToRange(lineNumber);
        return this.scriptLocationForRange(range.start, range.end);
    },

    scriptLocationForRange: function(start, end)
    {
        var position = start;
        var scriptRange = this._intersectingScriptRange(start, end);
        if (scriptRange)
            position = Math.max(position, scriptRange.start);
        var scriptLocation = this.positionToLocation(position);
        if (scriptRange)
            scriptLocation.sourceID = scriptRange.script.sourceID;
        return scriptLocation;
    },

    lineNumberToRange: function(lineNumber)
    {
        var previousLineEnd = this._lineEndings[lineNumber - 1] || 0;
        var lineEnd = this._lineEndings[lineNumber];
        return { start: previousLineEnd + 1, end: lineEnd };
    },

    _intersectingScriptRange: function(start, end)
    {
        for (var i = 0; i < this._scriptRanges.length; ++i) {
            var scriptRange = this._scriptRanges[i];
            if (start < scriptRange.end && end > scriptRange.start)
                return scriptRange;
        }
    }
}


WebInspector.FormattedSourceFrameContent = function(originalContent, text, mapping)
{
    this._originalContent = originalContent;
    this._formattedContent = new WebInspector.SourceFrameContent(text, []);
    this._mapping = mapping;
}

WebInspector.FormattedSourceFrameContent.prototype = {
    get text()
    {
        return this._formattedContent.text;
    },

    originalLocationToFormattedLocation: function(lineNumber, columnNumber)
    {
        var originalPosition = this._originalContent.locationToPosition(lineNumber, columnNumber);
        var formattedPosition = this._convertPosition(this._mapping.original, this._mapping.formatted, originalPosition);
        return this._formattedContent.positionToLocation(formattedPosition);
    },

    scriptLocationForFormattedLineNumber: function(lineNumber)
    {
        var range = this._formattedContent.lineNumberToRange(lineNumber);
        var start = this._convertPosition(this._mapping.formatted, this._mapping.original, range.start);
        var end = this._convertPosition(this._mapping.formatted, this._mapping.original, range.end);
        return this._originalContent.scriptLocationForRange(start, end);
    },

    _convertPosition: function(positions1, positions2, position)
    {
        var index = positions1.upperBound(position);
        var range1 = positions1[index] - positions1[index - 1];
        var range2 = positions2[index] - positions2[index - 1];
        var position2 = positions2[index - 1];
        if (range1)
            position2 += Math.round((position - positions1[index - 1]) * range2 / range1);
        return position2;
    }
}
