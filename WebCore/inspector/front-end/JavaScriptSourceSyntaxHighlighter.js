/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

WebInspector.JavaScriptSourceSyntaxHighlighter = function(table, sourceFrame) {
    WebInspector.SourceSyntaxHighlighter.call(this, table, sourceFrame);

    this.LexState = {
        Initial: 1,
        DivisionAllowed: 2,
    };
    this.ContinueState = {
        None: 0,
        Comment: 1,
        SingleQuoteString: 2,
        DoubleQuoteString: 3,
        RegExp: 4
    };
    
    this.nonToken = "";
    this.cursor = 0;
    this.lineIndex = -1;
    this.lineCode = "";
    this.newLine = null;
    this.lexState = this.LexState.Initial;
    this.continueState = this.ContinueState.None;
    
    this.rules = [{
        pattern: /^(?:\/\/.*)/,
        action: singleLineCommentAction
    }, {
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*\*+\/)/,
        action: multiLineSingleLineCommentAction
    }, {
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*)/,
        action: multiLineCommentStartAction
    }, {
        pattern: /^(?:(?:[^\*]|\*[^\/])*\*+\/)/,
        action: multiLineCommentEndAction,
        continueStateCondition: this.ContinueState.Comment
    }, {
        pattern: /^.*/,
        action: multiLineCommentMiddleAction,
        continueStateCondition: this.ContinueState.Comment
    }, {
        pattern: /^(?:(?:0|[1-9]\d*)\.\d+?(?:[eE](?:\d+|\+\d+|-\d+))?|\.\d+(?:[eE](?:\d+|\+\d+|-\d+))?|(?:0|[1-9]\d*)(?:[eE](?:\d+|\+\d+|-\d+))?|0x[0-9a-fA-F]+|0X[0-9a-fA-F]+)/,
        action: numericLiteralAction
    }, {
        pattern: /^(?:"(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*"|'(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*')/,
        action: stringLiteralAction
    }, {
        pattern: /^(?:'(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        action: singleQuoteStringStartAction
    }, {
        pattern: /^(?:(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*')/,
        action: singleQuoteStringEndAction,
        continueStateCondition: this.ContinueState.SingleQuoteString
    }, {
        pattern: /^(?:(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        action: singleQuoteStringMiddleAction,
        continueStateCondition: this.ContinueState.SingleQuoteString
    }, {
        pattern: /^(?:"(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        action: doubleQuoteStringStartAction
    }, {
        pattern: /^(?:(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*")/,
        action: doubleQuoteStringEndAction,
        continueStateCondition: this.ContinueState.DoubleQuoteString
    }, {
        pattern: /^(?:(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        action: doubleQuoteStringMiddleAction,
        continueStateCondition: this.ContinueState.DoubleQuoteString
    }, {
        pattern: /^(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)/,
        action: identOrKeywordAction
    }, {
        pattern: /^\)/,
        action: rightParenAction,
        dontAppendNonToken: true
    }, {
        pattern: /^(?:<=|>=|===|==|!=|!==|\+\+|\-\-|<<|>>|>>>|&&|\|\||\+=|\-=|\*=|%=|<<=|>>=|>>>=|&=|\|=|^=|[{}\(\[\]\.;,<>\+\-\*%&\|\^!~\?:=])/,
        action: punctuatorAction,
        dontAppendNonToken: true
    }, {
        pattern: /^(?:\/=?)/,
        action: divPunctuatorAction,
        lexStateCondition: this.LexState.DivisionAllowed,
        dontAppendNonToken: true
    }, {
        pattern: /^(?:\/(?:(?:\\.)|[^\\*\/])(?:(?:\\.)|[^\\/])*\/(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)/,
        action: regExpLiteralAction
    }, {
        pattern: /^(?:\/(?:(?:\\.)|[^\\*\/])(?:(?:\\.)|[^\\/])*)\\$/,
        action: regExpStartAction
    }, {
        pattern: /^(?:(?:(?:\\.)|[^\\/])*\/(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)/,
        action: regExpEndAction,
        continueStateCondition: this.ContinueState.RegExp
    }, {
        pattern: /^(?:(?:(?:\\.)|[^\\/])*)\\$/,
        action: regExpMiddleAction,
        continueStateCondition: this.ContinueState.RegExp
    }];
    
    function singleLineCommentAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
    }
    
    function multiLineSingleLineCommentAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
    }
    
    function multiLineCommentStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
        this.continueState = this.ContinueState.Comment;
    }
    
    function multiLineCommentEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
        this.continueState = this.ContinueState.None;
    }
    
    function multiLineCommentMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
    }
    
    function numericLiteralAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-number"));
        this.lexState = this.LexState.DivisionAllowed;
    }
    
    function stringLiteralAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.lexState = this.LexState.Initial;
    }
    
    function singleQuoteStringStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.continueState = this.ContinueState.SingleQuoteString;
    }
    
    function singleQuoteStringEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.continueState = this.ContinueState.None;
    }
    
    function singleQuoteStringMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
    }
    
    function doubleQuoteStringStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.continueState = this.ContinueState.DoubleQuoteString;
    }
    
    function doubleQuoteStringEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.continueState = this.ContinueState.None;
    }
    
    function doubleQuoteStringMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
    }
    
    function regExpLiteralAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-regexp"));
        this.lexState = this.LexState.Initial;
    }

    function regExpStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-regexp"));
        this.continueState = this.ContinueState.RegExp;
    }

    function regExpEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-regexp"));
        this.continueState = this.ContinueState.None;
    }

    function regExpMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-regexp"));
    }
    
    const keywords = {
        "null": true,
        "true": true,
        "false": true,
        "break": true,
        "case": true,
        "catch": true,
        "const": true,
        "default": true,
        "finally": true,
        "for": true,
        "instanceof": true,
        "new": true,
        "var": true,
        "continue": true,
        "function": true,
        "return": true,
        "void": true,
        "delete": true,
        "if": true,
        "this": true,
        "do": true,
        "while": true,
        "else": true,
        "in": true,
        "switch": true,
        "throw": true,
        "try": true,
        "typeof": true,
        "debugger": true,
        "class": true,
        "enum": true,
        "export": true,
        "extends": true,
        "import": true,
        "super": true,
        "get": true,
        "set": true
    };
    function identOrKeywordAction(token)
    {
        this.cursor += token.length;
        
        if (token in keywords) {
            this.newLine.appendChild(this.createSpan(token, "webkit-javascript-keyword"));
            this.lexState = this.LexState.Initial;
        } else {
            var identElement = this.createSpan(token, "webkit-javascript-ident");
            identElement.addEventListener("mouseover", showDatatip, false);
            this.newLine.appendChild(identElement);
            this.lexState = this.LexState.DivisionAllowed;
        }
    }
    
    function showDatatip(event) {
        if (!WebInspector.panels.scripts || !WebInspector.panels.scripts.paused)
            return;

        var node = event.target;
        var parts = [node.textContent];
        while (node.previousSibling && node.previousSibling.textContent === ".") {
            node = node.previousSibling.previousSibling;
            if (!node || !node.hasStyleClass("webkit-javascript-ident"))
                break;
            parts.unshift(node.textContent);
        }

        var expression = parts.join(".");

        WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression, false, "console", callback);
        function callback(result, exception)
        {
            if (exception)
                return;
            event.target.setAttribute("title", result.description);
            event.target.addEventListener("mouseout", onmouseout, false);
            
            function onmouseout(event)
            {
                event.target.removeAttribute("title");
                event.target.removeEventListener("mouseout", onmouseout, false);
            }
        }
    }
    
    function divPunctuatorAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    function rightParenAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DivisionAllowed;
    }
    
    function punctuatorAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
}

WebInspector.JavaScriptSourceSyntaxHighlighter.prototype.__proto__ = WebInspector.SourceSyntaxHighlighter.prototype;
