/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
"use strict";

class Lexer {
    constructor(origin, originKind, lineNumberOffset, text)
    {
        if (!isOriginKind(originKind))
            throw new Error("Bad origin kind: " + originKind);
        this._origin = origin;
        this._originKind = originKind;
        this._lineNumberOffset = lineNumberOffset;
        this._text = text;
        this._index = 0;
        this._stack = [];
    }
    
    get lineNumber()
    {
        return this.lineNumberForIndex(this._index);
    }
    
    get origin() { return this._origin; }
    
    get originString()
    {
        return this._origin + ":" + (this.lineNumber + 1);
    }
    
    get originKind() { return this._originKind; }
    
    lineNumberForIndex(index)
    {
        let matches = this._text.substring(0, index).match(/\n/g);
        return (matches ? matches.length : 0) + this._lineNumberOffset;
    }
    
    get state() { return {index: this._index, stack: this._stack.concat()}; }
    set state(value)
    {
        this._index = value.index;
        this._stack = value.stack;
    }
    
    static _textIsIdentifierImpl(text)
    {
        return /^[^\d\W]\w*/.test(text);
    }
    
    static textIsIdentifier(text)
    {
        return Lexer._textIsIdentifierImpl(text) && !RegExp.rightContext.length;
    }
    
    next()
    {
        if (this._stack.length)
            return this._stack.pop();
        
        if (this._index >= this._text.length)
            return null;
        
        const isCCommentBegin = /\/\*/;
        const isCPlusPlusCommentBegin = /\/\//;
        
        let result = (kind) => {
            let text = RegExp.lastMatch;
            let token = new LexerToken(this, this._index, kind, text);
            this._index += text.length;
            return token;
        };

        let relevantText;
        for (;;) {
            relevantText = this._text.substring(this._index);
            if (/^\s+/.test(relevantText)) {
                this._index += RegExp.lastMatch.length;
                continue;
            }
            if (/^\/\*/.test(relevantText)) {
                let endIndex = relevantText.search(/\*\//);
                if (endIndex < 0)
                    this.fail("Unterminated comment");
                this._index += endIndex;
                continue;
            }
            if (/^\/\/.*/.test(relevantText)) {
                this._index += RegExp.lastMatch.length;
                continue;
            }
            break;
        }
        
        if (this._index >= this._text.length)
            return null;

        // FIXME: Make this handle exp-form literals like 1e1.
        if (/^(([0-9]*\.[0-9]+[fd]?)|([0-9]+\.[0-9]*[fd]?))/.test(relevantText))
            return result("floatLiteral");
        
        // FIXME: Make this do Unicode.
        if (Lexer._textIsIdentifierImpl(relevantText)) {
            if (/^(struct|protocol|typedef|if|else|enum|continue|break|switch|case|default|for|while|do|return|constant|device|threadgroup|thread|operator|null|true|false)$/.test(RegExp.lastMatch))
                return result("keyword");
            
            if (this._originKind == "native" && /^(native|restricted)$/.test(RegExp.lastMatch))
                return result("keyword");
            
            return result("identifier");
        }

        if (/^0x[0-9a-fA-F]+u/.test(relevantText))
            return result("uintHexLiteral");

        if (/^0x[0-9a-fA-F]+/.test(relevantText))
            return result("intHexLiteral");

        if (/^[0-9]+u/.test(relevantText))
            return result("uintLiteral");

        if (/^[0-9]+/.test(relevantText))
            return result("intLiteral");
        
        if (/^<<|>>|->|>=|<=|==|!=|\+=|-=|\*=|\/=|%=|\^=|\|=|&=|\+\+|--|&&|\|\||([{}()\[\]?:=+*\/,.%!~^&|<>@;-])/.test(relevantText))
            return result("punctuation");
        
        let remaining = relevantText.substring(0, 20).split(/\s/)[0];
        if (!remaining.length)
            remaining = relevantText.substring(0, 20);
        this.fail("Unrecognized token beginning with: " + remaining);
    }
    
    push(token)
    {
        this._stack.push(token);
    }
    
    peek()
    {
        let result = this.next();
        this.push(result);
        return result;
    }
    
    fail(error)
    {
        throw new WSyntaxError(this.originString, error);
    }
    
    backtrackingScope(callback)
    {
        let state = this.state;
        try {
            return callback();
        } catch (e) {
            if (e instanceof WSyntaxError) {
                this.state = state;
                return null;
            }
            throw e;
        }
    }
    
    testScope(callback)
    {
        let state = this.state;
        try {
            callback();
            return true;
        } catch (e) {
            if (e instanceof WSyntaxError)
                return false;
            throw e;
        } finally {
            this.state = state;
        }
    }
}
