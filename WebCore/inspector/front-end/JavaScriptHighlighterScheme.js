/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.JavaScriptHighlighterScheme = function() {
    this.LexState = {
        Initial: 1,
        DivisionAllowed: 2
    };
    this.ContinueState = {
        None: 0,
        Comment: 1,
        SingleQuoteString: 2,
        DoubleQuoteString: 3,
        RegExp: 4
    };

    var keywords = [
        "null", "true", "false", "break", "case", "catch", "const", "default", "finally", "for",
        "instanceof", "new", "var", "continue", "function", "return", "void", "delete", "if",
        "this", "do", "while", "else", "in", "switch", "throw", "try", "typeof", "debugger",
        "class", "enum", "export", "extends", "import", "super", "get", "set"
    ].keySet();

    var tokenTypes = WebInspector.TextEditorHighlighter.TokenType;

    this.rules = [{
        name: "singleLineCommentAction",
        pattern: /^(?:\/\/.*)/,
        type: tokenTypes.Comment
    }, {
        name: "multiLineSingleLineCommentAction",
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*\*+\/)/,
        type: tokenTypes.Comment
    }, {
        name: "multiLineCommentStartAction",
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*)/,
        type: tokenTypes.Comment,
        postContinueState: this.ContinueState.Comment
    }, {
        name: "multiLineCommentEndAction",
        pattern: /^(?:(?:[^\*]|\*[^\/])*\*+\/)/,
        type: tokenTypes.Comment,
        preContinueState: this.ContinueState.Comment,
        postContinueState: this.ContinueState.None
    }, {
        name: "multiLineCommentMiddleAction",
        pattern: /^.*/,
        type: tokenTypes.Comment,
        preContinueState: this.ContinueState.Comment
    }, {
        name: "numericLiteralAction",
        pattern: /^(?:(?:0|[1-9]\d*)\.\d+?(?:[eE](?:\d+|\+\d+|-\d+))?|\.\d+(?:[eE](?:\d+|\+\d+|-\d+))?|(?:0|[1-9]\d*)(?:[eE](?:\d+|\+\d+|-\d+))?|0x[0-9a-fA-F]+|0X[0-9a-fA-F]+)\s*/,
        type: tokenTypes.Number,
        postLexState: this.LexState.DivisionAllowed
    }, {
        name: "stringLiteralAction",
        pattern: /^(?:"(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*"|'(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*')/,
        type: tokenTypes.String,
        postLexState: this.LexState.Initial
    }, {
        name: "singleQuoteStringStartAction",
        pattern: /^(?:'(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        type: tokenTypes.String,
        postContinueState:  this.ContinueState.SingleQuoteString
    }, {
        name: "singleQuoteStringEndAction",
        pattern: /^(?:(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*')/,
        type: tokenTypes.String,
        preContinueState: this.ContinueState.SingleQuoteString,
        postContinueState: this.ContinueState.None
    }, {
        name: "singleQuoteStringMiddleAction",
        pattern: /^(?:(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        type: tokenTypes.String,
        preContinueState: this.ContinueState.SingleQuoteString
    }, {
        name: "doubleQuoteStringStartAction",
        pattern: /^(?:"(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        type: tokenTypes.String,
        postContinueState: this.ContinueState.DoubleQuoteString
    }, {
        name: "doubleQuoteStringEndAction",
        pattern: /^(?:(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*")/,
        type: tokenTypes.String,
        preContinueState: this.ContinueState.DoubleQuoteString,
        postContinueState: this.ContinueState.None
    }, {
        name: "doubleQuoteStringMiddleAction",
        pattern: /^(?:(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        type: tokenTypes.String,
        preContinueState: this.ContinueState.DoubleQuoteString
    }, {
        name: "keywordAction",
        pattern: /^(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)/,
        keywords: keywords,
        type: tokenTypes.Keyword,
        postLexState: this.LexState.Initial
    }, {
        name: "identAction",
        pattern: /^(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)\s*/,
        postLexState: this.LexState.DivisionAllowed
    }, {
        name: "rightParenAction",
        pattern: /^\)\s*/,
        postLexState: this.LexState.DivisionAllowed
    }, {
        name: "punctuatorAction",
        pattern: /^(?:<=|>=|===|==|!=|!==|\+\+|\-\-|<<|>>|>>>|&&|\|\||\+=|\-=|\*=|%=|<<=|>>=|>>>=|&=|\|=|^=|[\s{}\(\[\]\.;,<>\+\-\*%&\|\^!~\?:=])/,
        postLexState: this.LexState.Initial
    }, {
        name: "divPunctuatorAction",
        pattern: /^(?:\/=?)/,
        preLexState: this.LexState.DivisionAllowed,
        postLexState: this.LexState.Initial
    }, {
        name: "regExpLiteralAction",
        pattern: /^(?:\/(?:(?:\\.)|[^\\*\/])(?:(?:\\.)|[^\\\/])*\/(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)/,
        type: tokenTypes.String,
        postLexState: this.LexState.Initial
    }, {
        name: "regExpStartAction",
        pattern: /^(?:\/(?:(?:\\.)|[^\\*\/])(?:(?:\\.)|[^\\\/])*)\\$/,
        type: tokenTypes.String,
        postContinueState: this.ContinueState.RegExp
    }, {
        name: "regExpEndAction",
        pattern: /^(?:(?:(?:\\.)|[^\\\/])*\/(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)/,
        type: tokenTypes.String,
        preContinueState: this.ContinueState.RegExp,
        postContinueState: this.ContinueState.None
    }, {
        name: "regExpMiddleAction",
        pattern: /^(?:(?:(?:\\.)|[^\\\/])*)\\$/,
        type: tokenTypes.String,
        preContinueState: this.ContinueState.RegExp
    }, {
        name: "whitespace",
        pattern: /^\s+$/
    }];
}

WebInspector.JavaScriptHighlighterScheme.prototype = {
    reset: function(highlighter)
    {
        highlighter.lexState = this.LexState.Initial;
        highlighter.continueState = this.ContinueState.None;
    }
}
