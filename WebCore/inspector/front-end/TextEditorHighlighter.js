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
    this._highlighterScheme = new WebInspector.JavaScriptHighlighterScheme();

    this._styles = [];
    this._styles[WebInspector.TextEditorHighlighter.TokenType.Comment] = "rgb(0, 116, 0)";
    this._styles[WebInspector.TextEditorHighlighter.TokenType.String] = "rgb(196, 26, 22)";
    this._styles[WebInspector.TextEditorHighlighter.TokenType.Keyword] = "rgb(170, 13, 145)";
    this._styles[WebInspector.TextEditorHighlighter.TokenType.Number] = "rgb(28, 0, 207)";
}

WebInspector.TextEditorHighlighter.TokenType = {
    Comment: 0,
    String: 1,
    Keyword: 2,
    Number: 3
}

WebInspector.TextEditorHighlighter.prototype = {
    highlight: function(startLine, endLine)
    {
        this._highlighterScheme.reset(this);
        // Rewind to the last highlighted line to gain proper highlighter context.
        while (startLine > 0 && !this._textModel.getAttribute(startLine - 1, 0, "highlighter-state"))
            startLine--;

        // Restore highlighter context taken from previous line.
        var state = this._textModel.getAttribute(startLine - 1, 0, "highlighter-state");
        if (state) {
            this.continueState = state.postContinueState;
            this.lexState = state.postLexState;
        }

        // Each line has associated state attribute with pre- and post-highlighter conditions.
        // Highlight lines from range until we find line precondition matching highlighter state.
        var damage = {};
        for (var i = startLine; i < endLine; ++i) {
            state = this._textModel.getAttribute(i, 0, "highlighter-state");
            if (state && state.preContinueState === this.continueState && state.preLexState === this.lexState) {
                // Following lines are up to date, no need re-highlight.
                this.continueState = state.postContinueState;
                this.lexState = state.postLexState;
                continue;
            }

            damage[i] = true;

            state = {};
            state.preContinueState = this.continueState;
            state.preLexState = this.lexState;

            this._textModel.removeAttributes(i, "highlight");
            var line = this._textModel.line(i);
            for (var j = 0; j < line.length;)
                j += this._lex(line.substring(j), i, j);

            state.postContinueState = this.continueState;
            state.postLexState = this.lexState;
            this._textModel.addAttribute(i, 0, "highlighter-state", state);
        }

        state = this._textModel.getAttribute(endLine, 0, "highlighter-state");

        if (state && (state.preContinueState !== this.continueState || state.preLexState !== this.lexState)) {
            // Requested highlight range is over, but we did not recover. Invalidate tail highlighting.
            for (var i = endLine; i < this._textModel.linesCount; ++i)
                this._textModel.removeAttributes(i, "highlighter-state");
        }

        return damage;
    },

    _lex: function(codeFragment, line, column) {
        var token = null;
        for (var i = 0; i < this._highlighterScheme.rules.length; i++) {
            var rule = this._highlighterScheme.rules[i];
            var ruleContinueStateCondition = typeof rule.preContinueState !== "undefined" ? rule.preContinueState : this._highlighterScheme.ContinueState.None;
            if (this.continueState !== ruleContinueStateCondition)
                continue;
            if (typeof rule.preLexState !== "undefined" && this.lexState !== rule.preLexState)
                continue;

            var match = rule.pattern.exec(codeFragment);
            if (match) {
                token = match[0];
                if (token && (!rule.keywords || (token in rule.keywords))) {
                    if (typeof rule.type === "number")
                        this._textModel.addAttribute(line, column, "highlight", { length: token.length, style: this._styles[rule.type] });
                    if (typeof rule.postLexState !== "undefined")
                        this.lexState = rule.postLexState;
                    if (typeof rule.postContinueState !== "undefined")
                        this.continueState = rule.postContinueState;
                    return token.length;
                }
            }
        }
        return 1;
    }
}
