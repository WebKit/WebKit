/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

function createCodeMirrorTextMarkers(type, pattern, matchFunction, codeMirror, range, callback)
{
    var createdMarkers = [];
    var start = range instanceof WebInspector.TextRange ? range.startLine : 0;
    var end = range instanceof WebInspector.TextRange ? range.endLine + 1 : codeMirror.lineCount();
    for (var lineNumber = start; lineNumber < end; ++lineNumber) {
        var lineContent = codeMirror.getLine(lineNumber);
        var match = pattern.exec(lineContent);
        while (match) {
            if (typeof matchFunction === "function" && !matchFunction(lineContent, match)) {
                match = pattern.exec(lineContent);
                continue;
            }

            var from = {line: lineNumber, ch: match.index};
            var to = {line: lineNumber, ch: match.index + match[0].length};

            var foundMarker = false;
            var markers = codeMirror.findMarksAt(to);
            for (var j = 0; j < markers.length; ++j) {
                if (WebInspector.TextMarker.textMarkerForCodeMirrorTextMarker(markers[j]).type === WebInspector.TextMarker.Type[type]) {
                    foundMarker = true;
                    break;
                }
            }

            if (foundMarker) {
                match = pattern.exec(lineContent);
                continue;
            }

            // We're not interested if the value is not inside a keyword.
            var tokenType = codeMirror.getTokenTypeAt(from);
            if (tokenType && !tokenType.includes("keyword")) {
                match = pattern.exec(lineContent);
                continue;
            }

            var valueString = match[0];
            var value = WebInspector[type].fromString(valueString);
            if (!value) {
                match = pattern.exec(lineContent);
                continue;
            }

            var marker = codeMirror.markText(from, to);
            marker = new WebInspector.TextMarker(marker, WebInspector.TextMarker.Type[type]);
            createdMarkers.push(marker);

            if (typeof callback === "function")
                callback(marker, value, valueString);

            match = pattern.exec(lineContent);
        }
    }

    return createdMarkers;
}

function createCodeMirrorColorTextMarkers(codeMirror, range, callback)
{
    // Matches rgba(0, 0, 0, 0.5), rgb(0, 0, 0), hsl(), hsla(), #fff, #ffff, #ffffff, #ffffffff, white
    let colorRegex = /((?:rgb|hsl)a?\([^)]+\)|#[0-9a-fA-F]{8}|#[0-9a-fA-F]{6}|#[0-9a-fA-F]{3,4}|\b\w+\b(?![-.]))/g;
    function matchFunction(lineContent, match) {
        if (!lineContent || !lineContent.length)
            return false;

        // In order determine if the matched color is inside a gradient, first
        // look before the text to find the first unmatched open parenthesis.
        // This parenthesis, if it exists, will be immediately after the CSS
        // funciton whose name can be checked to see if it matches a gradient.
        let openParenthesis = 0;
        let index = match.index;
        let c = null;
        while (c = lineContent[index]) {
            if (c === "(")
                ++openParenthesis;
            if (c === ")")
                --openParenthesis;

            if (openParenthesis > 0)
                break;

            --index;
            if (index < 0)
                break;
        }

        if (/(repeating-)?(linear|radial)-gradient$/.test(lineContent.substring(0, index)))
            return false;

        // Act as a negative look-behind and disallow the color from being prefixing with certain characters.
        return !(match.index > 0 && /[-.\"\']/.test(lineContent[match.index - 1]));
    }

    return createCodeMirrorTextMarkers("Color", colorRegex, matchFunction, codeMirror, range, callback);
}

function createCodeMirrorGradientTextMarkers(codeMirror, range, callback)
{
    var createdMarkers = [];

    var start = range instanceof WebInspector.TextRange ? range.startLine : 0;
    var end = range instanceof WebInspector.TextRange ? range.endLine + 1 : codeMirror.lineCount();

    var gradientRegex = /(repeating-)?(linear|radial)-gradient\s*\(\s*/g;

    for (var lineNumber = start; lineNumber < end; ++lineNumber) {
        var lineContent = codeMirror.getLine(lineNumber);
        var match = gradientRegex.exec(lineContent);
        while (match) {
            var startLine = lineNumber;
            var startChar = match.index;
            var endChar = match.index + match[0].length;

            var openParentheses = 0;
            var c = null;
            while (c = lineContent[endChar]) {
                if (c === "(")
                    openParentheses++;
                if (c === ")")
                    openParentheses--;

                if (openParentheses === -1) {
                    endChar++;
                    break;
                }

                endChar++;
                if (endChar >= lineContent.length) {
                    lineNumber++;
                    endChar = 0;
                    lineContent = codeMirror.getLine(lineNumber);
                    if (!lineContent)
                        break;
                }
            }

            if (openParentheses !== -1) {
                match = gradientRegex.exec(lineContent);
                continue;
            }

            var from = {line: startLine, ch: startChar};
            var to = {line: lineNumber, ch: endChar};

            var gradientString = codeMirror.getRange(from, to);
            var gradient = WebInspector.Gradient.fromString(gradientString);
            if (!gradient) {
                match = gradientRegex.exec(lineContent);
                continue;
            }

            var marker = new WebInspector.TextMarker(codeMirror.markText(from, to), WebInspector.TextMarker.Type.Gradient);

            createdMarkers.push(marker);

            if (typeof callback === "function")
                callback(marker, gradient, gradientString);

            match = gradientRegex.exec(lineContent);
        }
    }

    return createdMarkers;
}

function createCodeMirrorCubicBezierTextMarkers(codeMirror, range, callback)
{
    var cubicBezierRegex = /(cubic-bezier\([^)]+\)|\b\w+\b(?:-\b\w+\b){0,2})/g;
    return createCodeMirrorTextMarkers("CubicBezier", cubicBezierRegex, null, codeMirror, range, callback);
}

function createCodeMirrorSpringTextMarkers(codeMirror, range, callback)
{
    const springRegex = /(spring\([^)]+\))/g;
    return createCodeMirrorTextMarkers("Spring", springRegex, null, codeMirror, range, callback);
}
