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

WebInspector.ScriptFormatter = function(source)
{
    this._originalSource = source;
    this._originalLineEndings = source.findAll("\n");
    this._originalLineEndings.push(source.length);
}

WebInspector.ScriptFormatter.locationToPosition = function(lineEndings, location)
{
    var position = location.line ? lineEndings[location.line - 1] + 1 : 0;
    return position + location.column;
}

WebInspector.ScriptFormatter.positionToLocation = function(lineEndings, position)
{
    var location = {};
    location.line = lineEndings.upperBound(position - 1);
    if (!location.line)
        location.column = position;
    else
        location.column = position - lineEndings[location.line - 1] - 1;
    return location;
}

WebInspector.ScriptFormatter.prototype = {
    format: function(callback)
    {
        var worker = new Worker("scriptFormatterWorker.js");
        function messageHandler(event)
        {
            var formattedSource = event.data;
            this._formatted = true;
            this._formattedSource = formattedSource;
            this._formattedLineEndings = formattedSource.findAll("\n");
            this._formattedLineEndings.push(formattedSource.length);
            this._buildMapping();
            callback(formattedSource);
        }
        worker.onmessage = messageHandler.bind(this);
        worker.postMessage(this._originalSource);
    },

    _buildMapping: function()
    {
        this._originalSymbolPositions = [];
        this._formattedSymbolPositions = [];
        var lastCodePosition = 0;
        var regexp = /[\$\.\w]+|{|}|;/g;
        while (true) {
            var match = regexp.exec(this._formattedSource);
            if (!match)
                break;
            var position = this._originalSource.indexOf(match[0], lastCodePosition);
            if (position === -1)
                continue;
            this._originalSymbolPositions.push(position);
            this._formattedSymbolPositions.push(match.index);
            lastCodePosition = position + match[0].length;
        }
        this._originalSymbolPositions.push(this._originalSource.length);
        this._formattedSymbolPositions.push(this._formattedSource.length);
    },

    originalLineNumberToFormattedLineNumber: function(originalLineNumber)
    {
        if (!this._formatted)
            return originalLineNumber;
        var originalPosition = WebInspector.ScriptFormatter.locationToPosition(this._originalLineEndings, { line: originalLineNumber, column: 0 });
        return this.originalPositionToFormattedLineNumber(originalPosition);
    },

    formattedLineNumberToOriginalLineNumber: function(formattedLineNumber)
    {
        if (!this._formatted)
            return formattedLineNumber;
        var originalPosition = this.formattedLineNumberToOriginalPosition(formattedLineNumber);
        return WebInspector.ScriptFormatter.positionToLocation(this._originalLineEndings, originalPosition).line;
    },

    originalPositionToFormattedLineNumber: function(originalPosition)
    {
        var lineEndings = this._formatted ? this._formattedLineEndings : this._originalLineEndings;
        if (this._formatted)
            formattedPosition = this._convertPosition(this._originalSymbolPositions, this._formattedSymbolPositions, originalPosition);
        return WebInspector.ScriptFormatter.positionToLocation(lineEndings, formattedPosition).line;
    },

    formattedLineNumberToOriginalPosition: function(formattedLineNumber)
    {
        var lineEndings = this._formatted ? this._formattedLineEndings : this._originalLineEndings;
        var formattedPosition = WebInspector.ScriptFormatter.locationToPosition(lineEndings, { line: formattedLineNumber, column: 0 });
        if (!this._formatted)
            return formattedPosition;
        return this._convertPosition(this._formattedSymbolPositions, this._originalSymbolPositions, formattedPosition);
    },

    _convertPosition: function(symbolPositions1, symbolPositions2, position)
    {
        var index = symbolPositions1.upperBound(position);
        if (index === symbolPositions2.length - 1)
            return symbolPositions2[index] - 1;
        return symbolPositions2[index];
    }
}
