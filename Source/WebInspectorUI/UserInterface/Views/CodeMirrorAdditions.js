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

(function () {
    // By default CodeMirror defines syntax highlighting styles based on token
    // only and shared styles between modes. This limiting and does not match
    // what we have done in the Web Inspector. So this modifies the XML, CSS
    // and JavaScript modes to supply two styles for each token. One for the
    // token and one with the mode name.

    function tokenizeLinkString(stream, state)
    {
        console.assert(state._linkQuoteCharacter !== undefined);

        // Eat the string until the same quote is found that started the string.
        // If this is unquoted, then eat until whitespace or common parse errors.
        if (state._linkQuoteCharacter)
            stream.eatWhile(new RegExp("[^" + state._linkQuoteCharacter + "]"));
        else
            stream.eatWhile(/[^\s\u00a0=<>\"\']/);

        // If the stream isn't at the end of line then we found the end quote.
        // In the case, change _linkTokenize to parse the end of the link next.
        // Otherwise _linkTokenize will stay as-is to parse more of the link.
        if (!stream.eol())
            state._linkTokenize = tokenizeEndOfLinkString;

        return "link";
    }

    function tokenizeEndOfLinkString(stream, state)
    {
        console.assert(state._linkQuoteCharacter !== undefined);
        console.assert(state._linkBaseStyle);

        // Eat the quote character to style it with the base style.
        if (state._linkQuoteCharacter)
            stream.eat(state._linkQuoteCharacter);

        var style = state._linkBaseStyle;

        // Clean up the state.
        delete state._linkTokenize;
        delete state._linkQuoteCharacter;
        delete state._linkBaseStyle;

        return style;
    }

    function extendedXMLToken(stream, state)
    {
        if (state._linkTokenize) {
            // Call the link tokenizer instead.
            var style = state._linkTokenize(stream, state);
            return style && (style + " m-" + this.name);
        }

        // Remember the start position so we can rewind if needed.
        var startPosition = stream.pos;
        var style = this._token(stream, state);
        if (style === "attribute") {
            // Look for "href" or "src" attributes. If found then we should
            // expect a string later that should get the "link" style instead.
            var text = stream.current().toLowerCase();
            if (text === "href" || text === "src")
                state._expectLink = true;
            else
                delete state._expectLink;
        } else if (state._expectLink && style === "string") {
            var current = stream.current();

            // Unless current token is empty quotes, consume quote character
            // and tokenize link next.
            if (current !== "\"\"" && current !== "''") {
                delete state._expectLink;

                // This is a link, so setup the state to process it next.
                state._linkTokenize = tokenizeLinkString;
                state._linkBaseStyle = style;

                // The attribute may or may not be quoted.
                var quote = current[0];

                state._linkQuoteCharacter = quote === "'" || quote === "\"" ? quote : null;

                // Rewind the steam to the start of this token.
                stream.pos = startPosition;

                // Eat the open quote of the string so the string style
                // will be used for the quote character.
                if (state._linkQuoteCharacter)
                    stream.eat(state._linkQuoteCharacter);
            }
        } else if (style) {
            // We don't expect other tokens between attribute and string since
            // spaces and the equal character are not tokenized. So if we get
            // another token before a string then we stop expecting a link.
            delete state._expectLink;
        }

        return style && (style + " m-" + this.name);
    }

    function tokenizeCSSURLString(stream, state)
    {
        console.assert(state._urlQuoteCharacter);

        // If we are an unquoted url string, return whitespace blocks as a whitespace token (null).
        if (state._unquotedURLString && stream.eatSpace())
            return null;

        var ch = null;
        var escaped = false;
        var reachedEndOfURL = false;
        var lastNonWhitespace = stream.pos;
        var quote = state._urlQuoteCharacter;

        // Parse characters until the end of the stream/line or a proper end quote character.
        while ((ch = stream.next()) != null) {
            if (ch == quote && !escaped) {
                reachedEndOfURL = true;
                break;
            }
            escaped = !escaped && ch === "\\";
            if (!/[\s\u00a0]/.test(ch))
                lastNonWhitespace = stream.pos;
        }

        // If we are an unquoted url string, do not include trailing whitespace, rewind to the last real character.
        if (state._unquotedURLString)
            stream.pos = lastNonWhitespace;

        // If we have reached the proper the end of the url string, switch to the end tokenizer to reset the state.
        if (reachedEndOfURL) {
            if (!state._unquotedURLString)
                stream.backUp(1);
            this._urlTokenize = tokenizeEndOfCSSURLString;
        }

        return "link";
    }

    function tokenizeEndOfCSSURLString(stream, state)
    {
        console.assert(state._urlQuoteCharacter);
        console.assert(state._urlBaseStyle);

        // Eat the quote character to style it with the base style.
        if (!state._unquotedURLString)
            stream.eat(state._urlQuoteCharacter);

        var style = state._urlBaseStyle;

        delete state._urlTokenize;
        delete state._urlQuoteCharacter;
        delete state._urlBaseStyle;

        return style;
    }

    function extendedCSSToken(stream, state)
    {
        const hexColorRegex = /#(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b/g;

        if (state._urlTokenize) {
            // Call the link tokenizer instead.
            var style = state._urlTokenize(stream, state);
            return style && (style + " m-" + (this.alternateName || this.name));
        }

        // Remember the start position so we can rewind if needed.
        var startPosition = stream.pos;
        var style = this._token(stream, state);

        if (style) {
            if (style === "atom") {
                if (stream.current() === "url") {
                    // If the current text is "url" then we should expect the next string token to be a link.
                    state._expectLink = true;
                } else if (state._expectLink) {
                    // We expected a string and got it. This is a link. Parse it the way we want it.
                    delete state._expectLink;

                    // This is a link, so setup the state to process it next.
                    state._urlTokenize = tokenizeCSSURLString;
                    state._urlBaseStyle = style;

                    // The url may or may not be quoted.
                    var quote = stream.current()[0];
                    state._urlQuoteCharacter = quote === "'" || quote === "\"" ? quote : ")";
                    state._unquotedURLString = state._urlQuoteCharacter === ")";

                    // Rewind the steam to the start of this token.
                    stream.pos = startPosition;

                    // Eat the open quote of the string so the string style
                    // will be used for the quote character.
                    if (!state._unquotedURLString)
                        stream.eat(state._urlQuoteCharacter);
                } else if (hexColorRegex.test(stream.current()))
                    style = style + " hex-color";
            } else if (state._expectLink) {
                // We expected a string and didn't get one. Cleanup.
                delete state._expectLink;
            }
        }

        return style && (style + " m-" + (this.alternateName || this.name));
    }

    function extendedToken(stream, state)
    {
        // CodeMirror moves the original token function to _token when we extended it.
        // So call it to get the style that we will add an additional class name to.
        var style = this._token(stream, state);
        return style && (style + " m-" + (this.alternateName || this.name));
    }

    function extendedCSSRuleStartState(base)
    {
        // CodeMirror moves the original token function to _startState when we extended it.
        // So call it to get the original start state that we will modify.
        var state = this._startState(base);

        // Start off the state stack like it has already parsed a rule. This causes everything
        // after to be parsed as properties in a rule.
        state.state = "block";
        state.context.type = "block";

        return state;
    }

    function scrollCursorIntoView(codeMirror, event)
    {
        // We don't want to use the default implementation since it can cause massive jumping
        // when the editor is contained inside overflow elements.
        event.preventDefault();

        function delayedWork()
        {
            // Don't try to scroll unless the editor is focused.
            if (!codeMirror.getWrapperElement().classList.contains("CodeMirror-focused"))
                return;

            // The cursor element can contain multiple cursors. The first one is the blinky cursor,
            // which is the one we want to scroll into view. It can be missing, so check first.
            var cursorElement = codeMirror.getScrollerElement().getElementsByClassName("CodeMirror-cursor")[0];
            if (cursorElement)
                cursorElement.scrollIntoViewIfNeeded(false);
        }

        // We need to delay this because CodeMirror can fire scrollCursorIntoView as a view is being blurred
        // and another is being focused. The blurred editor still has the focused state when this event fires.
        // We don't want to scroll the blurred editor into view, only the focused editor.
        setTimeout(delayedWork, 0);
    }

    CodeMirror.extendMode("css", {token: extendedCSSToken});
    CodeMirror.extendMode("xml", {token: extendedXMLToken});
    CodeMirror.extendMode("javascript", {token: extendedToken});

    CodeMirror.defineMode("css-rule", CodeMirror.modes.css);
    CodeMirror.extendMode("css-rule", {token: extendedCSSToken, startState: extendedCSSRuleStartState, alternateName: "css"});

    CodeMirror.defineInitHook(function(codeMirror) {
        codeMirror.on("scrollCursorIntoView", scrollCursorIntoView);
    });

    CodeMirror.defineExtension("hasLineClass", function(line, where, className) {
        // This matches the arguments to addLineClass and removeLineClass.
        var classProperty = (where === "text" ? "textClass" : (where == "background" ? "bgClass" : "wrapClass"));
        var lineInfo = this.lineInfo(line);
        if (!lineInfo)
            return false;

        if (!lineInfo[classProperty])
            return false;

        // Test for the simple case.
        if (lineInfo[classProperty] === className)
            return true;

        // Do a quick check for the substring. This is faster than a regex, which requires escaping the input first.
        var index = lineInfo[classProperty].indexOf(className);
        if (index === -1)
            return false;

        // Check that it is surrounded by spaces. Add padding spaces first to work with beginning and end of string cases.
        var paddedClass = " " + lineInfo[classProperty] + " ";
        return paddedClass.indexOf(" " + className + " ", index) !== -1;
    });

    CodeMirror.defineExtension("setUniqueBookmark", function(position, options) {
        var marks = this.findMarksAt(position);
        for (var i = 0; i < marks.length; ++i) {
            if (marks[i].__uniqueBookmark) {
                marks[i].clear();
                break;
            }
        }

        var uniqueBookmark = this.setBookmark(position, options);
        uniqueBookmark.__uniqueBookmark = true;
        return uniqueBookmark;
    });

    CodeMirror.defineExtension("toggleLineClass", function(line, where, className) {
        if (this.hasLineClass(line, where, className)) {
            this.removeLineClass(line, where, className);
            return false;
        }

        this.addLineClass(line, where, className);
        return true;
    });

    CodeMirror.defineExtension("alterNumberInRange", function(amount, startPosition, endPosition, updateSelection) {
        // We don't try if the range is multiline, pass to another key handler.
        if (startPosition.line !== endPosition.line)
            return false;

        if (updateSelection) {
            // Remember the cursor position/selection.
            var selectionStart = this.getCursor("start");
            var selectionEnd = this.getCursor("end");
        }

        var line = this.getLine(startPosition.line);

        var foundPeriod = false;

        var start = NaN;
        var end = NaN;

        for (var i = startPosition.ch; i >= 0; --i) {
            var character = line.charAt(i);

            if (character === ".") {
                if (foundPeriod)
                    break;
                foundPeriod = true;
            } else if (character !== "-" && character !== "+" && isNaN(parseInt(character))) {
                // Found the end already, just scan backwards.
                if (i === startPosition.ch) {
                    end = i;
                    continue;
                }

                break;
            }

            start = i;
        }

        if (isNaN(end)) {
            for (var i = startPosition.ch + 1; i < line.length; ++i) {
                var character = line.charAt(i);

                if (character === ".") {
                    if (foundPeriod) {
                        end = i;
                        break;
                    }

                    foundPeriod = true;
                } else if (isNaN(parseInt(character))) {
                    end = i;
                    break;
                }

                end = i + 1;
            }
        }

        // No number range found, pass to another key handler.
        if (isNaN(start) || isNaN(end))
            return false;

        var number = parseFloat(line.substring(start, end));

        // Make the new number and constrain it to a precision of 6, this matches numbers the engine returns.
        // Use the Number constructor to forget the fixed precision, so 1.100000 will print as 1.1.
        var alteredNumber = Number((number + amount).toFixed(6));
        var alteredNumberString = alteredNumber.toString();

        var from = {line: startPosition.line, ch: start};
        var to = {line: startPosition.line, ch: end};

        this.replaceRange(alteredNumberString, from, to);

        if (updateSelection) {
            var previousLength = to.ch - from.ch;
            var newLength = alteredNumberString.length;

            // Fix up the selection so it follows the increase or decrease in the replacement length.
            if (previousLength != newLength) {
                if (selectionStart.line === from.line && selectionStart.ch > from.ch)
                    selectionStart.ch += newLength - previousLength;

                if (selectionEnd.line === from.line && selectionEnd.ch > from.ch)
                    selectionEnd.ch += newLength - previousLength;
            }

            this.setSelection(selectionStart, selectionEnd);
        }

        return true;
    });

    function alterNumber(amount, codeMirror)
    {
        function findNumberToken(position)
        {
            // CodeMirror includes the unit in the number token, so searching for
            // number tokens is the best way to get both the number and unit.
            var token = codeMirror.getTokenAt(position);
            if (token && token.type && /\bnumber\b/.test(token.type))
                return token;
            return null;
        }

        var position = codeMirror.getCursor("head");
        var token = findNumberToken(position);

        if (!token) {
            // If the cursor is at the outside beginning of the token, the previous
            // findNumberToken wont find it. So check the next column for a number too.
            position.ch += 1;
            token = findNumberToken(position);
        }

        if (!token)
            return CodeMirror.Pass;

        var foundNumber = codeMirror.alterNumberInRange(amount, {ch: token.start, line: position.line}, {ch: token.end, line: position.line}, true);
        if (!foundNumber)
            return CodeMirror.Pass;
    }

    CodeMirror.defineExtension("rectsForRange", function(range) {
        var lineRects = [];

        for (var line = range.start.line; line <= range.end.line; ++line) {
            var lineContent = this.getLine(line);

            var startChar = line === range.start.line ? range.start.ch : (lineContent.length - lineContent.trimLeft().length);
            var endChar = line === range.end.line ? range.end.ch : lineContent.length;
            var firstCharCoords = this.cursorCoords({ch: startChar, line: line});
            var endCharCoords = this.cursorCoords({ch: endChar, line: line});

            // Handle line wrapping.
            if (firstCharCoords.bottom !== endCharCoords.bottom) {
                var maxY = -Number.MAX_VALUE;
                for (var ch = startChar; ch <= endChar; ++ch) {
                    var coords = this.cursorCoords({ch: ch, line: line});
                    if (coords.bottom > maxY) {
                        if (ch > startChar) {
                            var maxX = Math.ceil(this.cursorCoords({ch: ch - 1, line: line}).right);
                            lineRects.push(new WebInspector.Rect(minX, minY, maxX - minX, maxY - minY));
                        }
                        var minX = Math.floor(coords.left);
                        var minY = Math.floor(coords.top)
                        maxY = Math.ceil(coords.bottom);
                    }
                }
                maxX = Math.ceil(coords.right);
                lineRects.push(new WebInspector.Rect(minX, minY, maxX - minX, maxY - minY));
            } else {
                var minX = Math.floor(firstCharCoords.left);
                var minY = Math.floor(firstCharCoords.top);
                var maxX = Math.ceil(endCharCoords.right);
                var maxY = Math.ceil(endCharCoords.bottom);
                lineRects.push(new WebInspector.Rect(minX, minY, maxX - minX, maxY - minY));
            }
        }
        return lineRects;
    });

    CodeMirror.defineExtension("createColorMarkers", function(range, callback) {
        var createdMarkers = [];

        var start = range instanceof WebInspector.TextRange ? range.startLine : 0;
        var end = range instanceof WebInspector.TextRange ? range.endLine + 1 : this.lineCount();

        // Matches rgba(0, 0, 0, 0.5), rgb(0, 0, 0), hsl(), hsla(), #fff, #ffffff, white
        const colorRegex = /((?:rgb|hsl)a?\([^)]+\)|#[0-9a-fA-F]{6}|#[0-9a-fA-F]{3}|\b\w+\b(?![-.]))/g;

        for (var lineNumber = start; lineNumber < end; ++lineNumber) {
            var lineContent = this.getLine(lineNumber);
            var match = colorRegex.exec(lineContent);
            while (match) {

                // Act as a negative look-behind and disallow the color from being prefixing with certain characters.
                if (match.index > 0 && /[-.]/.test(lineContent[match.index - 1])) {
                    match = colorRegex.exec(lineContent);
                    continue;
                }

                var from = {line: lineNumber, ch: match.index};
                var to = {line: lineNumber, ch: match.index + match[0].length};

                var foundColorMarker = false;
                var markers = this.findMarksAt(to);
                for (var j = 0; j < markers.length; ++j) {
                    if (WebInspector.TextMarker.textMarkerForCodeMirrorTextMarker(markers[j]).type === WebInspector.TextMarker.Type.Color) {
                        foundColorMarker = true;
                        break;
                    }
                }

                if (foundColorMarker) {
                    match = colorRegex.exec(lineContent);
                    continue;
                }

                // We're not interested in text within a CSS selector.
                var tokenType = this.getTokenTypeAt(from);
                if (tokenType && (tokenType.indexOf("builtin") !== -1 || tokenType.indexOf("tag") !== -1)) {
                    match = colorRegex.exec(lineContent);
                    continue;
                }

                var colorString = match[0];
                var color = WebInspector.Color.fromString(colorString);
                if (!color) {
                    match = colorRegex.exec(lineContent);
                    continue;
                }

                var marker = this.markText(from, to);
                marker = new WebInspector.TextMarker(marker, WebInspector.TextMarker.Type.Color);

                createdMarkers.push(marker);

                if (callback)
                    callback(marker, color, colorString);

                match = colorRegex.exec(lineContent);
            }
        }

        return createdMarkers;
    });

    CodeMirror.defineExtension("createGradientMarkers", function(range, callback) {
        var createdMarkers = [];

        var start = range instanceof WebInspector.TextRange ? range.startLine : 0;
        var end = range instanceof WebInspector.TextRange ? range.endLine + 1 : this.lineCount();

        const gradientRegex = /(repeating-)?(linear|radial)-gradient\s*\(\s*/g;

        for (var lineNumber = start; lineNumber < end; ++lineNumber) {
            var lineContent = this.getLine(lineNumber);
            var match = gradientRegex.exec(lineContent);
            while (match) {
                var startLine = lineNumber;
                var startChar = match.index;
                var endChar = match.index + match[0].length;

                var openParentheses = 0;
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
                        lineContent = this.getLine(lineNumber);
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

                var gradientString = this.getRange(from, to);
                var gradient = WebInspector.Gradient.fromString(gradientString);
                if (!gradient) {
                    match = gradientRegex.exec(lineContent);
                    continue;
                }

                var marker = new WebInspector.TextMarker(this.markText(from, to), WebInspector.TextMarker.Type.Gradient);

                createdMarkers.push(marker);

                if (callback)
                    callback(marker, gradient, gradientString);

                match = gradientRegex.exec(lineContent);
            }
        }

        return createdMarkers;
    });

    function ignoreKey(codeMirror)
    {
        // Do nothing to ignore the key.
    }

    CodeMirror.keyMap["default"] = {
        "Alt-Up": alterNumber.bind(null, 1),
        "Ctrl-Alt-Up": alterNumber.bind(null, 0.1),
        "Shift-Alt-Up": alterNumber.bind(null, 10),
        "Alt-PageUp": alterNumber.bind(null, 10),
        "Shift-Alt-PageUp": alterNumber.bind(null, 100),
        "Alt-Down": alterNumber.bind(null, -1),
        "Ctrl-Alt-Down": alterNumber.bind(null, -0.1),
        "Shift-Alt-Down": alterNumber.bind(null, -10),
        "Alt-PageDown": alterNumber.bind(null, -10),
        "Shift-Alt-PageDown": alterNumber.bind(null, -100),
        "Cmd-/": "toggleComment",
        "Shift-Tab": ignoreKey,
        fallthrough: "macDefault"
    };

    // Register some extra MIME-types for CodeMirror. These are in addition to the
    // ones CodeMirror already registers, like text/html, text/javascript, etc.
    const extraXMLTypes = ["text/xml", "text/xsl"];
    extraXMLTypes.forEach(function(type) {
        CodeMirror.defineMIME(type, "xml");
    });

    const extraHTMLTypes = ["application/xhtml+xml", "image/svg+xml"];
    extraHTMLTypes.forEach(function(type) {
        CodeMirror.defineMIME(type, "htmlmixed");
    });

    const extraJavaScriptTypes = ["text/ecmascript", "application/javascript", "application/ecmascript", "application/x-javascript",
        "text/x-javascript", "text/javascript1.1", "text/javascript1.2", "text/javascript1.3", "text/jscript", "text/livescript"];
    extraJavaScriptTypes.forEach(function(type) {
        CodeMirror.defineMIME(type, "javascript");
    });

    const extraJSONTypes = ["application/x-json", "text/x-json"];
    extraJSONTypes.forEach(function(type) {
        CodeMirror.defineMIME(type, {name: "javascript", json: true});
    });

})();

WebInspector.compareCodeMirrorPositions = function(a, b)
{
    var lineCompare = a.line - b.line;
    if (lineCompare !== 0)
        return lineCompare;

    var aColumn = "ch" in a ? a.ch : Number.MAX_VALUE;
    var bColumn = "ch" in b ? b.ch : Number.MAX_VALUE;
    return aColumn - bColumn;
};
