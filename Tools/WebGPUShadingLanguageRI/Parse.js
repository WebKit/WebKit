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

function parse(program, origin, lineNumberOffset, text)
{
    let lexer = new Lexer(origin, lineNumberOffset, text);
    
    // The hardest part of dealing with C-like languages is parsing variable declaration statements.
    // Let's consider if this happens in WSL. Here are the valid statements in WSL that being with an
    // identifier, if we assume that any expression can be a standalone statement.
    //
    //     x;
    //     x <binop> y;
    //     x < y;
    //     x < y > z;
    //     x = y;
    //     x.f = y;
    //     \exp = y;
    //     x[42] = y;
    //     x();
    //     x<y>();
    //     x y;
    //     x<y> z;
    //     device x[] y;
    //     x[42] y;
    //     device x^ y;
    //     thread x^^ y;
    //     x^thread^thread y;
    //     x^device^thread y;
    //
    // This has two problem areas:
    //
    //     - x<y>z can parse two different ways (as (x < y) > z or x<y> z).
    //     - x[42] could become either an assignment or a variable declaration.
    //     - x<y> could become either an assignment or a variable declaration.
    //
    // We solve the first problem by forbidding expressions as statements. The lack of function
    // pointers means that we can still allow function call statements - unlike in C, those cannot
    // have arbitrary expressions as the callee. The remaining two problems are solved by
    // backtracking. In all other respects, this is a simple recursive descent parser.
    
    function genericConsume(callback, explanation)
    {
        let token = lexer.next();
        if (!token)
            lexer.fail("Unexpected end of file");
        if (!callback(token))
            lexer.fail("Unexpected token: " + token.text + "; expected: " + explanation);
        return token;
    }
    
    function consume(...texts)
    {
        return genericConsume(token => texts.includes(token.text), texts);
    }
    
    function consumeKind(kind)
    {
        return genericConsume(token => token.kind == kind, kind);
    }
    
    function assertNext(...texts)
    {
        lexer.push(consume(...texts));
    }
    
    function genericTest(callback)
    {
        let token = lexer.peek();
        if (token && callback(token))
            return token;
        return null;
    }
    
    function test(...texts)
    {
        return genericTest(token => texts.includes(token.text));
    }
    
    function testKind(kind)
    {
        return genericTest(token => token.kind == kind);
    }
    
    function tryConsume(...texts)
    {
        let result = test(...texts);
        if (result)
            lexer.next();
        return result;
    }
    
    function tryConsumeKind(kind)
    {
        let result = testKind(kind);
        if (result)
            lexer.next();
        return result;
    }
    
    function parseProtocolRef()
    {
        let protocolToken = consumeKind("identifier");
        return new ProtocolRef(protocolToken, protocolToken.text);
    }
    
    function consumeEndOfTypeArgs()
    {
        let rightShift = tryConsume(">>");
        if (rightShift)
            lexer.push(new LexerToken(lexer, rightShift, rightShift.index, rightShift.kind, ">"));
        else
            consume(">");
    }
    
    function parseTypeParameters()
    {
        if (!test("<"))
            return [];
        
        let result = [];
        consume("<");
        while (!test(">")) {
            let constexpr = lexer.backtrackingScope(() => {
                let type = parseType();
                let name = consumeKind("identifier");
                assertNext(",", ">", ">>");
                return new ConstexprTypeParameter(type.origin, name, type);
            });
            if (constexpr)
                result.push(constexpr);
            else {
                let name = consumeKind("identifier");
                let protocol = tryConsume(":") ? parseProtocolRef() : null;
                result.push(new TypeVariable(name, name.text, protocol));
            }
            if (!tryConsume(","))
                break;
        }
        consumeEndOfTypeArgs();
        return result;
    }
    
    function parseTerm()
    {
        let token;
        if (token = tryConsume("null"))
            return new Null(token);
        if (token = tryConsumeKind("identifier"))
            return new VariableRef(token, token.text);
        if (token = tryConsumeKind("intLiteral")) {
            let intVersion = token.text | 0;
            if (intVersion != token.text)
                lexer.fail("Integer literal is not 32-bit integer");
            return new IntLiteral(token, intVersion);
        }
        if (token = tryConsumeKind("uintLiteral")) {
            let uintVersion = token.text >>> 0;
            if (uintVersion + "u" !== token.text)
                lexer.fail("Integer literal is not 32-bit unsigned integer");
            return new UintLiteral(token, uintVersion);
        }
        if (token = tryConsumeKind("doubleLiteral")) {
            token = consumeKind("doubleLiteral");
            return new DoubleLiteral(token, +token.text);
        }
        // FIXME: Need support for float literals and probably other literals too.
        consume("(");
        let result = parseExpression();
        consume(")");
        return result;
    }
    
    function parseConstexpr()
    {
        return parseTerm();
    }
    
    function parseTypeArguments()
    {
        if (!test("<"))
            return [];
        
        let result = [];
        consume("<");
        while (!test(">")) {
            // It's possible for a constexpr or type can syntactically overlap in the single
            // identifier case. Let's consider the possibilities:
            //
            //     T          could be type or constexpr
            //     T[]        only type
            //     T[42]      only type (constexpr cannot do indexing)
            //     42         only constexpr
            //
            // In the future we'll allow constexprs to do more things, and then we'll still have
            // the problem that something of the form T[1][2][3]... can either be a type or a
            // constexpr, and we can figure out in the checker which it is.
            let typeOrVariableRef = lexer.backtrackingScope(() => {
                let result = consumeKind("identifier");
                assertNext(",", ">", ">>");
                return new TypeOrVariableRef(result, result.text);
            });
            if (typeOrVariableRef)
                result.push(typeOrVariableRef);
            else {
                let constexpr = lexer.backtrackingScope(() => {
                    let result = parseConstexpr();
                    assertNext(",", ">", ">>");
                    return result;
                });
                if (constexpr)
                    result.push(constexpr);
                else
                    result.push(parseType());
            }
            if (!tryConsume(","))
                break;
        }
        consumeEndOfTypeArgs();
        return result;
    }
    
    function parseType()
    {
        let token;
        let addressSpace;
        let addressSpaceConsumed = false;
        if (token = tryConsume(...addressSpaces))
            addressSpace = token.text;
        
        let name = consumeKind("identifier");
        let typeArguments = parseTypeArguments();
        let type = new TypeRef(name, name.text, typeArguments);
        
        function getAddressSpace()
        {
            addressSpaceConsumed = true;
            if (addressSpace)
                return addressSpace;
            return tryConsume(...addressSpaces);
        }
        
        while (token = tryConsume("^", "[")) {
            if (token.text == "^") {
                type = new PtrType(token, getAddressSpace(), type);
                continue;
            }
            
            if (tryConsume("]")) {
                type = new ArrayRefType(token, getAddressSpace(), type);
                continue;
            }
            
            type = new ArrayType(token, type, parseConstexpr());
            consume("]");
        }
        
        if (addressSpace && !addressSpaceConsumed)
            lexer.fail("Address space specified for type that does not need address space");
        
        return type;
    }
    
    function parseTypeDef()
    {
        let origin = consume("typedef");
        let name = consumeKind("identifier").text;
        let typeParameters = parseTypeParameters();
        consume("=");
        let type = parseType();
        consume(";");
        return new TypeDef(origin, name, typeParameters, type);
    }
    
    function parseNative()
    {
        let origin = consume("native");
        let isType = lexer.backtrackingScope(() => {
            if (tryConsume("typedef"))
                return "normal";
            consume("primitive");
            consume("typedef");
            return "primitive";
        });
        if (isType) {
            let name = consumeKind("identifier");
            let parameters = parseTypeParameters();
            consume(";");
            return new NativeType(origin, name.text, isType == "primitive", parameters);
        }
        let returnType = parseType();
        let name = parseFuncName();
        let typeParameters = parseTypeParameters();
        let parameters = parseParameters();
        consume(";");
        return new NativeFunc(origin, name, returnType, typeParameters, parameters);
    }
    
    function genericParseLeft(texts, nextParser, constructor)
    {
        let left = nextParser();
        let token;
        while (token = tryConsume(...texts))
            left = constructor(token, left, nextParser());
        return left;
    }
    
    function parseLeftOperatorCall(texts, nextParser)
    {
        return genericParseLeft(
            texts, nextParser,
            (token, left, right) =>
                new CallExpression(token, "operator" + token.text, [], [left, right]));
    }
    
    function parseCallExpression()
    {
        let name = consumeKind("identifier");
        let typeArguments = parseTypeArguments();
        consume("(");
        let argumentList = [];
        while (!test(")")) {
            argumentList.push(parseExpression());
            if (!tryConsume(","))
                break;
        }
        consume(")");
        return new CallExpression(name, name.text, typeArguments, argumentList);
    }
    
    function parsePossibleSuffix()
    {
        // First check if this is a call expression.
        let isCallExpression = lexer.testScope(() => {
            consumeKind("identifier");
            parseTypeArguments();
            consume("(");
        });
        
        if (isCallExpression)
            return parseCallExpression();
        
        let left = parseTerm();
        let token;
        while (token = tryConsume("++", "--", ".", "->", "[")) {
            switch (token.text) {
            case "++":
            case "--":
                left = new SuffixCallAssignment(token, "operator" + token.text, left);
                break;
            case ".":
            case "->":
                if (token.text == "->")
                    left = new DereferenceExpression(token, left);
                left = new DotExpression(token, left, consumeKind("identifier"));
                break;
            case "[": {
                let index = parseExpression();
                consume("]");
                left = new DereferenceExpression(
                    token,
                    new CallExpression(token, "operator\\[]", [], [left, index]));
                break;
            }
            default:
                throw new Error("Bad token: " + token);
            }
        }
        return left;
    }
    
    function parsePossiblePrefix()
    {
        let token;
        if (token = tryConsume("++", "--", "+", "-", "!", "~"))
            return new CallAssignment(token, "operator" + token.text, parsePossiblePrefix());
        if (token = tryConsume("^"))
            return new DereferenceExpression(token, parsePossiblePrefix());
        if (token = tryConsume("\\"))
            return new MakePtrExpression(token, parsePossiblePrefix());
        if (token = tryConsume("&"))
            return new MakeArrayRefExpression(token, parsePossiblePrefix());
        return parsePossibleSuffix();
    }
    
    function parsePossibleProduct()
    {
        return parseLeftOperatorCall(["*", "/", "%"], parsePossiblePrefix);
    }
    
    function parsePossibleSum()
    {
        return parseLeftOperatorCall(["+", "-"], parsePossibleProduct);
    }
    
    function parsePossibleShift()
    {
        return parseLeftOperatorCall(["<<", ">>"], parsePossibleSum);
    }
    
    function parsePossibleRelationalInequality()
    {
        return parseLeftOperatorCall(["<", ">", "<=", "=>"], parsePossibleShift);
    }
    
    function parsePossibleRelationalEquality()
    {
        return genericParseLeft(
            ["==", "!="], parsePossibleRelationalInequality,
            (token, left, right) => {
                let result = new CallExpression(token, "operator==", [], [left, right]);
                if (token.text == "!=")
                    result = new CallExpression(token, "operator!", [], [result]);
                return result;
            });
    }
    
    function parsePossibleBitwiseAnd()
    {
        return parseLeftOperatorCall(["&"], parsePossibleRelationalEquality);
    }
    
    function parsePossibleBitwiseXor()
    {
        return parseLeftOperatorCall(["^"], parsePossibleBitwiseAnd);
    }
    
    function parsePossibleBitwiseOr()
    {
        return parseLeftOperatorCall(["|"], parsePossibleBitwiseXor);
    }
    
    function parseLeftLogicalExpression(texts, nextParser)
    {
        return genericParseLeft(
            texts, nextParser,
            (token, left, right) => new LogicalExpression(token, token.text, left, right));
    }
    
    function parsePossibleLogicalAnd()
    {
        return parseLeftLogicalExpression(["&&"], parsePossibleBitwiseOr);
    }
    
    function parsePossibleLogicalOr()
    {
        return parseLeftLogicalExpression(["||"], parsePossibleLogicalAnd);
    }
    
    function parsePossibleTernaryConditional()
    {
        let predicate = parsePossibleLogicalOr();
        let operator = tryConsume("?");
        if (!operator)
            return predicate;
        return new TernaryExpression(operator, predicate, parsePossibleAssignment(), parsePossibleAssignment());
    }
    
    function parsePossibleAssignment(mode)
    {
        let lhs = parsePossibleTernaryConditional();
        let operator = tryConsume("=", "+=", "-=", "*=", "/=", "%=", "^=", "|=", "&=");
        if (!operator) {
            if (mode == "required")
                lexer.fail("Expected assignment");
            return lhs;
        }
        if (operator.text == "=")
            return new Assignment(operator, lhs, parsePossibleAssignment());
        let name = "operator" + operator.text.substring(0, operator.text.length - 1);
        return new CallAssignment(operator, name, lhs, parsePossibleAssignment());
    }
    
    function parseAssignment()
    {
        return parsePossibleAssignment("required");
    }
    
    function parseEffectfulExpression()
    {
        let assignment = lexer.backtrackingScope(parseAssignment);
        if (assignment)
            return assignment;
        return parseCallExpression();
    }
    
    function genericParseCommaExpression(finalExpressionParser)
    {
        let list = [];
        let origin = lexer.peek();
        if (!origin)
            lexer.fail("Unexpected end of file");
        for (;;) {
            let effectfulExpression = lexer.backtrackingScope(() => {
                parseEffectfulExpression();
                consume(",");
            });
            if (!effectfulExpression) {
                let final = finalExpressionParser();
                list.push(final);
                break;
            }
            list.push(effectfulExpression);
        }
        return new CommaExpression(origin, list);
    }
    
    function parseCommaExpression()
    {
        return genericParseCommaExpression(parsePossibleAssignment);
    }
    
    function parseExpression()
    {
        return parseCommaExpression();
    }
    
    function parseEffectfulStatement()
    {
        let result = genericParseCommaExpression(parseEffectfulExpression);
        consume(";");
        return result;
    }
    
    function parseReturn()
    {
        let origin = consume("return");
        let expression = parseExpression();
        consume(";");
        return new Return(origin, expression);
    }
    
    function parseVariableDecls()
    {
        let type = parseType();
        let list = [];
        do {
            let name = consumeKind("identifier");
            let initializer = tryConsume("=") ? parseExpression() : null;
            list.push(new VariableDecl(name, name.text, type, initializer));
        } while (consume(",", ";").text == ",");
        return new CommaExpression(type.origin, list);
    }
    
    function parseStatement()
    {
        let token = lexer.peek();
        if (token.text == ";") {
            lexer.next();
            return null;
        }
        if (token.text == "return")
            return parseReturn();
        if (token.text == "break")
            return parseBreak();
        if (token.text == "continue")
            return parseContinue();
        if (token.text == "while")
            return parseWhile();
        if (token.text == "do")
            return parseDo();
        if (token.text == "for")
            return parseFor();
        let variableDecl = lexer.backtrackingScope(parseVariableDecls);
        if (variableDecl)
            return variableDecl;
        return parseEffectfulStatement();
    }
    
    function parseBlock()
    {
        let origin = consume("{");
        let block = new Block(origin);
        while (!test("}")) {
            let statement = parseStatement();
            if (statement)
                block.add(statement);
        }
        consume("}");
        return block;
    }
    
    function parseParameter()
    {
        let type = parseType();
        let name = tryConsumeKind("identifier");
        return new FuncParameter(type.origin, name ? name.text : null, type);
    }
    
    function parseParameters()
    {
        consume("(");
        let parameters = [];
        while (!test(")")) {
            parameters.push(parseParameter());
            if (!tryConsume(","))
                break;
        }
        consume(")");
        return parameters;
    }
    
    function parseFuncName()
    {
        if (tryConsume("operator")) {
            let token = consume("+", "-", "*", "/", "%", "^", "&", "|", "<", ">", "<=", ">=", "!", "==", "++", "--", "\\");
            if (token.text != "\\")
                return "operator" + token.text;
            consume("[");
            consume("]");
            return "operator\\[]";
        }
        return consumeKind("identifier").text;
    }
    
    function parseFuncDef()
    {
        let returnType = parseType();
        let name = parseFuncName();
        let typeParameters = parseTypeParameters();
        let parameters = parseParameters();
        let body = parseBlock();
        return new FuncDef(returnType.origin, name, returnType, typeParameters, parameters, body);
    }
    
    for (;;) {
        let token = lexer.peek();
        if (!token)
            return;
        if (token.text == ";")
            lexer.next();
        else if (token.text == "typedef")
            program.add(parseTypeDef());
        else if (token.text == "native")
            program.add(parseNative());
        else if (token.text == "struct")
            program.add(parseStructType());
        else if (token.text == "enum")
            program.add(parseEnum());
        else if (token.text == "protocol")
            program.add(parseProtocolDecl());
        else
            program.add(parseFuncDef());
    }
}

