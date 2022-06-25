/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WI.FormatterSourceMap = class FormatterSourceMap extends WI.Object
{
    constructor(originalLineEndings, formattedLineEndings, mapping)
    {
        super();

        this._originalLineEndings = originalLineEndings;
        this._formattedLineEndings = formattedLineEndings;
        this._mapping = mapping;
    }

    // Static

    static fromSourceMapData({originalLineEndings, formattedLineEndings, mapping})
    {
        return new WI.FormatterSourceMap(originalLineEndings, formattedLineEndings, mapping);
    }

    // Public

    originalToFormatted(lineNumber, columnNumber)
    {
        var originalPosition = this._locationToPosition(this._originalLineEndings, lineNumber || 0, columnNumber || 0);
        return this.originalPositionToFormatted(originalPosition);
    }

    originalPositionToFormatted(originalPosition)
    {
        var formattedPosition = this._convertPosition(this._mapping.original, this._mapping.formatted, originalPosition);
        return this._positionToLocation(this._formattedLineEndings, formattedPosition);
    }

    originalPositionToFormattedPosition(originalPosition)
    {
        return this._convertPosition(this._mapping.original, this._mapping.formatted, originalPosition);
    }

    formattedToOriginal(lineNumber, columnNumber)
    {
        var originalPosition = this.formattedToOriginalOffset(lineNumber, columnNumber);
        return this._positionToLocation(this._originalLineEndings, originalPosition);
    }

    formattedToOriginalOffset(lineNumber, columnNumber)
    {
        var formattedPosition = this._locationToPosition(this._formattedLineEndings, lineNumber || 0, columnNumber || 0);
        var originalPosition = this._convertPosition(this._mapping.formatted, this._mapping.original, formattedPosition);
        return originalPosition;
    }

    formattedPositionToOriginalPosition(formattedPosition)
    {
        return this._convertPosition(this._mapping.formatted, this._mapping.original, formattedPosition);
    }

    // Private

    _locationToPosition(lineEndings, lineNumber, columnNumber)
    {
        var lineOffset = lineNumber ? lineEndings[lineNumber - 1] + 1 : 0;
        return lineOffset + columnNumber;
    }

    _positionToLocation(lineEndings, position)
    {
        var lineNumber = lineEndings.upperBound(position - 1);
        if (!lineNumber)
            var columnNumber = position;
        else
            var columnNumber = position - lineEndings[lineNumber - 1] - 1;
        return {lineNumber, columnNumber};
    }

    _convertPosition(positions1, positions2, positionInPosition1)
    {
        var index = positions1.upperBound(positionInPosition1) - 1;
        var convertedPosition = positions2[index] + positionInPosition1 - positions1[index];
        if (index < positions2.length - 1 && convertedPosition > positions2[index + 1])
            convertedPosition = positions2[index + 1];
        return convertedPosition;
    }
};
