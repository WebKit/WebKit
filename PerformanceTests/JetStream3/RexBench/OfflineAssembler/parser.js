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

const GlobalAnnotation = "global";
const LocalAnnotation = "local";

class SourceFile
{
    constructor(fileName)
    {
        this._fileName = fileName;
        let fileNumber = SourceFile.fileNames.indexOf(fileName);
        if (fileNumber == -1) {
            SourceFile.fileNames.push(fileName);
            fileNumber = SourceFile.fileNames.length;
        } else
            fileNumber++; // File numbers are 1 based

        this._fileNumber = fileNumber;
    }

    get name()
    {
        return this._fileName;
    }

    get fileNumber()
    {
        return this._fileNumber;
    }
}

SourceFile.fileNames = [];

class CodeOrigin
{
    constructor(sourceFile, lineNumber)
    {
        this._sourceFile = sourceFile;
        this._lineNumber = lineNumber;
    }

    fileName()
    {
        return this._sourceFile.name;
    }

    debugDirective()
    {
        return emitWinAsm ? undefined : "\".loc " + this._sourceFile.fileNumber + " " + this._lineNumber + "\\n\"";
    }

    toString()
    {
        return this.fileName() + ":" + this._lineNumber;
    }

    get lineNumber()
    {
        return this._lineNumber;
    }
}

class IncludeFile
{
    constructor(moduleName, defaultDir)
    {
        this._fileName = moduleName + ".asm";
    }

    toString()
    {
        return fileName;
    }

    get fileName()
    {
        return this._fileName;
    }
}

IncludeFile.includeDirs = [];


class Token
{
    constructor(codeOrigin, string)
    {
        this._codeOrigin = codeOrigin;
        this._string = string;
    }

    isEqualTo(other)
    {
        if (other instanceof Token)
            return this.string === other.string;

        return this.string === other;
    }

    isNotEqualTo(other)
    {
        return !this.isEqualTo(other);
    }

    get string()
    {
        return this._string;
    }

    get codeOrigin()
    {
        return this._codeOrigin;
    }

    toString()
    {
        return "" + this._string + "\" at " + this._codeOrigin;
    }

    parseError(comment)
    {
        if (!comment || !comment.length)
            throw "Parse error: " + this;

        throw "Parse error: " + this + ": " + comment;
    }
}

class Annotation
{
    constructor(codeOrigin, type, string)
    {
        this.codeOrigin = codeOrigin;
        this.type = type;
        this.string = string;
    }

    get codeOrigin()
    {
        return this.codeOrigin;
    }

    get type()
    {
        return this.type;
    }

    get string()
    {
        return this.string;
    }
}


// The lexer. Takes a string and returns an array of tokens.

