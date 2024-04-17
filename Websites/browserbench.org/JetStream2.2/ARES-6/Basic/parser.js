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

function parse(tokenizer)
{
    let program;
    
    let pushBackBuffer = [];
    
    function nextToken()
    {
        if (pushBackBuffer.length)
            return pushBackBuffer.pop();
        let result = tokenizer.next();
        if (result.done)
            return {kind: "endOfFile", string: "<end of file>"};
        return result.value;
    }
    
    function pushToken(token)
    {
        pushBackBuffer.push(token);
    }
    
    function peekToken()
    {
        let result = nextToken();
        pushToken(result);
        return result;
    }
    
    function consumeKind(kind)
    {
        let token = nextToken();
        if (token.kind != kind) {
            throw new Error("At " + token.sourceLineNumber + ": expected " + kind + " but got: " + token.string);
        }
        return token;
    }
    
    function consumeToken(string)
    {
        let token = nextToken();
        if (token.string.toLowerCase() != string.toLowerCase())
            throw new Error("At " + token.sourceLineNumber + ": expected " + string + " but got: " + token.string);
        return token;
    }
    
    function parseVariable()
    {
        let name = consumeKind("identifier").string;
        let result = {evaluate: Basic.Variable, name, parameters: []};
        if (peekToken().string == "(") {
            do {
                nextToken();
                result.parameters.push(parseNumericExpression());
            } while (peekToken().string == ",");
            consumeToken(")");
        }
        return result;
    }
    
    function parseNumericExpression()
    {
        function parsePrimary()
        {
            let token = nextToken();
            switch (token.kind) {
            case "identifier": {
                let result = {evaluate: Basic.NumberApply, name: token.string, parameters: []};
                if (peekToken().string == "(") {
                    do {
                        nextToken();
                        result.parameters.push(parseNumericExpression());
                    } while (peekToken().string == ",");
                    consumeToken(")");
                }
                return result;
            }
                
            case "number":
                return {evaluate: Basic.Const, value: token.value};
                
            case "operator":
                switch (token.string) {
                case "(": {
                    let result = parseNumericExpression();
                    consumeToken(")");
                    return result;
                } }
                break;
            }
            throw new Error("At " + token.sourceLineNumber + ": expected identifier, number, or (, but got: " + token.string);
        }
        
        function parseFactor()
        {
            let primary = parsePrimary();
            
            let ok = true;
            while (ok) {
                switch (peekToken().string) {
                case "^":
                    nextToken();
                    primary = {evaluate: Basic.NumberPow, left: primary, right: parsePrimary()};
                    break;
                default:
                    ok = false;
                    break;
                }
            }
            
            return primary;
        }
        
        function parseTerm()
        {
            let factor = parseFactor();
            
            let ok = true;
            while (ok) {
                switch (peekToken().string) {
                case "*":
                    nextToken();
                    factor = {evaluate: Basic.NumberMul, left: factor, right: parseFactor()};
                    break;
                case "/":
                    nextToken();
                    factor = {evaluate: Basic.NumberDiv, left: factor, right: parseFactor()};
                    break;
                default:
                    ok = false;
                    break;
                }
            }
            
            return factor;
        }
        
        // Only the leading term in Basic can have a sign.
        let negate = false;
        switch (peekToken().string) {
        case "+":
            nextToken();
            break;
        case "-":
            negate = true;
            nextToken()
            break;
        }
        
        let term = parseTerm();
        if (negate)
            term = {evaluate: Basic.NumberNeg, term: term};
        
        let ok = true;
        while (ok) {
            switch (peekToken().string) {
            case "+":
                nextToken();
                term = {evaluate: Basic.NumberAdd, left: term, right: parseTerm()};
                break;
            case "-":
                nextToken();
                term = {evaluate: Basic.NumberSub, left: term, right: parseTerm()};
                break;
            default:
                ok = false;
                break;
            }
        }
        
        return term;
    }
    
    function parseConstant()
    {
        switch (peekToken().string) {
        case "+":
            nextToken();
            return consumeKind("number").value;
        case "-":
            nextToken();
            return -consumeKind("number").value;
        default:
            if (isStringExpression())
                return consumeKind("string").value;
            return consumeKind("number").value;
        }
    }
    
    function parseStringExpression()
    {
        let token = nextToken();
        switch (token.kind) {
        case "string":
            return {evaluate: Basic.Const, value: token.value};
        case "identifier":
            consumeToken("$");
            return {evaluate: Basic.StringVar, name: token.string};
        default:
            throw new Error("At " + token.sourceLineNumber + ": expected string expression but got " + token.string);
        }
    }
    
    function isStringExpression()
    {
        // A string expression must start with a string variable or a string constant.
        let token = nextToken();
        if (token.kind == "string") {
            pushToken(token);
            return true;
        }
        if (token.kind == "identifier") {
            let result = peekToken().string == "$";
            pushToken(token);
            return result;
        }
        pushToken(token);
        return false;
    }
    
    function parseRelationalExpression()
    {
        if (isStringExpression()) {
            let left = parseStringExpression();
            let operator = nextToken();
            let evaluate;
            switch (operator.string) {
            case "=":
                evaluate = Basic.Equals;
                break;
            case "<>":
                evaluate = Basic.NotEquals;
                break;
            default:
                throw new Error("At " + operator.sourceLineNumber + ": expected a string comparison operator but got: " + operator.string);
            }
            return {evaluate, left, right: parseStringExpression()};
        }
        
        let left = parseNumericExpression();
        let operator = nextToken();
        let evaluate;
        switch (operator.string) {
        case "=":
            evaluate = Basic.Equals;
            break;
        case "<>":
            evaluate = Basic.NotEquals;
            break;
        case "<":
            evaluate = Basic.LessThan;
            break;
        case ">":
            evaluate = Basic.GreaterThan;
            break;
        case "<=":
            evaluate = Basic.LessEqual;
            break;
        case ">=":
            evaluate = Basic.GreaterEqual;
            break;
        default:
            throw new Error("At " + operator.sourceLineNumber + ": expected a numeric comparison operator but got: " + operator.string);
        }
        return {evaluate, left, right: parseNumericExpression()};
    }
    
    function parseNonNegativeInteger()
    {
        let token = nextToken();
        if (!/^[0-9]+$/.test(token.string))
            throw new Error("At ", token.sourceLineNumber + ": expected a line number but got: " + token.string);
        return token.value;
    }
    
    function parseGoToStatement()
    {
        statement.kind = Basic.GoTo;
        statement.target = parseNonNegativeInteger();
    }
    
    function parseGoSubStatement()
    {
        statement.kind = Basic.GoSub;
        statement.target = parseNonNegativeInteger();
    }
    
    function parseStatement()
    {
        let statement = {};
        statement.lineNumber = consumeKind("userLineNumber").userLineNumber;
        program.statements.set(statement.lineNumber, statement);
        
        let command = nextToken();
        statement.sourceLineNumber = command.sourceLineNumber;
        switch (command.kind) {
        case "keyword":
            switch (command.string.toLowerCase()) {
            case "def":
                statement.process = Basic.Def;
                statement.name = consumeKind("identifier");
                statement.parameters = [];
                if (peekToken().string == "(") {
                    do {
                        nextToken();
                        statement.parameters.push(consumeKind("identifier"));
                    } while (peekToken().string == ",");
                }
                statement.expression = parseNumericExpression();
                break;
            case "let":
                statement.process = Basic.Let;
                statement.variable = parseVariable();
                consumeToken("=");
                if (statement.process == Basic.Let)
                    statement.expression = parseNumericExpression();
                else
                    statement.expression = parseStringExpression();
                break;
            case "go": {
                let next = nextToken();
                if (next.string == "to")
                    parseGoToStatement();
                else if (next.string == "sub")
                    parseGoSubStatement();
                else
                    throw new Error("At " + next.sourceLineNumber + ": expected to or sub but got: " + next.string);
                break;
            }
            case "goto":
                parseGoToStatement();
                break;
            case "gosub":
                parseGoSubStatement();
                break;
            case "if":
                statement.process = Basic.If;
                statement.condition = parseRelationalExpression();
                consumeToken("then");
                statement.target = parseNonNegativeInteger();
                break;
            case "return":
                statement.process = Basic.Return;
                break;
            case "stop":
                statement.process = Basic.Stop;
                break;
            case "on":
                statement.process = Basic.On;
                statement.expression = parseNumericExpression();
                if (peekToken().string == "go") {
                    consumeToken("go");
                    consumeToken("to");
                } else
                    consumeToken("goto");
                statement.targets = [];
                for (;;) {
                    statement.targets.push(parseNonNegativeInteger());
                    if (peekToken().string != ",")
                        break;
                    nextToken();
                }
                break;
            case "for":
                statement.process = Basic.For;
                statement.variable = consumeKind("identifier").string;
                consumeToken("=");
                statement.initial = parseNumericExpression();
                consumeToken("to");
                statement.limit = parseNumericExpression();
                if (peekToken().string == "step") {
                    nextToken();
                    statement.step = parseNumericExpression();
                } else
                    statement.step = {evaluate: Basic.Const, value: 1};
                consumeKind("newLine");
                let lastStatement = parseStatements();
                if (lastStatement.process != Basic.Next)
                    throw new Error("At " + lastStatement.sourceLineNumber + ": expected next statement");
                if (lastStatement.variable != statement.variable)
                    throw new Error("At " + lastStatement.sourceLineNumber + ": expected next for " + statement.variable + " but got " + lastStatement.variable);
                lastStatement.target = statement;
                statement.target = lastStatement;
                return statement;
            case "next":
                statement.process = Basic.Next;
                statement.variable = consumeKind("identifier").string;
                break;
            case "print": {
                statement.process = Basic.Print;
                statement.items = [];
                let ok = true;
                while (ok) {
                    switch (peekToken().string) {
                    case ",":
                        nextToken();
                        statement.items.push({kind: "comma"});
                        break;
                    case ";":
                        nextToken();
                        break;
                    case "tab":
                        nextToken();
                        consumeToken("(");
                        statement.items.push({kind: "tab", value: parseNumericExpression()});
                        break;
                    case "\n":
                        ok = false;
                        break;
                    default:
                        if (isStringExpression()) {
                            statement.items.push({kind: "string", value: parseStringExpression()});
                            break;
                        }
                        statement.items.push({kind: "number", value: parseNumericExpression()});
                        break;
                    }
                }
                break;
            }
            case "input":
                statement.process = Basic.Input;
                statement.items = [];
                for (;;) {
                    stament.items.push(parseVariable());
                    if (peekToken().string != ",")
                        break;
                    nextToken();
                }
                break;
            case "read":
                statement.process = Basic.Read;
                statement.items = [];
                for (;;) {
                    stament.items.push(parseVariable());
                    if (peekToken().string != ",")
                        break;
                    nextToken();
                }
                break;
            case "restore":
                statement.process = Basic.Restore;
                break;
            case "data":
                for (;;) {
                    program.data.push(parseConstant());
                    if (peekToken().string != ",")
                        break;
                    nextToken();
                }
                break;
            case "dim":
                statement.process = Basic.Dim;
                statement.items = [];
                for (;;) {
                    let name = consumeKind("identifier").string;
                    consumeToken("(");
                    let bounds = [];
                    bounds.push(parseNonNegativeInteger());
                    if (peekToken().string == ",") {
                        nextToken();
                        bounds.push(parseNonNegativeInteger());
                    }
                    consumeToken(")");
                    statement.items.push({name, bounds});
                    
                    if (peekToken().string != ",")
                        break;
                    consumeToken(",");
                }
                break;
            case "option": {
                consumeToken("base");
                let base = parseNonNegativeInteger();
                if (base != 0 && base != 1)
                    throw new Error("At " + command.sourceLineNumber + ": unexpected base: " + base);
                program.base = base;
                break;
            }
            case "randomize":
                statement.process = Basic.Randomize;
                break;
            case "end":
                statement.process = Basic.End;
                break;
            default:
                throw new Error("At " + command.sourceLineNumber + ": unexpected command but got: " + command.string);
            }
            break;
        case "remark":
            // Just ignore it.
            break;
        default:
            throw new Error("At " + command.sourceLineNumber + ": expected command but got: " + command.string + " (of kind " + command.kind + ")");
        }
        
        consumeKind("newLine");
        return statement;
    }
    
    function parseStatements()
    {
        let statement;
        do {
            statement = parseStatement();
        } while (!statement.process || !statement.process.isBlockEnd);
        return statement;
    }
    
    return {
        program()
        {
            program = {
                process: Basic.Program,
                statements: new Map(),
                data: [],
                base: 0
            };
            let lastStatement = parseStatements(program.statements);
            if (lastStatement.process != Basic.End)
                throw new Error("At " + lastStatement.sourceLineNumber + ": expected end");
            
            return program;
        },
        
        statement(program_)
        {
            program = program_;
            return parseStatement();
        }
    };
}

