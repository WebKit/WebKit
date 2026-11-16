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

function parse(program, origin, originKind, lineNumberOffset, text)
{
    let lexer = new Lexer(origin, originKind, lineNumberOffset, text);
    
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
                return new ConstexprTypeParameter(type.origin, name.text, type);
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
            return new NullLiteral(token);
        if (token = tryConsumeKind("identifier"))
            return new VariableRef(token, token.text);
        if (token = tryConsumeKind("intLiteral")) {
            let intVersion = (+token.text) | 0;
            if ("" + intVersion !== token.text)
                lexer.fail("Integer literal is not an integer: " + token.text);
            return new IntLiteral(token, intVersion);
        }
        if (token = tryConsumeKind("uintLiteral")) {
            let uintVersion = token.text.substr(0, token.text.length - 1) >>> 0;
            if (uintVersion + "u" !== token.text)
                lexer.fail("Integer literal is not 32-bit unsigned integer: " + token.text);
            return new UintLiteral(token, uintVersion);
        }
        if ((token = tryConsumeKind("intHexLiteral"))
            || (token = tryConsumeKind("uintHexLiteral"))) {
            let hexString = token.text.substr(2);
            if (token.kind == "uintHexLiteral")
                hexString = hexString.substr(0, hexString.length - 1);
            if (!hexString.length)
                throw new Error("Bad hex literal: " + token);
            let intVersion = parseInt(hexString, 16);
            if (token.kind == "intHexLiteral")
                intVersion = intVersion | 0;
            else
                intVersion = intVersion >>> 0;
            if (intVersion.toString(16) !== hexString)
                lexer.fail("Hex integer literal is not an integer: " + token.text);
            if (token.kind == "intHexLiteral")
                return new IntLiteral(token, intVersion);
            return new UintLiteral(token, intVersion >>> 0);
        }
        if (token = tryConsumeKind("doubleLiteral"))
            return new DoubleLiteral(token, +token.text);
        if (token = tryConsumeKind("floatLiteral")) {
            let text = token.text;
            let d = token.text.endsWith("d");
            let f = token.text.endsWith("f");
            if (d && f)
                throw new Error("Literal cannot be both a double literal and a float literal.");
            if (d || f)
                text = text.substring(0, text.length - 1);
            let value = parseFloat(text);
            if (d)
                return new DoubleLiteral(token, value);
            return new FloatLiteral(token, Math.fround(value));
        }
        if (token = tryConsume("true", "false"))
            return new BoolLiteral(token, token.text == "true");
        // FIXME: Need support for other literals too.
        consume("(");
        let result = parseExpression();
        consume(")");
        return result;
    }
    
    function parseConstexpr()
    {
        let token;
        if (token = tryConsume("-"))
            return new CallExpression(token, "operator" + token.text, [], [parseTerm()]);
        let left = parseTerm();
        if (token = tryConsume("."))
            left = new DotExpression(token, left, consumeKind("identifier").text);
        return left;
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
            return consume(...addressSpaces).text;
        }
        
        while (token = tryConsume("*", "[")) {
            if (token.text == "*") {
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
            let argument = parsePossibleAssignment();
            argumentList.push(argument);
            if (!tryConsume(","))
                break;
        }
        consume(")");
        let result = new CallExpression(name, name.text, typeArguments, argumentList);
        return result;
    }
    
    function isCallExpression()
    {
        return lexer.testScope(() => {
            consumeKind("identifier");
            parseTypeArguments();
            consume("(");
        });
    }
    
    function emitIncrement(token, old, extraArg)
    {
        let args = [old];
        if (extraArg)
            args.push(extraArg);
        
        let name = "operator" + token.text;
        if (/=$/.test(name))
            name = RegExp.leftContext;
        
        if (name == "operator")
            throw new Error("Invalid name: " + name);
        
        return new CallExpression(token, name, [], args);
    }
    
    function finishParsingPostIncrement(token, left)
    {
        let readModifyWrite = new ReadModifyWriteExpression(token, left);
        readModifyWrite.newValueExp = emitIncrement(token, readModifyWrite.oldValueRef());
        readModifyWrite.resultExp = readModifyWrite.oldValueRef();
        return readModifyWrite;
    }
    
    function parseSuffixOperator(left, acceptableOperators)
    {
        let token;
        while (token = tryConsume(...acceptableOperators)) {
            switch (token.text) {
            case "++":
            case "--":
                return finishParsingPostIncrement(token, left);
            case ".":
            case "->":
                if (token.text == "->")
                    left = new DereferenceExpression(token, left);
                left = new DotExpression(token, left, consumeKind("identifier").text);
                break;
            case "[": {
                let index = parseExpression();
                consume("]");
                left = new IndexExpression(token, left, index);
                break;
            }
            default:
                throw new Error("Bad token: " + token);
            }
        }
        return left;
    }
    
    function parsePossibleSuffix()
    {
        let acceptableOperators = ["++", "--", ".", "->", "["];
        let limitedOperators = [".", "->", "["];
        let left;
        if (isCallExpression()) {
            left = parseCallExpression();
            acceptableOperators = limitedOperators;
        } else
            left = parseTerm();
        
        return parseSuffixOperator(left, acceptableOperators);
    }
    
    function finishParsingPreIncrement(token, left, extraArg)
    {
        let readModifyWrite = new ReadModifyWriteExpression(token, left);
        readModifyWrite.newValueExp = emitIncrement(token, readModifyWrite.oldValueRef(), extraArg);
        readModifyWrite.resultExp = readModifyWrite.newValueRef();
        return readModifyWrite;
    }
    
    function parsePreIncrement()
    {
        let token = consume("++", "--");
        let left = parsePossiblePrefix();
        return finishParsingPreIncrement(token, left);
    }
    
    function parsePossiblePrefix()
    {
        let token;
        if (test("++", "--"))
            return parsePreIncrement();
        if (token = tryConsume("+", "-", "~"))
            return new CallExpression(token, "operator" + token.text, [], [parsePossiblePrefix()]);
        if (token = tryConsume("*"))
            return new DereferenceExpression(token, parsePossiblePrefix());
        if (token = tryConsume("&"))
            return new MakePtrExpression(token, parsePossiblePrefix());
        if (token = tryConsume("@"))
            return new MakeArrayRefExpression(token, parsePossiblePrefix());
        if (token = tryConsume("!")) {
            let remainder = parsePossiblePrefix();
            return new LogicalNot(token, new CallExpression(remainder.origin, "bool", [], [remainder]));
        }
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
        return parseLeftOperatorCall(["<", ">", "<=", ">="], parsePossibleShift);
    }
    
    function parsePossibleRelationalEquality()
    {
        return genericParseLeft(
            ["==", "!="], parsePossibleRelationalInequality,
            (token, left, right) => {
                let result = new CallExpression(token, "operator==", [], [left, right]);
                if (token.text == "!=")
                    result = new LogicalNot(token, result);
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
            (token, left, right) => new LogicalExpression(token, token.text, new CallExpression(left.origin, "bool", [], [left]), new CallExpression(right.origin, "bool", [], [right])));
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
        return finishParsingPreIncrement(operator, lhs, parsePossibleAssignment());
    }
    
    function parseAssignment()
    {
        return parsePossibleAssignment("required");
    }
    
    function parsePostIncrement()
    {
        let left = parseSuffixOperator(parseTerm(), ".", "->", "[");
        let token = consume("++", "--");
        return finishParsingPostIncrement(token, left);
    }
    
    function parseEffectfulExpression()
    {
        if (isCallExpression())
            return parseCallExpression();
        let preIncrement = lexer.backtrackingScope(parsePreIncrement);
        if (preIncrement)
            return preIncrement;
        let postIncrement = lexer.backtrackingScope(parsePostIncrement);
        if (postIncrement)
            return postIncrement;
        return parseAssignment();
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
        if (!list.length)
            throw new Error("Length should never be zero");
        if (list.length == 1)
            return list[0];
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
        if (tryConsume(";"))
            return new Return(origin, null);
        let expression = parseExpression();
        consume(";");
        return new Return(origin, expression);
    }
    
    function parseBreak()
    {
        let origin = consume("break");
        consume(";");
        return new Break(origin);
    }
    
    function parseContinue()
    {
        let origin = consume("continue");
        consume(";");
        return new Continue(origin);
    }

    function parseIfStatement()
    {
        let origin = consume("if");
        consume("(");
        let conditional = parseExpression();
        consume(")");
        let body = parseStatement();
        let elseBody;
        if (tryConsume("else"))
            elseBody = parseStatement();
        return new IfStatement(origin, new CallExpression(conditional.origin, "bool", [], [conditional]), body, elseBody);
    }

    function parseWhile()
    {
        let origin = consume("while");
        consume("(");
        let conditional = parseExpression();
        consume(")");
        let body = parseStatement();
        return new WhileLoop(origin, new CallExpression(conditional.origin, "bool", [], [conditional]), body);
    }

    function parseFor()
    {
        let origin = consume("for");
        consume("(");
        let initialization;
        if (tryConsume(";"))
            initialization = undefined;
        else {
            initialization = lexer.backtrackingScope(parseVariableDecls);
            if (!initialization)
                initialization = parseEffectfulStatement();
        }
        let condition = tryConsume(";");
        if (condition)
            condition = undefined;
        else {
            condition = parseExpression();
            consume(";");
            condition = new CallExpression(condition.origin, "bool", [], [condition]);
        }
        let increment;
        if (tryConsume(")"))
            increment = undefined;
        else {
            increment = parseExpression();
            consume(")");
        }
        let body = parseStatement();
        return new ForLoop(origin, initialization, condition, increment, body);
    }

    function parseDo()
    {
        let origin = consume("do");
        let body = parseStatement();
        consume("while");
        consume("(");
        let conditional = parseExpression();
        consume(")");
        return new DoWhileLoop(origin, body, new CallExpression(conditional.origin, "bool", [], [conditional]));
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
    
    function parseSwitchCase()
    {
        let token = consume("default", "case");
        let value;
        if (token.text == "case")
            value = parseConstexpr();
        consume(":");
        let body = parseBlockBody("}", "default", "case");
        return new SwitchCase(token, value, body);
    }
    
    function parseSwitchStatement()
    {
        let origin = consume("switch");
        consume("(");
        let value = parseExpression();
        consume(")");
        consume("{");
        let result = new SwitchStatement(origin, value);
        while (!tryConsume("}"))
            result.add(parseSwitchCase());
        return result;
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
        if (token.text == "if")
            return parseIfStatement();
        if (token.text == "switch")
            return parseSwitchStatement();
        if (token.text == "trap") {
            let origin = consume("trap");
            consume(";");
            return new TrapStatement(origin);
        }
        if (token.text == "{")
            return parseBlock();
        let variableDecl = lexer.backtrackingScope(parseVariableDecls);
        if (variableDecl)
            return variableDecl;
        return parseEffectfulStatement();
    }
    
    function parseBlockBody(...terminators)
    {
        let block = new Block(origin);
        while (!test(...terminators)) {
            let statement = parseStatement();
            if (statement)
                block.add(statement);
        }
        return block;
    }
    
    function parseBlock()
    {
        let origin = consume("{");
        let block = parseBlockBody("}");
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
            let token = consume("+", "-", "*", "/", "%", "^", "&", "|", "<", ">", "<=", ">=", "==", "++", "--", ".", "~", "<<", ">>", "[");
            if (token.text == "&") {
                if (tryConsume("[")) {
                    consume("]");
                    return "operator&[]";
                }
                if (tryConsume("."))
                    return "operator&." + consumeKind("identifier").text;
                return "operator&";
            }
            if (token.text == ".") {
                let result = "operator." + consumeKind("identifier").text;
                if (tryConsume("="))
                    result += "=";
                return result;
            }
            if (token.text == "[") {
                consume("]");
                let result = "operator[]";
                if (tryConsume("="))
                    result += "=";
                return result;
            }
            return "operator" + token.text;
        }
        return consumeKind("identifier").text;
    }

    function parseFuncDecl()
    {
        let origin;
        let returnType;
        let name;
        let typeParameters;
        let isCast;
        let shaderType;
        let operatorToken = tryConsume("operator");
        if (operatorToken) {
            origin = operatorToken;
            typeParameters = parseTypeParameters();
            returnType = parseType();
            name = "operator cast";
            isCast = true;
        } else {
            shaderType = tryConsume("vertex", "fragment");
            returnType = parseType();
            if (shaderType) {
                origin = shaderType;
                shaderType = shaderType.text;
            } else
                origin = returnType.origin;
            name = parseFuncName();
            typeParameters = parseTypeParameters();
            isCast = false;
        }
        let parameters = parseParameters();
        return new Func(origin, name, returnType, typeParameters, parameters, isCast, shaderType);
    }

    function parseProtocolFuncDecl()
    {
        let func = parseFuncDecl();
        return new ProtocolFuncDecl(func.origin, func.name, func.returnType, func.typeParameters, func.parameters, func.isCast, func.shaderType);
    }
    
    function parseFuncDef()
    {
        let func = parseFuncDecl();
        let body = parseBlock();
        return new FuncDef(func.origin, func.name, func.returnType, func.typeParameters, func.parameters, body, func.isCast, func.shaderType);
    }
    
    function parseProtocolDecl()
    {
        let origin = consume("protocol");
        let name = consumeKind("identifier").text;
        let result = new ProtocolDecl(origin, name);
        if (tryConsume(":")) {
            while (!test("{")) {
                result.addExtends(parseProtocolRef());
                if (!tryConsume(","))
                    break;
            }
        }
        consume("{");
        while (!tryConsume("}")) {
            result.add(parseProtocolFuncDecl());
            consume(";");
        }
        return result;
    }
    
    function parseField()
    {
        let type = parseType();
        let name = consumeKind("identifier");
        consume(";");
        return new Field(name, name.text, type);
    }
    
    function parseStructType()
    {
        let origin = consume("struct");
        let name = consumeKind("identifier").text;
        let typeParameters = parseTypeParameters();
        let result = new StructType(origin, name, typeParameters);
        consume("{");
        while (!tryConsume("}"))
            result.add(parseField());
        return result;
    }
    
    function parseNativeFunc()
    {
        let func = parseFuncDecl();
        consume(";");
        return new NativeFunc(func.origin, func.name, func.returnType, func.typeParameters, func.parameters, func.isCast, func.shaderType);
    }
    
    function parseNative()
    {
        let origin = consume("native");
        if (tryConsume("typedef")) {
            let name = consumeKind("identifier");
            let parameters = parseTypeParameters();
            consume(";");
            return new NativeType(origin, name.text, parameters);
        }
        return parseNativeFunc();
    }
    
    function parseRestrictedFuncDef()
    {
        consume("restricted");
        let result;
        if (tryConsume("native"))
            result = parseNativeFunc();
        else
            result = parseFuncDef();
        result.isRestricted = true;
        return result;
    }
    
    function parseEnumMember()
    {
        let name = consumeKind("identifier");
        let value = null;
        if (tryConsume("="))
            value = parseConstexpr();
        return new EnumMember(name, name.text, value);
    }
    
    function parseEnumType()
    {
        consume("enum");
        let name = consumeKind("identifier");
        let baseType;
        if (tryConsume(":"))
            baseType = parseType();
        else
            baseType = new TypeRef(name, "int", []);
        consume("{");
        let result = new EnumType(name, name.text, baseType);
        while (!test("}")) {
            result.add(parseEnumMember());
            if (!tryConsume(","))
                break;
        }
        consume("}");
        return result;
    }
    
    for (;;) {
        let token = lexer.peek();
        if (!token)
            return;
        if (token.text == ";")
            lexer.next();
        else if (token.text == "typedef")
            program.add(parseTypeDef());
        else if (originKind == "native" && token.text == "native")
            program.add(parseNative());
        else if (originKind == "native" && token.text == "restricted")
            program.add(parseRestrictedFuncDef());
        else if (token.text == "struct")
            program.add(parseStructType());
        else if (token.text == "enum")
            program.add(parseEnumType());
        else if (token.text == "protocol")
            program.add(parseProtocolDecl());
        else
            program.add(parseFuncDef());
    }
}