function lex(str, file)
{
    function scanRegExp(source, regexp)
    {
        return source.match(regexp);
    }

    let result = [];
    let lineNumber = 1;
    let annotation = null;
    let annotationType;
    let whitespaceFound = false;

    while (str.length)
    {
        let tokenMatch;
        let annotation;
        let annotationType;

        if (tokenMatch = scanRegExp(str, /^#([^\n]*)/))
            ; // comment, ignore
        else if (tokenMatch = scanRegExp(str, /^\/\/\ ?([^\n]*)/)) {
            // annotation
            annotation = tokenMatch[0];
            annotationType = whitespaceFound ? LocalAnnotation : GlobalAnnotation;
        } else if (tokenMatch = scanRegExp(str, /^\n/)) {
            /* We've found a '\n'.  Emit the last comment recorded if appropriate:
             * We need to parse annotations regardless of whether the backend does
             * anything with them or not. This is because the C++ backend may make
             * use of this for its cloopDo debugging utility even if
             * enableInstrAnnotations is not enabled.
             */
            if (annotation) {
                result.push(new Annotation(new CodeOrigin(file, lineNumber),
                                           annotationType, annotation));
                annotation = null;
            }
            result.push(new Token(new CodeOrigin(file, lineNumber), tokenMatch[0]));
            lineNumber++;
        } else if (tokenMatch = scanRegExp(str, /^[a-zA-Z]([a-zA-Z0-9_.]*)/))
            result.push(new Token(new CodeOrigin(file, lineNumber), tokenMatch[0]));
        else if (tokenMatch = scanRegExp(str, /^\.([a-zA-Z0-9_]*)/))
            result.push(new Token(new CodeOrigin(file, lineNumber), tokenMatch[0]));
        else if (tokenMatch = scanRegExp(str, /^_([a-zA-Z0-9_]*)/))
            result.push(new Token(new CodeOrigin(file, lineNumber), tokenMatch[0]));
        else if (tokenMatch = scanRegExp(str, /^([ \t]+)/)) {
            // whitespace, ignore
            whitespaceFound = true;
            str = str.slice(tokenMatch[0].length);
            continue;
        } else if (tokenMatch = scanRegExp(str, /^0x([0-9a-fA-F]+)/))
            result.push(new Token(new CodeOrigin(file, lineNumber), Number.parseInt(tokenMatch[1], 16)));
        else if (tokenMatch = scanRegExp(str, /^0([0-7]+)/))
            result.push(new Token(new CodeOrigin(file, lineNumber), Number.parseInt(tokenMatch[1], 8)));
        else if (tokenMatch = scanRegExp(str, /^([0-9]+)/))
            result.push(new Token(new CodeOrigin(file, lineNumber), tokenMatch[0]));
        else if (tokenMatch = scanRegExp(str,  /^::/))
            result.push(new Token(new CodeOrigin(file, lineNumber), tokenMatch[0]));
        else if (tokenMatch = scanRegExp(str, /^[:,\(\)\[\]=\+\-~\|&^*]/))
            result.push(new Token(new CodeOrigin(file, lineNumber), tokenMatch[0]));
        else if (tokenMatch = scanRegExp(str, /^\".*\"/))
            result.push(new Token(new CodeOrigin(file, lineNumber), tokenMatch[0]));
        else
            throw "Lexer error at " + (new CodeOrigin(file, lineNumber)) + ", unexpected sequence " + str.slice(0, 20);

        whitespaceFound = false;
        str = str.slice(tokenMatch[0].length);
    }

    return result;
}

// Token identification.

function isRegister(token)
{
    registerPattern.test(token.string);
}

function isInstruction(token)
{
    return instructionSet.has(token.string);
}

function isKeyword(token)
{
    return /^((true)|(false)|(if)|(then)|(else)|(elsif)|(end)|(and)|(or)|(not)|(global)|(macro)|(const)|(constexpr)|(sizeof)|(error)|(include))$/.test(token.string)
        || isRegister(token)
        || isInstruction(token);
}

function isIdentifier(token)
{
    return /^[a-zA-Z]([a-zA-Z0-9_.]*)$/.test(token.string) && !isKeyword(token);
}

function isLabel(token)
{
    let tokenString;
    if (token instanceof Token)
        tokenString = token.string;
    else
        tokenString = token;

    return /^_([a-zA-Z0-9_]*)$/.test(tokenString);
}

function isLocalLabel(token)
{
    let tokenString;
    if (token instanceof Token)
        tokenString = token.string;
    else
        tokenString = token;

    return /^\.([a-zA-Z0-9_]*)$/.test(tokenString);
}

function isVariable(token)
{
    return isIdentifier(token) || isRegister(token);
}

function isInteger(token)
{
    return /^[0-9]/.test(token.string);
}

function isString(token)
{
    return /^".*"/.test(token);
}


// The parser. Takes an array of tokens and returns an AST. Methods
// other than parse(tokens) are not for public consumption.

class Parser
{
    constructor(data, fileName)
    {
        this.tokens = lex(data, fileName);
        this.idx = 0;
        this.annotation = null;
    }

    parseError(comment)
    {
        if (this.tokens[this.idx])
            this.tokens[this.idx].parseError(comment);
        else {
            if (!comment.length)
                throw "Parse error at end of file";

            throw "Parse error at end of file: " + comment;
        }
    }

    consume(regexp)
    {
        if (regexp) {
            if (!regexp.test(this.tokens[this.idx].string))
                this.parseError();
        } else if (this.idx != this.tokens.length)
            this.parseError();

        this.idx++;
    }

    skipNewLine()
    {
        while (this.tokens[this.idx].isEqualTo("\n"))
            this.idx++;
    }

    parsePredicateAtom()
    {
        if (this.tokens[this.idx].isEqualTo("not")) {
            let codeOrigin = this.tokens[this.idx].codeOrigin;
            this.idx++;
            return new Not(codeOrigin, this.parsePredicateAtom());
        }
        if (this.tokens[this.idx].isEqualTo("(")) {
            this.idx++;
            skipNewLine();
            let result = this.parsePredicate();
            if (this.tokens[this.idx].isNotEqualTo(")"))
                parseError();
            this.idx++;
            return result;
        }
        if (this.tokens[this.idx].isEqualTo("true")) {
            let result = True.instance();
            this.idx++;
            return result;
        }
        if (this.tokens[this.idx].isEqualTo("false")) {
            let result = False.instance();
            this.idx++;
            return result;
        }
        if (isIdentifier(this.tokens[this.idx])) {
            let result = Setting.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string);
            this.idx++;
            return result;
        }

        this.parseError();
    }

    parsePredicateAnd()
    {
        let result = this.parsePredicateAtom();
        while (this.tokens[this.idx].isEqualTo("and")) {
            let codeOrigin = this.tokens[this.idx].codeOrigin;
            this.idx++;
            this.skipNewLine();
            let right = this.parsePredicateAtom();
            result = new And(codeOrigin, result, right);
        }

        return result;
    }

    parsePredicate()
    {
        // some examples of precedence:
        // not a and b -> (not a) and b
        // a and b or c -> (a and b) or c
        // a or b and c -> a or (b and c)

        let result = this.parsePredicateAnd();
        while (this.tokens[this.idx].isEqualTo("or")) {
            let codeOrigin = this.tokens[this.idx].codeOrigin;
            this.idx++;
            this.skipNewLine();
            let right = this.parsePredicateAnd();
            result = new Or(codeOrigin, result, right);
        }

        return result;
    }

    parseVariable()
    {
        let result;
        if (isRegister(this.tokens[this.idx])) {
            if (fprPattern.test(this.tokens[this.idx]))
                result = FPRegisterID.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string);
            else
                result = RegisterID.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string);
        } else if (isIdentifier(this.tokens[this.idx]))
            result = Variable.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string);
        else
            this.parseError();

        this.idx++;
        return result;
    }

    parseAddress(offset)
    {
        if (this.tokens[this.idx].isNotEqualTo("["))
            this.parseError();

        let codeOrigin = this.tokens[this.idx].codeOrigin;
        let result;

        // Three possibilities:
        // []       -> AbsoluteAddress
        // [a]      -> Address
        // [a,b]    -> BaseIndex with scale = 1
        // [a,b,c]  -> BaseIndex

        this.idx++;
        if (this.tokens[this.idx].isEqualTo("]")) {
            this.idx++;
            return new AbsoluteAddress(codeOrigin, offset);
        }
        let a = this.parseVariable();
        if (this.tokens[this.idx].isEqualTo("]"))
            result = new Address(codeOrigin, a, offset);
        else
        {
            if (this.tokens[this.idx].isNotEqualTo(","))
                this.parseError();
            this.idx++;
            let b = this.parseVariable();
            if (this.tokens[this.idx].isEqualTo("]"))
                result = new BaseIndex(codeOrigin, a, b, 1, offset);
            else {
                if (this.tokens[this.idx].isNotEqualTo(","))
                    this.parseError();
                this.idx++;
                if (!["1", "2", "4", "8"].includes(this.tokens[this.idx].string))
                    this.parseError();
                let c = Number.parseInt(this.tokens[this.idx]);
                this.idx++;
                if (this.tokens[this.idx].isNotEqualTo("]"))
                    this.parseError();
                result = new BaseIndex(codeOrigin, a, b, c, offset);
            }
        }
        this.idx++;
        return result;
    }

    parseColonColon()
    {
        this.skipNewLine();
        let firstToken = this.tokens[this.idx];
        let codeOrigin = this.tokens[this.idx].codeOrigin;
        if (!isIdentifier(this.tokens[this.idx]))
            this.parseError();
        let names = [this.tokens[this.idx].string];
        this.idx++;
        while (this.tokens[this.idx].isEqualTo("::")) {
            this.idx++;
            if (!isIdentifier(this.tokens[this.idx]))
                this.parseError();
            names.push(this.tokens[this.idx].string);
            this.idx++;
        }
        if (!names.length)
            firstToken.parseError();
        return { codeOrigin: codeOrigin, names: names };
    }

    parseTextInParens()
    {
        this.skipNewLine();
        let codeOrigin = this.tokens[this.idx].codeOrigin;
        if (this.tokens[this.idx].isNotEqualTo("("))
            throw "Missing \"(\" at " + codeOrigin;
        this.idx++;
        // need at least one item
        if (this.tokens[this.idx].isEqualTo(")"))
            throw "No items in list at " + codeOrigin;
        let numEnclosedParens = 0;
        let text = [];
        while (this.tokens[this.idx].isNotEqualTo(")") || numEnclosedParens > 0) {
            if (this.tokens[this.idx].isEqualTo("("))
                numEnclosedParens++;
            else if (this.tokens[this.idx].isEqualTo(")"))
                numEnclosedParens--;

            text.push(this.tokens[this.idx].string);
            this.idx++;
        }
        this.idx++;
        return { codeOrigin: codeOrigin, text: text };
    }

    parseExpressionAtom()
    {
        let result;
        this.skipNewLine();
        if (this.tokens[this.idx].isEqualTo("-")) {
            this.idx++;
            return new NegImmediate(this.tokens[this.idx - 1].codeOrigin, this.parseExpressionAtom());
        }
        if (this.tokens[this.idx].isEqualTo("~")) {
            this.idx++;
            return new BitnotImmediate(this.tokens[this.idx - 1].codeOrigin, this.parseExpressionAtom());
        }
        if (this.tokens[this.idx].isEqualTo("(")) {
            this.idx++;
            result = this.parseExpression();
            if (this.tokens[this.idx].isNotEqualTo(")"))
                this.parseError();
            this.idx++;
            return result;
        }
        if (isInteger(this.tokens[this.idx])) {
            result = new Immediate(this.tokens[this.idx].codeOrigin, Number.parseInt(this.tokens[this.idx]));
            this.idx++;
            return result;
        }
        if (isString(this.tokens[this.idx])) {
            result = new StringLiteral(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string);
            this.idx++;
            return result;
        }
        if (isIdentifier(this.tokens[this.idx])) {
            let {codeOrigin, names} = this.parseColonColon();
            if (names.length > 1)
                return StructOffset.forField(codeOrigin, names.slice(0, -1).join('::'), names[names.length - 1]);

            return Variable.forName(codeOrigin, names[0]);
        }
        if (isRegister(this.tokens[this.idx]))
            return this.parseVariable();
        if (this.tokens[this.idx].isEqualTo("sizeof")) {
            this.idx++;
            let {codeOrigin, names} = this.parseColonColon();
            return Sizeof.forName(codeOrigin, names.join("::"));
        }
        if (this.tokens[this.idx].isEqualTo("constexpr")) {
            this.idx++;
            this.skipNewLine();
            let codeOrigin;
            let text;
            let names;
            if (this.tokens[this.idx].isEqualTo("(")) {
                ({ codeOrigin, text } = this.parseTextInParens());
                text = text.join("");
            } else {
                ({ codeOrigin, names } = this.parseColonColon());
                text = names.join("::");
            }
            return ConstExpr.forName(codeOrigin, text);
        }
        if (isLabel(this.tokens[this.idx])) {
            result = new LabelReference(this.tokens[this.idx].codeOrigin, Label.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string));
            this.idx++;
            return result;
        }
        if (isLocalLabel(this.tokens[this.idx])) {
            result = new LocalLabelReference(this.tokens[this.idx].codeOrigin, LocalLabel.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string));
            this.idx++;
            return result;
        }
        this.parseError();
    }

    parseExpressionMul()
    {
        this.skipNewLine();
        let result = this.parseExpressionAtom();
        while (this.tokens[this.idx].isEqualTo("*")) {
            if (this.tokens[this.idx].isEqualTo("*")) {
                this.idx++;
                result = new MulImmediates(this.tokens[this.idx - 1].codeOrigin, result, this.parseExpressionAtom());
            } else
                throw "Invalid token " + this.tokens[this.idx] + " in multiply expression";
        }
        return result;
    }

    couldBeExpression()
    {
        return this.tokens[this.idx].isEqualTo("-") || this.tokens[this.idx].isEqualTo("~") || this.tokens[this.idx].isEqualTo("constexpr") || this.tokens[this.idx].isEqualTo("sizeof") || isInteger(this.tokens[this.idx]) || isString(this.tokens[this.idx]) || isVariable(this.tokens[this.idx]) || this.tokens[this.idx].isEqualTo("(");
    }

    parseExpressionAdd()
    {
        this.skipNewLine();
        let result = this.parseExpressionMul();
        while (this.tokens[this.idx].isEqualTo("+") || this.tokens[this.idx].isEqualTo("-")) {
            if (this.tokens[this.idx].isEqualTo("+")) {
                this.idx++;
                result = new AddImmediates(this.tokens[this.idx - 1].codeOrigin, result, this.parseExpressionMul());
            } else if (this.tokens[this.idx].isEqualTo("-")) {
                this.idx++;
                result = new SubImmediates(this.tokens[this.idx - 1].codeOrigin, result, this.parseExpressionMul());
            } else
                throw "Invalid token " + this.tokens[this.idx] + " in addition expression";
        }
        return result;
    }

    parseExpressionAnd()
    {
        this.skipNewLine();
        let result = this.parseExpressionAdd();
        while (this.tokens[this.idx].isEqualTo("&")) {
            this.idx++;
            result = new AndImmediates(this.tokens[this.idx - 1].codeOrigin, result, this.parseExpressionAdd());
        }
        return result;
    }

    parseExpression()
    {
        this.skipNewLine();
        let result = this.parseExpressionAnd();
        while (this.tokens[this.idx].isEqualTo("|") || this.tokens[this.idx].isEqualTo("^")) {
            if (this.tokens[this.idx].isEqualTo("|")) {
                this.idx++;
                result = new OrImmediates(this.tokens[this.idx - 1].codeOrigin, result, this.parseExpressionAnd());
            } else if (this.tokens[this.idx].isEqualTo("^")) {
                this.idx++;
                result = new XorImmediates(this.tokens[this.idx - 1].codeOrigin, result, this.parseExpressionAnd());
            } else
                throw "Invalid token " + this.tokens[this.idx] + " in expression";
        }
        return result;
    }

    parseOperand(comment)
    {
        this.skipNewLine();
        if (this.couldBeExpression()) {
            let expr = this.parseExpression();
            if (this.tokens[this.idx].isEqualTo("["))
                return this.parseAddress(expr);

            return expr;
        }
        if (this.tokens[this.idx].isEqualTo("["))
            return this.parseAddress(new Immediate(this.tokens[this.idx].codeOrigin, 0));
        if (isLabel(this.tokens[this.idx])) {
            let result = new LabelReference(this.tokens[this.idx].codeOrigin, Label.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string));
            this.idx++;
            return result;
        }
        if (isLocalLabel(this.tokens[this.idx])) {
            let result = new LocalLabelReference(this.tokens[this.idx].codeOrigin, LocalLabel.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string));
            this.idx++;
            return result;
        }

        this.parseError(comment);
    }

    parseMacroVariables()
    {
        this.skipNewLine();
        this.consume(/^\($/);
        let variables = [];
        while (true) {
            this.skipNewLine();
            if (this.tokens[this.idx].isEqualTo(")")) {
                this.idx++;
                break
            } else if (isIdentifier(this.tokens[this.idx])) {
                 variables.push(Variable.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string));
                this.idx++;
                this.skipNewLine();
                if (this.tokens[this.idx].isEqualTo(")")) {
                    this.idx++;
                    break;
                } else if (this.tokens[this.idx].isEqualTo(","))
                    this.idx++;
                else
                    this.parseError();
            } else
                this.parseError();
        }
        return variables;
    }

    parseSequence(final, comment)
    {
        let firstCodeOrigin = this.tokens[this.idx].codeOrigin;
        let list = [];
        while (true) {
            if ((this.idx == this.tokens.length && !final) || (final && final.test(this.tokens[this.idx].string)))
                break;
            else if (this.tokens[this.idx] instanceof Annotation) {
                // This is the only place where we can encounter a global
                // annotation, and hence need to be able to distinguish between
                // them.
                // globalAnnotations are the ones that start from column 0. All
                // others are considered localAnnotations.  The only reason to
                // distinguish between them is so that we can format the output
                // nicely as one would expect.

                let codeOrigin = this.tokens[this.idx].codeOrigin;
                let annotationOpcode = (this.tokens[this.idx].type == GlobalAnnotation) ? "globalAnnotation" : "localAnnotation";
                list.push(new Instruction(codeOrigin, annotationOpcode, [], this.tokens[this.idx].string));
                this.annotation = null;
                this.idx += 2 // Consume the newline as well.
            } else if (this.tokens[this.idx].isEqualTo("\n")) {
                // ignore
                this.idx++;
            } else if (this.tokens[this.idx].isEqualTo("const")) {
                this.idx++;
                if (!isVariable(this.tokens[this.idx]))
                    this.parseError();
                let variable = Variable.forName(this.tokens[this.idx].codeOrigin, this.tokens[this.idx].string);
                this.idx++;
                if (this.tokens[this.idx].isNotEqualTo("="))
                    this.parseError();
                this.idx++;
                let value = this.parseOperand("while inside of const " + variable.name);
                list.push(new ConstDecl(this.tokens[this.idx].codeOrigin, variable, value));
            } else if (this.tokens[this.idx].isEqualTo("error")) {
                list.push(new Error(this.tokens[this.idx].codeOrigin));
                this.idx++;
            } else if (this.tokens[this.idx].isEqualTo("if")) {
                let codeOrigin = this.tokens[this.idx].codeOrigin;
                this.idx++;
                this.skipNewLine();
                let predicate = this.parsePredicate();
                this.consume(/^((then)|(\n))$/);
                this.skipNewLine();
                let ifThenElse = new IfThenElse(codeOrigin, predicate, this.parseSequence(/^((else)|(end)|(elsif))$/, "while inside of \"if " + predicate.dump() + "\""));
                list.push(ifThenElse);
                while (this.tokens[this.idx].isEqualTo("elsif")) {
                    codeOrigin = this.tokens[this.idx].codeOrigin;
                    this.idx++;
                    this.skipNewLine();
                    predicate = this.parsePredicate();
                    this.consume(/^((then)|(\n))$/);
                    this.skipNewLine();
                    let elseCase = new IfThenElse(codeOrigin, predicate, this.parseSequence(/^((else)|(end)|(elsif))$/, "while inside of \"if " + predicate.dump() + "\""));
                    ifThenElse.elseCase = elseCase;
                    ifThenElse = elseCase;
                }
                if (this.tokens[this.idx].isEqualTo("else")) {
                    this.idx++;
                    ifThenElse.elseCase = this.parseSequence(/^end$/, "while inside of else case for \"if " + predicate.dump() + "\"");
                    this.idx++;
                } else {
                    if (this.tokens[this.idx].isNotEqualTo("end"))
                        this.parseError();
                    this.idx++;
                }
            } else if (this.tokens[this.idx].isEqualTo("macro")) {
                let codeOrigin = this.tokens[this.idx].codeOrigin;
                this.idx++;
                this.skipNewLine();
                if (!isIdentifier(this.tokens[this.idx]))
                    this.parseError();
                let name = this.tokens[this.idx].string;
                this.idx++;
                let variables = this.parseMacroVariables();
                let body = this.parseSequence(/^end$/, "while inside of macro " + name);
                this.idx++;
                list.push(new Macro(codeOrigin, name, variables, body));
            } else if (this.tokens[this.idx].isEqualTo("global")) {
                let codeOrigin = this.tokens[this.idx].codeOrigin;
                this.idx++;
                this.skipNewLine();
                if (!isLabel(this.tokens[this.idx]))
                    this.parseError();
                let name = this.tokens[this.idx].string;
                this.idx++;
                Label.setAsGlobal(codeOrigin, name);
            } else if (isInstruction(this.tokens[this.idx])) {
                let codeOrigin = this.tokens[this.idx].codeOrigin;
                let name = this.tokens[this.idx].string;
                this.idx++;
                if ((!final && this.idx == this.tokens.size) || (final && final.test(this.tokens[this.idx]))) {
                    // Zero operand instruction, and it's the last one.
                    list.push(new Instruction(codeOrigin, name, [], this.annotation));
                    this.annotation = null;
                    break;
                } else if (this.tokens[this.idx] instanceof Annotation) {
                    list.push(new Instruction(codeOrigin, name, [], this.tokens[this.idx].string));
                    this.annotation = null;
                    this.idx += 2 // Consume the newline as well.
                } else if (this.tokens[this.idx].isEqualTo("\n")) {
                    // Zero operand instruction.
                    list.push(new Instruction(codeOrigin, name, [], this.annotation));
                    this.annotation = null;
                    this.idx++;
                } else {
                    // It's definitely an instruction, and it has at least one operand.
                    let operands = [];
                    let endOfSequence = false;
                    while (true) {
                        operands.push(this.parseOperand("while inside of instruction " + name));
                        if ((!final && this.idx == this.tokens.size) || (final && final.test(this.tokens[this.idx].string))) {
                            // The end of the instruction and of the sequence.
                            endOfSequence = true;
                            break;
                        } else if (this.tokens[this.idx].isEqualTo(",")) {
                            // Has another operand.
                            this.idx++;
                        } else if (this.tokens[this.idx] instanceof Annotation) {
                            this.annotation = this.tokens[this.idx].string;
                            this.idx += 2; // Consume the newline as well.
                            break;
                        } else if (this.tokens[this.idx].isEqualTo("\n")) {
                            // The end of the instruction.
                            this.idx++;
                            break;
                        } else
                            this.parseError("Expected a comma, newline, or " + final + " after " + operands[operands.length - 1].dump());
                    }
                    list.push(new Instruction(codeOrigin, name, operands, this.annotation));
                    this.annotation = null;
                    if (endOfSequence)
                        break;
                }
            } else if (isIdentifier(this.tokens[this.idx])) {
                // Check for potential macro invocation:
                let codeOrigin = this.tokens[this.idx].codeOrigin;
                let name = this.tokens[this.idx].string;
                this.idx++;
                if (this.tokens[this.idx].isEqualTo("(")) {
                    // Macro invocation.
                    this.idx++;
                    let operands = [];
                    this.skipNewLine();
                    if (this.tokens[this.idx].isEqualTo(")"))
                        this.idx++;
                    else {
                        while (true) {
                            this.skipNewLine();
                            if (this.tokens[this.idx].isEqualTo("macro")) {
                                // It's a macro lambda!
                                let codeOriginInner = this.tokens[this.idx].codeOrigin;
                                this.idx++;
                                let variables = this.parseMacroVariables();
                                let body = this.parseSequence(/^end$/, "while inside of anonymous macro passed as argument to " + name);
                                this.idx++;
                                operands.push(new Macro(codeOriginInner, "", variables, body));
                            } else
                                operands.push(this.parseOperand("while inside of macro call to " + name));

                            this.skipNewLine();
                            if (this.tokens[this.idx].isEqualTo(")")) {
                                this.idx++;
                                break
                            } else if (this.tokens[this.idx].isEqualTo(","))
                                this.idx++;
                            else
                                this.parseError("Unexpected " + this.tokens[this.idx].string + " while parsing invocation of macro " + name);

                        }
                    }
                    // Check if there's a trailing annotation after the macro invoke:
                    if (this.tokens[this.idx] instanceof Annotation) {
                        this.annotation = this.tokens[this.idx].string;
                        this.idx += 2 // Consume the newline as well.
                    }
                    list.push(new MacroCall(codeOrigin, name, operands, this.annotation));
                    this.annotation = null;
                } else
                    this.parseError("Expected \"(\" after " + name);
            } else if (isLabel(this.tokens[this.idx]) || isLocalLabel(this.tokens[this.idx])) {
                let codeOrigin = this.tokens[this.idx].codeOrigin;
                let name = this.tokens[this.idx].string;
                this.idx++;
                if (this.tokens[this.idx].isNotEqualTo(":"))
                    this.parseError();
                // It's a label.
                if (isLabel(name))
                    list.push(Label.forName(codeOrigin, name, true));
                else
                    list.push(LocalLabel.forName(codeOrigin, name));

                this.idx++;
            } else if (this.tokens[this.idx].isEqualTo("include")) {
                this.idx++;
                if (!isIdentifier(this.tokens[this.idx]))
                    this.parseError();
                let moduleName = this.tokens[this.idx].string;
                let fileName = new IncludeFile(moduleName, this.tokens[this.idx].codeOrigin.fileName.dirname).fileName;
                this.idx++;
                list.push(parse(fileName));
            } else
                this.parseError("Expecting terminal " + final + " " + comment);
        }
        return new Sequence(firstCodeOrigin, list);
    }

    parseIncludes(final, comment)
    {
        let firstCodeOrigin = this.tokens[this.idx].codeOrigin;
        let fileList = [];
        fileList.push(this.tokens[this.idx].codeOrigin.fileName);
        while (true) {
            if ((this.idx == this.tokens.length && !final) || (final && final.test(this.tokens[this.idx])))
                break;
            else if (this.tokens[this.idx].isEqualTo("include")) {
                this.idx++;
                if (!isIdentifier(this.tokens[this.idx]))
                    this.parseError();
                let moduleName = this.tokens[this.idx].string;
                let fileName = new IncludeFile(moduleName, this.tokens[this.idx].codeOrigin.fileName.dirname).fileName;
                this.idx++;

                fileList.push(fileName);
            } else
                this.idx++;
        }

        return fileList;
    }
}

function parseData(data, fileName)
{
    let parser = new Parser(data, new SourceFile(fileName));
    return parser.parseSequence(null, "");
}

function parse(fileName)
{
    return parseData(File.open(fileName).read(), fileName);
}
