/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

// Loosely based on ECMA 55 sections 4-8, but loosened to allow for modern conventions, like
// multi-character variable names. But this doesn't go too far - in particular, this doesn't do
// unicode, because that would require more thought.
function* lex(string)
{
    let sourceLineNumber = 0;
    for (let line of string.split("\n")) {
        ++sourceLineNumber;
        
        function consumeWhitespace()
        {
            if (/^\s+/.test(line))
                line = RegExp.rightContext;
        }
   
        function consume(kind)
        {
            line = RegExp.rightContext;
            return {kind, string: RegExp.lastMatch, sourceLineNumber, userLineNumber};
        }
        
        const isIdentifier = /^[a-z_]([a-z0-9_]*)/i;
        const isNumber = /^(([0-9]+(\.([0-9]*))?)|(\.[0-9]+)(e([+-]?)([0-9]+))?)/i;
        const isString = /^\"([^\"]|(\"\"))*\"/;
        const isKeyword = /^((base)|(data)|(def)|(dim)|(end)|(for)|(go)|(gosub)|(goto)|(if)|(input)|(let)|(next)|(on)|(option)|(print)|(randomize)|(read)|(restore)|(return)|(step)|(stop)|(sub)|(then)|(to))/i;
        const isOperator = /^(-|\+|\*|\/|\^|\(|\)|(<[>=]?)|(>=?)|=|,|\$|;)/;
        const isRem = /^rem\s.*/;
        
        consumeWhitespace();
        
        if (!/^[0-9]+/.test(line))
            throw new Error("At line " + sourceLineNumber + ": Expect line number: " + line);
        let userLineNumber = +RegExp.lastMatch;
        line = RegExp.rightContext;
        yield {kind: "userLineNumber", string: RegExp.lastMatch, sourceLineNumber, userLineNumber};
        
        consumeWhitespace();
        
        while (line.length) {
            if (isKeyword.test(line))
                yield consume("keyword");
            else if (isIdentifier.test(line))
                yield consume("identifier");
            else if (isNumber.test(line)) {
                let token = consume("number");
                token.value = +token.string;
                yield token;
            } else if (isString.test(line)) {
                let token = consume("string");
                token.value = "";
                for (let i = 1; i < token.string.length - 1; ++i) {
                    let char = token.string.charAt(i);
                    if (char == "\"")
                        i++;
                    token.value += char;
                }
                yield token;
            } else if (isOperator.test(line))
                yield consume("operator");
            else if (isRem.test(line))
                yield consume("remark");
            else
                throw new Error("At line " + sourceLineNumber + ": Cannot lex token: " + line);
            consumeWhitespace();
        }
        
        // Note: this is necessary for the parser, which may look-ahead without checking if we're
        // done. Fortunately, it won't look-ahead past a newLine.
        yield {kind: "newLine", string:"\n", sourceLineNumber, userLineNumber};
    }
}
