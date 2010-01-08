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

WebInspector.TextEditorHighlighter = function(textModel)
{
    this._textModel = textModel;
    this._tokenizer = new WebInspector.JavaScriptTokenizer();

    this._styles = [];
    this._styles["comment"] = "rgb(0, 116, 0)";
    this._styles["string"] = "rgb(196, 26, 22)";
    this._styles["regex"] = "rgb(196, 26, 22)";
    this._styles["keyword"] = "rgb(170, 13, 145)";
    this._styles["number"] = "rgb(28, 0, 207)";
}

WebInspector.TextEditorHighlighter.prototype = {
    highlight: function(startLine, endLine)
    {
        // Rewind to the last highlighted line to gain proper highlighter context.
        while (startLine > 0 && !this._textModel.getAttribute(startLine - 1, 0, "highlighter-state"))
            startLine--;

        // Restore highlighter context taken from previous line.
        var state = this._textModel.getAttribute(startLine - 1, 0, "highlighter-state");
         if (state)
             this._tokenizer.condition = state.postCondition;
         else
             this._tokenizer.condition = this._tokenizer.initialCondition;
        // Each line has associated state attribute with pre- and post-highlighter conditions.
        // Highlight lines from range until we find line precondition matching highlighter state.
        var damage = {};
        for (var i = startLine; i < endLine; ++i) {
            state = this._textModel.getAttribute(i, 0, "highlighter-state");
            if (state && state.preCondition === this._tokenizer.condition) {
                // Following lines are up to date, no need re-highlight.
                this._tokenizer.condition = state.postCondition;
                continue;
            }

            damage[i] = true;

            state = {};
            state.preCondition = this._tokenizer.condition;

            this._textModel.removeAttributes(i, "highlight");
            this._lex(this._textModel.line(i), i);

            state.postCondition = this._tokenizer.condition;
            this._textModel.addAttribute(i, 0, "highlighter-state", state);
        }

        state = this._textModel.getAttribute(endLine, 0, "highlighter-state");

        if (state && state.preCondition !== this._tokenizer.condition) {
            // Requested highlight range is over, but we did not recover. Invalidate tail highlighting.
            for (var i = endLine; i < this._textModel.linesCount; ++i)
                this._textModel.removeAttributes(i, "highlighter-state");
        }
        return damage;
    },

    _lex: function(line, lineNumber) {
         this._tokenizer.line = line;
         var column = 0;
         do {
             var newColumn = this._tokenizer.nextToken(column);
             var tokenType = this._tokenizer.tokenType;
             if (tokenType)
                 this._textModel.addAttribute(lineNumber, column, "highlight", { length: newColumn - column, style: this._styles[tokenType] });
             column = newColumn;
         } while (column < line.length)
    }
}
