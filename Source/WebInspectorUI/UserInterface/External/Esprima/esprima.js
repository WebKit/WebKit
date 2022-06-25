(function webpackUniversalModuleDefinition(root, factory) {
/* istanbul ignore next */
    if(typeof exports === 'object' && typeof module === 'object')
        module.exports = factory();
    else if(typeof define === 'function' && define.amd)
        define([], factory);
/* istanbul ignore next */
    else if(typeof exports === 'object')
        exports["esprima"] = factory();
    else
        root["esprima"] = factory();
})(this, function() {
return /******/ (function(modules) { // webpackBootstrap
/******/     // The module cache
/******/     var installedModules = {};

/******/     // The require function
/******/     function __webpack_require__(moduleId) {

/******/         // Check if module is in cache
/* istanbul ignore if */
/******/         if(installedModules[moduleId])
/******/             return installedModules[moduleId].exports;

/******/         // Create a new module (and put it into the cache)
/******/         var module = installedModules[moduleId] = {
/******/             exports: {},
/******/             id: moduleId,
/******/             loaded: false
/******/         };

/******/         // Execute the module function
/******/         modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);

/******/         // Flag the module as loaded
/******/         module.loaded = true;

/******/         // Return the exports of the module
/******/         return module.exports;
/******/     }


/******/     // expose the modules object (__webpack_modules__)
/******/     __webpack_require__.m = modules;

/******/     // expose the module cache
/******/     __webpack_require__.c = installedModules;

/******/     // __webpack_public_path__
/******/     __webpack_require__.p = "";

/******/     // Load entry module and return exports
/******/     return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ function(module, exports, __webpack_require__) {

    "use strict";
    /*
      Copyright JS Foundation and other contributors, https://js.foundation/

      Redistribution and use in source and binary forms, with or without
      modification, are permitted provided that the following conditions are met:

        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
          notice, this list of conditions and the following disclaimer in the
          documentation and/or other materials provided with the distribution.

      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
      AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
      IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
      ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
      DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
      (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
      LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
      ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
      (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
      THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    */
    Object.defineProperty(exports, "__esModule", { value: true });
    const comment_handler_1 = __webpack_require__(1);
    const jsx_parser_1 = __webpack_require__(3);
    const parser_1 = __webpack_require__(8);
    const tokenizer_1 = __webpack_require__(15);
    function parse(code, options, delegate) {
        let commentHandler = null;
        const proxyDelegate = (node, metadata) => {
            if (delegate) {
                delegate(node, metadata);
            }
            if (commentHandler) {
                commentHandler.visit(node, metadata);
            }
        };
        let parserDelegate = (typeof delegate === 'function') ? proxyDelegate : null;
        let collectComment = false;
        if (options) {
            collectComment = (typeof options.comment === 'boolean' && options.comment);
            const attachComment = (typeof options.attachComment === 'boolean' && options.attachComment);
            if (collectComment || attachComment) {
                commentHandler = new comment_handler_1.CommentHandler();
                commentHandler.attach = attachComment;
                options.comment = true;
                parserDelegate = proxyDelegate;
            }
        }
        let isModule = false;
        if (options && typeof options.sourceType === 'string') {
            isModule = (options.sourceType === 'module');
        }
        let parser;
        if (options && typeof options.jsx === 'boolean' && options.jsx) {
            parser = new jsx_parser_1.JSXParser(code, options, parserDelegate);
        }
        else {
            parser = new parser_1.Parser(code, options, parserDelegate);
        }
        const program = isModule ? parser.parseModule() : parser.parseScript();
        const ast = program;
        if (collectComment && commentHandler) {
            ast.comments = commentHandler.comments;
        }
        if (parser.config.tokens) {
            ast.tokens = parser.tokens;
        }
        if (parser.config.tolerant) {
            ast.errors = parser.errorHandler.errors;
        }
        return ast;
    }
    exports.parse = parse;
    function parseModule(code, options, delegate) {
        const parsingOptions = options || {};
        parsingOptions.sourceType = 'module';
        return parse(code, parsingOptions, delegate);
    }
    exports.parseModule = parseModule;
    function parseScript(code, options, delegate) {
        const parsingOptions = options || {};
        parsingOptions.sourceType = 'script';
        return parse(code, parsingOptions, delegate);
    }
    exports.parseScript = parseScript;
    function tokenize(code, options, delegate) {
        const tokenizer = new tokenizer_1.Tokenizer(code, options);
        let tokens;
        tokens = [];
        try {
            while (true) {
                let token = tokenizer.getNextToken();
                if (!token) {
                    break;
                }
                if (delegate) {
                    token = delegate(token);
                }
                tokens.push(token);
            }
        }
        catch (e) {
            tokenizer.errorHandler.tolerate(e);
        }
        if (tokenizer.errorHandler.tolerant) {
            tokens.errors = tokenizer.errors();
        }
        return tokens;
    }
    exports.tokenize = tokenize;
    var syntax_1 = __webpack_require__(2);
    exports.Syntax = syntax_1.Syntax;
    // Sync with *.json manifests.
    exports.version = '4.0.0-dev';


/***/ },
/* 1 */
/***/ function(module, exports, __webpack_require__) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    const syntax_1 = __webpack_require__(2);
    class CommentHandler {
        constructor() {
            this.attach = false;
            this.comments = [];
            this.stack = [];
            this.leading = [];
            this.trailing = [];
        }
        insertInnerComments(node, metadata) {
            //  innnerComments for properties empty block
            //  `function a() {/** comments **\/}`
            if (node.type === syntax_1.Syntax.BlockStatement && node.body.length === 0) {
                const innerComments = [];
                for (let i = this.leading.length - 1; i >= 0; --i) {
                    const entry = this.leading[i];
                    if (metadata.end.offset >= entry.start) {
                        innerComments.unshift(entry.comment);
                        this.leading.splice(i, 1);
                        this.trailing.splice(i, 1);
                    }
                }
                if (innerComments.length) {
                    node.innerComments = innerComments;
                }
            }
        }
        findTrailingComments(metadata) {
            let trailingComments = [];
            if (this.trailing.length > 0) {
                for (let i = this.trailing.length - 1; i >= 0; --i) {
                    const entry = this.trailing[i];
                    if (entry.start >= metadata.end.offset) {
                        trailingComments.unshift(entry.comment);
                    }
                }
                this.trailing.length = 0;
                return trailingComments;
            }
            const last = this.stack[this.stack.length - 1];
            if (last && last.node.trailingComments) {
                const firstComment = last.node.trailingComments[0];
                if (firstComment && firstComment.range[0] >= metadata.end.offset) {
                    trailingComments = last.node.trailingComments;
                    delete last.node.trailingComments;
                }
            }
            return trailingComments;
        }
        findLeadingComments(metadata) {
            const leadingComments = [];
            let target;
            while (this.stack.length > 0) {
                const entry = this.stack[this.stack.length - 1];
                if (entry && entry.start >= metadata.start.offset) {
                    target = entry.node;
                    this.stack.pop();
                }
                else {
                    break;
                }
            }
            if (target) {
                const count = target.leadingComments ? target.leadingComments.length : 0;
                for (let i = count - 1; i >= 0; --i) {
                    const comment = target.leadingComments[i];
                    if (comment.range[1] <= metadata.start.offset) {
                        leadingComments.unshift(comment);
                        target.leadingComments.splice(i, 1);
                    }
                }
                if (target.leadingComments && target.leadingComments.length === 0) {
                    delete target.leadingComments;
                }
                return leadingComments;
            }
            for (let i = this.leading.length - 1; i >= 0; --i) {
                const entry = this.leading[i];
                if (entry.start <= metadata.start.offset) {
                    leadingComments.unshift(entry.comment);
                    this.leading.splice(i, 1);
                }
            }
            return leadingComments;
        }
        visitNode(node, metadata) {
            if (node.type === syntax_1.Syntax.Program && node.body.length > 0) {
                return;
            }
            this.insertInnerComments(node, metadata);
            const trailingComments = this.findTrailingComments(metadata);
            const leadingComments = this.findLeadingComments(metadata);
            if (leadingComments.length > 0) {
                node.leadingComments = leadingComments;
            }
            if (trailingComments.length > 0) {
                node.trailingComments = trailingComments;
            }
            this.stack.push({
                node: node,
                start: metadata.start.offset
            });
        }
        visitComment(node, metadata) {
            const type = (node.type[0] === 'L') ? 'Line' : 'Block';
            const comment = {
                type: type,
                value: node.value
            };
            if (node.range) {
                comment.range = node.range;
            }
            if (node.loc) {
                comment.loc = node.loc;
            }
            this.comments.push(comment);
            if (this.attach) {
                const entry = {
                    comment: {
                        type: type,
                        value: node.value,
                        range: [metadata.start.offset, metadata.end.offset]
                    },
                    start: metadata.start.offset
                };
                if (node.loc) {
                    entry.comment.loc = node.loc;
                }
                node.type = type;
                this.leading.push(entry);
                this.trailing.push(entry);
            }
        }
        visit(node, metadata) {
            if (node.type === 'LineComment') {
                this.visitComment(node, metadata);
            }
            else if (node.type === 'BlockComment') {
                this.visitComment(node, metadata);
            }
            else if (this.attach) {
                this.visitNode(node, metadata);
            }
        }
    }
    exports.CommentHandler = CommentHandler;


/***/ },
/* 2 */
/***/ function(module, exports) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.Syntax = {
        AssignmentExpression: 'AssignmentExpression',
        AssignmentPattern: 'AssignmentPattern',
        ArrayExpression: 'ArrayExpression',
        ArrayPattern: 'ArrayPattern',
        ArrowFunctionExpression: 'ArrowFunctionExpression',
        AwaitExpression: 'AwaitExpression',
        BlockStatement: 'BlockStatement',
        BinaryExpression: 'BinaryExpression',
        BreakStatement: 'BreakStatement',
        CallExpression: 'CallExpression',
        CatchClause: 'CatchClause',
        ClassBody: 'ClassBody',
        ClassDeclaration: 'ClassDeclaration',
        ClassExpression: 'ClassExpression',
        ConditionalExpression: 'ConditionalExpression',
        ContinueStatement: 'ContinueStatement',
        DoWhileStatement: 'DoWhileStatement',
        DebuggerStatement: 'DebuggerStatement',
        EmptyStatement: 'EmptyStatement',
        ExportAllDeclaration: 'ExportAllDeclaration',
        ExportDefaultDeclaration: 'ExportDefaultDeclaration',
        ExportNamedDeclaration: 'ExportNamedDeclaration',
        ExportSpecifier: 'ExportSpecifier',
        ExpressionStatement: 'ExpressionStatement',
        ForStatement: 'ForStatement',
        ForOfStatement: 'ForOfStatement',
        ForInStatement: 'ForInStatement',
        FunctionDeclaration: 'FunctionDeclaration',
        FunctionExpression: 'FunctionExpression',
        Identifier: 'Identifier',
        IfStatement: 'IfStatement',
        Import: 'Import',
        ImportDeclaration: 'ImportDeclaration',
        ImportDefaultSpecifier: 'ImportDefaultSpecifier',
        ImportNamespaceSpecifier: 'ImportNamespaceSpecifier',
        ImportSpecifier: 'ImportSpecifier',
        Literal: 'Literal',
        LabeledStatement: 'LabeledStatement',
        LogicalExpression: 'LogicalExpression',
        MemberExpression: 'MemberExpression',
        MetaProperty: 'MetaProperty',
        MethodDefinition: 'MethodDefinition',
        NewExpression: 'NewExpression',
        ObjectExpression: 'ObjectExpression',
        ObjectPattern: 'ObjectPattern',
        Program: 'Program',
        Property: 'Property',
        RestElement: 'RestElement',
        ReturnStatement: 'ReturnStatement',
        SequenceExpression: 'SequenceExpression',
        SpreadElement: 'SpreadElement',
        Super: 'Super',
        SwitchCase: 'SwitchCase',
        SwitchStatement: 'SwitchStatement',
        TaggedTemplateExpression: 'TaggedTemplateExpression',
        TemplateElement: 'TemplateElement',
        TemplateLiteral: 'TemplateLiteral',
        ThisExpression: 'ThisExpression',
        ThrowStatement: 'ThrowStatement',
        TryStatement: 'TryStatement',
        UnaryExpression: 'UnaryExpression',
        UpdateExpression: 'UpdateExpression',
        VariableDeclaration: 'VariableDeclaration',
        VariableDeclarator: 'VariableDeclarator',
        WhileStatement: 'WhileStatement',
        WithStatement: 'WithStatement',
        YieldExpression: 'YieldExpression'
    };


/***/ },
/* 3 */
/***/ function(module, exports, __webpack_require__) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    const character_1 = __webpack_require__(4);
    const JSXNode = __webpack_require__(5);
    const jsx_syntax_1 = __webpack_require__(6);
    const Node = __webpack_require__(7);
    const parser_1 = __webpack_require__(8);
    const token_1 = __webpack_require__(13);
    const xhtml_entities_1 = __webpack_require__(14);
    token_1.TokenName[100 /* Identifier */] = 'JSXIdentifier';
    token_1.TokenName[101 /* Text */] = 'JSXText';
    // Fully qualified element name, e.g. <svg:path> returns "svg:path"
    function getQualifiedElementName(elementName) {
        let qualifiedName;
        switch (elementName.type) {
            case jsx_syntax_1.JSXSyntax.JSXIdentifier:
                const id = elementName;
                qualifiedName = id.name;
                break;
            case jsx_syntax_1.JSXSyntax.JSXNamespacedName:
                const ns = elementName;
                qualifiedName = getQualifiedElementName(ns.namespace) + ':' +
                    getQualifiedElementName(ns.name);
                break;
            case jsx_syntax_1.JSXSyntax.JSXMemberExpression:
                const expr = elementName;
                qualifiedName = getQualifiedElementName(expr.object) + '.' +
                    getQualifiedElementName(expr.property);
                break;
            /* istanbul ignore next */
            default:
                break;
        }
        return qualifiedName;
    }
    class JSXParser extends parser_1.Parser {
        constructor(code, options, delegate) {
            super(code, options, delegate);
        }
        parsePrimaryExpression() {
            return this.match('<') ? this.parseJSXRoot() : super.parsePrimaryExpression();
        }
        startJSX() {
            // Unwind the scanner before the lookahead token.
            this.scanner.index = this.startMarker.index;
            this.scanner.lineNumber = this.startMarker.line;
            this.scanner.lineStart = this.startMarker.index - this.startMarker.column;
        }
        finishJSX() {
            // Prime the next lookahead.
            this.nextToken();
        }
        reenterJSX() {
            this.startJSX();
            this.expectJSX('}');
            // Pop the closing '}' added from the lookahead.
            if (this.config.tokens) {
                this.tokens.pop();
            }
        }
        createJSXNode() {
            this.collectComments();
            return {
                index: this.scanner.index,
                line: this.scanner.lineNumber,
                column: this.scanner.index - this.scanner.lineStart
            };
        }
        createJSXChildNode() {
            return {
                index: this.scanner.index,
                line: this.scanner.lineNumber,
                column: this.scanner.index - this.scanner.lineStart
            };
        }
        scanXHTMLEntity(quote) {
            let result = '&';
            let valid = true;
            let terminated = false;
            let numeric = false;
            let hex = false;
            while (!this.scanner.eof() && valid && !terminated) {
                const ch = this.scanner.source[this.scanner.index];
                if (ch === quote) {
                    break;
                }
                terminated = (ch === ';');
                result += ch;
                ++this.scanner.index;
                if (!terminated) {
                    switch (result.length) {
                        case 2:
                            // e.g. '&#123;'
                            numeric = (ch === '#');
                            break;
                        case 3:
                            if (numeric) {
                                // e.g. '&#x41;'
                                hex = (ch === 'x');
                                valid = hex || character_1.Character.isDecimalDigit(ch.charCodeAt(0));
                                numeric = numeric && !hex;
                            }
                            break;
                        default:
                            valid = valid && !(numeric && !character_1.Character.isDecimalDigit(ch.charCodeAt(0)));
                            valid = valid && !(hex && !character_1.Character.isHexDigit(ch.charCodeAt(0)));
                            break;
                    }
                }
            }
            if (valid && terminated && result.length > 2) {
                // e.g. '&#x41;' becomes just '#x41'
                const str = result.substr(1, result.length - 2);
                if (numeric && str.length > 1) {
                    result = String.fromCharCode(parseInt(str.substr(1), 10));
                }
                else if (hex && str.length > 2) {
                    result = String.fromCharCode(parseInt('0' + str.substr(1), 16));
                }
                else if (!numeric && !hex && xhtml_entities_1.XHTMLEntities[str]) {
                    result = xhtml_entities_1.XHTMLEntities[str];
                }
            }
            return result;
        }
        // Scan the next JSX token. This replaces Scanner#lex when in JSX mode.
        lexJSX() {
            const cp = this.scanner.source.charCodeAt(this.scanner.index);
            // < > / : = { }
            if (cp === 60 || cp === 62 || cp === 47 || cp === 58 || cp === 61 || cp === 123 || cp === 125) {
                const value = this.scanner.source[this.scanner.index++];
                return {
                    type: 7 /* Punctuator */,
                    value: value,
                    lineNumber: this.scanner.lineNumber,
                    lineStart: this.scanner.lineStart,
                    start: this.scanner.index - 1,
                    end: this.scanner.index
                };
            }
            // " '
            if (cp === 34 || cp === 39) {
                const start = this.scanner.index;
                const quote = this.scanner.source[this.scanner.index++];
                let str = '';
                while (!this.scanner.eof()) {
                    const ch = this.scanner.source[this.scanner.index++];
                    if (ch === quote) {
                        break;
                    }
                    else if (ch === '&') {
                        str += this.scanXHTMLEntity(quote);
                    }
                    else {
                        str += ch;
                    }
                }
                return {
                    type: 8 /* StringLiteral */,
                    value: str,
                    lineNumber: this.scanner.lineNumber,
                    lineStart: this.scanner.lineStart,
                    start: start,
                    end: this.scanner.index
                };
            }
            // ... or .
            if (cp === 46) {
                const n1 = this.scanner.source.charCodeAt(this.scanner.index + 1);
                const n2 = this.scanner.source.charCodeAt(this.scanner.index + 2);
                const value = (n1 === 46 && n2 === 46) ? '...' : '.';
                const start = this.scanner.index;
                this.scanner.index += value.length;
                return {
                    type: 7 /* Punctuator */,
                    value: value,
                    lineNumber: this.scanner.lineNumber,
                    lineStart: this.scanner.lineStart,
                    start: start,
                    end: this.scanner.index
                };
            }
            // `
            if (cp === 96) {
                // Only placeholder, since it will be rescanned as a real assignment expression.
                return {
                    type: 10 /* Template */,
                    value: '',
                    lineNumber: this.scanner.lineNumber,
                    lineStart: this.scanner.lineStart,
                    start: this.scanner.index,
                    end: this.scanner.index
                };
            }
            // Identifer can not contain backslash (char code 92).
            if (character_1.Character.isIdentifierStart(cp) && (cp !== 92)) {
                const start = this.scanner.index;
                ++this.scanner.index;
                while (!this.scanner.eof()) {
                    const ch = this.scanner.source.charCodeAt(this.scanner.index);
                    if (character_1.Character.isIdentifierPart(ch) && (ch !== 92)) {
                        ++this.scanner.index;
                    }
                    else if (ch === 45) {
                        // Hyphen (char code 45) can be part of an identifier.
                        ++this.scanner.index;
                    }
                    else {
                        break;
                    }
                }
                const id = this.scanner.source.slice(start, this.scanner.index);
                return {
                    type: 100 /* Identifier */,
                    value: id,
                    lineNumber: this.scanner.lineNumber,
                    lineStart: this.scanner.lineStart,
                    start: start,
                    end: this.scanner.index
                };
            }
            return this.scanner.lex();
        }
        nextJSXToken() {
            this.collectComments();
            this.startMarker.index = this.scanner.index;
            this.startMarker.line = this.scanner.lineNumber;
            this.startMarker.column = this.scanner.index - this.scanner.lineStart;
            const token = this.lexJSX();
            this.lastMarker.index = this.scanner.index;
            this.lastMarker.line = this.scanner.lineNumber;
            this.lastMarker.column = this.scanner.index - this.scanner.lineStart;
            if (this.config.tokens) {
                this.tokens.push(this.convertToken(token));
            }
            return token;
        }
        nextJSXText() {
            this.startMarker.index = this.scanner.index;
            this.startMarker.line = this.scanner.lineNumber;
            this.startMarker.column = this.scanner.index - this.scanner.lineStart;
            const start = this.scanner.index;
            let text = '';
            while (!this.scanner.eof()) {
                const ch = this.scanner.source[this.scanner.index];
                if (ch === '{' || ch === '<') {
                    break;
                }
                ++this.scanner.index;
                text += ch;
                if (character_1.Character.isLineTerminator(ch.charCodeAt(0))) {
                    ++this.scanner.lineNumber;
                    if (ch === '\r' && this.scanner.source[this.scanner.index] === '\n') {
                        ++this.scanner.index;
                    }
                    this.scanner.lineStart = this.scanner.index;
                }
            }
            this.lastMarker.index = this.scanner.index;
            this.lastMarker.line = this.scanner.lineNumber;
            this.lastMarker.column = this.scanner.index - this.scanner.lineStart;
            const token = {
                type: 101 /* Text */,
                value: text,
                lineNumber: this.scanner.lineNumber,
                lineStart: this.scanner.lineStart,
                start: start,
                end: this.scanner.index
            };
            if ((text.length > 0) && this.config.tokens) {
                this.tokens.push(this.convertToken(token));
            }
            return token;
        }
        peekJSXToken() {
            const state = this.scanner.saveState();
            this.scanner.scanComments();
            const next = this.lexJSX();
            this.scanner.restoreState(state);
            return next;
        }
        // Expect the next JSX token to match the specified punctuator.
        // If not, an exception will be thrown.
        expectJSX(value) {
            const token = this.nextJSXToken();
            if (token.type !== 7 /* Punctuator */ || token.value !== value) {
                this.throwUnexpectedToken(token);
            }
        }
        // Return true if the next JSX token matches the specified punctuator.
        matchJSX(value) {
            const next = this.peekJSXToken();
            return next.type === 7 /* Punctuator */ && next.value === value;
        }
        parseJSXIdentifier() {
            const node = this.createJSXNode();
            const token = this.nextJSXToken();
            if (token.type !== 100 /* Identifier */) {
                this.throwUnexpectedToken(token);
            }
            return this.finalize(node, new JSXNode.JSXIdentifier(token.value));
        }
        parseJSXElementName() {
            const node = this.createJSXNode();
            let elementName = this.parseJSXIdentifier();
            if (this.matchJSX(':')) {
                const namespace = elementName;
                this.expectJSX(':');
                const name = this.parseJSXIdentifier();
                elementName = this.finalize(node, new JSXNode.JSXNamespacedName(namespace, name));
            }
            else if (this.matchJSX('.')) {
                while (this.matchJSX('.')) {
                    const object = elementName;
                    this.expectJSX('.');
                    const property = this.parseJSXIdentifier();
                    elementName = this.finalize(node, new JSXNode.JSXMemberExpression(object, property));
                }
            }
            return elementName;
        }
        parseJSXAttributeName() {
            const node = this.createJSXNode();
            let attributeName;
            const identifier = this.parseJSXIdentifier();
            if (this.matchJSX(':')) {
                const namespace = identifier;
                this.expectJSX(':');
                const name = this.parseJSXIdentifier();
                attributeName = this.finalize(node, new JSXNode.JSXNamespacedName(namespace, name));
            }
            else {
                attributeName = identifier;
            }
            return attributeName;
        }
        parseJSXStringLiteralAttribute() {
            const node = this.createJSXNode();
            const token = this.nextJSXToken();
            if (token.type !== 8 /* StringLiteral */) {
                this.throwUnexpectedToken(token);
            }
            const raw = this.getTokenRaw(token);
            return this.finalize(node, new Node.Literal(token.value, raw));
        }
        parseJSXExpressionAttribute() {
            const node = this.createJSXNode();
            this.expectJSX('{');
            this.finishJSX();
            if (this.match('}')) {
                this.tolerateError('JSX attributes must only be assigned a non-empty expression');
            }
            const expression = this.parseAssignmentExpression();
            this.reenterJSX();
            return this.finalize(node, new JSXNode.JSXExpressionContainer(expression));
        }
        parseJSXAttributeValue() {
            return this.matchJSX('{') ? this.parseJSXExpressionAttribute() :
                this.matchJSX('<') ? this.parseJSXElement() : this.parseJSXStringLiteralAttribute();
        }
        parseJSXNameValueAttribute() {
            const node = this.createJSXNode();
            const name = this.parseJSXAttributeName();
            let value = null;
            if (this.matchJSX('=')) {
                this.expectJSX('=');
                value = this.parseJSXAttributeValue();
            }
            return this.finalize(node, new JSXNode.JSXAttribute(name, value));
        }
        parseJSXSpreadAttribute() {
            const node = this.createJSXNode();
            this.expectJSX('{');
            this.expectJSX('...');
            this.finishJSX();
            const argument = this.parseAssignmentExpression();
            this.reenterJSX();
            return this.finalize(node, new JSXNode.JSXSpreadAttribute(argument));
        }
        parseJSXAttributes() {
            const attributes = [];
            while (!this.matchJSX('/') && !this.matchJSX('>')) {
                const attribute = this.matchJSX('{') ? this.parseJSXSpreadAttribute() :
                    this.parseJSXNameValueAttribute();
                attributes.push(attribute);
            }
            return attributes;
        }
        parseJSXOpeningElement() {
            const node = this.createJSXNode();
            this.expectJSX('<');
            const name = this.parseJSXElementName();
            const attributes = this.parseJSXAttributes();
            const selfClosing = this.matchJSX('/');
            if (selfClosing) {
                this.expectJSX('/');
            }
            this.expectJSX('>');
            return this.finalize(node, new JSXNode.JSXOpeningElement(name, selfClosing, attributes));
        }
        parseJSXBoundaryElement() {
            const node = this.createJSXNode();
            this.expectJSX('<');
            if (this.matchJSX('/')) {
                this.expectJSX('/');
                const elementName = this.parseJSXElementName();
                this.expectJSX('>');
                return this.finalize(node, new JSXNode.JSXClosingElement(elementName));
            }
            const name = this.parseJSXElementName();
            const attributes = this.parseJSXAttributes();
            const selfClosing = this.matchJSX('/');
            if (selfClosing) {
                this.expectJSX('/');
            }
            this.expectJSX('>');
            return this.finalize(node, new JSXNode.JSXOpeningElement(name, selfClosing, attributes));
        }
        parseJSXEmptyExpression() {
            const node = this.createJSXChildNode();
            this.collectComments();
            this.lastMarker.index = this.scanner.index;
            this.lastMarker.line = this.scanner.lineNumber;
            this.lastMarker.column = this.scanner.index - this.scanner.lineStart;
            return this.finalize(node, new JSXNode.JSXEmptyExpression());
        }
        parseJSXExpressionContainer() {
            const node = this.createJSXNode();
            this.expectJSX('{');
            let expression;
            if (this.matchJSX('}')) {
                expression = this.parseJSXEmptyExpression();
                this.expectJSX('}');
            }
            else {
                this.finishJSX();
                expression = this.parseAssignmentExpression();
                this.reenterJSX();
            }
            return this.finalize(node, new JSXNode.JSXExpressionContainer(expression));
        }
        parseJSXChildren() {
            const children = [];
            while (!this.scanner.eof()) {
                const node = this.createJSXChildNode();
                const token = this.nextJSXText();
                if (token.start < token.end) {
                    const raw = this.getTokenRaw(token);
                    const child = this.finalize(node, new JSXNode.JSXText(token.value, raw));
                    children.push(child);
                }
                if (this.scanner.source[this.scanner.index] === '{') {
                    const container = this.parseJSXExpressionContainer();
                    children.push(container);
                }
                else {
                    break;
                }
            }
            return children;
        }
        parseComplexJSXElement(el) {
            const stack = [];
            while (!this.scanner.eof()) {
                el.children = el.children.concat(this.parseJSXChildren());
                const node = this.createJSXChildNode();
                const element = this.parseJSXBoundaryElement();
                if (element.type === jsx_syntax_1.JSXSyntax.JSXOpeningElement) {
                    const opening = element;
                    if (opening.selfClosing) {
                        const child = this.finalize(node, new JSXNode.JSXElement(opening, [], null));
                        el.children.push(child);
                    }
                    else {
                        stack.push(el);
                        el = { node, opening, closing: null, children: [] };
                    }
                }
                if (element.type === jsx_syntax_1.JSXSyntax.JSXClosingElement) {
                    el.closing = element;
                    const open = getQualifiedElementName(el.opening.name);
                    const close = getQualifiedElementName(el.closing.name);
                    if (open !== close) {
                        this.tolerateError('Expected corresponding JSX closing tag for %0', open);
                    }
                    if (stack.length > 0) {
                        const child = this.finalize(el.node, new JSXNode.JSXElement(el.opening, el.children, el.closing));
                        el = stack[stack.length - 1];
                        el.children.push(child);
                        stack.pop();
                    }
                    else {
                        break;
                    }
                }
            }
            return el;
        }
        parseJSXElement() {
            const node = this.createJSXNode();
            const opening = this.parseJSXOpeningElement();
            let children = [];
            let closing = null;
            if (!opening.selfClosing) {
                const el = this.parseComplexJSXElement({ node, opening, closing, children });
                children = el.children;
                closing = el.closing;
            }
            return this.finalize(node, new JSXNode.JSXElement(opening, children, closing));
        }
        parseJSXRoot() {
            // Pop the opening '<' added from the lookahead.
            if (this.config.tokens) {
                this.tokens.pop();
            }
            this.startJSX();
            const element = this.parseJSXElement();
            this.finishJSX();
            return element;
        }
        isStartOfExpression() {
            return super.isStartOfExpression() || this.match('<');
        }
    }
    exports.JSXParser = JSXParser;


/***/ },
/* 4 */
/***/ function(module, exports) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    // See also tools/generate-unicode-regex.js.
    const Regex = {
        // Unicode v12.1.0 NonAsciiIdentifierStart:
        NonAsciiIdentifierStart: /[\xAA\xB5\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0559\u0560-\u0588\u05D0-\u05EA\u05EF-\u05F2\u0620-\u064A\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE\u06EF\u06FA-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07CA-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u0860-\u086A\u08A0-\u08B4\u08B6-\u08BD\u0904-\u0939\u093D\u0950\u0958-\u0961\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09F0\u09F1\u09FC\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0AF9\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B71\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58-\u0C5A\u0C60\u0C61\u0C80\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDE\u0CE0\u0CE1\u0CF1\u0CF2\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D54-\u0D56\u0D5F-\u0D61\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E81\u0E82\u0E84\u0E86-\u0E8A\u0E8C-\u0EA3\u0EA5\u0EA7-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0EDC-\u0EDF\u0F00\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1711\u1720-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u1820-\u1878\u1880-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1950-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u1A00-\u1A16\u1A20-\u1A54\u1AA7\u1B05-\u1B33\u1B45-\u1B4B\u1B83-\u1BA0\u1BAE\u1BAF\u1BBA-\u1BE5\u1C00-\u1C23\u1C4D-\u1C4F\u1C5A-\u1C7D\u1C80-\u1C88\u1C90-\u1CBA\u1CBD-\u1CBF\u1CE9-\u1CEC\u1CEE-\u1CF3\u1CF5\u1CF6\u1CFA\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2071\u207F\u2090-\u209C\u2102\u2107\u210A-\u2113\u2115\u2118-\u211D\u2124\u2126\u2128\u212A-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309B-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312F\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FEF\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA61F\uA62A\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA7BF\uA7C2-\uA7C6\uA7F7-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA840-\uA873\uA882-\uA8B3\uA8F2-\uA8F7\uA8FB\uA8FD\uA8FE\uA90A-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF\uA9E0-\uA9E4\uA9E6-\uA9EF\uA9FA-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB67\uAB70-\uABE2\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD40-\uDD74\uDE80-\uDE9C\uDEA0-\uDED0\uDF00-\uDF1F\uDF2D-\uDF4A\uDF50-\uDF75\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCB0-\uDCD3\uDCD8-\uDCFB\uDD00-\uDD27\uDD30-\uDD63\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC60-\uDC76\uDC80-\uDC9E\uDCE0-\uDCF2\uDCF4\uDCF5\uDD00-\uDD15\uDD20-\uDD39\uDD80-\uDDB7\uDDBE\uDDBF\uDE00\uDE10-\uDE13\uDE15-\uDE17\uDE19-\uDE35\uDE60-\uDE7C\uDE80-\uDE9C\uDEC0-\uDEC7\uDEC9-\uDEE4\uDF00-\uDF35\uDF40-\uDF55\uDF60-\uDF72\uDF80-\uDF91]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2\uDD00-\uDD23\uDF00-\uDF1C\uDF27\uDF30-\uDF45\uDFE0-\uDFF6]|\uD804[\uDC03-\uDC37\uDC83-\uDCAF\uDCD0-\uDCE8\uDD03-\uDD26\uDD44\uDD50-\uDD72\uDD76\uDD83-\uDDB2\uDDC1-\uDDC4\uDDDA\uDDDC\uDE00-\uDE11\uDE13-\uDE2B\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEDE\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3D\uDF50\uDF5D-\uDF61]|\uD805[\uDC00-\uDC34\uDC47-\uDC4A\uDC5F\uDC80-\uDCAF\uDCC4\uDCC5\uDCC7\uDD80-\uDDAE\uDDD8-\uDDDB\uDE00-\uDE2F\uDE44\uDE80-\uDEAA\uDEB8\uDF00-\uDF1A]|\uD806[\uDC00-\uDC2B\uDCA0-\uDCDF\uDCFF\uDDA0-\uDDA7\uDDAA-\uDDD0\uDDE1\uDDE3\uDE00\uDE0B-\uDE32\uDE3A\uDE50\uDE5C-\uDE89\uDE9D\uDEC0-\uDEF8]|\uD807[\uDC00-\uDC08\uDC0A-\uDC2E\uDC40\uDC72-\uDC8F\uDD00-\uDD06\uDD08\uDD09\uDD0B-\uDD30\uDD46\uDD60-\uDD65\uDD67\uDD68\uDD6A-\uDD89\uDD98\uDEE0-\uDEF2]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6E\uDC80-\uDD43]|[\uD80C\uD81C-\uD820\uD840-\uD868\uD86A-\uD86C\uD86F-\uD872\uD874-\uD879][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2E]|\uD811[\uDC00-\uDE46]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDED0-\uDEED\uDF00-\uDF2F\uDF40-\uDF43\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDE40-\uDE7F\uDF00-\uDF4A\uDF50\uDF93-\uDF9F\uDFE0\uDFE1\uDFE3]|\uD821[\uDC00-\uDFF7]|\uD822[\uDC00-\uDEF2]|\uD82C[\uDC00-\uDD1E\uDD50-\uDD52\uDD64-\uDD67\uDD70-\uDEFB]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA5\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB]|\uD838[\uDD00-\uDD2C\uDD37-\uDD3D\uDD4E\uDEC0-\uDEEB]|\uD83A[\uDC00-\uDCC4\uDD00-\uDD43\uDD4B]|\uD83B[\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD869[\uDC00-\uDED6\uDF00-\uDFFF]|\uD86D[\uDC00-\uDF34\uDF40-\uDFFF]|\uD86E[\uDC00-\uDC1D\uDC20-\uDFFF]|\uD873[\uDC00-\uDEA1\uDEB0-\uDFFF]|\uD87A[\uDC00-\uDFE0]|\uD87E[\uDC00-\uDE1D]/,
        // Unicode v12.1.0 NonAsciiIdentifierPart:
        NonAsciiIdentifierPart: /[\xAA\xB5\xB7\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0300-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u0483-\u0487\u048A-\u052F\u0531-\u0556\u0559\u0560-\u0588\u0591-\u05BD\u05BF\u05C1\u05C2\u05C4\u05C5\u05C7\u05D0-\u05EA\u05EF-\u05F2\u0610-\u061A\u0620-\u0669\u066E-\u06D3\u06D5-\u06DC\u06DF-\u06E8\u06EA-\u06FC\u06FF\u0710-\u074A\u074D-\u07B1\u07C0-\u07F5\u07FA\u07FD\u0800-\u082D\u0840-\u085B\u0860-\u086A\u08A0-\u08B4\u08B6-\u08BD\u08D3-\u08E1\u08E3-\u0963\u0966-\u096F\u0971-\u0983\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BC-\u09C4\u09C7\u09C8\u09CB-\u09CE\u09D7\u09DC\u09DD\u09DF-\u09E3\u09E6-\u09F1\u09FC\u09FE\u0A01-\u0A03\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A3C\u0A3E-\u0A42\u0A47\u0A48\u0A4B-\u0A4D\u0A51\u0A59-\u0A5C\u0A5E\u0A66-\u0A75\u0A81-\u0A83\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABC-\u0AC5\u0AC7-\u0AC9\u0ACB-\u0ACD\u0AD0\u0AE0-\u0AE3\u0AE6-\u0AEF\u0AF9-\u0AFF\u0B01-\u0B03\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3C-\u0B44\u0B47\u0B48\u0B4B-\u0B4D\u0B56\u0B57\u0B5C\u0B5D\u0B5F-\u0B63\u0B66-\u0B6F\u0B71\u0B82\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BBE-\u0BC2\u0BC6-\u0BC8\u0BCA-\u0BCD\u0BD0\u0BD7\u0BE6-\u0BEF\u0C00-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D-\u0C44\u0C46-\u0C48\u0C4A-\u0C4D\u0C55\u0C56\u0C58-\u0C5A\u0C60-\u0C63\u0C66-\u0C6F\u0C80-\u0C83\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBC-\u0CC4\u0CC6-\u0CC8\u0CCA-\u0CCD\u0CD5\u0CD6\u0CDE\u0CE0-\u0CE3\u0CE6-\u0CEF\u0CF1\u0CF2\u0D00-\u0D03\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D44\u0D46-\u0D48\u0D4A-\u0D4E\u0D54-\u0D57\u0D5F-\u0D63\u0D66-\u0D6F\u0D7A-\u0D7F\u0D82\u0D83\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DCA\u0DCF-\u0DD4\u0DD6\u0DD8-\u0DDF\u0DE6-\u0DEF\u0DF2\u0DF3\u0E01-\u0E3A\u0E40-\u0E4E\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E86-\u0E8A\u0E8C-\u0EA3\u0EA5\u0EA7-\u0EBD\u0EC0-\u0EC4\u0EC6\u0EC8-\u0ECD\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F18\u0F19\u0F20-\u0F29\u0F35\u0F37\u0F39\u0F3E-\u0F47\u0F49-\u0F6C\u0F71-\u0F84\u0F86-\u0F97\u0F99-\u0FBC\u0FC6\u1000-\u1049\u1050-\u109D\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u135D-\u135F\u1369-\u1371\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1714\u1720-\u1734\u1740-\u1753\u1760-\u176C\u176E-\u1770\u1772\u1773\u1780-\u17D3\u17D7\u17DC\u17DD\u17E0-\u17E9\u180B-\u180D\u1810-\u1819\u1820-\u1878\u1880-\u18AA\u18B0-\u18F5\u1900-\u191E\u1920-\u192B\u1930-\u193B\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19DA\u1A00-\u1A1B\u1A20-\u1A5E\u1A60-\u1A7C\u1A7F-\u1A89\u1A90-\u1A99\u1AA7\u1AB0-\u1ABD\u1B00-\u1B4B\u1B50-\u1B59\u1B6B-\u1B73\u1B80-\u1BF3\u1C00-\u1C37\u1C40-\u1C49\u1C4D-\u1C7D\u1C80-\u1C88\u1C90-\u1CBA\u1CBD-\u1CBF\u1CD0-\u1CD2\u1CD4-\u1CFA\u1D00-\u1DF9\u1DFB-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u200C\u200D\u203F\u2040\u2054\u2071\u207F\u2090-\u209C\u20D0-\u20DC\u20E1\u20E5-\u20F0\u2102\u2107\u210A-\u2113\u2115\u2118-\u211D\u2124\u2126\u2128\u212A-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D7F-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2DE0-\u2DFF\u3005-\u3007\u3021-\u302F\u3031-\u3035\u3038-\u303C\u3041-\u3096\u3099-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312F\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FEF\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66F\uA674-\uA67D\uA67F-\uA6F1\uA717-\uA71F\uA722-\uA788\uA78B-\uA7BF\uA7C2-\uA7C6\uA7F7-\uA827\uA840-\uA873\uA880-\uA8C5\uA8D0-\uA8D9\uA8E0-\uA8F7\uA8FB\uA8FD-\uA92D\uA930-\uA953\uA960-\uA97C\uA980-\uA9C0\uA9CF-\uA9D9\uA9E0-\uA9FE\uAA00-\uAA36\uAA40-\uAA4D\uAA50-\uAA59\uAA60-\uAA76\uAA7A-\uAAC2\uAADB-\uAADD\uAAE0-\uAAEF\uAAF2-\uAAF6\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB67\uAB70-\uABEA\uABEC\uABED\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE00-\uFE0F\uFE20-\uFE2F\uFE33\uFE34\uFE4D-\uFE4F\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF3F\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD40-\uDD74\uDDFD\uDE80-\uDE9C\uDEA0-\uDED0\uDEE0\uDF00-\uDF1F\uDF2D-\uDF4A\uDF50-\uDF7A\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCA0-\uDCA9\uDCB0-\uDCD3\uDCD8-\uDCFB\uDD00-\uDD27\uDD30-\uDD63\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC60-\uDC76\uDC80-\uDC9E\uDCE0-\uDCF2\uDCF4\uDCF5\uDD00-\uDD15\uDD20-\uDD39\uDD80-\uDDB7\uDDBE\uDDBF\uDE00-\uDE03\uDE05\uDE06\uDE0C-\uDE13\uDE15-\uDE17\uDE19-\uDE35\uDE38-\uDE3A\uDE3F\uDE60-\uDE7C\uDE80-\uDE9C\uDEC0-\uDEC7\uDEC9-\uDEE6\uDF00-\uDF35\uDF40-\uDF55\uDF60-\uDF72\uDF80-\uDF91]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2\uDD00-\uDD27\uDD30-\uDD39\uDF00-\uDF1C\uDF27\uDF30-\uDF50\uDFE0-\uDFF6]|\uD804[\uDC00-\uDC46\uDC66-\uDC6F\uDC7F-\uDCBA\uDCD0-\uDCE8\uDCF0-\uDCF9\uDD00-\uDD34\uDD36-\uDD3F\uDD44-\uDD46\uDD50-\uDD73\uDD76\uDD80-\uDDC4\uDDC9-\uDDCC\uDDD0-\uDDDA\uDDDC\uDE00-\uDE11\uDE13-\uDE37\uDE3E\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEEA\uDEF0-\uDEF9\uDF00-\uDF03\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3B-\uDF44\uDF47\uDF48\uDF4B-\uDF4D\uDF50\uDF57\uDF5D-\uDF63\uDF66-\uDF6C\uDF70-\uDF74]|\uD805[\uDC00-\uDC4A\uDC50-\uDC59\uDC5E\uDC5F\uDC80-\uDCC5\uDCC7\uDCD0-\uDCD9\uDD80-\uDDB5\uDDB8-\uDDC0\uDDD8-\uDDDD\uDE00-\uDE40\uDE44\uDE50-\uDE59\uDE80-\uDEB8\uDEC0-\uDEC9\uDF00-\uDF1A\uDF1D-\uDF2B\uDF30-\uDF39]|\uD806[\uDC00-\uDC3A\uDCA0-\uDCE9\uDCFF\uDDA0-\uDDA7\uDDAA-\uDDD7\uDDDA-\uDDE1\uDDE3\uDDE4\uDE00-\uDE3E\uDE47\uDE50-\uDE99\uDE9D\uDEC0-\uDEF8]|\uD807[\uDC00-\uDC08\uDC0A-\uDC36\uDC38-\uDC40\uDC50-\uDC59\uDC72-\uDC8F\uDC92-\uDCA7\uDCA9-\uDCB6\uDD00-\uDD06\uDD08\uDD09\uDD0B-\uDD36\uDD3A\uDD3C\uDD3D\uDD3F-\uDD47\uDD50-\uDD59\uDD60-\uDD65\uDD67\uDD68\uDD6A-\uDD8E\uDD90\uDD91\uDD93-\uDD98\uDDA0-\uDDA9\uDEE0-\uDEF6]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6E\uDC80-\uDD43]|[\uD80C\uD81C-\uD820\uD840-\uD868\uD86A-\uD86C\uD86F-\uD872\uD874-\uD879][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2E]|\uD811[\uDC00-\uDE46]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDE60-\uDE69\uDED0-\uDEED\uDEF0-\uDEF4\uDF00-\uDF36\uDF40-\uDF43\uDF50-\uDF59\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDE40-\uDE7F\uDF00-\uDF4A\uDF4F-\uDF87\uDF8F-\uDF9F\uDFE0\uDFE1\uDFE3]|\uD821[\uDC00-\uDFF7]|\uD822[\uDC00-\uDEF2]|\uD82C[\uDC00-\uDD1E\uDD50-\uDD52\uDD64-\uDD67\uDD70-\uDEFB]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99\uDC9D\uDC9E]|\uD834[\uDD65-\uDD69\uDD6D-\uDD72\uDD7B-\uDD82\uDD85-\uDD8B\uDDAA-\uDDAD\uDE42-\uDE44]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA5\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB\uDFCE-\uDFFF]|\uD836[\uDE00-\uDE36\uDE3B-\uDE6C\uDE75\uDE84\uDE9B-\uDE9F\uDEA1-\uDEAF]|\uD838[\uDC00-\uDC06\uDC08-\uDC18\uDC1B-\uDC21\uDC23\uDC24\uDC26-\uDC2A\uDD00-\uDD2C\uDD30-\uDD3D\uDD40-\uDD49\uDD4E\uDEC0-\uDEF9]|\uD83A[\uDC00-\uDCC4\uDCD0-\uDCD6\uDD00-\uDD4B\uDD50-\uDD59]|\uD83B[\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD869[\uDC00-\uDED6\uDF00-\uDFFF]|\uD86D[\uDC00-\uDF34\uDF40-\uDFFF]|\uD86E[\uDC00-\uDC1D\uDC20-\uDFFF]|\uD873[\uDC00-\uDEA1\uDEB0-\uDFFF]|\uD87A[\uDC00-\uDFE0]|\uD87E[\uDC00-\uDE1D]|\uDB40[\uDD00-\uDDEF]/
    };
    exports.Character = {
        /* tslint:disable:no-bitwise */
        fromCodePoint(cp) {
            return (cp < 0x10000) ? String.fromCharCode(cp) :
                String.fromCharCode(0xD800 + ((cp - 0x10000) >> 10)) +
                    String.fromCharCode(0xDC00 + ((cp - 0x10000) & 1023));
        },
        // https://tc39.github.io/ecma262/#sec-white-space
        isWhiteSpace(cp) {
            return (cp === 0x20) || (cp === 0x09) || (cp === 0x0B) || (cp === 0x0C) || (cp === 0xA0) ||
                (cp >= 0x1680 && [0x1680, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A, 0x202F, 0x205F, 0x3000, 0xFEFF].indexOf(cp) >= 0);
        },
        // https://tc39.github.io/ecma262/#sec-line-terminators
        isLineTerminator(cp) {
            return (cp === 0x0A) || (cp === 0x0D) || (cp === 0x2028) || (cp === 0x2029);
        },
        // https://tc39.github.io/ecma262/#sec-names-and-keywords
        isIdentifierStart(cp) {
            return (cp === 0x24) || (cp === 0x5F) || // $ (dollar) and _ (underscore)
                (cp >= 0x41 && cp <= 0x5A) || // A..Z
                (cp >= 0x61 && cp <= 0x7A) || // a..z
                (cp === 0x5C) || // \ (backslash)
                ((cp >= 0x80) && Regex.NonAsciiIdentifierStart.test(exports.Character.fromCodePoint(cp)));
        },
        isIdentifierPart(cp) {
            return (cp === 0x24) || (cp === 0x5F) || // $ (dollar) and _ (underscore)
                (cp >= 0x41 && cp <= 0x5A) || // A..Z
                (cp >= 0x61 && cp <= 0x7A) || // a..z
                (cp >= 0x30 && cp <= 0x39) || // 0..9
                (cp === 0x5C) || // \ (backslash)
                ((cp >= 0x80) && Regex.NonAsciiIdentifierPart.test(exports.Character.fromCodePoint(cp)));
        },
        // https://tc39.github.io/ecma262/#sec-literals-numeric-literals
        isDecimalDigit(cp) {
            return (cp >= 0x30 && cp <= 0x39); // 0..9
        },
        isHexDigit(cp) {
            return (cp >= 0x30 && cp <= 0x39) || // 0..9
                (cp >= 0x41 && cp <= 0x46) || // A..F
                (cp >= 0x61 && cp <= 0x66); // a..f
        },
        isOctalDigit(cp) {
            return (cp >= 0x30 && cp <= 0x37); // 0..7
        }
    };


/***/ },
/* 5 */
/***/ function(module, exports, __webpack_require__) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    const jsx_syntax_1 = __webpack_require__(6);
    /* tslint:disable:max-classes-per-file */
    class JSXClosingElement {
        constructor(name) {
            this.type = jsx_syntax_1.JSXSyntax.JSXClosingElement;
            this.name = name;
        }
    }
    exports.JSXClosingElement = JSXClosingElement;
    class JSXElement {
        constructor(openingElement, children, closingElement) {
            this.type = jsx_syntax_1.JSXSyntax.JSXElement;
            this.openingElement = openingElement;
            this.children = children;
            this.closingElement = closingElement;
        }
    }
    exports.JSXElement = JSXElement;
    class JSXEmptyExpression {
        constructor() {
            this.type = jsx_syntax_1.JSXSyntax.JSXEmptyExpression;
        }
    }
    exports.JSXEmptyExpression = JSXEmptyExpression;
    class JSXExpressionContainer {
        constructor(expression) {
            this.type = jsx_syntax_1.JSXSyntax.JSXExpressionContainer;
            this.expression = expression;
        }
    }
    exports.JSXExpressionContainer = JSXExpressionContainer;
    class JSXIdentifier {
        constructor(name) {
            this.type = jsx_syntax_1.JSXSyntax.JSXIdentifier;
            this.name = name;
        }
    }
    exports.JSXIdentifier = JSXIdentifier;
    class JSXMemberExpression {
        constructor(object, property) {
            this.type = jsx_syntax_1.JSXSyntax.JSXMemberExpression;
            this.object = object;
            this.property = property;
        }
    }
    exports.JSXMemberExpression = JSXMemberExpression;
    class JSXAttribute {
        constructor(name, value) {
            this.type = jsx_syntax_1.JSXSyntax.JSXAttribute;
            this.name = name;
            this.value = value;
        }
    }
    exports.JSXAttribute = JSXAttribute;
    class JSXNamespacedName {
        constructor(namespace, name) {
            this.type = jsx_syntax_1.JSXSyntax.JSXNamespacedName;
            this.namespace = namespace;
            this.name = name;
        }
    }
    exports.JSXNamespacedName = JSXNamespacedName;
    class JSXOpeningElement {
        constructor(name, selfClosing, attributes) {
            this.type = jsx_syntax_1.JSXSyntax.JSXOpeningElement;
            this.name = name;
            this.selfClosing = selfClosing;
            this.attributes = attributes;
        }
    }
    exports.JSXOpeningElement = JSXOpeningElement;
    class JSXSpreadAttribute {
        constructor(argument) {
            this.type = jsx_syntax_1.JSXSyntax.JSXSpreadAttribute;
            this.argument = argument;
        }
    }
    exports.JSXSpreadAttribute = JSXSpreadAttribute;
    class JSXText {
        constructor(value, raw) {
            this.type = jsx_syntax_1.JSXSyntax.JSXText;
            this.value = value;
            this.raw = raw;
        }
    }
    exports.JSXText = JSXText;


/***/ },
/* 6 */
/***/ function(module, exports) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.JSXSyntax = {
        JSXAttribute: 'JSXAttribute',
        JSXClosingElement: 'JSXClosingElement',
        JSXElement: 'JSXElement',
        JSXEmptyExpression: 'JSXEmptyExpression',
        JSXExpressionContainer: 'JSXExpressionContainer',
        JSXIdentifier: 'JSXIdentifier',
        JSXMemberExpression: 'JSXMemberExpression',
        JSXNamespacedName: 'JSXNamespacedName',
        JSXOpeningElement: 'JSXOpeningElement',
        JSXSpreadAttribute: 'JSXSpreadAttribute',
        JSXText: 'JSXText'
    };


/***/ },
/* 7 */
/***/ function(module, exports, __webpack_require__) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    const syntax_1 = __webpack_require__(2);
    /* tslint:disable:max-classes-per-file */
    class ArrayExpression {
        constructor(elements) {
            this.type = syntax_1.Syntax.ArrayExpression;
            this.elements = elements;
        }
    }
    exports.ArrayExpression = ArrayExpression;
    class ArrayPattern {
        constructor(elements) {
            this.type = syntax_1.Syntax.ArrayPattern;
            this.elements = elements;
        }
    }
    exports.ArrayPattern = ArrayPattern;
    class ArrowFunctionExpression {
        constructor(params, body, expression) {
            this.type = syntax_1.Syntax.ArrowFunctionExpression;
            this.id = null;
            this.params = params;
            this.body = body;
            this.generator = false;
            this.expression = expression;
            this.async = false;
        }
    }
    exports.ArrowFunctionExpression = ArrowFunctionExpression;
    class AssignmentExpression {
        constructor(operator, left, right) {
            this.type = syntax_1.Syntax.AssignmentExpression;
            this.operator = operator;
            this.left = left;
            this.right = right;
        }
    }
    exports.AssignmentExpression = AssignmentExpression;
    class AssignmentPattern {
        constructor(left, right) {
            this.type = syntax_1.Syntax.AssignmentPattern;
            this.left = left;
            this.right = right;
        }
    }
    exports.AssignmentPattern = AssignmentPattern;
    class AsyncArrowFunctionExpression {
        constructor(params, body, expression) {
            this.type = syntax_1.Syntax.ArrowFunctionExpression;
            this.id = null;
            this.params = params;
            this.body = body;
            this.generator = false;
            this.expression = expression;
            this.async = true;
        }
    }
    exports.AsyncArrowFunctionExpression = AsyncArrowFunctionExpression;
    class AsyncFunctionDeclaration {
        constructor(id, params, body, generator) {
            this.type = syntax_1.Syntax.FunctionDeclaration;
            this.id = id;
            this.params = params;
            this.body = body;
            this.generator = generator;
            this.expression = false;
            this.async = true;
        }
    }
    exports.AsyncFunctionDeclaration = AsyncFunctionDeclaration;
    class AsyncFunctionExpression {
        constructor(id, params, body, generator) {
            this.type = syntax_1.Syntax.FunctionExpression;
            this.id = id;
            this.params = params;
            this.body = body;
            this.generator = generator;
            this.expression = false;
            this.async = true;
        }
    }
    exports.AsyncFunctionExpression = AsyncFunctionExpression;
    class AwaitExpression {
        constructor(argument) {
            this.type = syntax_1.Syntax.AwaitExpression;
            this.argument = argument;
        }
    }
    exports.AwaitExpression = AwaitExpression;
    class BigIntLiteral {
        constructor(value, bigint, raw) {
            this.type = syntax_1.Syntax.Literal;
            this.value = value;
            this.bigint = bigint;
            this.raw = raw;
        }
    }
    exports.BigIntLiteral = BigIntLiteral;
    class BinaryExpression {
        constructor(operator, left, right) {
            const logical = (operator === '||' || operator === '&&');
            this.type = logical ? syntax_1.Syntax.LogicalExpression : syntax_1.Syntax.BinaryExpression;
            this.operator = operator;
            this.left = left;
            this.right = right;
        }
    }
    exports.BinaryExpression = BinaryExpression;
    class BlockStatement {
        constructor(body) {
            this.type = syntax_1.Syntax.BlockStatement;
            this.body = body;
        }
    }
    exports.BlockStatement = BlockStatement;
    class BreakStatement {
        constructor(label) {
            this.type = syntax_1.Syntax.BreakStatement;
            this.label = label;
        }
    }
    exports.BreakStatement = BreakStatement;
    class CallExpression {
        constructor(callee, args) {
            this.type = syntax_1.Syntax.CallExpression;
            this.callee = callee;
            this.arguments = args;
        }
    }
    exports.CallExpression = CallExpression;
    class CatchClause {
        constructor(param, body) {
            this.type = syntax_1.Syntax.CatchClause;
            this.param = param;
            this.body = body;
        }
    }
    exports.CatchClause = CatchClause;
    class ClassBody {
        constructor(body) {
            this.type = syntax_1.Syntax.ClassBody;
            this.body = body;
        }
    }
    exports.ClassBody = ClassBody;
    class ClassDeclaration {
        constructor(id, superClass, body) {
            this.type = syntax_1.Syntax.ClassDeclaration;
            this.id = id;
            this.superClass = superClass;
            this.body = body;
        }
    }
    exports.ClassDeclaration = ClassDeclaration;
    class ClassExpression {
        constructor(id, superClass, body) {
            this.type = syntax_1.Syntax.ClassExpression;
            this.id = id;
            this.superClass = superClass;
            this.body = body;
        }
    }
    exports.ClassExpression = ClassExpression;
    class ComputedMemberExpression {
        constructor(object, property) {
            this.type = syntax_1.Syntax.MemberExpression;
            this.computed = true;
            this.object = object;
            this.property = property;
        }
    }
    exports.ComputedMemberExpression = ComputedMemberExpression;
    class ConditionalExpression {
        constructor(test, consequent, alternate) {
            this.type = syntax_1.Syntax.ConditionalExpression;
            this.test = test;
            this.consequent = consequent;
            this.alternate = alternate;
        }
    }
    exports.ConditionalExpression = ConditionalExpression;
    class ContinueStatement {
        constructor(label) {
            this.type = syntax_1.Syntax.ContinueStatement;
            this.label = label;
        }
    }
    exports.ContinueStatement = ContinueStatement;
    class DebuggerStatement {
        constructor() {
            this.type = syntax_1.Syntax.DebuggerStatement;
        }
    }
    exports.DebuggerStatement = DebuggerStatement;
    class Directive {
        constructor(expression, directive) {
            this.type = syntax_1.Syntax.ExpressionStatement;
            this.expression = expression;
            this.directive = directive;
        }
    }
    exports.Directive = Directive;
    class DoWhileStatement {
        constructor(body, test) {
            this.type = syntax_1.Syntax.DoWhileStatement;
            this.body = body;
            this.test = test;
        }
    }
    exports.DoWhileStatement = DoWhileStatement;
    class EmptyStatement {
        constructor() {
            this.type = syntax_1.Syntax.EmptyStatement;
        }
    }
    exports.EmptyStatement = EmptyStatement;
    class ExportAllDeclaration {
        constructor(source) {
            this.type = syntax_1.Syntax.ExportAllDeclaration;
            this.source = source;
        }
    }
    exports.ExportAllDeclaration = ExportAllDeclaration;
    class ExportDefaultDeclaration {
        constructor(declaration) {
            this.type = syntax_1.Syntax.ExportDefaultDeclaration;
            this.declaration = declaration;
        }
    }
    exports.ExportDefaultDeclaration = ExportDefaultDeclaration;
    class ExportNamedDeclaration {
        constructor(declaration, specifiers, source) {
            this.type = syntax_1.Syntax.ExportNamedDeclaration;
            this.declaration = declaration;
            this.specifiers = specifiers;
            this.source = source;
        }
    }
    exports.ExportNamedDeclaration = ExportNamedDeclaration;
    class ExportSpecifier {
        constructor(local, exported) {
            this.type = syntax_1.Syntax.ExportSpecifier;
            this.exported = exported;
            this.local = local;
        }
    }
    exports.ExportSpecifier = ExportSpecifier;
    class ExpressionStatement {
        constructor(expression) {
            this.type = syntax_1.Syntax.ExpressionStatement;
            this.expression = expression;
        }
    }
    exports.ExpressionStatement = ExpressionStatement;
    class ForInStatement {
        constructor(left, right, body) {
            this.type = syntax_1.Syntax.ForInStatement;
            this.left = left;
            this.right = right;
            this.body = body;
            this.each = false;
        }
    }
    exports.ForInStatement = ForInStatement;
    class ForOfStatement {
        constructor(left, right, body, isAwait) {
            this.type = syntax_1.Syntax.ForOfStatement;
            this.left = left;
            this.right = right;
            this.body = body;
            this.await = isAwait;
        }
    }
    exports.ForOfStatement = ForOfStatement;
    class ForStatement {
        constructor(init, test, update, body) {
            this.type = syntax_1.Syntax.ForStatement;
            this.init = init;
            this.test = test;
            this.update = update;
            this.body = body;
        }
    }
    exports.ForStatement = ForStatement;
    class FunctionDeclaration {
        constructor(id, params, body, generator) {
            this.type = syntax_1.Syntax.FunctionDeclaration;
            this.id = id;
            this.params = params;
            this.body = body;
            this.generator = generator;
            this.expression = false;
            this.async = false;
        }
    }
    exports.FunctionDeclaration = FunctionDeclaration;
    class FunctionExpression {
        constructor(id, params, body, generator) {
            this.type = syntax_1.Syntax.FunctionExpression;
            this.id = id;
            this.params = params;
            this.body = body;
            this.generator = generator;
            this.expression = false;
            this.async = false;
        }
    }
    exports.FunctionExpression = FunctionExpression;
    class Identifier {
        constructor(name) {
            this.type = syntax_1.Syntax.Identifier;
            this.name = name;
        }
    }
    exports.Identifier = Identifier;
    class IfStatement {
        constructor(test, consequent, alternate) {
            this.type = syntax_1.Syntax.IfStatement;
            this.test = test;
            this.consequent = consequent;
            this.alternate = alternate;
        }
    }
    exports.IfStatement = IfStatement;
    class Import {
        constructor() {
            this.type = syntax_1.Syntax.Import;
        }
    }
    exports.Import = Import;
    class ImportDeclaration {
        constructor(specifiers, source) {
            this.type = syntax_1.Syntax.ImportDeclaration;
            this.specifiers = specifiers;
            this.source = source;
        }
    }
    exports.ImportDeclaration = ImportDeclaration;
    class ImportDefaultSpecifier {
        constructor(local) {
            this.type = syntax_1.Syntax.ImportDefaultSpecifier;
            this.local = local;
        }
    }
    exports.ImportDefaultSpecifier = ImportDefaultSpecifier;
    class ImportNamespaceSpecifier {
        constructor(local) {
            this.type = syntax_1.Syntax.ImportNamespaceSpecifier;
            this.local = local;
        }
    }
    exports.ImportNamespaceSpecifier = ImportNamespaceSpecifier;
    class ImportSpecifier {
        constructor(local, imported) {
            this.type = syntax_1.Syntax.ImportSpecifier;
            this.local = local;
            this.imported = imported;
        }
    }
    exports.ImportSpecifier = ImportSpecifier;
    class LabeledStatement {
        constructor(label, body) {
            this.type = syntax_1.Syntax.LabeledStatement;
            this.label = label;
            this.body = body;
        }
    }
    exports.LabeledStatement = LabeledStatement;
    class Literal {
        constructor(value, raw) {
            this.type = syntax_1.Syntax.Literal;
            this.value = value;
            this.raw = raw;
        }
    }
    exports.Literal = Literal;
    class MetaProperty {
        constructor(meta, property) {
            this.type = syntax_1.Syntax.MetaProperty;
            this.meta = meta;
            this.property = property;
        }
    }
    exports.MetaProperty = MetaProperty;
    class MethodDefinition {
        constructor(key, computed, value, kind, isStatic) {
            this.type = syntax_1.Syntax.MethodDefinition;
            this.key = key;
            this.computed = computed;
            this.value = value;
            this.kind = kind;
            this.static = isStatic;
        }
    }
    exports.MethodDefinition = MethodDefinition;
    class Module {
        constructor(body) {
            this.type = syntax_1.Syntax.Program;
            this.body = body;
            this.sourceType = 'module';
        }
    }
    exports.Module = Module;
    class NewExpression {
        constructor(callee, args) {
            this.type = syntax_1.Syntax.NewExpression;
            this.callee = callee;
            this.arguments = args;
        }
    }
    exports.NewExpression = NewExpression;
    class ObjectExpression {
        constructor(properties) {
            this.type = syntax_1.Syntax.ObjectExpression;
            this.properties = properties;
        }
    }
    exports.ObjectExpression = ObjectExpression;
    class ObjectPattern {
        constructor(properties) {
            this.type = syntax_1.Syntax.ObjectPattern;
            this.properties = properties;
        }
    }
    exports.ObjectPattern = ObjectPattern;
    class Property {
        constructor(kind, key, computed, value, method, shorthand) {
            this.type = syntax_1.Syntax.Property;
            this.key = key;
            this.computed = computed;
            this.value = value;
            this.kind = kind;
            this.method = method;
            this.shorthand = shorthand;
        }
    }
    exports.Property = Property;
    class RegexLiteral {
        constructor(value, raw, pattern, flags) {
            this.type = syntax_1.Syntax.Literal;
            this.value = value;
            this.raw = raw;
            this.regex = { pattern, flags };
        }
    }
    exports.RegexLiteral = RegexLiteral;
    class RestElement {
        constructor(argument) {
            this.type = syntax_1.Syntax.RestElement;
            this.argument = argument;
        }
    }
    exports.RestElement = RestElement;
    class ReturnStatement {
        constructor(argument) {
            this.type = syntax_1.Syntax.ReturnStatement;
            this.argument = argument;
        }
    }
    exports.ReturnStatement = ReturnStatement;
    class Script {
        constructor(body) {
            this.type = syntax_1.Syntax.Program;
            this.body = body;
            this.sourceType = 'script';
        }
    }
    exports.Script = Script;
    class SequenceExpression {
        constructor(expressions) {
            this.type = syntax_1.Syntax.SequenceExpression;
            this.expressions = expressions;
        }
    }
    exports.SequenceExpression = SequenceExpression;
    class SpreadElement {
        constructor(argument) {
            this.type = syntax_1.Syntax.SpreadElement;
            this.argument = argument;
        }
    }
    exports.SpreadElement = SpreadElement;
    class StaticMemberExpression {
        constructor(object, property) {
            this.type = syntax_1.Syntax.MemberExpression;
            this.computed = false;
            this.object = object;
            this.property = property;
        }
    }
    exports.StaticMemberExpression = StaticMemberExpression;
    class Super {
        constructor() {
            this.type = syntax_1.Syntax.Super;
        }
    }
    exports.Super = Super;
    class SwitchCase {
        constructor(test, consequent) {
            this.type = syntax_1.Syntax.SwitchCase;
            this.test = test;
            this.consequent = consequent;
        }
    }
    exports.SwitchCase = SwitchCase;
    class SwitchStatement {
        constructor(discriminant, cases) {
            this.type = syntax_1.Syntax.SwitchStatement;
            this.discriminant = discriminant;
            this.cases = cases;
        }
    }
    exports.SwitchStatement = SwitchStatement;
    class TaggedTemplateExpression {
        constructor(tag, quasi) {
            this.type = syntax_1.Syntax.TaggedTemplateExpression;
            this.tag = tag;
            this.quasi = quasi;
        }
    }
    exports.TaggedTemplateExpression = TaggedTemplateExpression;
    class TemplateElement {
        constructor(value, tail) {
            this.type = syntax_1.Syntax.TemplateElement;
            this.value = value;
            this.tail = tail;
        }
    }
    exports.TemplateElement = TemplateElement;
    class TemplateLiteral {
        constructor(quasis, expressions) {
            this.type = syntax_1.Syntax.TemplateLiteral;
            this.quasis = quasis;
            this.expressions = expressions;
        }
    }
    exports.TemplateLiteral = TemplateLiteral;
    class ThisExpression {
        constructor() {
            this.type = syntax_1.Syntax.ThisExpression;
        }
    }
    exports.ThisExpression = ThisExpression;
    class ThrowStatement {
        constructor(argument) {
            this.type = syntax_1.Syntax.ThrowStatement;
            this.argument = argument;
        }
    }
    exports.ThrowStatement = ThrowStatement;
    class TryStatement {
        constructor(block, handler, finalizer) {
            this.type = syntax_1.Syntax.TryStatement;
            this.block = block;
            this.handler = handler;
            this.finalizer = finalizer;
        }
    }
    exports.TryStatement = TryStatement;
    class UnaryExpression {
        constructor(operator, argument) {
            this.type = syntax_1.Syntax.UnaryExpression;
            this.operator = operator;
            this.argument = argument;
            this.prefix = true;
        }
    }
    exports.UnaryExpression = UnaryExpression;
    class UpdateExpression {
        constructor(operator, argument, prefix) {
            this.type = syntax_1.Syntax.UpdateExpression;
            this.operator = operator;
            this.argument = argument;
            this.prefix = prefix;
        }
    }
    exports.UpdateExpression = UpdateExpression;
    class VariableDeclaration {
        constructor(declarations, kind) {
            this.type = syntax_1.Syntax.VariableDeclaration;
            this.declarations = declarations;
            this.kind = kind;
        }
    }
    exports.VariableDeclaration = VariableDeclaration;
    class VariableDeclarator {
        constructor(id, init) {
            this.type = syntax_1.Syntax.VariableDeclarator;
            this.id = id;
            this.init = init;
        }
    }
    exports.VariableDeclarator = VariableDeclarator;
    class WhileStatement {
        constructor(test, body) {
            this.type = syntax_1.Syntax.WhileStatement;
            this.test = test;
            this.body = body;
        }
    }
    exports.WhileStatement = WhileStatement;
    class WithStatement {
        constructor(object, body) {
            this.type = syntax_1.Syntax.WithStatement;
            this.object = object;
            this.body = body;
        }
    }
    exports.WithStatement = WithStatement;
    class YieldExpression {
        constructor(argument, delegate) {
            this.type = syntax_1.Syntax.YieldExpression;
            this.argument = argument;
            this.delegate = delegate;
        }
    }
    exports.YieldExpression = YieldExpression;


/***/ },
/* 8 */
/***/ function(module, exports, __webpack_require__) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    const assert_1 = __webpack_require__(9);
    const error_handler_1 = __webpack_require__(10);
    const messages_1 = __webpack_require__(11);
    const Node = __webpack_require__(7);
    const scanner_1 = __webpack_require__(12);
    const syntax_1 = __webpack_require__(2);
    const token_1 = __webpack_require__(13);
    const ArrowParameterPlaceHolder = 'ArrowParameterPlaceHolder';
    class Parser {
        constructor(code, options = {}, delegate) {
            this.config = {
                range: (typeof options.range === 'boolean') && options.range,
                loc: (typeof options.loc === 'boolean') && options.loc,
                source: null,
                tokens: (typeof options.tokens === 'boolean') && options.tokens,
                comment: (typeof options.comment === 'boolean') && options.comment,
                tolerant: (typeof options.tolerant === 'boolean') && options.tolerant
            };
            if (this.config.loc && options.source && options.source !== null) {
                this.config.source = String(options.source);
            }
            this.delegate = delegate;
            this.errorHandler = new error_handler_1.ErrorHandler();
            this.errorHandler.tolerant = this.config.tolerant;
            this.scanner = new scanner_1.Scanner(code, this.errorHandler);
            this.scanner.trackComment = this.config.comment;
            this.operatorPrecedence = {
                ')': 0,
                ';': 0,
                ',': 0,
                '=': 0,
                ']': 0,
                '||': 1,
                '&&': 2,
                '|': 3,
                '^': 4,
                '&': 5,
                '==': 6,
                '!=': 6,
                '===': 6,
                '!==': 6,
                '<': 7,
                '>': 7,
                '<=': 7,
                '>=': 7,
                '<<': 8,
                '>>': 8,
                '>>>': 8,
                '+': 9,
                '-': 9,
                '*': 11,
                '/': 11,
                '%': 11
            };
            this.lookahead = {
                type: 2 /* EOF */,
                value: '',
                lineNumber: this.scanner.lineNumber,
                lineStart: 0,
                start: 0,
                end: 0
            };
            this.hasLineTerminator = false;
            this.context = {
                isModule: false,
                await: false,
                allowIn: true,
                allowStrictDirective: true,
                allowYield: true,
                firstCoverInitializedNameError: null,
                isAssignmentTarget: false,
                isBindingElement: false,
                inFunctionBody: false,
                inIteration: false,
                inSwitch: false,
                labelSet: {},
                strict: false
            };
            this.tokens = [];
            this.startMarker = {
                index: 0,
                line: this.scanner.lineNumber,
                column: 0
            };
            this.lastMarker = {
                index: 0,
                line: this.scanner.lineNumber,
                column: 0
            };
            this.nextToken();
            this.lastMarker = {
                index: this.scanner.index,
                line: this.scanner.lineNumber,
                column: this.scanner.index - this.scanner.lineStart
            };
        }
        throwError(messageFormat, ...values) {
            const args = Array.prototype.slice.call(arguments, 1);
            const msg = messageFormat.replace(/%(\d)/g, (whole, idx) => {
                assert_1.assert(idx < args.length, 'Message reference must be in range');
                return args[idx];
            });
            const index = this.lastMarker.index;
            const line = this.lastMarker.line;
            const column = this.lastMarker.column + 1;
            throw this.errorHandler.createError(index, line, column, msg);
        }
        tolerateError(messageFormat, ...values) {
            const args = Array.prototype.slice.call(arguments, 1);
            const msg = messageFormat.replace(/%(\d)/g, (whole, idx) => {
                assert_1.assert(idx < args.length, 'Message reference must be in range');
                return args[idx];
            });
            const index = this.lastMarker.index;
            const line = this.scanner.lineNumber;
            const column = this.lastMarker.column + 1;
            this.errorHandler.tolerateError(index, line, column, msg);
        }
        // Throw an exception because of the token.
        unexpectedTokenError(token, message) {
            let msg = message || messages_1.Messages.UnexpectedToken;
            let value;
            if (token) {
                if (!message) {
                    msg = (token.type === 2 /* EOF */) ? messages_1.Messages.UnexpectedEOS :
                        (token.type === 3 /* Identifier */) ? messages_1.Messages.UnexpectedIdentifier :
                            (token.type === 6 /* NumericLiteral */) ? messages_1.Messages.UnexpectedNumber :
                                (token.type === 8 /* StringLiteral */) ? messages_1.Messages.UnexpectedString :
                                    (token.type === 10 /* Template */) ? messages_1.Messages.UnexpectedTemplate :
                                        messages_1.Messages.UnexpectedToken;
                    if (token.type === 4 /* Keyword */) {
                        if (this.scanner.isFutureReservedWord(token.value)) {
                            msg = messages_1.Messages.UnexpectedReserved;
                        }
                        else if (this.context.strict && this.scanner.isStrictModeReservedWord(token.value)) {
                            msg = messages_1.Messages.StrictReservedWord;
                        }
                    }
                }
                value = token.value;
            }
            else {
                value = 'ILLEGAL';
            }
            msg = msg.replace('%0', value);
            if (token && typeof token.lineNumber === 'number') {
                const index = token.start;
                const line = token.lineNumber;
                const lastMarkerLineStart = this.lastMarker.index - this.lastMarker.column;
                const column = token.start - lastMarkerLineStart + 1;
                return this.errorHandler.createError(index, line, column, msg);
            }
            else {
                const index = this.lastMarker.index;
                const line = this.lastMarker.line;
                const column = this.lastMarker.column + 1;
                return this.errorHandler.createError(index, line, column, msg);
            }
        }
        throwUnexpectedToken(token, message) {
            throw this.unexpectedTokenError(token, message);
        }
        tolerateUnexpectedToken(token, message) {
            this.errorHandler.tolerate(this.unexpectedTokenError(token, message));
        }
        collectComments() {
            if (!this.config.comment) {
                this.scanner.scanComments();
            }
            else {
                const comments = this.scanner.scanComments();
                if (comments.length > 0 && this.delegate) {
                    for (let i = 0; i < comments.length; ++i) {
                        const e = comments[i];
                        let node;
                        node = {
                            type: e.multiLine ? 'BlockComment' : 'LineComment',
                            value: this.scanner.source.slice(e.slice[0], e.slice[1])
                        };
                        if (this.config.range) {
                            node.range = e.range;
                        }
                        if (this.config.loc) {
                            node.loc = e.loc;
                        }
                        const metadata = {
                            start: {
                                line: e.loc.start.line,
                                column: e.loc.start.column,
                                offset: e.range[0]
                            },
                            end: {
                                line: e.loc.end.line,
                                column: e.loc.end.column,
                                offset: e.range[1]
                            }
                        };
                        this.delegate(node, metadata);
                    }
                }
            }
        }
        // From internal representation to an external structure
        getTokenRaw(token) {
            return this.scanner.source.slice(token.start, token.end);
        }
        convertToken(token) {
            const t = {
                type: token_1.TokenName[token.type],
                value: this.getTokenRaw(token)
            };
            if (this.config.range) {
                t.range = [token.start, token.end];
            }
            if (this.config.loc) {
                t.loc = {
                    start: {
                        line: this.startMarker.line,
                        column: this.startMarker.column
                    },
                    end: {
                        line: this.scanner.lineNumber,
                        column: this.scanner.index - this.scanner.lineStart
                    }
                };
            }
            if (token.type === 9 /* RegularExpression */) {
                const pattern = token.pattern;
                const flags = token.flags;
                t.regex = { pattern, flags };
            }
            return t;
        }
        nextToken() {
            const token = this.lookahead;
            this.lastMarker.index = this.scanner.index;
            this.lastMarker.line = this.scanner.lineNumber;
            this.lastMarker.column = this.scanner.index - this.scanner.lineStart;
            this.collectComments();
            if (this.scanner.index !== this.startMarker.index) {
                this.startMarker.index = this.scanner.index;
                this.startMarker.line = this.scanner.lineNumber;
                this.startMarker.column = this.scanner.index - this.scanner.lineStart;
            }
            const next = this.scanner.lex();
            this.hasLineTerminator = (token.lineNumber !== next.lineNumber);
            if (next && this.context.strict && next.type === 3 /* Identifier */) {
                if (this.scanner.isStrictModeReservedWord(next.value)) {
                    next.type = 4 /* Keyword */;
                }
            }
            this.lookahead = next;
            if (this.config.tokens && next.type !== 2 /* EOF */) {
                this.tokens.push(this.convertToken(next));
            }
            return token;
        }
        nextRegexToken() {
            this.collectComments();
            const token = this.scanner.scanRegExp();
            if (this.config.tokens) {
                // Pop the previous token, '/' or '/='
                // This is added from the lookahead token.
                this.tokens.pop();
                this.tokens.push(this.convertToken(token));
            }
            // Prime the next lookahead.
            this.lookahead = token;
            this.nextToken();
            return token;
        }
        createNode() {
            return {
                index: this.startMarker.index,
                line: this.startMarker.line,
                column: this.startMarker.column
            };
        }
        startNode(token, lastLineStart = 0) {
            let column = token.start - token.lineStart;
            let line = token.lineNumber;
            if (column < 0) {
                column += lastLineStart;
                line--;
            }
            return {
                index: token.start,
                line: line,
                column: column
            };
        }
        finalize(marker, node) {
            if (this.config.range) {
                node.range = [marker.index, this.lastMarker.index];
            }
            if (this.config.loc) {
                node.loc = {
                    start: {
                        line: marker.line,
                        column: marker.column,
                    },
                    end: {
                        line: this.lastMarker.line,
                        column: this.lastMarker.column
                    }
                };
                if (this.config.source) {
                    node.loc.source = this.config.source;
                }
            }
            if (this.delegate) {
                const metadata = {
                    start: {
                        line: marker.line,
                        column: marker.column,
                        offset: marker.index
                    },
                    end: {
                        line: this.lastMarker.line,
                        column: this.lastMarker.column,
                        offset: this.lastMarker.index
                    }
                };
                this.delegate(node, metadata);
            }
            return node;
        }
        // Expect the next token to match the specified punctuator.
        // If not, an exception will be thrown.
        expect(value) {
            const token = this.nextToken();
            if (token.type !== 7 /* Punctuator */ || token.value !== value) {
                this.throwUnexpectedToken(token);
            }
        }
        // Quietly expect a comma when in tolerant mode, otherwise delegates to expect().
        expectCommaSeparator() {
            if (this.config.tolerant) {
                const token = this.lookahead;
                if (token.type === 7 /* Punctuator */ && token.value === ',') {
                    this.nextToken();
                }
                else if (token.type === 7 /* Punctuator */ && token.value === ';') {
                    this.nextToken();
                    this.tolerateUnexpectedToken(token);
                }
                else {
                    this.tolerateUnexpectedToken(token, messages_1.Messages.UnexpectedToken);
                }
            }
            else {
                this.expect(',');
            }
        }
        // Expect the next token to match the specified keyword.
        // If not, an exception will be thrown.
        expectKeyword(keyword) {
            const token = this.nextToken();
            if (token.type !== 4 /* Keyword */ || token.value !== keyword) {
                this.throwUnexpectedToken(token);
            }
        }
        // Return true if the next token matches the specified punctuator.
        match(value) {
            return this.lookahead.type === 7 /* Punctuator */ && this.lookahead.value === value;
        }
        // Return true if the next token matches the specified keyword
        matchKeyword(keyword) {
            return this.lookahead.type === 4 /* Keyword */ && this.lookahead.value === keyword;
        }
        // Return true if the next token matches the specified contextual keyword
        // (where an identifier is sometimes a keyword depending on the context)
        matchContextualKeyword(keyword) {
            return this.lookahead.type === 3 /* Identifier */ && this.lookahead.value === keyword;
        }
        // Return true if the next token is an assignment operator
        matchAssign() {
            if (this.lookahead.type !== 7 /* Punctuator */) {
                return false;
            }
            const op = this.lookahead.value;
            return op === '=' ||
                op === '*=' ||
                op === '**=' ||
                op === '/=' ||
                op === '%=' ||
                op === '+=' ||
                op === '-=' ||
                op === '<<=' ||
                op === '>>=' ||
                op === '>>>=' ||
                op === '&=' ||
                op === '^=' ||
                op === '|=';
        }
        // Cover grammar support.
        //
        // When an assignment expression position starts with an left parenthesis, the determination of the type
        // of the syntax is to be deferred arbitrarily long until the end of the parentheses pair (plus a lookahead)
        // or the first comma. This situation also defers the determination of all the expressions nested in the pair.
        //
        // There are three productions that can be parsed in a parentheses pair that needs to be determined
        // after the outermost pair is closed. They are:
        //
        //   1. AssignmentExpression
        //   2. BindingElements
        //   3. AssignmentTargets
        //
        // In order to avoid exponential backtracking, we use two flags to denote if the production can be
        // binding element or assignment target.
        //
        // The three productions have the relationship:
        //
        //   BindingElements ⊆ AssignmentTargets ⊆ AssignmentExpression
        //
        // with a single exception that CoverInitializedName when used directly in an Expression, generates
        // an early error. Therefore, we need the third state, firstCoverInitializedNameError, to track the
        // first usage of CoverInitializedName and report it when we reached the end of the parentheses pair.
        //
        // isolateCoverGrammar function runs the given parser function with a new cover grammar context, and it does not
        // effect the current flags. This means the production the parser parses is only used as an expression. Therefore
        // the CoverInitializedName check is conducted.
        //
        // inheritCoverGrammar function runs the given parse function with a new cover grammar context, and it propagates
        // the flags outside of the parser. This means the production the parser parses is used as a part of a potential
        // pattern. The CoverInitializedName check is deferred.
        isolateCoverGrammar(parseFunction) {
            const previousIsBindingElement = this.context.isBindingElement;
            const previousIsAssignmentTarget = this.context.isAssignmentTarget;
            const previousFirstCoverInitializedNameError = this.context.firstCoverInitializedNameError;
            this.context.isBindingElement = true;
            this.context.isAssignmentTarget = true;
            this.context.firstCoverInitializedNameError = null;
            const result = parseFunction.call(this);
            if (this.context.firstCoverInitializedNameError !== null) {
                this.throwUnexpectedToken(this.context.firstCoverInitializedNameError);
            }
            this.context.isBindingElement = previousIsBindingElement;
            this.context.isAssignmentTarget = previousIsAssignmentTarget;
            this.context.firstCoverInitializedNameError = previousFirstCoverInitializedNameError;
            return result;
        }
        inheritCoverGrammar(parseFunction) {
            const previousIsBindingElement = this.context.isBindingElement;
            const previousIsAssignmentTarget = this.context.isAssignmentTarget;
            const previousFirstCoverInitializedNameError = this.context.firstCoverInitializedNameError;
            this.context.isBindingElement = true;
            this.context.isAssignmentTarget = true;
            this.context.firstCoverInitializedNameError = null;
            const result = parseFunction.call(this);
            this.context.isBindingElement = this.context.isBindingElement && previousIsBindingElement;
            this.context.isAssignmentTarget = this.context.isAssignmentTarget && previousIsAssignmentTarget;
            this.context.firstCoverInitializedNameError = previousFirstCoverInitializedNameError || this.context.firstCoverInitializedNameError;
            return result;
        }
        consumeSemicolon() {
            if (this.match(';')) {
                this.nextToken();
            }
            else if (!this.hasLineTerminator) {
                if (this.lookahead.type !== 2 /* EOF */ && !this.match('}')) {
                    this.throwUnexpectedToken(this.lookahead);
                }
                this.lastMarker.index = this.startMarker.index;
                this.lastMarker.line = this.startMarker.line;
                this.lastMarker.column = this.startMarker.column;
            }
        }
        // https://tc39.github.io/ecma262/#sec-primary-expression
        parsePrimaryExpression() {
            const node = this.createNode();
            let expr;
            let token, raw;
            switch (this.lookahead.type) {
                case 3 /* Identifier */:
                    if ((this.context.isModule || this.context.await) && this.lookahead.value === 'await') {
                        this.tolerateUnexpectedToken(this.lookahead);
                    }
                    expr = this.matchAsyncFunction() ? this.parseFunctionExpression() : this.finalize(node, new Node.Identifier(this.nextToken().value));
                    break;
                case 6 /* NumericLiteral */:
                case 8 /* StringLiteral */:
                    if (this.context.strict && this.lookahead.octal) {
                        this.tolerateUnexpectedToken(this.lookahead, messages_1.Messages.StrictOctalLiteral);
                    }
                    this.context.isAssignmentTarget = false;
                    this.context.isBindingElement = false;
                    token = this.nextToken();
                    raw = this.getTokenRaw(token);
                    if (token.bigint) {
                        const bigintString = token.value !== null ? token.value.toString() : /* istanbul ignore next */ this.scanner.source.slice(token.start, token.end - 1);
                        expr = this.finalize(node, new Node.BigIntLiteral(token.value, bigintString, raw));
                    }
                    else {
                        expr = this.finalize(node, new Node.Literal(token.value, raw));
                    }
                    break;
                case 1 /* BooleanLiteral */:
                    this.context.isAssignmentTarget = false;
                    this.context.isBindingElement = false;
                    token = this.nextToken();
                    raw = this.getTokenRaw(token);
                    expr = this.finalize(node, new Node.Literal(token.value === 'true', raw));
                    break;
                case 5 /* NullLiteral */:
                    this.context.isAssignmentTarget = false;
                    this.context.isBindingElement = false;
                    token = this.nextToken();
                    raw = this.getTokenRaw(token);
                    expr = this.finalize(node, new Node.Literal(null, raw));
                    break;
                case 10 /* Template */:
                    expr = this.parseTemplateLiteral();
                    break;
                case 7 /* Punctuator */:
                    switch (this.lookahead.value) {
                        case '(':
                            this.context.isBindingElement = false;
                            expr = this.inheritCoverGrammar(this.parseGroupExpression);
                            break;
                        case '[':
                            expr = this.inheritCoverGrammar(this.parseArrayInitializer);
                            break;
                        case '{':
                            expr = this.inheritCoverGrammar(this.parseObjectInitializer);
                            break;
                        case '/':
                        case '/=':
                            this.context.isAssignmentTarget = false;
                            this.context.isBindingElement = false;
                            this.scanner.index = this.startMarker.index;
                            token = this.nextRegexToken();
                            raw = this.getTokenRaw(token);
                            expr = this.finalize(node, new Node.RegexLiteral(token.regex, raw, token.pattern, token.flags));
                            break;
                        default:
                            expr = this.throwUnexpectedToken(this.nextToken());
                    }
                    break;
                case 4 /* Keyword */:
                    if (!this.context.strict && this.context.allowYield && this.matchKeyword('yield')) {
                        expr = this.parseIdentifierName();
                    }
                    else if (!this.context.strict && this.matchKeyword('let')) {
                        expr = this.finalize(node, new Node.Identifier(this.nextToken().value));
                    }
                    else {
                        this.context.isAssignmentTarget = false;
                        this.context.isBindingElement = false;
                        if (this.matchKeyword('function')) {
                            expr = this.parseFunctionExpression();
                        }
                        else if (this.matchKeyword('this')) {
                            this.nextToken();
                            expr = this.finalize(node, new Node.ThisExpression());
                        }
                        else if (this.matchKeyword('class')) {
                            expr = this.parseClassExpression();
                        }
                        else if (this.matchImportCall()) {
                            expr = this.parseImportCall();
                        }
                        else {
                            expr = this.throwUnexpectedToken(this.nextToken());
                        }
                    }
                    break;
                default:
                    expr = this.throwUnexpectedToken(this.nextToken());
            }
            return expr;
        }
        // https://tc39.github.io/ecma262/#sec-array-initializer
        parseSpreadElement() {
            const node = this.createNode();
            this.expect('...');
            const arg = this.inheritCoverGrammar(this.parseAssignmentExpression);
            return this.finalize(node, new Node.SpreadElement(arg));
        }
        parseArrayInitializer() {
            const node = this.createNode();
            const elements = [];
            this.expect('[');
            while (!this.match(']')) {
                if (this.match(',')) {
                    this.nextToken();
                    elements.push(null);
                }
                else if (this.match('...')) {
                    const element = this.parseSpreadElement();
                    if (!this.match(']')) {
                        this.context.isAssignmentTarget = false;
                        this.context.isBindingElement = false;
                        this.expect(',');
                    }
                    elements.push(element);
                }
                else {
                    elements.push(this.inheritCoverGrammar(this.parseAssignmentExpression));
                    if (!this.match(']')) {
                        this.expect(',');
                    }
                }
            }
            this.expect(']');
            return this.finalize(node, new Node.ArrayExpression(elements));
        }
        // https://tc39.github.io/ecma262/#sec-object-initializer
        parsePropertyMethod(params) {
            this.context.isAssignmentTarget = false;
            this.context.isBindingElement = false;
            const previousStrict = this.context.strict;
            const previousAllowStrictDirective = this.context.allowStrictDirective;
            this.context.allowStrictDirective = params.simple;
            const body = this.isolateCoverGrammar(this.parseFunctionSourceElements);
            if (this.context.strict && params.firstRestricted) {
                this.tolerateUnexpectedToken(params.firstRestricted, params.message);
            }
            if (this.context.strict && params.stricted) {
                this.tolerateUnexpectedToken(params.stricted, params.message);
            }
            this.context.strict = previousStrict;
            this.context.allowStrictDirective = previousAllowStrictDirective;
            return body;
        }
        parsePropertyMethodFunction() {
            const isGenerator = false;
            const node = this.createNode();
            const previousAllowYield = this.context.allowYield;
            this.context.allowYield = true;
            const params = this.parseFormalParameters();
            const method = this.parsePropertyMethod(params);
            this.context.allowYield = previousAllowYield;
            return this.finalize(node, new Node.FunctionExpression(null, params.params, method, isGenerator));
        }
        parsePropertyMethodAsyncFunction(isGenerator) {
            const node = this.createNode();
            const previousAllowYield = this.context.allowYield;
            const previousAwait = this.context.await;
            this.context.allowYield = false;
            this.context.await = true;
            const params = this.parseFormalParameters();
            const method = this.parsePropertyMethod(params);
            this.context.allowYield = previousAllowYield;
            this.context.await = previousAwait;
            return this.finalize(node, new Node.AsyncFunctionExpression(null, params.params, method, isGenerator));
        }
        parseObjectPropertyKey() {
            const node = this.createNode();
            const token = this.nextToken();
            let key;
            switch (token.type) {
                case 8 /* StringLiteral */:
                case 6 /* NumericLiteral */:
                    if (this.context.strict && token.octal) {
                        this.tolerateUnexpectedToken(token, messages_1.Messages.StrictOctalLiteral);
                    }
                    const raw = this.getTokenRaw(token);
                    key = this.finalize(node, new Node.Literal(token.value, raw));
                    break;
                case 3 /* Identifier */:
                case 1 /* BooleanLiteral */:
                case 5 /* NullLiteral */:
                case 4 /* Keyword */:
                    key = this.finalize(node, new Node.Identifier(token.value));
                    break;
                case 7 /* Punctuator */:
                    if (token.value === '[') {
                        key = this.isolateCoverGrammar(this.parseAssignmentExpression);
                        this.expect(']');
                    }
                    else {
                        key = this.throwUnexpectedToken(token);
                    }
                    break;
                default:
                    key = this.throwUnexpectedToken(token);
            }
            return key;
        }
        isPropertyKey(key, value) {
            return (key.type === syntax_1.Syntax.Identifier && key.name === value) ||
                (key.type === syntax_1.Syntax.Literal && key.value === value);
        }
        parseObjectProperty(hasProto) {
            const node = this.createNode();
            const token = this.lookahead;
            let kind;
            let key = null;
            let value = null;
            let computed = false;
            let method = false;
            let shorthand = false;
            let isAsync = false;
            let isAsyncGenerator = false;
            if (token.type === 3 /* Identifier */) {
                const id = token.value;
                this.nextToken();
                computed = this.match('[');
                isAsync = !this.hasLineTerminator && (id === 'async') && !this.match(':') && !this.match('(') && !this.match(',');
                isAsyncGenerator = this.match('*');
                if (isAsyncGenerator) {
                    this.nextToken();
                }
                key = isAsync ? this.parseObjectPropertyKey() : this.finalize(node, new Node.Identifier(id));
            }
            else if (this.match('*')) {
                this.nextToken();
            }
            else {
                computed = this.match('[');
                key = this.parseObjectPropertyKey();
            }
            const lookaheadPropertyKey = this.qualifiedPropertyName(this.lookahead);
            if (token.type === 3 /* Identifier */ && !isAsync && token.value === 'get' && lookaheadPropertyKey) {
                kind = 'get';
                computed = this.match('[');
                key = this.parseObjectPropertyKey();
                this.context.allowYield = false;
                value = this.parseGetterMethod();
            }
            else if (token.type === 3 /* Identifier */ && !isAsync && token.value === 'set' && lookaheadPropertyKey) {
                kind = 'set';
                computed = this.match('[');
                key = this.parseObjectPropertyKey();
                value = this.parseSetterMethod();
            }
            else if (token.type === 7 /* Punctuator */ && token.value === '*' && lookaheadPropertyKey) {
                kind = 'init';
                computed = this.match('[');
                key = this.parseObjectPropertyKey();
                value = this.parseGeneratorMethod();
                method = true;
            }
            else {
                if (!key) {
                    this.throwUnexpectedToken(this.lookahead);
                }
                kind = 'init';
                if (this.match(':') && !isAsync) {
                    if (!computed && this.isPropertyKey(key, '__proto__')) {
                        if (hasProto.value) {
                            this.tolerateError(messages_1.Messages.DuplicateProtoProperty);
                        }
                        hasProto.value = true;
                    }
                    this.nextToken();
                    value = this.inheritCoverGrammar(this.parseAssignmentExpression);
                }
                else if (this.match('(')) {
                    value = isAsync ? this.parsePropertyMethodAsyncFunction(isAsyncGenerator) : this.parsePropertyMethodFunction();
                    method = true;
                }
                else if (token.type === 3 /* Identifier */) {
                    const id = this.finalize(node, new Node.Identifier(token.value));
                    if (this.match('=')) {
                        this.context.firstCoverInitializedNameError = this.lookahead;
                        this.nextToken();
                        shorthand = true;
                        const init = this.isolateCoverGrammar(this.parseAssignmentExpression);
                        value = this.finalize(node, new Node.AssignmentPattern(id, init));
                    }
                    else {
                        shorthand = true;
                        value = id;
                    }
                }
                else {
                    this.throwUnexpectedToken(this.nextToken());
                }
            }
            return this.finalize(node, new Node.Property(kind, key, computed, value, method, shorthand));
        }
        parseObjectInitializer() {
            const node = this.createNode();
            this.expect('{');
            const properties = [];
            const hasProto = { value: false };
            while (!this.match('}')) {
                properties.push(this.match('...') ? this.parseSpreadElement() : this.parseObjectProperty(hasProto));
                if (!this.match('}')) {
                    this.expectCommaSeparator();
                }
            }
            this.expect('}');
            return this.finalize(node, new Node.ObjectExpression(properties));
        }
        // https://tc39.github.io/ecma262/#sec-template-literals
        parseTemplateHead() {
            assert_1.assert(this.lookahead.head, 'Template literal must start with a template head');
            const node = this.createNode();
            const token = this.nextToken();
            const raw = token.value;
            const cooked = token.cooked;
            return this.finalize(node, new Node.TemplateElement({ raw, cooked }, token.tail));
        }
        parseTemplateElement() {
            if (this.lookahead.type !== 10 /* Template */) {
                this.throwUnexpectedToken();
            }
            const node = this.createNode();
            const token = this.nextToken();
            const raw = token.value;
            const cooked = token.cooked;
            return this.finalize(node, new Node.TemplateElement({ raw, cooked }, token.tail));
        }
        parseTemplateLiteral() {
            const node = this.createNode();
            const expressions = [];
            const quasis = [];
            let quasi = this.parseTemplateHead();
            quasis.push(quasi);
            while (!quasi.tail) {
                expressions.push(this.parseExpression());
                quasi = this.parseTemplateElement();
                quasis.push(quasi);
            }
            return this.finalize(node, new Node.TemplateLiteral(quasis, expressions));
        }
        // https://tc39.github.io/ecma262/#sec-grouping-operator
        reinterpretExpressionAsPattern(expr) {
            switch (expr.type) {
                case syntax_1.Syntax.Identifier:
                case syntax_1.Syntax.MemberExpression:
                case syntax_1.Syntax.RestElement:
                case syntax_1.Syntax.AssignmentPattern:
                    break;
                case syntax_1.Syntax.SpreadElement:
                    expr.type = syntax_1.Syntax.RestElement;
                    this.reinterpretExpressionAsPattern(expr.argument);
                    break;
                case syntax_1.Syntax.ArrayExpression:
                    expr.type = syntax_1.Syntax.ArrayPattern;
                    for (let i = 0; i < expr.elements.length; i++) {
                        if (expr.elements[i] !== null) {
                            this.reinterpretExpressionAsPattern(expr.elements[i]);
                        }
                    }
                    break;
                case syntax_1.Syntax.ObjectExpression:
                    expr.type = syntax_1.Syntax.ObjectPattern;
                    for (let i = 0; i < expr.properties.length; i++) {
                        const property = expr.properties[i];
                        this.reinterpretExpressionAsPattern(property.type === syntax_1.Syntax.SpreadElement ? property : property.value);
                    }
                    break;
                case syntax_1.Syntax.AssignmentExpression:
                    expr.type = syntax_1.Syntax.AssignmentPattern;
                    delete expr.operator;
                    this.reinterpretExpressionAsPattern(expr.left);
                    break;
                default:
                    // Allow other node type for tolerant parsing.
                    break;
            }
        }
        parseGroupExpression() {
            let expr;
            this.expect('(');
            if (this.match(')')) {
                this.nextToken();
                if (!this.match('=>')) {
                    this.expect('=>');
                }
                expr = {
                    type: ArrowParameterPlaceHolder,
                    params: [],
                    async: false
                };
            }
            else {
                const startToken = this.lookahead;
                const params = [];
                if (this.match('...')) {
                    expr = this.parseRestElement(params);
                    this.expect(')');
                    if (!this.match('=>')) {
                        this.expect('=>');
                    }
                    expr = {
                        type: ArrowParameterPlaceHolder,
                        params: [expr],
                        async: false
                    };
                }
                else {
                    let arrow = false;
                    this.context.isBindingElement = true;
                    expr = this.inheritCoverGrammar(this.parseAssignmentExpression);
                    if (this.match(',')) {
                        const expressions = [];
                        this.context.isAssignmentTarget = false;
                        expressions.push(expr);
                        while (this.lookahead.type !== 2 /* EOF */) {
                            if (!this.match(',')) {
                                break;
                            }
                            this.nextToken();
                            if (this.match(')')) {
                                this.nextToken();
                                for (let i = 0; i < expressions.length; i++) {
                                    this.reinterpretExpressionAsPattern(expressions[i]);
                                }
                                arrow = true;
                                expr = {
                                    type: ArrowParameterPlaceHolder,
                                    params: expressions,
                                    async: false
                                };
                            }
                            else if (this.match('...')) {
                                if (!this.context.isBindingElement) {
                                    this.throwUnexpectedToken(this.lookahead);
                                }
                                expressions.push(this.parseRestElement(params));
                                this.expect(')');
                                if (!this.match('=>')) {
                                    this.expect('=>');
                                }
                                this.context.isBindingElement = false;
                                for (let i = 0; i < expressions.length; i++) {
                                    this.reinterpretExpressionAsPattern(expressions[i]);
                                }
                                arrow = true;
                                expr = {
                                    type: ArrowParameterPlaceHolder,
                                    params: expressions,
                                    async: false
                                };
                            }
                            else {
                                expressions.push(this.inheritCoverGrammar(this.parseAssignmentExpression));
                            }
                            if (arrow) {
                                break;
                            }
                        }
                        if (!arrow) {
                            expr = this.finalize(this.startNode(startToken), new Node.SequenceExpression(expressions));
                        }
                    }
                    if (!arrow) {
                        this.expect(')');
                        if (this.match('=>')) {
                            if (expr.type === syntax_1.Syntax.Identifier && expr.name === 'yield') {
                                arrow = true;
                                expr = {
                                    type: ArrowParameterPlaceHolder,
                                    params: [expr],
                                    async: false
                                };
                            }
                            if (!arrow) {
                                if (!this.context.isBindingElement) {
                                    this.throwUnexpectedToken(this.lookahead);
                                }
                                if (expr.type === syntax_1.Syntax.SequenceExpression) {
                                    for (let i = 0; i < expr.expressions.length; i++) {
                                        this.reinterpretExpressionAsPattern(expr.expressions[i]);
                                    }
                                }
                                else {
                                    this.reinterpretExpressionAsPattern(expr);
                                }
                                const parameters = (expr.type === syntax_1.Syntax.SequenceExpression ? expr.expressions : [expr]);
                                expr = {
                                    type: ArrowParameterPlaceHolder,
                                    params: parameters,
                                    async: false
                                };
                            }
                        }
                        this.context.isBindingElement = false;
                    }
                }
            }
            return expr;
        }
        // https://tc39.github.io/ecma262/#sec-left-hand-side-expressions
        parseArguments() {
            this.expect('(');
            const args = [];
            if (!this.match(')')) {
                while (true) {
                    const expr = this.match('...') ? this.parseSpreadElement() :
                        this.isolateCoverGrammar(this.parseAssignmentExpression);
                    args.push(expr);
                    if (this.match(')')) {
                        break;
                    }
                    this.expectCommaSeparator();
                    if (this.match(')')) {
                        break;
                    }
                }
            }
            this.expect(')');
            return args;
        }
        isIdentifierName(token) {
            return token.type === 3 /* Identifier */ ||
                token.type === 4 /* Keyword */ ||
                token.type === 1 /* BooleanLiteral */ ||
                token.type === 5 /* NullLiteral */;
        }
        parseIdentifierName() {
            const node = this.createNode();
            const token = this.nextToken();
            if (!this.isIdentifierName(token)) {
                this.throwUnexpectedToken(token);
            }
            return this.finalize(node, new Node.Identifier(token.value));
        }
        parseNewExpression() {
            const node = this.createNode();
            const id = this.parseIdentifierName();
            assert_1.assert(id.name === 'new', 'New expression must start with `new`');
            let expr;
            if (this.match('.')) {
                this.nextToken();
                if (this.lookahead.type === 3 /* Identifier */ && this.context.inFunctionBody && this.lookahead.value === 'target') {
                    const property = this.parseIdentifierName();
                    expr = new Node.MetaProperty(id, property);
                }
                else {
                    this.throwUnexpectedToken(this.lookahead);
                }
            }
            else if (this.matchKeyword('import')) {
                this.throwUnexpectedToken(this.lookahead);
            }
            else {
                const callee = this.isolateCoverGrammar(this.parseLeftHandSideExpression);
                const args = this.match('(') ? this.parseArguments() : [];
                expr = new Node.NewExpression(callee, args);
                this.context.isAssignmentTarget = false;
                this.context.isBindingElement = false;
            }
            return this.finalize(node, expr);
        }
        parseAsyncArgument() {
            const arg = this.parseAssignmentExpression();
            this.context.firstCoverInitializedNameError = null;
            return arg;
        }
        parseAsyncArguments() {
            this.expect('(');
            const args = [];
            if (!this.match(')')) {
                while (true) {
                    const expr = this.match('...') ? this.parseSpreadElement() :
                        this.isolateCoverGrammar(this.parseAsyncArgument);
                    args.push(expr);
                    if (this.match(')')) {
                        break;
                    }
                    this.expectCommaSeparator();
                    if (this.match(')')) {
                        break;
                    }
                }
            }
            this.expect(')');
            return args;
        }
        matchImportCall() {
            let match = this.matchKeyword('import');
            if (match) {
                const state = this.scanner.saveState();
                this.scanner.scanComments();
                const next = this.scanner.lex();
                this.scanner.restoreState(state);
                match = (next.type === 7 /* Punctuator */) && (next.value === '(');
            }
            return match;
        }
        parseImportCall() {
            const node = this.createNode();
            this.expectKeyword('import');
            return this.finalize(node, new Node.Import());
        }
        parseLeftHandSideExpressionAllowCall() {
            const startToken = this.lookahead;
            const maybeAsync = this.matchContextualKeyword('async');
            const previousAllowIn = this.context.allowIn;
            this.context.allowIn = true;
            let expr;
            if (this.matchKeyword('super') && this.context.inFunctionBody) {
                expr = this.createNode();
                this.nextToken();
                expr = this.finalize(expr, new Node.Super());
                if (!this.match('(') && !this.match('.') && !this.match('[')) {
                    this.throwUnexpectedToken(this.lookahead);
                }
            }
            else {
                expr = this.inheritCoverGrammar(this.matchKeyword('new') ? this.parseNewExpression : this.parsePrimaryExpression);
            }
            while (true) {
                if (this.match('.')) {
                    this.context.isBindingElement = false;
                    this.context.isAssignmentTarget = true;
                    this.expect('.');
                    const property = this.parseIdentifierName();
                    expr = this.finalize(this.startNode(startToken), new Node.StaticMemberExpression(expr, property));
                }
                else if (this.match('(')) {
                    const asyncArrow = maybeAsync && (startToken.lineNumber === this.lookahead.lineNumber);
                    this.context.isBindingElement = false;
                    this.context.isAssignmentTarget = false;
                    const args = asyncArrow ? this.parseAsyncArguments() : this.parseArguments();
                    if (expr.type === syntax_1.Syntax.Import && args.length !== 1) {
                        this.tolerateError(messages_1.Messages.BadImportCallArity);
                    }
                    expr = this.finalize(this.startNode(startToken), new Node.CallExpression(expr, args));
                    if (asyncArrow && this.match('=>')) {
                        for (let i = 0; i < args.length; ++i) {
                            this.reinterpretExpressionAsPattern(args[i]);
                        }
                        expr = {
                            type: ArrowParameterPlaceHolder,
                            params: args,
                            async: true
                        };
                    }
                }
                else if (this.match('[')) {
                    this.context.isBindingElement = false;
                    this.context.isAssignmentTarget = true;
                    this.expect('[');
                    const property = this.isolateCoverGrammar(this.parseExpression);
                    this.expect(']');
                    expr = this.finalize(this.startNode(startToken), new Node.ComputedMemberExpression(expr, property));
                }
                else if (this.lookahead.type === 10 /* Template */ && this.lookahead.head) {
                    const quasi = this.parseTemplateLiteral();
                    expr = this.finalize(this.startNode(startToken), new Node.TaggedTemplateExpression(expr, quasi));
                }
                else {
                    break;
                }
            }
            this.context.allowIn = previousAllowIn;
            return expr;
        }
        parseSuper() {
            const node = this.createNode();
            this.expectKeyword('super');
            if (!this.match('[') && !this.match('.')) {
                this.throwUnexpectedToken(this.lookahead);
            }
            return this.finalize(node, new Node.Super());
        }
        parseLeftHandSideExpression() {
            assert_1.assert(this.context.allowIn, 'callee of new expression always allow in keyword.');
            const node = this.startNode(this.lookahead);
            let expr = (this.matchKeyword('super') && this.context.inFunctionBody) ? this.parseSuper() :
                this.inheritCoverGrammar(this.matchKeyword('new') ? this.parseNewExpression : this.parsePrimaryExpression);
            while (true) {
                if (this.match('[')) {
                    this.context.isBindingElement = false;
                    this.context.isAssignmentTarget = true;
                    this.expect('[');
                    const property = this.isolateCoverGrammar(this.parseExpression);
                    this.expect(']');
                    expr = this.finalize(node, new Node.ComputedMemberExpression(expr, property));
                }
                else if (this.match('.')) {
                    this.context.isBindingElement = false;
                    this.context.isAssignmentTarget = true;
                    this.expect('.');
                    const property = this.parseIdentifierName();
                    expr = this.finalize(node, new Node.StaticMemberExpression(expr, property));
                }
                else if (this.lookahead.type === 10 /* Template */ && this.lookahead.head) {
                    const quasi = this.parseTemplateLiteral();
                    expr = this.finalize(node, new Node.TaggedTemplateExpression(expr, quasi));
                }
                else {
                    break;
                }
            }
            return expr;
        }
        // https://tc39.github.io/ecma262/#sec-update-expressions
        parseUpdateExpression() {
            let expr;
            const startToken = this.lookahead;
            if (this.match('++') || this.match('--')) {
                const node = this.startNode(startToken);
                const token = this.nextToken();
                expr = this.inheritCoverGrammar(this.parseUnaryExpression);
                if (this.context.strict && expr.type === syntax_1.Syntax.Identifier && this.scanner.isRestrictedWord(expr.name)) {
                    this.tolerateError(messages_1.Messages.StrictLHSPrefix);
                }
                if (!this.context.isAssignmentTarget) {
                    this.tolerateError(messages_1.Messages.InvalidLHSInAssignment);
                }
                const prefix = true;
                expr = this.finalize(node, new Node.UpdateExpression(token.value, expr, prefix));
                this.context.isAssignmentTarget = false;
                this.context.isBindingElement = false;
            }
            else {
                expr = this.inheritCoverGrammar(this.parseLeftHandSideExpressionAllowCall);
                if (!this.hasLineTerminator && this.lookahead.type === 7 /* Punctuator */) {
                    if (this.match('++') || this.match('--')) {
                        if (this.context.strict && expr.type === syntax_1.Syntax.Identifier && this.scanner.isRestrictedWord(expr.name)) {
                            this.tolerateError(messages_1.Messages.StrictLHSPostfix);
                        }
                        if (!this.context.isAssignmentTarget) {
                            this.tolerateError(messages_1.Messages.InvalidLHSInAssignment);
                        }
                        this.context.isAssignmentTarget = false;
                        this.context.isBindingElement = false;
                        const operator = this.nextToken().value;
                        const prefix = false;
                        expr = this.finalize(this.startNode(startToken), new Node.UpdateExpression(operator, expr, prefix));
                    }
                }
            }
            return expr;
        }
        // https://tc39.github.io/ecma262/#sec-unary-operators
        parseAwaitExpression() {
            const node = this.createNode();
            this.nextToken();
            const argument = this.parseUnaryExpression();
            return this.finalize(node, new Node.AwaitExpression(argument));
        }
        parseUnaryExpression() {
            let expr;
            if (this.match('+') || this.match('-') || this.match('~') || this.match('!') ||
                this.matchKeyword('delete') || this.matchKeyword('void') || this.matchKeyword('typeof')) {
                const node = this.startNode(this.lookahead);
                const token = this.nextToken();
                expr = this.inheritCoverGrammar(this.parseUnaryExpression);
                expr = this.finalize(node, new Node.UnaryExpression(token.value, expr));
                if (this.context.strict && expr.operator === 'delete' && expr.argument.type === syntax_1.Syntax.Identifier) {
                    this.tolerateError(messages_1.Messages.StrictDelete);
                }
                this.context.isAssignmentTarget = false;
                this.context.isBindingElement = false;
            }
            else if (this.context.await && this.matchContextualKeyword('await')) {
                expr = this.parseAwaitExpression();
            }
            else {
                expr = this.parseUpdateExpression();
            }
            return expr;
        }
        parseExponentiationExpression() {
            const startToken = this.lookahead;
            let expr = this.inheritCoverGrammar(this.parseUnaryExpression);
            if (expr.type !== syntax_1.Syntax.UnaryExpression && this.match('**')) {
                this.nextToken();
                this.context.isAssignmentTarget = false;
                this.context.isBindingElement = false;
                const left = expr;
                const right = this.isolateCoverGrammar(this.parseExponentiationExpression);
                expr = this.finalize(this.startNode(startToken), new Node.BinaryExpression('**', left, right));
            }
            return expr;
        }
        // https://tc39.github.io/ecma262/#sec-exp-operator
        // https://tc39.github.io/ecma262/#sec-multiplicative-operators
        // https://tc39.github.io/ecma262/#sec-additive-operators
        // https://tc39.github.io/ecma262/#sec-bitwise-shift-operators
        // https://tc39.github.io/ecma262/#sec-relational-operators
        // https://tc39.github.io/ecma262/#sec-equality-operators
        // https://tc39.github.io/ecma262/#sec-binary-bitwise-operators
        // https://tc39.github.io/ecma262/#sec-binary-logical-operators
        binaryPrecedence(token) {
            const op = token.value;
            let precedence;
            if (token.type === 7 /* Punctuator */) {
                precedence = this.operatorPrecedence[op] || 0;
            }
            else if (token.type === 4 /* Keyword */) {
                precedence = (op === 'instanceof' || (this.context.allowIn && op === 'in')) ? 7 : 0;
            }
            else {
                precedence = 0;
            }
            return precedence;
        }
        parseBinaryExpression() {
            const startToken = this.lookahead;
            let expr = this.inheritCoverGrammar(this.parseExponentiationExpression);
            const token = this.lookahead;
            let prec = this.binaryPrecedence(token);
            if (prec > 0) {
                this.nextToken();
                this.context.isAssignmentTarget = false;
                this.context.isBindingElement = false;
                const markers = [startToken, this.lookahead];
                let left = expr;
                let right = this.isolateCoverGrammar(this.parseExponentiationExpression);
                const stack = [left, token.value, right];
                const precedences = [prec];
                while (true) {
                    prec = this.binaryPrecedence(this.lookahead);
                    if (prec <= 0) {
                        break;
                    }
                    // Reduce: make a binary expression from the three topmost entries.
                    while ((stack.length > 2) && (prec <= precedences[precedences.length - 1])) {
                        right = stack.pop();
                        const operator = stack.pop();
                        precedences.pop();
                        left = stack.pop();
                        markers.pop();
                        const node = this.startNode(markers[markers.length - 1]);
                        stack.push(this.finalize(node, new Node.BinaryExpression(operator, left, right)));
                    }
                    // Shift.
                    stack.push(this.nextToken().value);
                    precedences.push(prec);
                    markers.push(this.lookahead);
                    stack.push(this.isolateCoverGrammar(this.parseExponentiationExpression));
                }
                // Final reduce to clean-up the stack.
                let i = stack.length - 1;
                expr = stack[i];
                let lastMarker = markers.pop();
                while (i > 1) {
                    const marker = markers.pop();
                    const lastLineStart = lastMarker && lastMarker.lineStart;
                    const node = this.startNode(marker, lastLineStart);
                    const operator = stack[i - 1];
                    expr = this.finalize(node, new Node.BinaryExpression(operator, stack[i - 2], expr));
                    i -= 2;
                    lastMarker = marker;
                }
            }
            return expr;
        }
        // https://tc39.github.io/ecma262/#sec-conditional-operator
        parseConditionalExpression() {
            const startToken = this.lookahead;
            let expr = this.inheritCoverGrammar(this.parseBinaryExpression);
            if (this.match('?')) {
                this.nextToken();
                const previousAllowIn = this.context.allowIn;
                this.context.allowIn = true;
                const consequent = this.isolateCoverGrammar(this.parseAssignmentExpression);
                this.context.allowIn = previousAllowIn;
                this.expect(':');
                const alternate = this.isolateCoverGrammar(this.parseAssignmentExpression);
                expr = this.finalize(this.startNode(startToken), new Node.ConditionalExpression(expr, consequent, alternate));
                this.context.isAssignmentTarget = false;
                this.context.isBindingElement = false;
            }
            return expr;
        }
        // https://tc39.github.io/ecma262/#sec-assignment-operators
        checkPatternParam(options, param) {
            switch (param.type) {
                case syntax_1.Syntax.Identifier:
                    this.validateParam(options, param, param.name);
                    break;
                case syntax_1.Syntax.RestElement:
                    this.checkPatternParam(options, param.argument);
                    break;
                case syntax_1.Syntax.AssignmentPattern:
                    this.checkPatternParam(options, param.left);
                    break;
                case syntax_1.Syntax.ArrayPattern:
                    for (let i = 0; i < param.elements.length; i++) {
                        if (param.elements[i] !== null) {
                            this.checkPatternParam(options, param.elements[i]);
                        }
                    }
                    break;
                case syntax_1.Syntax.ObjectPattern:
                    for (let i = 0; i < param.properties.length; i++) {
                        const property = param.properties[i];
                        this.checkPatternParam(options, (property.type === syntax_1.Syntax.RestElement) ? property : property.value);
                    }
                    break;
                default:
                    break;
            }
            options.simple = options.simple && (param instanceof Node.Identifier);
        }
        reinterpretAsCoverFormalsList(expr) {
            let params = [expr];
            let options;
            let asyncArrow = false;
            switch (expr.type) {
                case syntax_1.Syntax.Identifier:
                    break;
                case ArrowParameterPlaceHolder:
                    params = expr.params;
                    asyncArrow = expr.async;
                    break;
                default:
                    return null;
            }
            options = {
                simple: true,
                paramSet: {}
            };
            for (let i = 0; i < params.length; ++i) {
                const param = params[i];
                if (param.type === syntax_1.Syntax.AssignmentPattern) {
                    if (param.right.type === syntax_1.Syntax.YieldExpression) {
                        if (param.right.argument) {
                            this.throwUnexpectedToken(this.lookahead);
                        }
                        param.right.type = syntax_1.Syntax.Identifier;
                        param.right.name = 'yield';
                        delete param.right.argument;
                        delete param.right.delegate;
                    }
                }
                else if (asyncArrow && param.type === syntax_1.Syntax.Identifier && param.name === 'await') {
                    this.throwUnexpectedToken(this.lookahead);
                }
                this.checkPatternParam(options, param);
                params[i] = param;
            }
            if (this.context.strict || !this.context.allowYield) {
                for (let i = 0; i < params.length; ++i) {
                    const param = params[i];
                    if (param.type === syntax_1.Syntax.YieldExpression) {
                        this.throwUnexpectedToken(this.lookahead);
                    }
                }
            }
            if (options.message === messages_1.Messages.StrictParamDupe) {
                const token = this.context.strict ? options.stricted : options.firstRestricted;
                this.throwUnexpectedToken(token, options.message);
            }
            return {
                simple: options.simple,
                params: params,
                stricted: options.stricted,
                firstRestricted: options.firstRestricted,
                message: options.message
            };
        }
        parseAssignmentExpression() {
            let expr;
            if (!this.context.allowYield && this.matchKeyword('yield')) {
                expr = this.parseYieldExpression();
            }
            else {
                const startToken = this.lookahead;
                let token = startToken;
                expr = this.parseConditionalExpression();
                if (token.type === 3 /* Identifier */ && (token.lineNumber === this.lookahead.lineNumber) && token.value === 'async') {
                    if (this.lookahead.type === 3 /* Identifier */ || this.matchKeyword('yield')) {
                        const arg = this.parsePrimaryExpression();
                        this.reinterpretExpressionAsPattern(arg);
                        expr = {
                            type: ArrowParameterPlaceHolder,
                            params: [arg],
                            async: true
                        };
                    }
                }
                if (expr.type === ArrowParameterPlaceHolder || this.match('=>')) {
                    // https://tc39.github.io/ecma262/#sec-arrow-function-definitions
                    this.context.isAssignmentTarget = false;
                    this.context.isBindingElement = false;
                    const isAsync = expr.async;
                    const list = this.reinterpretAsCoverFormalsList(expr);
                    if (list) {
                        if (this.hasLineTerminator) {
                            this.tolerateUnexpectedToken(this.lookahead);
                        }
                        this.context.firstCoverInitializedNameError = null;
                        const previousStrict = this.context.strict;
                        const previousAllowStrictDirective = this.context.allowStrictDirective;
                        this.context.allowStrictDirective = list.simple;
                        const previousAllowYield = this.context.allowYield;
                        const previousAwait = this.context.await;
                        this.context.allowYield = true;
                        this.context.await = isAsync;
                        const node = this.startNode(startToken);
                        this.expect('=>');
                        let body;
                        if (this.match('{')) {
                            const previousAllowIn = this.context.allowIn;
                            this.context.allowIn = true;
                            body = this.parseFunctionSourceElements();
                            this.context.allowIn = previousAllowIn;
                        }
                        else {
                            body = this.isolateCoverGrammar(this.parseAssignmentExpression);
                        }
                        const expression = body.type !== syntax_1.Syntax.BlockStatement;
                        if (this.context.strict && list.firstRestricted) {
                            this.throwUnexpectedToken(list.firstRestricted, list.message);
                        }
                        if (this.context.strict && list.stricted) {
                            this.tolerateUnexpectedToken(list.stricted, list.message);
                        }
                        expr = isAsync ? this.finalize(node, new Node.AsyncArrowFunctionExpression(list.params, body, expression)) :
                            this.finalize(node, new Node.ArrowFunctionExpression(list.params, body, expression));
                        this.context.strict = previousStrict;
                        this.context.allowStrictDirective = previousAllowStrictDirective;
                        this.context.allowYield = previousAllowYield;
                        this.context.await = previousAwait;
                    }
                }
                else {
                    if (this.matchAssign()) {
                        if (!this.context.isAssignmentTarget) {
                            this.tolerateError(messages_1.Messages.InvalidLHSInAssignment);
                        }
                        if (this.context.strict && expr.type === syntax_1.Syntax.Identifier) {
                            const id = expr;
                            if (this.scanner.isRestrictedWord(id.name)) {
                                this.tolerateUnexpectedToken(token, messages_1.Messages.StrictLHSAssignment);
                            }
                            if (this.scanner.isStrictModeReservedWord(id.name)) {
                                this.tolerateUnexpectedToken(token, messages_1.Messages.StrictReservedWord);
                            }
                        }
                        if (!this.match('=')) {
                            this.context.isAssignmentTarget = false;
                            this.context.isBindingElement = false;
                        }
                        else {
                            this.reinterpretExpressionAsPattern(expr);
                        }
                        token = this.nextToken();
                        const operator = token.value;
                        const right = this.isolateCoverGrammar(this.parseAssignmentExpression);
                        expr = this.finalize(this.startNode(startToken), new Node.AssignmentExpression(operator, expr, right));
                        this.context.firstCoverInitializedNameError = null;
                    }
                }
            }
            return expr;
        }
        // https://tc39.github.io/ecma262/#sec-comma-operator
        parseExpression() {
            const startToken = this.lookahead;
            let expr = this.isolateCoverGrammar(this.parseAssignmentExpression);
            if (this.match(',')) {
                const expressions = [];
                expressions.push(expr);
                while (this.lookahead.type !== 2 /* EOF */) {
                    if (!this.match(',')) {
                        break;
                    }
                    this.nextToken();
                    expressions.push(this.isolateCoverGrammar(this.parseAssignmentExpression));
                }
                expr = this.finalize(this.startNode(startToken), new Node.SequenceExpression(expressions));
            }
            return expr;
        }
        // https://tc39.github.io/ecma262/#sec-block
        parseStatementListItem() {
            let statement;
            this.context.isAssignmentTarget = true;
            this.context.isBindingElement = true;
            if (this.lookahead.type === 4 /* Keyword */) {
                switch (this.lookahead.value) {
                    case 'export':
                        if (!this.context.isModule) {
                            this.tolerateUnexpectedToken(this.lookahead, messages_1.Messages.IllegalExportDeclaration);
                        }
                        statement = this.parseExportDeclaration();
                        break;
                    case 'import':
                        if (this.matchImportCall()) {
                            statement = this.parseExpressionStatement();
                        }
                        else {
                            if (!this.context.isModule) {
                                this.tolerateUnexpectedToken(this.lookahead, messages_1.Messages.IllegalImportDeclaration);
                            }
                            statement = this.parseImportDeclaration();
                        }
                        break;
                    case 'const':
                        statement = this.parseLexicalDeclaration({ inFor: false });
                        break;
                    case 'function':
                        statement = this.parseFunctionDeclaration();
                        break;
                    case 'class':
                        statement = this.parseClassDeclaration();
                        break;
                    case 'let':
                        statement = this.isLexicalDeclaration() ? this.parseLexicalDeclaration({ inFor: false }) : this.parseStatement();
                        break;
                    default:
                        statement = this.parseStatement();
                        break;
                }
            }
            else {
                statement = this.parseStatement();
            }
            return statement;
        }
        parseBlock() {
            const node = this.createNode();
            this.expect('{');
            const block = [];
            while (true) {
                if (this.match('}')) {
                    break;
                }
                block.push(this.parseStatementListItem());
            }
            this.expect('}');
            return this.finalize(node, new Node.BlockStatement(block));
        }
        // https://tc39.github.io/ecma262/#sec-let-and-const-declarations
        parseLexicalBinding(kind, options) {
            const node = this.createNode();
            const params = [];
            const id = this.parsePattern(params, kind);
            if (this.context.strict && id.type === syntax_1.Syntax.Identifier) {
                if (this.scanner.isRestrictedWord(id.name)) {
                    this.tolerateError(messages_1.Messages.StrictVarName);
                }
            }
            let init = null;
            if (kind === 'const') {
                if (!this.matchKeyword('in') && !this.matchContextualKeyword('of')) {
                    if (this.match('=')) {
                        this.nextToken();
                        init = this.isolateCoverGrammar(this.parseAssignmentExpression);
                    }
                    else {
                        this.throwError(messages_1.Messages.DeclarationMissingInitializer, 'const');
                    }
                }
            }
            else if ((!options.inFor && id.type !== syntax_1.Syntax.Identifier) || this.match('=')) {
                this.expect('=');
                init = this.isolateCoverGrammar(this.parseAssignmentExpression);
            }
            return this.finalize(node, new Node.VariableDeclarator(id, init));
        }
        parseBindingList(kind, options) {
            const list = [this.parseLexicalBinding(kind, options)];
            while (this.match(',')) {
                this.nextToken();
                list.push(this.parseLexicalBinding(kind, options));
            }
            return list;
        }
        isLexicalDeclaration() {
            const state = this.scanner.saveState();
            this.scanner.scanComments();
            const next = this.scanner.lex();
            this.scanner.restoreState(state);
            return (next.type === 3 /* Identifier */) ||
                (next.type === 7 /* Punctuator */ && next.value === '[') ||
                (next.type === 7 /* Punctuator */ && next.value === '{') ||
                (next.type === 4 /* Keyword */ && next.value === 'let') ||
                (next.type === 4 /* Keyword */ && next.value === 'yield');
        }
        parseLexicalDeclaration(options) {
            const node = this.createNode();
            const kind = this.nextToken().value;
            assert_1.assert(kind === 'let' || kind === 'const', 'Lexical declaration must be either let or const');
            const declarations = this.parseBindingList(kind, options);
            this.consumeSemicolon();
            return this.finalize(node, new Node.VariableDeclaration(declarations, kind));
        }
        // https://tc39.github.io/ecma262/#sec-destructuring-binding-patterns
        parseBindingRestElement(params, kind) {
            const node = this.createNode();
            this.expect('...');
            const arg = this.parsePattern(params, kind);
            return this.finalize(node, new Node.RestElement(arg));
        }
        parseArrayPattern(params, kind) {
            const node = this.createNode();
            this.expect('[');
            const elements = [];
            while (!this.match(']')) {
                if (this.match(',')) {
                    this.nextToken();
                    elements.push(null);
                }
                else {
                    if (this.match('...')) {
                        elements.push(this.parseBindingRestElement(params, kind));
                        break;
                    }
                    else {
                        elements.push(this.parsePatternWithDefault(params, kind));
                    }
                    if (!this.match(']')) {
                        this.expect(',');
                    }
                }
            }
            this.expect(']');
            return this.finalize(node, new Node.ArrayPattern(elements));
        }
        parsePropertyPattern(params, kind) {
            const node = this.createNode();
            let computed = false;
            let shorthand = false;
            const method = false;
            let key;
            let value;
            if (this.lookahead.type === 3 /* Identifier */) {
                const keyToken = this.lookahead;
                key = this.parseVariableIdentifier();
                const init = this.finalize(node, new Node.Identifier(keyToken.value));
                if (this.match('=')) {
                    params.push(keyToken);
                    shorthand = true;
                    this.nextToken();
                    const expr = this.parseAssignmentExpression();
                    value = this.finalize(this.startNode(keyToken), new Node.AssignmentPattern(init, expr));
                }
                else if (!this.match(':')) {
                    params.push(keyToken);
                    shorthand = true;
                    value = init;
                }
                else {
                    this.expect(':');
                    value = this.parsePatternWithDefault(params, kind);
                }
            }
            else {
                computed = this.match('[');
                key = this.parseObjectPropertyKey();
                this.expect(':');
                value = this.parsePatternWithDefault(params, kind);
            }
            return this.finalize(node, new Node.Property('init', key, computed, value, method, shorthand));
        }
        parseRestProperty(params, kind) {
            const node = this.createNode();
            this.expect('...');
            const arg = this.parsePattern(params);
            if (this.match('=')) {
                this.throwError(messages_1.Messages.DefaultRestProperty);
            }
            if (!this.match('}')) {
                this.throwError(messages_1.Messages.PropertyAfterRestProperty);
            }
            return this.finalize(node, new Node.RestElement(arg));
        }
        parseObjectPattern(params, kind) {
            const node = this.createNode();
            const properties = [];
            this.expect('{');
            while (!this.match('}')) {
                properties.push(this.match('...') ? this.parseRestProperty(params, kind) : this.parsePropertyPattern(params, kind));
                if (!this.match('}')) {
                    this.expect(',');
                }
            }
            this.expect('}');
            return this.finalize(node, new Node.ObjectPattern(properties));
        }
        parsePattern(params, kind) {
            let pattern;
            if (this.match('[')) {
                pattern = this.parseArrayPattern(params, kind);
            }
            else if (this.match('{')) {
                pattern = this.parseObjectPattern(params, kind);
            }
            else {
                if (this.matchKeyword('let') && (kind === 'const' || kind === 'let')) {
                    this.tolerateUnexpectedToken(this.lookahead, messages_1.Messages.LetInLexicalBinding);
                }
                params.push(this.lookahead);
                pattern = this.parseVariableIdentifier(kind);
            }
            return pattern;
        }
        parsePatternWithDefault(params, kind) {
            const startToken = this.lookahead;
            let pattern = this.parsePattern(params, kind);
            if (this.match('=')) {
                this.nextToken();
                const previousAllowYield = this.context.allowYield;
                this.context.allowYield = true;
                const right = this.isolateCoverGrammar(this.parseAssignmentExpression);
                this.context.allowYield = previousAllowYield;
                pattern = this.finalize(this.startNode(startToken), new Node.AssignmentPattern(pattern, right));
            }
            return pattern;
        }
        // https://tc39.github.io/ecma262/#sec-variable-statement
        parseVariableIdentifier(kind) {
            const node = this.createNode();
            const token = this.nextToken();
            if (token.type === 4 /* Keyword */ && token.value === 'yield') {
                if (this.context.strict) {
                    this.tolerateUnexpectedToken(token, messages_1.Messages.StrictReservedWord);
                }
                else if (!this.context.allowYield) {
                    this.throwUnexpectedToken(token);
                }
            }
            else if (token.type !== 3 /* Identifier */) {
                if (this.context.strict && token.type === 4 /* Keyword */ && this.scanner.isStrictModeReservedWord(token.value)) {
                    this.tolerateUnexpectedToken(token, messages_1.Messages.StrictReservedWord);
                }
                else {
                    if (this.context.strict || token.value !== 'let' || kind !== 'var') {
                        this.throwUnexpectedToken(token);
                    }
                }
            }
            else if ((this.context.isModule || this.context.await) && token.type === 3 /* Identifier */ && token.value === 'await') {
                this.tolerateUnexpectedToken(token);
            }
            return this.finalize(node, new Node.Identifier(token.value));
        }
        parseVariableDeclaration(options) {
            const node = this.createNode();
            const params = [];
            const id = this.parsePattern(params, 'var');
            if (this.context.strict && id.type === syntax_1.Syntax.Identifier) {
                if (this.scanner.isRestrictedWord(id.name)) {
                    this.tolerateError(messages_1.Messages.StrictVarName);
                }
            }
            let init = null;
            if (this.match('=')) {
                this.nextToken();
                init = this.isolateCoverGrammar(this.parseAssignmentExpression);
            }
            else if (id.type !== syntax_1.Syntax.Identifier && !options.inFor) {
                this.expect('=');
            }
            return this.finalize(node, new Node.VariableDeclarator(id, init));
        }
        parseVariableDeclarationList(options) {
            const opt = { inFor: options.inFor };
            const list = [];
            list.push(this.parseVariableDeclaration(opt));
            while (this.match(',')) {
                this.nextToken();
                list.push(this.parseVariableDeclaration(opt));
            }
            return list;
        }
        parseVariableStatement() {
            const node = this.createNode();
            this.expectKeyword('var');
            const declarations = this.parseVariableDeclarationList({ inFor: false });
            this.consumeSemicolon();
            return this.finalize(node, new Node.VariableDeclaration(declarations, 'var'));
        }
        // https://tc39.github.io/ecma262/#sec-empty-statement
        parseEmptyStatement() {
            const node = this.createNode();
            this.expect(';');
            return this.finalize(node, new Node.EmptyStatement());
        }
        // https://tc39.github.io/ecma262/#sec-expression-statement
        parseExpressionStatement() {
            const node = this.createNode();
            const expr = this.parseExpression();
            this.consumeSemicolon();
            return this.finalize(node, new Node.ExpressionStatement(expr));
        }
        // https://tc39.github.io/ecma262/#sec-if-statement
        parseIfClause() {
            if (this.context.strict && this.matchKeyword('function')) {
                this.tolerateError(messages_1.Messages.StrictFunction);
            }
            return this.parseStatement();
        }
        parseIfStatement() {
            const node = this.createNode();
            let consequent;
            let alternate = null;
            this.expectKeyword('if');
            this.expect('(');
            const test = this.parseExpression();
            if (!this.match(')') && this.config.tolerant) {
                this.tolerateUnexpectedToken(this.nextToken());
                consequent = this.finalize(this.createNode(), new Node.EmptyStatement());
            }
            else {
                this.expect(')');
                consequent = this.parseIfClause();
                if (this.matchKeyword('else')) {
                    this.nextToken();
                    alternate = this.parseIfClause();
                }
            }
            return this.finalize(node, new Node.IfStatement(test, consequent, alternate));
        }
        // https://tc39.github.io/ecma262/#sec-do-while-statement
        parseDoWhileStatement() {
            const node = this.createNode();
            this.expectKeyword('do');
            const previousInIteration = this.context.inIteration;
            this.context.inIteration = true;
            const body = this.parseStatement();
            this.context.inIteration = previousInIteration;
            this.expectKeyword('while');
            this.expect('(');
            const test = this.parseExpression();
            if (!this.match(')') && this.config.tolerant) {
                this.tolerateUnexpectedToken(this.nextToken());
            }
            else {
                this.expect(')');
                if (this.match(';')) {
                    this.nextToken();
                }
            }
            return this.finalize(node, new Node.DoWhileStatement(body, test));
        }
        // https://tc39.github.io/ecma262/#sec-while-statement
        parseWhileStatement() {
            const node = this.createNode();
            let body;
            this.expectKeyword('while');
            this.expect('(');
            const test = this.parseExpression();
            if (!this.match(')') && this.config.tolerant) {
                this.tolerateUnexpectedToken(this.nextToken());
                body = this.finalize(this.createNode(), new Node.EmptyStatement());
            }
            else {
                this.expect(')');
                const previousInIteration = this.context.inIteration;
                this.context.inIteration = true;
                body = this.parseStatement();
                this.context.inIteration = previousInIteration;
            }
            return this.finalize(node, new Node.WhileStatement(test, body));
        }
        // https://tc39.github.io/ecma262/#sec-for-statement
        // https://tc39.github.io/ecma262/#sec-for-in-and-for-of-statements
        parseForStatement() {
            let init = null;
            let test = null;
            let update = null;
            let forIn = true;
            let isForAwaitOf = false;
            let left, right;
            const node = this.createNode();
            this.expectKeyword('for');
            if (this.matchContextualKeyword('await')) {
                if (!this.context.await) {
                    this.throwUnexpectedToken(this.lookahead, messages_1.Messages.ForAwaitInNonAsync);
                }
                this.nextToken();
                isForAwaitOf = true;
            }
            this.expect('(');
            if (this.match(';')) {
                if (isForAwaitOf) {
                    this.throwUnexpectedToken(this.lookahead, messages_1.Messages.ForAwaitOfSemicolon);
                }
                this.nextToken();
            }
            else {
                if (this.matchKeyword('var')) {
                    init = this.createNode();
                    this.nextToken();
                    const previousAllowIn = this.context.allowIn;
                    this.context.allowIn = false;
                    const declarations = this.parseVariableDeclarationList({ inFor: true });
                    this.context.allowIn = previousAllowIn;
                    if (declarations.length === 1 && this.matchKeyword('in')) {
                        if (isForAwaitOf) {
                            this.throwUnexpectedToken(this.lookahead, messages_1.Messages.ForAwaitOfSawIn);
                        }
                        const decl = declarations[0];
                        if (decl.init && (decl.id.type === syntax_1.Syntax.ArrayPattern || decl.id.type === syntax_1.Syntax.ObjectPattern || this.context.strict)) {
                            this.tolerateError(messages_1.Messages.ForInOfLoopInitializer, 'for-in');
                        }
                        init = this.finalize(init, new Node.VariableDeclaration(declarations, 'var'));
                        this.nextToken();
                        left = init;
                        right = this.parseExpression();
                        init = null;
                    }
                    else if (declarations.length === 1 && declarations[0].init === null && this.matchContextualKeyword('of')) {
                        init = this.finalize(init, new Node.VariableDeclaration(declarations, 'var'));
                        this.nextToken();
                        left = init;
                        right = this.parseAssignmentExpression();
                        init = null;
                        forIn = false;
                    }
                    else {
                        init = this.finalize(init, new Node.VariableDeclaration(declarations, 'var'));
                        this.expect(';');
                    }
                }
                else if (this.matchKeyword('const') || this.matchKeyword('let')) {
                    init = this.createNode();
                    const kind = this.nextToken().value;
                    if (!this.context.strict && this.lookahead.value === 'in') {
                        init = this.finalize(init, new Node.Identifier(kind));
                        this.nextToken();
                        left = init;
                        right = this.parseExpression();
                        init = null;
                    }
                    else {
                        const previousAllowIn = this.context.allowIn;
                        this.context.allowIn = false;
                        const declarations = this.parseBindingList(kind, { inFor: true });
                        this.context.allowIn = previousAllowIn;
                        if (declarations.length === 1 && declarations[0].init === null && this.matchKeyword('in')) {
                            if (isForAwaitOf) {
                                this.throwUnexpectedToken(this.lookahead, messages_1.Messages.ForAwaitOfSawIn);
                            }
                            init = this.finalize(init, new Node.VariableDeclaration(declarations, kind));
                            this.nextToken();
                            left = init;
                            right = this.parseExpression();
                            init = null;
                        }
                        else if (declarations.length === 1 && declarations[0].init === null && this.matchContextualKeyword('of')) {
                            init = this.finalize(init, new Node.VariableDeclaration(declarations, kind));
                            this.nextToken();
                            left = init;
                            right = this.parseAssignmentExpression();
                            init = null;
                            forIn = false;
                        }
                        else {
                            this.consumeSemicolon();
                            init = this.finalize(init, new Node.VariableDeclaration(declarations, kind));
                        }
                    }
                }
                else {
                    const initStartToken = this.lookahead;
                    const previousIsBindingElement = this.context.isBindingElement;
                    const previousIsAssignmentTarget = this.context.isAssignmentTarget;
                    const previousFirstCoverInitializedNameError = this.context.firstCoverInitializedNameError;
                    const previousAllowIn = this.context.allowIn;
                    this.context.allowIn = false;
                    init = this.inheritCoverGrammar(this.parseAssignmentExpression);
                    this.context.allowIn = previousAllowIn;
                    if (this.matchKeyword('in')) {
                        if (!this.context.isAssignmentTarget || init.type === syntax_1.Syntax.AssignmentExpression) {
                            this.tolerateError(messages_1.Messages.InvalidLHSInForIn);
                        }
                        if (isForAwaitOf) {
                            this.throwUnexpectedToken(this.lookahead, messages_1.Messages.ForAwaitOfSawIn);
                        }
                        this.nextToken();
                        this.reinterpretExpressionAsPattern(init);
                        left = init;
                        right = this.parseExpression();
                        init = null;
                    }
                    else if (this.matchContextualKeyword('of')) {
                        if (!this.context.isAssignmentTarget || init.type === syntax_1.Syntax.AssignmentExpression) {
                            this.tolerateError(messages_1.Messages.InvalidLHSInForLoop);
                        }
                        this.nextToken();
                        this.reinterpretExpressionAsPattern(init);
                        left = init;
                        right = this.parseAssignmentExpression();
                        init = null;
                        forIn = false;
                    }
                    else {
                        // The `init` node was not parsed isolated, but we would have wanted it to.
                        this.context.isBindingElement = previousIsBindingElement;
                        this.context.isAssignmentTarget = previousIsAssignmentTarget;
                        this.context.firstCoverInitializedNameError = previousFirstCoverInitializedNameError;
                        if (this.match(',')) {
                            const initSeq = [init];
                            while (this.match(',')) {
                                this.nextToken();
                                initSeq.push(this.isolateCoverGrammar(this.parseAssignmentExpression));
                            }
                            init = this.finalize(this.startNode(initStartToken), new Node.SequenceExpression(initSeq));
                        }
                        this.expect(';');
                    }
                }
            }
            if (typeof left === 'undefined') {
                if (!this.match(';')) {
                    test = this.isolateCoverGrammar(this.parseExpression);
                }
                this.expect(';');
                if (!this.match(')')) {
                    update = this.isolateCoverGrammar(this.parseExpression);
                }
            }
            let body;
            if (!this.match(')') && this.config.tolerant) {
                this.tolerateUnexpectedToken(this.nextToken());
                body = this.finalize(this.createNode(), new Node.EmptyStatement());
            }
            else {
                this.expect(')');
                const previousInIteration = this.context.inIteration;
                this.context.inIteration = true;
                body = this.isolateCoverGrammar(this.parseStatement);
                this.context.inIteration = previousInIteration;
            }
            return (typeof left === 'undefined') ?
                this.finalize(node, new Node.ForStatement(init, test, update, body)) :
                forIn ? this.finalize(node, new Node.ForInStatement(left, right, body)) :
                    this.finalize(node, new Node.ForOfStatement(left, right, body, isForAwaitOf));
        }
        // https://tc39.github.io/ecma262/#sec-continue-statement
        parseContinueStatement() {
            const node = this.createNode();
            this.expectKeyword('continue');
            let label = null;
            if (this.lookahead.type === 3 /* Identifier */ && !this.hasLineTerminator) {
                const id = this.parseVariableIdentifier();
                label = id;
                const key = '$' + id.name;
                if (!Object.prototype.hasOwnProperty.call(this.context.labelSet, key)) {
                    this.throwError(messages_1.Messages.UnknownLabel, id.name);
                }
            }
            this.consumeSemicolon();
            if (label === null && !this.context.inIteration) {
                this.throwError(messages_1.Messages.IllegalContinue);
            }
            return this.finalize(node, new Node.ContinueStatement(label));
        }
        // https://tc39.github.io/ecma262/#sec-break-statement
        parseBreakStatement() {
            const node = this.createNode();
            this.expectKeyword('break');
            let label = null;
            if (this.lookahead.type === 3 /* Identifier */ && !this.hasLineTerminator) {
                const id = this.parseVariableIdentifier();
                const key = '$' + id.name;
                if (!Object.prototype.hasOwnProperty.call(this.context.labelSet, key)) {
                    this.throwError(messages_1.Messages.UnknownLabel, id.name);
                }
                label = id;
            }
            this.consumeSemicolon();
            if (label === null && !this.context.inIteration && !this.context.inSwitch) {
                this.throwError(messages_1.Messages.IllegalBreak);
            }
            return this.finalize(node, new Node.BreakStatement(label));
        }
        // https://tc39.github.io/ecma262/#sec-return-statement
        parseReturnStatement() {
            if (!this.context.inFunctionBody) {
                this.tolerateError(messages_1.Messages.IllegalReturn);
            }
            const node = this.createNode();
            this.expectKeyword('return');
            const hasArgument = (!this.match(';') && !this.match('}') &&
                !this.hasLineTerminator && this.lookahead.type !== 2 /* EOF */) ||
                this.lookahead.type === 8 /* StringLiteral */ ||
                this.lookahead.type === 10 /* Template */;
            const argument = hasArgument ? this.parseExpression() : null;
            this.consumeSemicolon();
            return this.finalize(node, new Node.ReturnStatement(argument));
        }
        // https://tc39.github.io/ecma262/#sec-with-statement
        parseWithStatement() {
            if (this.context.strict) {
                this.tolerateError(messages_1.Messages.StrictModeWith);
            }
            const node = this.createNode();
            let body;
            this.expectKeyword('with');
            this.expect('(');
            const object = this.parseExpression();
            if (!this.match(')') && this.config.tolerant) {
                this.tolerateUnexpectedToken(this.nextToken());
                body = this.finalize(this.createNode(), new Node.EmptyStatement());
            }
            else {
                this.expect(')');
                body = this.parseStatement();
            }
            return this.finalize(node, new Node.WithStatement(object, body));
        }
        // https://tc39.github.io/ecma262/#sec-switch-statement
        parseSwitchCase() {
            const node = this.createNode();
            let test;
            if (this.matchKeyword('default')) {
                this.nextToken();
                test = null;
            }
            else {
                this.expectKeyword('case');
                test = this.parseExpression();
            }
            this.expect(':');
            const consequent = [];
            while (true) {
                if (this.match('}') || this.matchKeyword('default') || this.matchKeyword('case')) {
                    break;
                }
                consequent.push(this.parseStatementListItem());
            }
            return this.finalize(node, new Node.SwitchCase(test, consequent));
        }
        parseSwitchStatement() {
            const node = this.createNode();
            this.expectKeyword('switch');
            this.expect('(');
            const discriminant = this.parseExpression();
            this.expect(')');
            const previousInSwitch = this.context.inSwitch;
            this.context.inSwitch = true;
            const cases = [];
            let defaultFound = false;
            this.expect('{');
            while (true) {
                if (this.match('}')) {
                    break;
                }
                const clause = this.parseSwitchCase();
                if (clause.test === null) {
                    if (defaultFound) {
                        this.throwError(messages_1.Messages.MultipleDefaultsInSwitch);
                    }
                    defaultFound = true;
                }
                cases.push(clause);
            }
            this.expect('}');
            this.context.inSwitch = previousInSwitch;
            return this.finalize(node, new Node.SwitchStatement(discriminant, cases));
        }
        // https://tc39.github.io/ecma262/#sec-labelled-statements
        parseLabelledStatement() {
            const node = this.createNode();
            const expr = this.parseExpression();
            let statement;
            if ((expr.type === syntax_1.Syntax.Identifier) && this.match(':')) {
                this.nextToken();
                const id = expr;
                const key = '$' + id.name;
                if (Object.prototype.hasOwnProperty.call(this.context.labelSet, key)) {
                    this.throwError(messages_1.Messages.Redeclaration, 'Label', id.name);
                }
                this.context.labelSet[key] = true;
                let body;
                if (this.matchKeyword('class')) {
                    this.tolerateUnexpectedToken(this.lookahead);
                    body = this.parseClassDeclaration();
                }
                else if (this.matchKeyword('function')) {
                    const token = this.lookahead;
                    const declaration = this.parseFunctionDeclaration();
                    if (this.context.strict) {
                        this.tolerateUnexpectedToken(token, messages_1.Messages.StrictFunction);
                    }
                    else if (declaration.generator) {
                        this.tolerateUnexpectedToken(token, messages_1.Messages.GeneratorInLegacyContext);
                    }
                    body = declaration;
                }
                else {
                    body = this.parseStatement();
                }
                delete this.context.labelSet[key];
                statement = new Node.LabeledStatement(id, body);
            }
            else {
                this.consumeSemicolon();
                statement = new Node.ExpressionStatement(expr);
            }
            return this.finalize(node, statement);
        }
        // https://tc39.github.io/ecma262/#sec-throw-statement
        parseThrowStatement() {
            const node = this.createNode();
            this.expectKeyword('throw');
            if (this.hasLineTerminator) {
                this.throwError(messages_1.Messages.NewlineAfterThrow);
            }
            const argument = this.parseExpression();
            this.consumeSemicolon();
            return this.finalize(node, new Node.ThrowStatement(argument));
        }
        // https://tc39.github.io/ecma262/#sec-try-statement
        parseCatchClause() {
            const node = this.createNode();
            let body;
            this.expectKeyword('catch');
            if (!this.match('(')) {
                body = this.parseBlock();
                return this.finalize(node, new Node.CatchClause(null, body));
            }
            this.expect('(');
            if (this.match(')')) {
                this.throwUnexpectedToken(this.lookahead);
            }
            const params = [];
            const param = this.parsePattern(params);
            const paramMap = {};
            for (let i = 0; i < params.length; i++) {
                const key = '$' + params[i].value;
                if (Object.prototype.hasOwnProperty.call(paramMap, key)) {
                    this.tolerateError(messages_1.Messages.DuplicateBinding, params[i].value);
                }
                paramMap[key] = true;
            }
            if (this.context.strict && param.type === syntax_1.Syntax.Identifier) {
                if (this.scanner.isRestrictedWord(param.name)) {
                    this.tolerateError(messages_1.Messages.StrictCatchVariable);
                }
            }
            this.expect(')');
            body = this.parseBlock();
            return this.finalize(node, new Node.CatchClause(param, body));
        }
        parseFinallyClause() {
            this.expectKeyword('finally');
            return this.parseBlock();
        }
        parseTryStatement() {
            const node = this.createNode();
            this.expectKeyword('try');
            const block = this.parseBlock();
            const handler = this.matchKeyword('catch') ? this.parseCatchClause() : null;
            const finalizer = this.matchKeyword('finally') ? this.parseFinallyClause() : null;
            if (!handler && !finalizer) {
                this.throwError(messages_1.Messages.NoCatchOrFinally);
            }
            return this.finalize(node, new Node.TryStatement(block, handler, finalizer));
        }
        // https://tc39.github.io/ecma262/#sec-debugger-statement
        parseDebuggerStatement() {
            const node = this.createNode();
            this.expectKeyword('debugger');
            this.consumeSemicolon();
            return this.finalize(node, new Node.DebuggerStatement());
        }
        // https://tc39.github.io/ecma262/#sec-ecmascript-language-statements-and-declarations
        parseStatement() {
            let statement;
            switch (this.lookahead.type) {
                case 1 /* BooleanLiteral */:
                case 5 /* NullLiteral */:
                case 6 /* NumericLiteral */:
                case 8 /* StringLiteral */:
                case 10 /* Template */:
                case 9 /* RegularExpression */:
                    statement = this.parseExpressionStatement();
                    break;
                case 7 /* Punctuator */:
                    const value = this.lookahead.value;
                    if (value === '{') {
                        statement = this.parseBlock();
                    }
                    else if (value === '(') {
                        statement = this.parseExpressionStatement();
                    }
                    else if (value === ';') {
                        statement = this.parseEmptyStatement();
                    }
                    else {
                        statement = this.parseExpressionStatement();
                    }
                    break;
                case 3 /* Identifier */:
                    statement = this.matchAsyncFunction() ? this.parseFunctionDeclaration() : this.parseLabelledStatement();
                    break;
                case 4 /* Keyword */:
                    switch (this.lookahead.value) {
                        case 'break':
                            statement = this.parseBreakStatement();
                            break;
                        case 'continue':
                            statement = this.parseContinueStatement();
                            break;
                        case 'debugger':
                            statement = this.parseDebuggerStatement();
                            break;
                        case 'do':
                            statement = this.parseDoWhileStatement();
                            break;
                        case 'for':
                            statement = this.parseForStatement();
                            break;
                        case 'function':
                            statement = this.parseFunctionDeclaration();
                            break;
                        case 'if':
                            statement = this.parseIfStatement();
                            break;
                        case 'return':
                            statement = this.parseReturnStatement();
                            break;
                        case 'switch':
                            statement = this.parseSwitchStatement();
                            break;
                        case 'throw':
                            statement = this.parseThrowStatement();
                            break;
                        case 'try':
                            statement = this.parseTryStatement();
                            break;
                        case 'var':
                            statement = this.parseVariableStatement();
                            break;
                        case 'while':
                            statement = this.parseWhileStatement();
                            break;
                        case 'with':
                            statement = this.parseWithStatement();
                            break;
                        default:
                            statement = this.parseExpressionStatement();
                            break;
                    }
                    break;
                default:
                    statement = this.throwUnexpectedToken(this.lookahead);
            }
            return statement;
        }
        // https://tc39.github.io/ecma262/#sec-function-definitions
        parseFunctionSourceElements() {
            const node = this.createNode();
            this.expect('{');
            const body = this.parseDirectivePrologues();
            const previousLabelSet = this.context.labelSet;
            const previousInIteration = this.context.inIteration;
            const previousInSwitch = this.context.inSwitch;
            const previousInFunctionBody = this.context.inFunctionBody;
            this.context.labelSet = {};
            this.context.inIteration = false;
            this.context.inSwitch = false;
            this.context.inFunctionBody = true;
            while (this.lookahead.type !== 2 /* EOF */) {
                if (this.match('}')) {
                    break;
                }
                body.push(this.parseStatementListItem());
            }
            this.expect('}');
            this.context.labelSet = previousLabelSet;
            this.context.inIteration = previousInIteration;
            this.context.inSwitch = previousInSwitch;
            this.context.inFunctionBody = previousInFunctionBody;
            return this.finalize(node, new Node.BlockStatement(body));
        }
        validateParam(options, param, name) {
            const key = '$' + name;
            if (this.context.strict) {
                if (this.scanner.isRestrictedWord(name)) {
                    options.stricted = param;
                    options.message = messages_1.Messages.StrictParamName;
                }
                if (Object.prototype.hasOwnProperty.call(options.paramSet, key)) {
                    options.stricted = param;
                    options.message = messages_1.Messages.StrictParamDupe;
                }
            }
            else if (!options.firstRestricted) {
                if (this.scanner.isRestrictedWord(name)) {
                    options.firstRestricted = param;
                    options.message = messages_1.Messages.StrictParamName;
                }
                else if (this.scanner.isStrictModeReservedWord(name)) {
                    options.firstRestricted = param;
                    options.message = messages_1.Messages.StrictReservedWord;
                }
                else if (Object.prototype.hasOwnProperty.call(options.paramSet, key)) {
                    options.stricted = param;
                    options.message = messages_1.Messages.StrictParamDupe;
                }
            }
            /* istanbul ignore next */
            if (typeof Object.defineProperty === 'function') {
                Object.defineProperty(options.paramSet, key, { value: true, enumerable: true, writable: true, configurable: true });
            }
            else {
                options.paramSet[key] = true;
            }
        }
        parseRestElement(params) {
            const node = this.createNode();
            this.expect('...');
            const arg = this.parsePattern(params);
            if (this.match('=')) {
                this.throwError(messages_1.Messages.DefaultRestParameter);
            }
            if (!this.match(')')) {
                this.throwError(messages_1.Messages.ParameterAfterRestParameter);
            }
            return this.finalize(node, new Node.RestElement(arg));
        }
        parseFormalParameter(options) {
            const params = [];
            const param = this.match('...') ? this.parseRestElement(params) : this.parsePatternWithDefault(params);
            for (let i = 0; i < params.length; i++) {
                this.validateParam(options, params[i], params[i].value);
            }
            options.simple = options.simple && (param instanceof Node.Identifier);
            options.params.push(param);
        }
        parseFormalParameters(firstRestricted) {
            let options;
            options = {
                simple: true,
                params: [],
                firstRestricted: firstRestricted
            };
            this.expect('(');
            if (!this.match(')')) {
                options.paramSet = {};
                while (this.lookahead.type !== 2 /* EOF */) {
                    this.parseFormalParameter(options);
                    if (this.match(')')) {
                        break;
                    }
                    this.expect(',');
                    if (this.match(')')) {
                        break;
                    }
                }
            }
            this.expect(')');
            return {
                simple: options.simple,
                params: options.params,
                stricted: options.stricted,
                firstRestricted: options.firstRestricted,
                message: options.message
            };
        }
        matchAsyncFunction() {
            let match = this.matchContextualKeyword('async');
            if (match) {
                const state = this.scanner.saveState();
                this.scanner.scanComments();
                const next = this.scanner.lex();
                this.scanner.restoreState(state);
                match = (state.lineNumber === next.lineNumber) && (next.type === 4 /* Keyword */) && (next.value === 'function');
            }
            return match;
        }
        parseFunctionDeclaration(identifierIsOptional) {
            const node = this.createNode();
            const isAsync = this.matchContextualKeyword('async');
            if (isAsync) {
                this.nextToken();
            }
            this.expectKeyword('function');
            const isGenerator = this.match('*');
            if (isGenerator) {
                this.nextToken();
            }
            let message;
            let id = null;
            let firstRestricted = null;
            if (!identifierIsOptional || !this.match('(')) {
                const token = this.lookahead;
                id = this.parseVariableIdentifier();
                if (this.context.strict) {
                    if (this.scanner.isRestrictedWord(token.value)) {
                        this.tolerateUnexpectedToken(token, messages_1.Messages.StrictFunctionName);
                    }
                }
                else {
                    if (this.scanner.isRestrictedWord(token.value)) {
                        firstRestricted = token;
                        message = messages_1.Messages.StrictFunctionName;
                    }
                    else if (this.scanner.isStrictModeReservedWord(token.value)) {
                        firstRestricted = token;
                        message = messages_1.Messages.StrictReservedWord;
                    }
                }
            }
            const previousAllowAwait = this.context.await;
            const previousAllowYield = this.context.allowYield;
            this.context.await = isAsync;
            this.context.allowYield = !isGenerator;
            const formalParameters = this.parseFormalParameters(firstRestricted);
            const params = formalParameters.params;
            const stricted = formalParameters.stricted;
            firstRestricted = formalParameters.firstRestricted;
            if (formalParameters.message) {
                message = formalParameters.message;
            }
            const previousStrict = this.context.strict;
            const previousAllowStrictDirective = this.context.allowStrictDirective;
            this.context.allowStrictDirective = formalParameters.simple;
            const body = this.parseFunctionSourceElements();
            if (this.context.strict && firstRestricted) {
                this.throwUnexpectedToken(firstRestricted, message);
            }
            if (this.context.strict && stricted) {
                this.tolerateUnexpectedToken(stricted, message);
            }
            this.context.strict = previousStrict;
            this.context.allowStrictDirective = previousAllowStrictDirective;
            this.context.await = previousAllowAwait;
            this.context.allowYield = previousAllowYield;
            return isAsync ? this.finalize(node, new Node.AsyncFunctionDeclaration(id, params, body, isGenerator)) :
                this.finalize(node, new Node.FunctionDeclaration(id, params, body, isGenerator));
        }
        parseFunctionExpression() {
            const node = this.createNode();
            const isAsync = this.matchContextualKeyword('async');
            if (isAsync) {
                this.nextToken();
            }
            this.expectKeyword('function');
            const isGenerator = this.match('*');
            if (isGenerator) {
                this.nextToken();
            }
            let message;
            let id = null;
            let firstRestricted;
            const previousAllowAwait = this.context.await;
            const previousAllowYield = this.context.allowYield;
            this.context.await = isAsync;
            this.context.allowYield = !isGenerator;
            if (!this.match('(')) {
                const token = this.lookahead;
                id = (!this.context.strict && !isGenerator && this.matchKeyword('yield')) ? this.parseIdentifierName() : this.parseVariableIdentifier();
                if (this.context.strict) {
                    if (this.scanner.isRestrictedWord(token.value)) {
                        this.tolerateUnexpectedToken(token, messages_1.Messages.StrictFunctionName);
                    }
                }
                else {
                    if (this.scanner.isRestrictedWord(token.value)) {
                        firstRestricted = token;
                        message = messages_1.Messages.StrictFunctionName;
                    }
                    else if (this.scanner.isStrictModeReservedWord(token.value)) {
                        firstRestricted = token;
                        message = messages_1.Messages.StrictReservedWord;
                    }
                }
            }
            const formalParameters = this.parseFormalParameters(firstRestricted);
            const params = formalParameters.params;
            const stricted = formalParameters.stricted;
            firstRestricted = formalParameters.firstRestricted;
            if (formalParameters.message) {
                message = formalParameters.message;
            }
            const previousStrict = this.context.strict;
            const previousAllowStrictDirective = this.context.allowStrictDirective;
            this.context.allowStrictDirective = formalParameters.simple;
            const body = this.parseFunctionSourceElements();
            if (this.context.strict && firstRestricted) {
                this.throwUnexpectedToken(firstRestricted, message);
            }
            if (this.context.strict && stricted) {
                this.tolerateUnexpectedToken(stricted, message);
            }
            this.context.strict = previousStrict;
            this.context.allowStrictDirective = previousAllowStrictDirective;
            this.context.await = previousAllowAwait;
            this.context.allowYield = previousAllowYield;
            return isAsync ? this.finalize(node, new Node.AsyncFunctionExpression(id, params, body, isGenerator)) :
                this.finalize(node, new Node.FunctionExpression(id, params, body, isGenerator));
        }
        // https://tc39.github.io/ecma262/#sec-directive-prologues-and-the-use-strict-directive
        parseDirective() {
            const token = this.lookahead;
            const node = this.createNode();
            const expr = this.parseExpression();
            const directive = (expr.type === syntax_1.Syntax.Literal) ? this.getTokenRaw(token).slice(1, -1) : null;
            this.consumeSemicolon();
            return this.finalize(node, directive ? new Node.Directive(expr, directive) : new Node.ExpressionStatement(expr));
        }
        parseDirectivePrologues() {
            let firstRestricted = null;
            const body = [];
            while (true) {
                const token = this.lookahead;
                if (token.type !== 8 /* StringLiteral */) {
                    break;
                }
                const statement = this.parseDirective();
                body.push(statement);
                const directive = statement.directive;
                if (typeof directive !== 'string') {
                    break;
                }
                if (directive === 'use strict') {
                    this.context.strict = true;
                    if (firstRestricted) {
                        this.tolerateUnexpectedToken(firstRestricted, messages_1.Messages.StrictOctalLiteral);
                    }
                    if (!this.context.allowStrictDirective) {
                        this.tolerateUnexpectedToken(token, messages_1.Messages.IllegalLanguageModeDirective);
                    }
                }
                else {
                    if (!firstRestricted && token.octal) {
                        firstRestricted = token;
                    }
                }
            }
            return body;
        }
        // https://tc39.github.io/ecma262/#sec-method-definitions
        qualifiedPropertyName(token) {
            switch (token.type) {
                case 3 /* Identifier */:
                case 8 /* StringLiteral */:
                case 1 /* BooleanLiteral */:
                case 5 /* NullLiteral */:
                case 6 /* NumericLiteral */:
                case 4 /* Keyword */:
                    return true;
                case 7 /* Punctuator */:
                    return token.value === '[';
                default:
                    break;
            }
            return false;
        }
        parseGetterMethod() {
            const node = this.createNode();
            const isGenerator = false;
            const previousAllowYield = this.context.allowYield;
            this.context.allowYield = !isGenerator;
            const formalParameters = this.parseFormalParameters();
            if (formalParameters.params.length > 0) {
                this.tolerateError(messages_1.Messages.BadGetterArity);
            }
            const method = this.parsePropertyMethod(formalParameters);
            this.context.allowYield = previousAllowYield;
            return this.finalize(node, new Node.FunctionExpression(null, formalParameters.params, method, isGenerator));
        }
        parseSetterMethod() {
            const node = this.createNode();
            const isGenerator = false;
            const previousAllowYield = this.context.allowYield;
            this.context.allowYield = !isGenerator;
            const formalParameters = this.parseFormalParameters();
            if (formalParameters.params.length !== 1) {
                this.tolerateError(messages_1.Messages.BadSetterArity);
            }
            else if (formalParameters.params[0] instanceof Node.RestElement) {
                this.tolerateError(messages_1.Messages.BadSetterRestParameter);
            }
            const method = this.parsePropertyMethod(formalParameters);
            this.context.allowYield = previousAllowYield;
            return this.finalize(node, new Node.FunctionExpression(null, formalParameters.params, method, isGenerator));
        }
        parseGeneratorMethod() {
            const node = this.createNode();
            const isGenerator = true;
            const previousAllowYield = this.context.allowYield;
            this.context.allowYield = true;
            const params = this.parseFormalParameters();
            this.context.allowYield = false;
            const method = this.parsePropertyMethod(params);
            this.context.allowYield = previousAllowYield;
            return this.finalize(node, new Node.FunctionExpression(null, params.params, method, isGenerator));
        }
        // https://tc39.github.io/ecma262/#sec-generator-function-definitions
        isStartOfExpression() {
            let start = true;
            const value = this.lookahead.value;
            switch (this.lookahead.type) {
                case 7 /* Punctuator */:
                    start = (value === '[') || (value === '(') || (value === '{') ||
                        (value === '+') || (value === '-') ||
                        (value === '!') || (value === '~') ||
                        (value === '++') || (value === '--') ||
                        (value === '/') || (value === '/='); // regular expression literal
                    break;
                case 4 /* Keyword */:
                    start = (value === 'class') || (value === 'delete') ||
                        (value === 'function') || (value === 'let') || (value === 'new') ||
                        (value === 'super') || (value === 'this') || (value === 'typeof') ||
                        (value === 'void') || (value === 'yield');
                    break;
                default:
                    break;
            }
            return start;
        }
        parseYieldExpression() {
            const node = this.createNode();
            this.expectKeyword('yield');
            let argument = null;
            let delegate = false;
            if (!this.hasLineTerminator) {
                const previousAllowYield = this.context.allowYield;
                this.context.allowYield = false;
                delegate = this.match('*');
                if (delegate) {
                    this.nextToken();
                    argument = this.parseAssignmentExpression();
                }
                else if (this.isStartOfExpression()) {
                    argument = this.parseAssignmentExpression();
                }
                this.context.allowYield = previousAllowYield;
            }
            return this.finalize(node, new Node.YieldExpression(argument, delegate));
        }
        // https://tc39.github.io/ecma262/#sec-class-definitions
        parseClassElement(hasConstructor) {
            let token = this.lookahead;
            const node = this.createNode();
            let kind = '';
            let key = null;
            let value = null;
            let computed = false;
            let method = false;
            let isStatic = false;
            let isAsync = false;
            let isAsyncGenerator = false;
            if (this.match('*')) {
                this.nextToken();
            }
            else {
                computed = this.match('[');
                key = this.parseObjectPropertyKey();
                const id = key;
                if (id.name === 'static' && (this.qualifiedPropertyName(this.lookahead) || this.match('*'))) {
                    token = this.lookahead;
                    isStatic = true;
                    computed = this.match('[');
                    if (this.match('*')) {
                        this.nextToken();
                    }
                    else {
                        key = this.parseObjectPropertyKey();
                    }
                }
                if ((token.type === 3 /* Identifier */) && !this.hasLineTerminator && (token.value === 'async')) {
                    const punctuator = this.lookahead.value;
                    if (punctuator !== ':' && punctuator !== '(') {
                        isAsync = true;
                        isAsyncGenerator = this.match('*');
                        if (isAsyncGenerator) {
                            this.nextToken();
                        }
                        token = this.lookahead;
                        key = this.parseObjectPropertyKey();
                        if (token.type === 3 /* Identifier */ && token.value === 'constructor') {
                            this.tolerateUnexpectedToken(token, messages_1.Messages.ConstructorIsAsync);
                        }
                    }
                }
            }
            const lookaheadPropertyKey = this.qualifiedPropertyName(this.lookahead);
            if (token.type === 3 /* Identifier */) {
                if (token.value === 'get' && lookaheadPropertyKey) {
                    kind = 'get';
                    computed = this.match('[');
                    key = this.parseObjectPropertyKey();
                    this.context.allowYield = false;
                    value = this.parseGetterMethod();
                }
                else if (token.value === 'set' && lookaheadPropertyKey) {
                    kind = 'set';
                    computed = this.match('[');
                    key = this.parseObjectPropertyKey();
                    value = this.parseSetterMethod();
                }
            }
            else if (token.type === 7 /* Punctuator */ && token.value === '*' && lookaheadPropertyKey) {
                kind = 'init';
                computed = this.match('[');
                key = this.parseObjectPropertyKey();
                value = this.parseGeneratorMethod();
                method = true;
            }
            if (!kind && key && this.match('(')) {
                kind = 'init';
                value = isAsync ? this.parsePropertyMethodAsyncFunction(isAsyncGenerator) : this.parsePropertyMethodFunction();
                method = true;
            }
            if (!kind) {
                this.throwUnexpectedToken(this.lookahead);
            }
            if (kind === 'init') {
                kind = 'method';
            }
            if (!computed) {
                if (isStatic && this.isPropertyKey(key, 'prototype')) {
                    this.throwUnexpectedToken(token, messages_1.Messages.StaticPrototype);
                }
                if (!isStatic && this.isPropertyKey(key, 'constructor')) {
                    if (kind !== 'method' || !method || (value && value.generator)) {
                        this.throwUnexpectedToken(token, messages_1.Messages.ConstructorSpecialMethod);
                    }
                    if (hasConstructor.value) {
                        this.throwUnexpectedToken(token, messages_1.Messages.DuplicateConstructor);
                    }
                    else {
                        hasConstructor.value = true;
                    }
                    kind = 'constructor';
                }
            }
            return this.finalize(node, new Node.MethodDefinition(key, computed, value, kind, isStatic));
        }
        parseClassElementList() {
            const body = [];
            const hasConstructor = { value: false };
            this.expect('{');
            while (!this.match('}')) {
                if (this.match(';')) {
                    this.nextToken();
                }
                else {
                    body.push(this.parseClassElement(hasConstructor));
                }
            }
            this.expect('}');
            return body;
        }
        parseClassBody() {
            const node = this.createNode();
            const elementList = this.parseClassElementList();
            return this.finalize(node, new Node.ClassBody(elementList));
        }
        parseClassDeclaration(identifierIsOptional) {
            const node = this.createNode();
            const previousStrict = this.context.strict;
            this.context.strict = true;
            this.expectKeyword('class');
            const id = (identifierIsOptional && (this.lookahead.type !== 3 /* Identifier */)) ? null : this.parseVariableIdentifier();
            let superClass = null;
            if (this.matchKeyword('extends')) {
                this.nextToken();
                superClass = this.isolateCoverGrammar(this.parseLeftHandSideExpressionAllowCall);
            }
            const classBody = this.parseClassBody();
            this.context.strict = previousStrict;
            return this.finalize(node, new Node.ClassDeclaration(id, superClass, classBody));
        }
        parseClassExpression() {
            const node = this.createNode();
            const previousStrict = this.context.strict;
            this.context.strict = true;
            this.expectKeyword('class');
            const id = (this.lookahead.type === 3 /* Identifier */) ? this.parseVariableIdentifier() : null;
            let superClass = null;
            if (this.matchKeyword('extends')) {
                this.nextToken();
                superClass = this.isolateCoverGrammar(this.parseLeftHandSideExpressionAllowCall);
            }
            const classBody = this.parseClassBody();
            this.context.strict = previousStrict;
            return this.finalize(node, new Node.ClassExpression(id, superClass, classBody));
        }
        // https://tc39.github.io/ecma262/#sec-scripts
        // https://tc39.github.io/ecma262/#sec-modules
        parseModule() {
            this.context.strict = true;
            this.context.isModule = true;
            this.scanner.isModule = true;
            const node = this.createNode();
            const body = this.parseDirectivePrologues();
            while (this.lookahead.type !== 2 /* EOF */) {
                body.push(this.parseStatementListItem());
            }
            return this.finalize(node, new Node.Module(body));
        }
        parseScript() {
            const node = this.createNode();
            const body = this.parseDirectivePrologues();
            while (this.lookahead.type !== 2 /* EOF */) {
                body.push(this.parseStatementListItem());
            }
            return this.finalize(node, new Node.Script(body));
        }
        // https://tc39.github.io/ecma262/#sec-imports
        parseModuleSpecifier() {
            const node = this.createNode();
            if (this.lookahead.type !== 8 /* StringLiteral */) {
                this.throwError(messages_1.Messages.InvalidModuleSpecifier);
            }
            const token = this.nextToken();
            const raw = this.getTokenRaw(token);
            return this.finalize(node, new Node.Literal(token.value, raw));
        }
        // import {<foo as bar>} ...;
        parseImportSpecifier() {
            const node = this.createNode();
            let imported;
            let local;
            if (this.lookahead.type === 3 /* Identifier */) {
                imported = this.parseVariableIdentifier();
                local = imported;
                if (this.matchContextualKeyword('as')) {
                    this.nextToken();
                    local = this.parseVariableIdentifier();
                }
            }
            else {
                imported = this.parseIdentifierName();
                local = imported;
                if (this.matchContextualKeyword('as')) {
                    this.nextToken();
                    local = this.parseVariableIdentifier();
                }
                else {
                    this.throwUnexpectedToken(this.nextToken());
                }
            }
            return this.finalize(node, new Node.ImportSpecifier(local, imported));
        }
        // {foo, bar as bas}
        parseNamedImports() {
            this.expect('{');
            const specifiers = [];
            while (!this.match('}')) {
                specifiers.push(this.parseImportSpecifier());
                if (!this.match('}')) {
                    this.expect(',');
                }
            }
            this.expect('}');
            return specifiers;
        }
        // import <foo> ...;
        parseImportDefaultSpecifier() {
            const node = this.createNode();
            const local = this.parseIdentifierName();
            return this.finalize(node, new Node.ImportDefaultSpecifier(local));
        }
        // import <* as foo> ...;
        parseImportNamespaceSpecifier() {
            const node = this.createNode();
            this.expect('*');
            if (!this.matchContextualKeyword('as')) {
                this.throwError(messages_1.Messages.NoAsAfterImportNamespace);
            }
            this.nextToken();
            const local = this.parseIdentifierName();
            return this.finalize(node, new Node.ImportNamespaceSpecifier(local));
        }
        parseImportDeclaration() {
            if (this.context.inFunctionBody) {
                this.throwError(messages_1.Messages.IllegalImportDeclaration);
            }
            const node = this.createNode();
            this.expectKeyword('import');
            let src;
            let specifiers = [];
            if (this.lookahead.type === 8 /* StringLiteral */) {
                // import 'foo';
                src = this.parseModuleSpecifier();
            }
            else {
                if (this.match('{')) {
                    // import {bar}
                    specifiers = specifiers.concat(this.parseNamedImports());
                }
                else if (this.match('*')) {
                    // import * as foo
                    specifiers.push(this.parseImportNamespaceSpecifier());
                }
                else if (this.isIdentifierName(this.lookahead) && !this.matchKeyword('default')) {
                    // import foo
                    specifiers.push(this.parseImportDefaultSpecifier());
                    if (this.match(',')) {
                        this.nextToken();
                        if (this.match('*')) {
                            // import foo, * as foo
                            specifiers.push(this.parseImportNamespaceSpecifier());
                        }
                        else if (this.match('{')) {
                            // import foo, {bar}
                            specifiers = specifiers.concat(this.parseNamedImports());
                        }
                        else {
                            this.throwUnexpectedToken(this.lookahead);
                        }
                    }
                }
                else {
                    this.throwUnexpectedToken(this.nextToken());
                }
                if (!this.matchContextualKeyword('from')) {
                    const message = this.lookahead.value ? messages_1.Messages.UnexpectedToken : messages_1.Messages.MissingFromClause;
                    this.throwError(message, this.lookahead.value);
                }
                this.nextToken();
                src = this.parseModuleSpecifier();
            }
            this.consumeSemicolon();
            return this.finalize(node, new Node.ImportDeclaration(specifiers, src));
        }
        // https://tc39.github.io/ecma262/#sec-exports
        parseExportSpecifier() {
            const node = this.createNode();
            const local = this.parseIdentifierName();
            let exported = local;
            if (this.matchContextualKeyword('as')) {
                this.nextToken();
                exported = this.parseIdentifierName();
            }
            return this.finalize(node, new Node.ExportSpecifier(local, exported));
        }
        parseExportDeclaration() {
            if (this.context.inFunctionBody) {
                this.throwError(messages_1.Messages.IllegalExportDeclaration);
            }
            const node = this.createNode();
            this.expectKeyword('export');
            let exportDeclaration;
            if (this.matchKeyword('default')) {
                // export default ...
                this.nextToken();
                if (this.matchKeyword('function')) {
                    // export default function foo () {}
                    // export default function () {}
                    const declaration = this.parseFunctionDeclaration(true);
                    exportDeclaration = this.finalize(node, new Node.ExportDefaultDeclaration(declaration));
                }
                else if (this.matchKeyword('class')) {
                    // export default class foo {}
                    const declaration = this.parseClassDeclaration(true);
                    exportDeclaration = this.finalize(node, new Node.ExportDefaultDeclaration(declaration));
                }
                else if (this.matchContextualKeyword('async')) {
                    // export default async function f () {}
                    // export default async function () {}
                    // export default async x => x
                    const declaration = this.matchAsyncFunction() ? this.parseFunctionDeclaration(true) : this.parseAssignmentExpression();
                    exportDeclaration = this.finalize(node, new Node.ExportDefaultDeclaration(declaration));
                }
                else {
                    if (this.matchContextualKeyword('from')) {
                        this.throwError(messages_1.Messages.UnexpectedToken, this.lookahead.value);
                    }
                    // export default {};
                    // export default [];
                    // export default (1 + 2);
                    const declaration = this.match('{') ? this.parseObjectInitializer() :
                        this.match('[') ? this.parseArrayInitializer() : this.parseAssignmentExpression();
                    this.consumeSemicolon();
                    exportDeclaration = this.finalize(node, new Node.ExportDefaultDeclaration(declaration));
                }
            }
            else if (this.match('*')) {
                // export * from 'foo';
                this.nextToken();
                if (!this.matchContextualKeyword('from')) {
                    const message = this.lookahead.value ? messages_1.Messages.UnexpectedToken : messages_1.Messages.MissingFromClause;
                    this.throwError(message, this.lookahead.value);
                }
                this.nextToken();
                const src = this.parseModuleSpecifier();
                this.consumeSemicolon();
                exportDeclaration = this.finalize(node, new Node.ExportAllDeclaration(src));
            }
            else if (this.lookahead.type === 4 /* Keyword */) {
                // export var f = 1;
                let declaration;
                switch (this.lookahead.value) {
                    case 'let':
                    case 'const':
                        declaration = this.parseLexicalDeclaration({ inFor: false });
                        break;
                    case 'var':
                    case 'class':
                    case 'function':
                        declaration = this.parseStatementListItem();
                        break;
                    default:
                        this.throwUnexpectedToken(this.lookahead);
                }
                exportDeclaration = this.finalize(node, new Node.ExportNamedDeclaration(declaration, [], null));
            }
            else if (this.matchAsyncFunction()) {
                const declaration = this.parseFunctionDeclaration();
                exportDeclaration = this.finalize(node, new Node.ExportNamedDeclaration(declaration, [], null));
            }
            else {
                const specifiers = [];
                let source = null;
                let isExportFromIdentifier = false;
                this.expect('{');
                while (!this.match('}')) {
                    isExportFromIdentifier = isExportFromIdentifier || this.matchKeyword('default');
                    specifiers.push(this.parseExportSpecifier());
                    if (!this.match('}')) {
                        this.expect(',');
                    }
                }
                this.expect('}');
                if (this.matchContextualKeyword('from')) {
                    // export {default} from 'foo';
                    // export {foo} from 'foo';
                    this.nextToken();
                    source = this.parseModuleSpecifier();
                    this.consumeSemicolon();
                }
                else if (isExportFromIdentifier) {
                    // export {default}; // missing fromClause
                    const message = this.lookahead.value ? messages_1.Messages.UnexpectedToken : messages_1.Messages.MissingFromClause;
                    this.throwError(message, this.lookahead.value);
                }
                else {
                    // export {foo};
                    this.consumeSemicolon();
                }
                exportDeclaration = this.finalize(node, new Node.ExportNamedDeclaration(null, specifiers, source));
            }
            return exportDeclaration;
        }
    }
    exports.Parser = Parser;


/***/ },
/* 9 */
/***/ function(module, exports) {

    "use strict";
    // Ensure the condition is true, otherwise throw an error.
    // This is only to have a better contract semantic, i.e. another safety net
    // to catch a logic error. The condition shall be fulfilled in normal case.
    // Do NOT use this to enforce a certain condition on any user input.
    Object.defineProperty(exports, "__esModule", { value: true });
    function assert(condition, message) {
        /* istanbul ignore if */
        if (!condition) {
            throw new Error('ASSERT: ' + message);
        }
    }
    exports.assert = assert;


/***/ },
/* 10 */
/***/ function(module, exports) {

    "use strict";
    /* tslint:disable:max-classes-per-file */
    Object.defineProperty(exports, "__esModule", { value: true });
    class ErrorHandler {
        constructor() {
            this.errors = [];
            this.tolerant = false;
        }
        recordError(error) {
            this.errors.push(error);
        }
        tolerate(error) {
            if (this.tolerant) {
                this.recordError(error);
            }
            else {
                throw error;
            }
        }
        constructError(msg, column) {
            let error = new Error(msg);
            try {
                throw error;
            }
            catch (base) {
                /* istanbul ignore else */
                if (Object.create && Object.defineProperty) {
                    error = Object.create(base);
                    Object.defineProperty(error, 'column', { value: column });
                }
            }
            /* istanbul ignore next */
            return error;
        }
        createError(index, line, col, description) {
            const msg = 'Line ' + line + ': ' + description;
            const error = this.constructError(msg, col);
            error.index = index;
            error.lineNumber = line;
            error.description = description;
            return error;
        }
        throwError(index, line, col, description) {
            throw this.createError(index, line, col, description);
        }
        tolerateError(index, line, col, description) {
            const error = this.createError(index, line, col, description);
            if (this.tolerant) {
                this.recordError(error);
            }
            else {
                throw error;
            }
        }
    }
    exports.ErrorHandler = ErrorHandler;


/***/ },
/* 11 */
/***/ function(module, exports) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    // Error messages should be identical to V8.
    exports.Messages = {
        BadImportCallArity: 'Unexpected token',
        BadGetterArity: 'Getter must not have any formal parameters',
        BadSetterArity: 'Setter must have exactly one formal parameter',
        BadSetterRestParameter: 'Setter function argument must not be a rest parameter',
        ConstructorIsAsync: 'Class constructor may not be an async method',
        ConstructorSpecialMethod: 'Class constructor may not be an accessor',
        DeclarationMissingInitializer: 'Missing initializer in %0 declaration',
        DefaultRestParameter: 'Unexpected token =',
        DefaultRestProperty: 'Unexpected token =',
        DuplicateBinding: 'Duplicate binding %0',
        DuplicateConstructor: 'A class may only have one constructor',
        DuplicateProtoProperty: 'Duplicate __proto__ fields are not allowed in object literals',
        ForAwaitInNonAsync: 'for-await-of can only be used in an async function or async generator',
        ForAwaitOfSemicolon: 'Unexpected \';\' in for-await-of header',
        ForAwaitOfSawIn: 'Unexpected \'in\' in for-await-of header',
        ForInOfLoopInitializer: '%0 loop variable declaration may not have an initializer',
        GeneratorInLegacyContext: 'Generator declarations are not allowed in legacy contexts',
        IllegalBreak: 'Illegal break statement',
        IllegalContinue: 'Illegal continue statement',
        IllegalExportDeclaration: 'Unexpected token',
        IllegalImportDeclaration: 'Unexpected token',
        IllegalLanguageModeDirective: 'Illegal \'use strict\' directive in function with non-simple parameter list',
        IllegalReturn: 'Illegal return statement',
        InvalidEscapedReservedWord: 'Keyword must not contain escaped characters',
        InvalidHexEscapeSequence: 'Invalid hexadecimal escape sequence',
        InvalidLHSInAssignment: 'Invalid left-hand side in assignment',
        InvalidLHSInForIn: 'Invalid left-hand side in for-in',
        InvalidLHSInForLoop: 'Invalid left-hand side in for-loop',
        InvalidModuleSpecifier: 'Unexpected token',
        InvalidNumericSeparatorAfterLeadingZero: 'Numeric serparator can not be used after leading 0',
        InvalidNumericSeparatorAfterNumericLiteral: 'Numeric separators are not allowed at the end of numeric literals',
        InvalidRegExp: 'Invalid regular expression',
        LetInLexicalBinding: 'let is disallowed as a lexically bound name',
        MissingFromClause: 'Unexpected token',
        MultipleDefaultsInSwitch: 'More than one default clause in switch statement',
        NewlineAfterThrow: 'Illegal newline after throw',
        NoAsAfterImportNamespace: 'Unexpected token',
        NoCatchOrFinally: 'Missing catch or finally after try',
        ParameterAfterRestParameter: 'Rest parameter must be last formal parameter',
        PropertyAfterRestProperty: 'Unexpected token',
        Redeclaration: '%0 \'%1\' has already been declared',
        StaticPrototype: 'Classes may not have static property named prototype',
        StrictCatchVariable: 'Catch variable may not be eval or arguments in strict mode',
        StrictDelete: 'Delete of an unqualified identifier in strict mode.',
        StrictFunction: 'In strict mode code, functions can only be declared at top level or inside a block',
        StrictFunctionName: 'Function name may not be eval or arguments in strict mode',
        StrictLHSAssignment: 'Assignment to eval or arguments is not allowed in strict mode',
        StrictLHSPostfix: 'Postfix increment/decrement may not have eval or arguments operand in strict mode',
        StrictLHSPrefix: 'Prefix increment/decrement may not have eval or arguments operand in strict mode',
        StrictModeWith: 'Strict mode code may not include a with statement',
        StrictOctalLiteral: 'Octal literals are not allowed in strict mode.',
        StrictParamDupe: 'Strict mode function may not have duplicate parameter names',
        StrictParamName: 'Parameter name eval or arguments is not allowed in strict mode',
        StrictReservedWord: 'Use of future reserved word in strict mode',
        StrictVarName: 'Variable name may not be eval or arguments in strict mode',
        TemplateOctalLiteral: 'Octal literals are not allowed in template strings.',
        UnexpectedEOS: 'Unexpected end of input',
        UnexpectedIdentifier: 'Unexpected identifier',
        UnexpectedNumber: 'Unexpected number',
        UnexpectedReserved: 'Unexpected reserved word',
        UnexpectedString: 'Unexpected string',
        UnexpectedTemplate: 'Unexpected quasi %0',
        UnexpectedToken: 'Unexpected token %0',
        UnexpectedTokenIllegal: 'Unexpected token ILLEGAL',
        UnknownLabel: 'Undefined label \'%0\'',
        UnterminatedRegExp: 'Invalid regular expression: missing /'
    };


/***/ },
/* 12 */
/***/ function(module, exports, __webpack_require__) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    const assert_1 = __webpack_require__(9);
    const character_1 = __webpack_require__(4);
    const messages_1 = __webpack_require__(11);
    function hexValue(ch) {
        return '0123456789abcdef'.indexOf(ch.toLowerCase());
    }
    function octalValue(ch) {
        return '01234567'.indexOf(ch);
    }
    class Scanner {
        constructor(code, handler) {
            this.source = code;
            this.errorHandler = handler;
            this.trackComment = false;
            this.isModule = false;
            this.length = code.length;
            this.index = 0;
            this.lineNumber = (code.length > 0) ? 1 : 0;
            this.lineStart = 0;
            this.curlyStack = [];
        }
        saveState() {
            return {
                index: this.index,
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                curlyStack: this.curlyStack.slice()
            };
        }
        restoreState(state) {
            this.index = state.index;
            this.lineNumber = state.lineNumber;
            this.lineStart = state.lineStart;
            this.curlyStack = state.curlyStack;
        }
        eof() {
            return this.index >= this.length;
        }
        throwUnexpectedToken(message = messages_1.Messages.UnexpectedTokenIllegal) {
            return this.errorHandler.throwError(this.index, this.lineNumber, this.index - this.lineStart + 1, message);
        }
        tolerateUnexpectedToken(message = messages_1.Messages.UnexpectedTokenIllegal) {
            this.errorHandler.tolerateError(this.index, this.lineNumber, this.index - this.lineStart + 1, message);
        }
        // https://tc39.github.io/ecma262/#sec-comments
        skipSingleLineComment(offset) {
            let comments = [];
            let start, loc;
            if (this.trackComment) {
                comments = [];
                start = this.index - offset;
                loc = {
                    start: {
                        line: this.lineNumber,
                        column: this.index - this.lineStart - offset
                    },
                    end: {}
                };
            }
            while (!this.eof()) {
                const ch = this.source.charCodeAt(this.index);
                ++this.index;
                if (character_1.Character.isLineTerminator(ch)) {
                    if (this.trackComment) {
                        loc.end = {
                            line: this.lineNumber,
                            column: this.index - this.lineStart - 1
                        };
                        const entry = {
                            multiLine: false,
                            slice: [start + offset, this.index - 1],
                            range: [start, this.index - 1],
                            loc: loc
                        };
                        comments.push(entry);
                    }
                    if (ch === 13 && this.source.charCodeAt(this.index) === 10) {
                        ++this.index;
                    }
                    ++this.lineNumber;
                    this.lineStart = this.index;
                    return comments;
                }
            }
            if (this.trackComment) {
                loc.end = {
                    line: this.lineNumber,
                    column: this.index - this.lineStart
                };
                const entry = {
                    multiLine: false,
                    slice: [start + offset, this.index],
                    range: [start, this.index],
                    loc: loc
                };
                comments.push(entry);
            }
            return comments;
        }
        skipMultiLineComment() {
            let comments = [];
            let start, loc;
            if (this.trackComment) {
                comments = [];
                start = this.index - 2;
                loc = {
                    start: {
                        line: this.lineNumber,
                        column: this.index - this.lineStart - 2
                    },
                    end: {}
                };
            }
            while (!this.eof()) {
                const ch = this.source.charCodeAt(this.index);
                if (character_1.Character.isLineTerminator(ch)) {
                    if (ch === 0x0D && this.source.charCodeAt(this.index + 1) === 0x0A) {
                        ++this.index;
                    }
                    ++this.lineNumber;
                    ++this.index;
                    this.lineStart = this.index;
                }
                else if (ch === 0x2A) {
                    // Block comment ends with '*/'.
                    if (this.source.charCodeAt(this.index + 1) === 0x2F) {
                        this.index += 2;
                        if (this.trackComment) {
                            loc.end = {
                                line: this.lineNumber,
                                column: this.index - this.lineStart
                            };
                            const entry = {
                                multiLine: true,
                                slice: [start + 2, this.index - 2],
                                range: [start, this.index],
                                loc: loc
                            };
                            comments.push(entry);
                        }
                        return comments;
                    }
                    ++this.index;
                }
                else {
                    ++this.index;
                }
            }
            // Ran off the end of the file - the whole thing is a comment
            if (this.trackComment) {
                loc.end = {
                    line: this.lineNumber,
                    column: this.index - this.lineStart
                };
                const entry = {
                    multiLine: true,
                    slice: [start + 2, this.index],
                    range: [start, this.index],
                    loc: loc
                };
                comments.push(entry);
            }
            this.tolerateUnexpectedToken();
            return comments;
        }
        scanComments() {
            let comments;
            if (this.trackComment) {
                comments = [];
            }
            let start = (this.index === 0);
            while (!this.eof()) {
                let ch = this.source.charCodeAt(this.index);
                if (character_1.Character.isWhiteSpace(ch)) {
                    ++this.index;
                }
                else if (character_1.Character.isLineTerminator(ch)) {
                    ++this.index;
                    if (ch === 0x0D && this.source.charCodeAt(this.index) === 0x0A) {
                        ++this.index;
                    }
                    ++this.lineNumber;
                    this.lineStart = this.index;
                    start = true;
                }
                else if (ch === 0x2F) { // U+002F is '/'
                    ch = this.source.charCodeAt(this.index + 1);
                    if (ch === 0x2F) {
                        this.index += 2;
                        const comment = this.skipSingleLineComment(2);
                        if (this.trackComment) {
                            comments = comments.concat(comment);
                        }
                        start = true;
                    }
                    else if (ch === 0x2A) { // U+002A is '*'
                        this.index += 2;
                        const comment = this.skipMultiLineComment();
                        if (this.trackComment) {
                            comments = comments.concat(comment);
                        }
                    }
                    else {
                        break;
                    }
                }
                else if (start && ch === 0x2D) { // U+002D is '-'
                    // U+003E is '>'
                    if ((this.source.charCodeAt(this.index + 1) === 0x2D) && (this.source.charCodeAt(this.index + 2) === 0x3E)) {
                        // '-->' is a single-line comment
                        this.index += 3;
                        const comment = this.skipSingleLineComment(3);
                        if (this.trackComment) {
                            comments = comments.concat(comment);
                        }
                    }
                    else {
                        break;
                    }
                }
                else if (ch === 0x3C && !this.isModule) { // U+003C is '<'
                    if (this.source.slice(this.index + 1, this.index + 4) === '!--') {
                        this.index += 4; // `<!--`
                        const comment = this.skipSingleLineComment(4);
                        if (this.trackComment) {
                            comments = comments.concat(comment);
                        }
                    }
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }
            return comments;
        }
        // https://tc39.github.io/ecma262/#sec-future-reserved-words
        isFutureReservedWord(id) {
            switch (id) {
                case 'enum':
                case 'export':
                case 'import':
                case 'super':
                    return true;
                default:
                    return false;
            }
        }
        isStrictModeReservedWord(id) {
            switch (id) {
                case 'implements':
                case 'interface':
                case 'package':
                case 'private':
                case 'protected':
                case 'public':
                case 'static':
                case 'yield':
                case 'let':
                    return true;
                default:
                    return false;
            }
        }
        isRestrictedWord(id) {
            return id === 'eval' || id === 'arguments';
        }
        // https://tc39.github.io/ecma262/#sec-keywords
        isKeyword(id) {
            switch (id.length) {
                case 2:
                    return (id === 'if') || (id === 'in') || (id === 'do');
                case 3:
                    return (id === 'var') || (id === 'for') || (id === 'new') ||
                        (id === 'try') || (id === 'let');
                case 4:
                    return (id === 'this') || (id === 'else') || (id === 'case') ||
                        (id === 'void') || (id === 'with') || (id === 'enum');
                case 5:
                    return (id === 'while') || (id === 'break') || (id === 'catch') ||
                        (id === 'throw') || (id === 'const') || (id === 'yield') ||
                        (id === 'class') || (id === 'super');
                case 6:
                    return (id === 'return') || (id === 'typeof') || (id === 'delete') ||
                        (id === 'switch') || (id === 'export') || (id === 'import');
                case 7:
                    return (id === 'default') || (id === 'finally') || (id === 'extends');
                case 8:
                    return (id === 'function') || (id === 'continue') || (id === 'debugger');
                case 10:
                    return (id === 'instanceof');
                default:
                    return false;
            }
        }
        codePointAt(i) {
            let cp = this.source.charCodeAt(i);
            if (cp >= 0xD800 && cp <= 0xDBFF) {
                const second = this.source.charCodeAt(i + 1);
                if (second >= 0xDC00 && second <= 0xDFFF) {
                    const first = cp;
                    cp = (first - 0xD800) * 0x400 + second - 0xDC00 + 0x10000;
                }
            }
            return cp;
        }
        scanHexEscape(prefix) {
            const len = (prefix === 'u') ? 4 : 2;
            let code = 0;
            for (let i = 0; i < len; ++i) {
                if (!this.eof() && character_1.Character.isHexDigit(this.source.charCodeAt(this.index))) {
                    code = code * 16 + hexValue(this.source[this.index++]);
                }
                else {
                    return null;
                }
            }
            return String.fromCharCode(code);
        }
        scanUnicodeCodePointEscape() {
            let ch = this.source[this.index];
            let code = 0;
            // At least, one hex digit is required.
            if (ch === '}') {
                this.throwUnexpectedToken();
            }
            while (!this.eof()) {
                ch = this.source[this.index++];
                if (!character_1.Character.isHexDigit(ch.charCodeAt(0))) {
                    break;
                }
                code = code * 16 + hexValue(ch);
            }
            if (code > 0x10FFFF || ch !== '}') {
                this.throwUnexpectedToken();
            }
            return character_1.Character.fromCodePoint(code);
        }
        getIdentifier() {
            const start = this.index++;
            while (!this.eof()) {
                const ch = this.source.charCodeAt(this.index);
                if (ch === 0x5C) {
                    // Blackslash (U+005C) marks Unicode escape sequence.
                    this.index = start;
                    return this.getComplexIdentifier();
                }
                else if (ch >= 0xD800 && ch < 0xDFFF) {
                    // Need to handle surrogate pairs.
                    this.index = start;
                    return this.getComplexIdentifier();
                }
                if (character_1.Character.isIdentifierPart(ch)) {
                    ++this.index;
                }
                else {
                    break;
                }
            }
            return this.source.slice(start, this.index);
        }
        getComplexIdentifier() {
            let cp = this.codePointAt(this.index);
            let id = character_1.Character.fromCodePoint(cp);
            this.index += id.length;
            // '\u' (U+005C, U+0075) denotes an escaped character.
            let ch;
            if (cp === 0x5C) {
                if (this.source.charCodeAt(this.index) !== 0x75) {
                    this.throwUnexpectedToken();
                }
                ++this.index;
                if (this.source[this.index] === '{') {
                    ++this.index;
                    ch = this.scanUnicodeCodePointEscape();
                }
                else {
                    ch = this.scanHexEscape('u');
                    if (ch === null || ch === '\\' || !character_1.Character.isIdentifierStart(ch.charCodeAt(0))) {
                        this.throwUnexpectedToken();
                    }
                }
                id = ch;
            }
            while (!this.eof()) {
                cp = this.codePointAt(this.index);
                if (!character_1.Character.isIdentifierPart(cp)) {
                    break;
                }
                ch = character_1.Character.fromCodePoint(cp);
                id += ch;
                this.index += ch.length;
                // '\u' (U+005C, U+0075) denotes an escaped character.
                if (cp === 0x5C) {
                    id = id.substr(0, id.length - 1);
                    if (this.source.charCodeAt(this.index) !== 0x75) {
                        this.throwUnexpectedToken();
                    }
                    ++this.index;
                    if (this.source[this.index] === '{') {
                        ++this.index;
                        ch = this.scanUnicodeCodePointEscape();
                    }
                    else {
                        ch = this.scanHexEscape('u');
                        if (ch === null || ch === '\\' || !character_1.Character.isIdentifierPart(ch.charCodeAt(0))) {
                            this.throwUnexpectedToken();
                        }
                    }
                    id += ch;
                }
            }
            return id;
        }
        octalToDecimal(ch) {
            // \0 is not octal escape sequence
            let octal = (ch !== '0');
            let code = octalValue(ch);
            if (!this.eof() && character_1.Character.isOctalDigit(this.source.charCodeAt(this.index))) {
                octal = true;
                code = code * 8 + octalValue(this.source[this.index++]);
                // 3 digits are only allowed when string starts
                // with 0, 1, 2, 3
                if ('0123'.indexOf(ch) >= 0 && !this.eof() && character_1.Character.isOctalDigit(this.source.charCodeAt(this.index))) {
                    code = code * 8 + octalValue(this.source[this.index++]);
                }
            }
            return {
                code: code,
                octal: octal
            };
        }
        // https://tc39.github.io/ecma262/#sec-names-and-keywords
        scanIdentifier() {
            let type;
            const start = this.index;
            // Backslash (U+005C) starts an escaped character.
            const id = (this.source.charCodeAt(start) === 0x5C) ? this.getComplexIdentifier() : this.getIdentifier();
            // There is no keyword or literal with only one character.
            // Thus, it must be an identifier.
            if (id.length === 1) {
                type = 3 /* Identifier */;
            }
            else if (this.isKeyword(id)) {
                type = 4 /* Keyword */;
            }
            else if (id === 'null') {
                type = 5 /* NullLiteral */;
            }
            else if (id === 'true' || id === 'false') {
                type = 1 /* BooleanLiteral */;
            }
            else {
                type = 3 /* Identifier */;
            }
            if (type !== 3 /* Identifier */ && (start + id.length !== this.index)) {
                const restore = this.index;
                this.index = start;
                this.tolerateUnexpectedToken(messages_1.Messages.InvalidEscapedReservedWord);
                this.index = restore;
            }
            return {
                type: type,
                value: id,
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                start: start,
                end: this.index
            };
        }
        // https://tc39.github.io/ecma262/#sec-punctuators
        scanPunctuator() {
            const start = this.index;
            // Check for most common single-character punctuators.
            let str = this.source[this.index];
            switch (str) {
                case '(':
                case '{':
                    if (str === '{') {
                        this.curlyStack.push('{');
                    }
                    ++this.index;
                    break;
                case '.':
                    ++this.index;
                    if (this.source[this.index] === '.' && this.source[this.index + 1] === '.') {
                        // Spread operator: ...
                        this.index += 2;
                        str = '...';
                    }
                    break;
                case '}':
                    ++this.index;
                    this.curlyStack.pop();
                    break;
                case ')':
                case ';':
                case ',':
                case '[':
                case ']':
                case ':':
                case '?':
                case '~':
                    ++this.index;
                    break;
                default:
                    // 4-character punctuator.
                    str = this.source.substr(this.index, 4);
                    if (str === '>>>=') {
                        this.index += 4;
                    }
                    else {
                        // 3-character punctuators.
                        str = str.substr(0, 3);
                        if (str === '===' || str === '!==' || str === '>>>' ||
                            str === '<<=' || str === '>>=' || str === '**=') {
                            this.index += 3;
                        }
                        else {
                            // 2-character punctuators.
                            str = str.substr(0, 2);
                            if (str === '&&' || str === '||' || str === '==' || str === '!=' ||
                                str === '+=' || str === '-=' || str === '*=' || str === '/=' ||
                                str === '++' || str === '--' || str === '<<' || str === '>>' ||
                                str === '&=' || str === '|=' || str === '^=' || str === '%=' ||
                                str === '<=' || str === '>=' || str === '=>' || str === '**') {
                                this.index += 2;
                            }
                            else {
                                // 1-character punctuators.
                                str = this.source[this.index];
                                if ('<>=!+-*%&|^/'.indexOf(str) >= 0) {
                                    ++this.index;
                                }
                            }
                        }
                    }
            }
            if (this.index === start) {
                this.throwUnexpectedToken();
            }
            return {
                type: 7 /* Punctuator */,
                value: str,
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                start: start,
                end: this.index
            };
        }
        // https://tc39.github.io/ecma262/#sec-literals-numeric-literals
        scanHexLiteral(start) {
            let num = '';
            let nextInvalidNumericSeparatorPosition = this.index;
            while (character_1.Character.isHexDigit(this.source.charCodeAt(this.index)) || this.source[this.index] === '_') {
                if (this.source[this.index] === '_') {
                    if (this.index === nextInvalidNumericSeparatorPosition) {
                        this.throwUnexpectedToken();
                    }
                    ++this.index;
                    nextInvalidNumericSeparatorPosition = this.index;
                    continue;
                }
                num += this.source[this.index++];
            }
            if (num.length === 0) {
                this.throwUnexpectedToken();
            }
            if (this.index === nextInvalidNumericSeparatorPosition) {
                this.throwUnexpectedToken(messages_1.Messages.InvalidNumericSeparatorAfterNumericLiteral);
            }
            let bigint = false;
            if (this.source[this.index] === 'n') {
                ++this.index;
                bigint = true;
            }
            if (character_1.Character.isIdentifierStart(this.source.charCodeAt(this.index))) {
                this.throwUnexpectedToken();
            }
            return {
                type: 6 /* NumericLiteral */,
                value: parseInt('0x' + num, 16),
                bigint: bigint,
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                start: start,
                end: this.index
            };
        }
        scanBinaryLiteral(start) {
            let num = '';
            let ch;
            let nextInvalidNumericSeparatorPosition = this.index;
            while (this.source[this.index] === '0' || this.source[this.index] === '1' || this.source[this.index] === '_') {
                if (this.source[this.index] === '_') {
                    if (this.index === nextInvalidNumericSeparatorPosition) {
                        this.throwUnexpectedToken();
                    }
                    ++this.index;
                    nextInvalidNumericSeparatorPosition = this.index;
                    continue;
                }
                num += this.source[this.index++];
            }
            if (num.length === 0) {
                // only 0b or 0B
                this.throwUnexpectedToken();
            }
            if (this.index === nextInvalidNumericSeparatorPosition) {
                this.throwUnexpectedToken(messages_1.Messages.InvalidNumericSeparatorAfterNumericLiteral);
            }
            let bigint = false;
            if (this.source[this.index] === 'n') {
                ++this.index;
                bigint = true;
            }
            if (!this.eof()) {
                ch = this.source.charCodeAt(this.index);
                /* istanbul ignore else */
                if (character_1.Character.isIdentifierStart(ch) || character_1.Character.isDecimalDigit(ch)) {
                    this.throwUnexpectedToken();
                }
            }
            return {
                type: 6 /* NumericLiteral */,
                value: parseInt(num, 2),
                bigint: bigint,
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                start: start,
                end: this.index
            };
        }
        scanOctalLiteral(prefix, start, legacy) {
            let num = '';
            let octal = false;
            if (character_1.Character.isOctalDigit(prefix.charCodeAt(0))) {
                octal = true;
                num = '0' + this.source[this.index++];
            }
            else {
                ++this.index;
            }
            let nextInvalidNumericSeparatorPosition = legacy ? -1 : this.index;
            while (character_1.Character.isOctalDigit(this.source.charCodeAt(this.index)) || this.source[this.index] === '_') {
                if (this.source[this.index] === '_') {
                    if (legacy || this.index === nextInvalidNumericSeparatorPosition) {
                        this.throwUnexpectedToken();
                    }
                    ++this.index;
                    nextInvalidNumericSeparatorPosition = this.index;
                    continue;
                }
                num += this.source[this.index++];
            }
            if (!octal && num.length === 0) {
                // only 0o or 0O
                this.throwUnexpectedToken();
            }
            if (this.index === nextInvalidNumericSeparatorPosition) {
                this.throwUnexpectedToken(messages_1.Messages.InvalidNumericSeparatorAfterNumericLiteral);
            }
            let bigint = false;
            if (!legacy && this.source[this.index] === 'n') {
                ++this.index;
                bigint = true;
            }
            if (character_1.Character.isIdentifierStart(this.source.charCodeAt(this.index)) || character_1.Character.isDecimalDigit(this.source.charCodeAt(this.index))) {
                this.throwUnexpectedToken();
            }
            return {
                type: 6 /* NumericLiteral */,
                value: parseInt(num, 8),
                octal: octal,
                bigint: bigint,
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                start: start,
                end: this.index
            };
        }
        isImplicitOctalLiteral() {
            // Implicit octal, unless there is a non-octal digit.
            // (Annex B.1.1 on Numeric Literals)
            for (let i = this.index + 1; i < this.length; ++i) {
                const ch = this.source[i];
                if (ch === '8' || ch === '9') {
                    return false;
                }
                if (!character_1.Character.isOctalDigit(ch.charCodeAt(0))) {
                    return true;
                }
            }
            return true;
        }
        scanNumericLiteral() {
            const start = this.index;
            let ch = this.source[start];
            assert_1.assert(character_1.Character.isDecimalDigit(ch.charCodeAt(0)) || (ch === '.'), 'Numeric literal must start with a decimal digit or a decimal point');
            let nextInvalidNumericSeparatorPosition = -1;
            let num = '';
            if (ch !== '.') {
                num = this.source[this.index++];
                ch = this.source[this.index];
                // Hex number starts with '0x'.
                // Octal number starts with '0'.
                // Octal number in ES6 starts with '0o'.
                // Binary number in ES6 starts with '0b'.
                let wasZeroPrefixed = false;
                if (num === '0') {
                    if (ch === 'x' || ch === 'X') {
                        ++this.index;
                        return this.scanHexLiteral(start);
                    }
                    if (ch === 'b' || ch === 'B') {
                        ++this.index;
                        return this.scanBinaryLiteral(start);
                    }
                    if (ch === 'o' || ch === 'O') {
                        const legacy = false;
                        return this.scanOctalLiteral(ch, start, legacy);
                    }
                    if (ch && character_1.Character.isOctalDigit(ch.charCodeAt(0))) {
                        if (this.isImplicitOctalLiteral()) {
                            const legacy = true;
                            return this.scanOctalLiteral(ch, start, legacy);
                        }
                    }
                    if (ch === '_') {
                        this.throwUnexpectedToken(messages_1.Messages.InvalidNumericSeparatorAfterLeadingZero);
                    }
                    wasZeroPrefixed = true;
                }
                while (character_1.Character.isDecimalDigit(this.source.charCodeAt(this.index)) || this.source[this.index] === '_') {
                    if (this.source[this.index] === '_') {
                        if (wasZeroPrefixed || this.index === nextInvalidNumericSeparatorPosition) {
                            this.throwUnexpectedToken();
                        }
                        ++this.index;
                        nextInvalidNumericSeparatorPosition = this.index;
                        continue;
                    }
                    num += this.source[this.index++];
                }
                if (this.index === nextInvalidNumericSeparatorPosition) {
                    this.throwUnexpectedToken(messages_1.Messages.InvalidNumericSeparatorAfterLeadingZero);
                }
                ch = this.source[this.index];
                if (ch === 'n' && (!wasZeroPrefixed || num === '0')) {
                    ++this.index;
                    if (character_1.Character.isIdentifierStart(this.source.charCodeAt(this.index))) {
                        this.throwUnexpectedToken();
                    }
                    const value = globalThis.BigInt ? globalThis.BigInt(this.source.slice(start, this.index - 1).replace(/_/g, '')) : /* istanbul ignore next */ null;
                    return {
                        type: 6 /* NumericLiteral */,
                        value: value,
                        bigint: true,
                        lineNumber: this.lineNumber,
                        lineStart: this.lineStart,
                        start: start,
                        end: this.index
                    };
                }
            }
            if (ch === '.') {
                num += this.source[this.index++];
                if (this.source[this.index] === '_') {
                    this.throwUnexpectedToken();
                }
                nextInvalidNumericSeparatorPosition = -1;
                while (character_1.Character.isDecimalDigit(this.source.charCodeAt(this.index)) || this.source[this.index] === '_') {
                    if (this.source[this.index] === '_') {
                        if (this.index === nextInvalidNumericSeparatorPosition) {
                            this.throwUnexpectedToken();
                        }
                        ++this.index;
                        nextInvalidNumericSeparatorPosition = this.index;
                        continue;
                    }
                    num += this.source[this.index++];
                }
                ch = this.source[this.index];
                if (this.index === nextInvalidNumericSeparatorPosition) {
                    this.throwUnexpectedToken(messages_1.Messages.InvalidNumericSeparatorAfterNumericLiteral);
                }
            }
            if (ch === 'e' || ch === 'E') {
                num += this.source[this.index++];
                ch = this.source[this.index];
                if (ch === '_') {
                    this.throwUnexpectedToken();
                }
                if (ch === '+' || ch === '-') {
                    num += this.source[this.index++];
                    if (this.source[this.index] === '_') {
                        this.throwUnexpectedToken();
                    }
                }
                nextInvalidNumericSeparatorPosition = -1;
                if (character_1.Character.isDecimalDigit(this.source.charCodeAt(this.index))) {
                    while (character_1.Character.isDecimalDigit(this.source.charCodeAt(this.index)) || this.source[this.index] === '_') {
                        if (this.source[this.index] === '_') {
                            if (this.index === nextInvalidNumericSeparatorPosition) {
                                this.throwUnexpectedToken();
                            }
                            ++this.index;
                            nextInvalidNumericSeparatorPosition = this.index;
                            continue;
                        }
                        num += this.source[this.index++];
                    }
                }
                else {
                    this.throwUnexpectedToken();
                }
            }
            if (this.index === nextInvalidNumericSeparatorPosition) {
                this.throwUnexpectedToken(messages_1.Messages.InvalidNumericSeparatorAfterNumericLiteral);
            }
            if (character_1.Character.isIdentifierStart(this.source.charCodeAt(this.index))) {
                this.throwUnexpectedToken();
            }
            return {
                type: 6 /* NumericLiteral */,
                value: parseFloat(num),
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                start: start,
                end: this.index
            };
        }
        // https://tc39.github.io/ecma262/#sec-literals-string-literals
        scanStringLiteral() {
            const start = this.index;
            let quote = this.source[start];
            assert_1.assert((quote === '\'' || quote === '"'), 'String literal must starts with a quote');
            ++this.index;
            let octal = false;
            let str = '';
            while (!this.eof()) {
                let ch = this.source[this.index++];
                if (ch === quote) {
                    quote = '';
                    break;
                }
                else if (ch === '\\') {
                    ch = this.source[this.index++];
                    if (!ch || !character_1.Character.isLineTerminator(ch.charCodeAt(0))) {
                        switch (ch) {
                            case 'u':
                                if (this.source[this.index] === '{') {
                                    ++this.index;
                                    str += this.scanUnicodeCodePointEscape();
                                }
                                else {
                                    const unescapedChar = this.scanHexEscape(ch);
                                    if (unescapedChar === null) {
                                        this.throwUnexpectedToken();
                                    }
                                    str += unescapedChar;
                                }
                                break;
                            case 'x':
                                const unescaped = this.scanHexEscape(ch);
                                if (unescaped === null) {
                                    this.throwUnexpectedToken(messages_1.Messages.InvalidHexEscapeSequence);
                                }
                                str += unescaped;
                                break;
                            case 'n':
                                str += '\n';
                                break;
                            case 'r':
                                str += '\r';
                                break;
                            case 't':
                                str += '\t';
                                break;
                            case 'b':
                                str += '\b';
                                break;
                            case 'f':
                                str += '\f';
                                break;
                            case 'v':
                                str += '\x0B';
                                break;
                            case '8':
                            case '9':
                                str += ch;
                                this.tolerateUnexpectedToken();
                                break;
                            default:
                                if (ch && character_1.Character.isOctalDigit(ch.charCodeAt(0))) {
                                    const octToDec = this.octalToDecimal(ch);
                                    octal = octToDec.octal || octal;
                                    str += String.fromCharCode(octToDec.code);
                                }
                                else {
                                    str += ch;
                                }
                                break;
                        }
                    }
                    else {
                        ++this.lineNumber;
                        if (ch === '\r' && this.source[this.index] === '\n') {
                            ++this.index;
                        }
                        this.lineStart = this.index;
                    }
                }
                else if (character_1.Character.isLineTerminator(ch.charCodeAt(0))) {
                    break;
                }
                else {
                    str += ch;
                }
            }
            if (quote !== '') {
                this.index = start;
                this.throwUnexpectedToken();
            }
            return {
                type: 8 /* StringLiteral */,
                value: str,
                octal: octal,
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                start: start,
                end: this.index
            };
        }
        // https://tc39.github.io/ecma262/#sec-template-literal-lexical-components
        scanTemplate() {
            let cooked = '';
            let terminated = false;
            const start = this.index;
            const head = (this.source[start] === '`');
            let tail = false;
            let rawOffset = 2;
            ++this.index;
            while (!this.eof()) {
                let ch = this.source[this.index++];
                if (ch === '`') {
                    rawOffset = 1;
                    tail = true;
                    terminated = true;
                    break;
                }
                else if (ch === '$') {
                    if (this.source[this.index] === '{') {
                        this.curlyStack.push('${');
                        ++this.index;
                        terminated = true;
                        break;
                    }
                    cooked += ch;
                }
                else if (ch === '\\') {
                    ch = this.source[this.index++];
                    if (!character_1.Character.isLineTerminator(ch.charCodeAt(0))) {
                        switch (ch) {
                            case 'n':
                                cooked += '\n';
                                break;
                            case 'r':
                                cooked += '\r';
                                break;
                            case 't':
                                cooked += '\t';
                                break;
                            case 'u':
                                if (this.source[this.index] === '{') {
                                    ++this.index;
                                    cooked += this.scanUnicodeCodePointEscape();
                                }
                                else {
                                    const restore = this.index;
                                    const unescapedChar = this.scanHexEscape(ch);
                                    if (unescapedChar !== null) {
                                        cooked += unescapedChar;
                                    }
                                    else {
                                        this.index = restore;
                                        cooked += ch;
                                    }
                                }
                                break;
                            case 'x':
                                const unescaped = this.scanHexEscape(ch);
                                if (unescaped === null) {
                                    this.throwUnexpectedToken(messages_1.Messages.InvalidHexEscapeSequence);
                                }
                                cooked += unescaped;
                                break;
                            case 'b':
                                cooked += '\b';
                                break;
                            case 'f':
                                cooked += '\f';
                                break;
                            case 'v':
                                cooked += '\v';
                                break;
                            default:
                                if (ch === '0') {
                                    if (character_1.Character.isDecimalDigit(this.source.charCodeAt(this.index))) {
                                        // Illegal: \01 \02 and so on
                                        this.throwUnexpectedToken(messages_1.Messages.TemplateOctalLiteral);
                                    }
                                    cooked += '\0';
                                }
                                else if (character_1.Character.isOctalDigit(ch.charCodeAt(0))) {
                                    // Illegal: \1 \2
                                    this.throwUnexpectedToken(messages_1.Messages.TemplateOctalLiteral);
                                }
                                else {
                                    cooked += ch;
                                }
                                break;
                        }
                    }
                    else {
                        ++this.lineNumber;
                        if (ch === '\r' && this.source[this.index] === '\n') {
                            ++this.index;
                        }
                        this.lineStart = this.index;
                    }
                }
                else if (character_1.Character.isLineTerminator(ch.charCodeAt(0))) {
                    ++this.lineNumber;
                    if (ch === '\r' && this.source[this.index] === '\n') {
                        ++this.index;
                    }
                    this.lineStart = this.index;
                    cooked += '\n';
                }
                else {
                    cooked += ch;
                }
            }
            if (!terminated) {
                this.throwUnexpectedToken();
            }
            if (!head) {
                this.curlyStack.pop();
            }
            return {
                type: 10 /* Template */,
                value: this.source.slice(start + 1, this.index - rawOffset),
                cooked: cooked,
                head: head,
                tail: tail,
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                start: start,
                end: this.index
            };
        }
        // https://tc39.github.io/ecma262/#sec-literals-regular-expression-literals
        testRegExp(pattern, flags) {
            // The BMP character to use as a replacement for astral symbols when
            // translating an ES6 "u"-flagged pattern to an ES5-compatible
            // approximation.
            // Note: replacing with '\uFFFF' enables false positives in unlikely
            // scenarios. For example, `[\u{1044f}-\u{10440}]` is an invalid
            // pattern that would not be detected by this substitution.
            const astralSubstitute = '\uFFFF';
            let tmp = pattern;
            const self = this;
            if (flags.indexOf('u') >= 0) {
                tmp = tmp
                    // Replace every Unicode escape sequence with the equivalent
                    // BMP character or a constant ASCII code point in the case of
                    // astral symbols. (See the above note on `astralSubstitute`
                    // for more information.)
                    .replace(/\\u\{([0-9a-fA-F]+)\}|\\u([a-fA-F0-9]{4})/g, ($0, $1, $2) => {
                    const codePoint = parseInt($1 || $2, 16);
                    if (codePoint > 0x10FFFF) {
                        self.throwUnexpectedToken(messages_1.Messages.InvalidRegExp);
                    }
                    if (codePoint <= 0xFFFF) {
                        return String.fromCharCode(codePoint);
                    }
                    return astralSubstitute;
                })
                    // Replace each paired surrogate with a single ASCII symbol to
                    // avoid throwing on regular expressions that are only valid in
                    // combination with the "u" flag.
                    .replace(/[\uD800-\uDBFF][\uDC00-\uDFFF]/g, astralSubstitute);
            }
            // First, detect invalid regular expressions.
            try {
                RegExp(tmp);
            }
            catch (e) {
                this.throwUnexpectedToken(messages_1.Messages.InvalidRegExp);
            }
            // Return a regular expression object for this pattern-flag pair, or
            // `null` in case the current environment doesn't support the flags it
            // uses.
            try {
                return new RegExp(pattern, flags);
            }
            catch (exception) {
                /* istanbul ignore next */
                return null;
            }
        }
        scanRegExpBody() {
            let ch = this.source[this.index];
            assert_1.assert(ch === '/', 'Regular expression literal must start with a slash');
            let str = this.source[this.index++];
            let classMarker = false;
            let terminated = false;
            while (!this.eof()) {
                ch = this.source[this.index++];
                str += ch;
                if (ch === '\\') {
                    ch = this.source[this.index++];
                    // https://tc39.github.io/ecma262/#sec-literals-regular-expression-literals
                    if (character_1.Character.isLineTerminator(ch.charCodeAt(0))) {
                        this.throwUnexpectedToken(messages_1.Messages.UnterminatedRegExp);
                    }
                    str += ch;
                }
                else if (character_1.Character.isLineTerminator(ch.charCodeAt(0))) {
                    this.throwUnexpectedToken(messages_1.Messages.UnterminatedRegExp);
                }
                else if (classMarker) {
                    if (ch === ']') {
                        classMarker = false;
                    }
                }
                else {
                    if (ch === '/') {
                        terminated = true;
                        break;
                    }
                    else if (ch === '[') {
                        classMarker = true;
                    }
                }
            }
            if (!terminated) {
                this.throwUnexpectedToken(messages_1.Messages.UnterminatedRegExp);
            }
            // Exclude leading and trailing slash.
            return str.substr(1, str.length - 2);
        }
        scanRegExpFlags() {
            let str = '';
            let flags = '';
            while (!this.eof()) {
                let ch = this.source[this.index];
                if (!character_1.Character.isIdentifierPart(ch.charCodeAt(0))) {
                    break;
                }
                ++this.index;
                if (ch === '\\' && !this.eof()) {
                    ch = this.source[this.index];
                    if (ch === 'u') {
                        ++this.index;
                        let restore = this.index;
                        const char = this.scanHexEscape('u');
                        if (char !== null) {
                            flags += char;
                            for (str += '\\u'; restore < this.index; ++restore) {
                                str += this.source[restore];
                            }
                        }
                        else {
                            this.index = restore;
                            flags += 'u';
                            str += '\\u';
                        }
                        this.tolerateUnexpectedToken();
                    }
                    else {
                        str += '\\';
                        this.tolerateUnexpectedToken();
                    }
                }
                else {
                    flags += ch;
                    str += ch;
                }
            }
            return flags;
        }
        scanRegExp() {
            const start = this.index;
            const pattern = this.scanRegExpBody();
            const flags = this.scanRegExpFlags();
            const value = this.testRegExp(pattern, flags);
            return {
                type: 9 /* RegularExpression */,
                value: '',
                pattern: pattern,
                flags: flags,
                regex: value,
                lineNumber: this.lineNumber,
                lineStart: this.lineStart,
                start: start,
                end: this.index
            };
        }
        lex() {
            if (this.eof()) {
                return {
                    type: 2 /* EOF */,
                    value: '',
                    lineNumber: this.lineNumber,
                    lineStart: this.lineStart,
                    start: this.index,
                    end: this.index
                };
            }
            const cp = this.source.charCodeAt(this.index);
            if (character_1.Character.isIdentifierStart(cp)) {
                return this.scanIdentifier();
            }
            // Very common: ( and ) and ;
            if (cp === 0x28 || cp === 0x29 || cp === 0x3B) {
                return this.scanPunctuator();
            }
            // String literal starts with single quote (U+0027) or double quote (U+0022).
            if (cp === 0x27 || cp === 0x22) {
                return this.scanStringLiteral();
            }
            // Dot (.) U+002E can also start a floating-point number, hence the need
            // to check the next character.
            if (cp === 0x2E) {
                if (character_1.Character.isDecimalDigit(this.source.charCodeAt(this.index + 1))) {
                    return this.scanNumericLiteral();
                }
                return this.scanPunctuator();
            }
            if (character_1.Character.isDecimalDigit(cp)) {
                return this.scanNumericLiteral();
            }
            // Template literals start with ` (U+0060) for template head
            // or } (U+007D) for template middle or template tail.
            if (cp === 0x60 || (cp === 0x7D && this.curlyStack[this.curlyStack.length - 1] === '${')) {
                return this.scanTemplate();
            }
            // Possible identifier start in a surrogate pair.
            if (cp >= 0xD800 && cp < 0xDFFF) {
                if (character_1.Character.isIdentifierStart(this.codePointAt(this.index))) {
                    return this.scanIdentifier();
                }
            }
            return this.scanPunctuator();
        }
    }
    exports.Scanner = Scanner;


/***/ },
/* 13 */
/***/ function(module, exports) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.TokenName = {};
    exports.TokenName[1 /* BooleanLiteral */] = 'Boolean';
    exports.TokenName[2 /* EOF */] = '<end>';
    exports.TokenName[3 /* Identifier */] = 'Identifier';
    exports.TokenName[4 /* Keyword */] = 'Keyword';
    exports.TokenName[5 /* NullLiteral */] = 'Null';
    exports.TokenName[6 /* NumericLiteral */] = 'Numeric';
    exports.TokenName[7 /* Punctuator */] = 'Punctuator';
    exports.TokenName[8 /* StringLiteral */] = 'String';
    exports.TokenName[9 /* RegularExpression */] = 'RegularExpression';
    exports.TokenName[10 /* Template */] = 'Template';


/***/ },
/* 14 */
/***/ function(module, exports) {

    "use strict";
    // Generated by generate-xhtml-entities.js. DO NOT MODIFY!
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.XHTMLEntities = {
        quot: '\u0022',
        amp: '\u0026',
        apos: '\u0027',
        gt: '\u003E',
        nbsp: '\u00A0',
        iexcl: '\u00A1',
        cent: '\u00A2',
        pound: '\u00A3',
        curren: '\u00A4',
        yen: '\u00A5',
        brvbar: '\u00A6',
        sect: '\u00A7',
        uml: '\u00A8',
        copy: '\u00A9',
        ordf: '\u00AA',
        laquo: '\u00AB',
        not: '\u00AC',
        shy: '\u00AD',
        reg: '\u00AE',
        macr: '\u00AF',
        deg: '\u00B0',
        plusmn: '\u00B1',
        sup2: '\u00B2',
        sup3: '\u00B3',
        acute: '\u00B4',
        micro: '\u00B5',
        para: '\u00B6',
        middot: '\u00B7',
        cedil: '\u00B8',
        sup1: '\u00B9',
        ordm: '\u00BA',
        raquo: '\u00BB',
        frac14: '\u00BC',
        frac12: '\u00BD',
        frac34: '\u00BE',
        iquest: '\u00BF',
        Agrave: '\u00C0',
        Aacute: '\u00C1',
        Acirc: '\u00C2',
        Atilde: '\u00C3',
        Auml: '\u00C4',
        Aring: '\u00C5',
        AElig: '\u00C6',
        Ccedil: '\u00C7',
        Egrave: '\u00C8',
        Eacute: '\u00C9',
        Ecirc: '\u00CA',
        Euml: '\u00CB',
        Igrave: '\u00CC',
        Iacute: '\u00CD',
        Icirc: '\u00CE',
        Iuml: '\u00CF',
        ETH: '\u00D0',
        Ntilde: '\u00D1',
        Ograve: '\u00D2',
        Oacute: '\u00D3',
        Ocirc: '\u00D4',
        Otilde: '\u00D5',
        Ouml: '\u00D6',
        times: '\u00D7',
        Oslash: '\u00D8',
        Ugrave: '\u00D9',
        Uacute: '\u00DA',
        Ucirc: '\u00DB',
        Uuml: '\u00DC',
        Yacute: '\u00DD',
        THORN: '\u00DE',
        szlig: '\u00DF',
        agrave: '\u00E0',
        aacute: '\u00E1',
        acirc: '\u00E2',
        atilde: '\u00E3',
        auml: '\u00E4',
        aring: '\u00E5',
        aelig: '\u00E6',
        ccedil: '\u00E7',
        egrave: '\u00E8',
        eacute: '\u00E9',
        ecirc: '\u00EA',
        euml: '\u00EB',
        igrave: '\u00EC',
        iacute: '\u00ED',
        icirc: '\u00EE',
        iuml: '\u00EF',
        eth: '\u00F0',
        ntilde: '\u00F1',
        ograve: '\u00F2',
        oacute: '\u00F3',
        ocirc: '\u00F4',
        otilde: '\u00F5',
        ouml: '\u00F6',
        divide: '\u00F7',
        oslash: '\u00F8',
        ugrave: '\u00F9',
        uacute: '\u00FA',
        ucirc: '\u00FB',
        uuml: '\u00FC',
        yacute: '\u00FD',
        thorn: '\u00FE',
        yuml: '\u00FF',
        OElig: '\u0152',
        oelig: '\u0153',
        Scaron: '\u0160',
        scaron: '\u0161',
        Yuml: '\u0178',
        fnof: '\u0192',
        circ: '\u02C6',
        tilde: '\u02DC',
        Alpha: '\u0391',
        Beta: '\u0392',
        Gamma: '\u0393',
        Delta: '\u0394',
        Epsilon: '\u0395',
        Zeta: '\u0396',
        Eta: '\u0397',
        Theta: '\u0398',
        Iota: '\u0399',
        Kappa: '\u039A',
        Lambda: '\u039B',
        Mu: '\u039C',
        Nu: '\u039D',
        Xi: '\u039E',
        Omicron: '\u039F',
        Pi: '\u03A0',
        Rho: '\u03A1',
        Sigma: '\u03A3',
        Tau: '\u03A4',
        Upsilon: '\u03A5',
        Phi: '\u03A6',
        Chi: '\u03A7',
        Psi: '\u03A8',
        Omega: '\u03A9',
        alpha: '\u03B1',
        beta: '\u03B2',
        gamma: '\u03B3',
        delta: '\u03B4',
        epsilon: '\u03B5',
        zeta: '\u03B6',
        eta: '\u03B7',
        theta: '\u03B8',
        iota: '\u03B9',
        kappa: '\u03BA',
        lambda: '\u03BB',
        mu: '\u03BC',
        nu: '\u03BD',
        xi: '\u03BE',
        omicron: '\u03BF',
        pi: '\u03C0',
        rho: '\u03C1',
        sigmaf: '\u03C2',
        sigma: '\u03C3',
        tau: '\u03C4',
        upsilon: '\u03C5',
        phi: '\u03C6',
        chi: '\u03C7',
        psi: '\u03C8',
        omega: '\u03C9',
        thetasym: '\u03D1',
        upsih: '\u03D2',
        piv: '\u03D6',
        ensp: '\u2002',
        emsp: '\u2003',
        thinsp: '\u2009',
        zwnj: '\u200C',
        zwj: '\u200D',
        lrm: '\u200E',
        rlm: '\u200F',
        ndash: '\u2013',
        mdash: '\u2014',
        lsquo: '\u2018',
        rsquo: '\u2019',
        sbquo: '\u201A',
        ldquo: '\u201C',
        rdquo: '\u201D',
        bdquo: '\u201E',
        dagger: '\u2020',
        Dagger: '\u2021',
        bull: '\u2022',
        hellip: '\u2026',
        permil: '\u2030',
        prime: '\u2032',
        Prime: '\u2033',
        lsaquo: '\u2039',
        rsaquo: '\u203A',
        oline: '\u203E',
        frasl: '\u2044',
        euro: '\u20AC',
        image: '\u2111',
        weierp: '\u2118',
        real: '\u211C',
        trade: '\u2122',
        alefsym: '\u2135',
        larr: '\u2190',
        uarr: '\u2191',
        rarr: '\u2192',
        darr: '\u2193',
        harr: '\u2194',
        crarr: '\u21B5',
        lArr: '\u21D0',
        uArr: '\u21D1',
        rArr: '\u21D2',
        dArr: '\u21D3',
        hArr: '\u21D4',
        forall: '\u2200',
        part: '\u2202',
        exist: '\u2203',
        empty: '\u2205',
        nabla: '\u2207',
        isin: '\u2208',
        notin: '\u2209',
        ni: '\u220B',
        prod: '\u220F',
        sum: '\u2211',
        minus: '\u2212',
        lowast: '\u2217',
        radic: '\u221A',
        prop: '\u221D',
        infin: '\u221E',
        ang: '\u2220',
        and: '\u2227',
        or: '\u2228',
        cap: '\u2229',
        cup: '\u222A',
        int: '\u222B',
        there4: '\u2234',
        sim: '\u223C',
        cong: '\u2245',
        asymp: '\u2248',
        ne: '\u2260',
        equiv: '\u2261',
        le: '\u2264',
        ge: '\u2265',
        sub: '\u2282',
        sup: '\u2283',
        nsub: '\u2284',
        sube: '\u2286',
        supe: '\u2287',
        oplus: '\u2295',
        otimes: '\u2297',
        perp: '\u22A5',
        sdot: '\u22C5',
        lceil: '\u2308',
        rceil: '\u2309',
        lfloor: '\u230A',
        rfloor: '\u230B',
        loz: '\u25CA',
        spades: '\u2660',
        clubs: '\u2663',
        hearts: '\u2665',
        diams: '\u2666',
        lang: '\u27E8',
        rang: '\u27E9'
    };


/***/ },
/* 15 */
/***/ function(module, exports, __webpack_require__) {

    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    const error_handler_1 = __webpack_require__(10);
    const scanner_1 = __webpack_require__(12);
    const token_1 = __webpack_require__(13);
    class Reader {
        constructor() {
            this.values = [];
            this.curly = this.paren = -1;
        }
        // A function following one of those tokens is an expression.
        beforeFunctionExpression(t) {
            return ['(', '{', '[', 'in', 'typeof', 'instanceof', 'new',
                'return', 'case', 'delete', 'throw', 'void',
                // assignment operators
                '=', '+=', '-=', '*=', '**=', '/=', '%=', '<<=', '>>=', '>>>=',
                '&=', '|=', '^=', ',',
                // binary/unary operators
                '+', '-', '*', '**', '/', '%', '++', '--', '<<', '>>', '>>>', '&',
                '|', '^', '!', '~', '&&', '||', '?', ':', '===', '==', '>=',
                '<=', '<', '>', '!=', '!=='].indexOf(t) >= 0;
        }
        // Determine if forward slash (/) is an operator or part of a regular expression
        // https://github.com/mozilla/sweet.js/wiki/design
        isRegexStart() {
            const previous = this.values[this.values.length - 1];
            let regex = (previous !== null);
            switch (previous) {
                case 'this':
                case ']':
                    regex = false;
                    break;
                case ')':
                    const keyword = this.values[this.paren - 1];
                    regex = (keyword === 'if' || keyword === 'while' || keyword === 'for' || keyword === 'with');
                    break;
                case '}':
                    // Dividing a function by anything makes little sense,
                    // but we have to check for that.
                    regex = true;
                    if (this.values[this.curly - 3] === 'function') {
                        // Anonymous function, e.g. function(){} /42
                        const check = this.values[this.curly - 4];
                        regex = check ? !this.beforeFunctionExpression(check) : false;
                    }
                    else if (this.values[this.curly - 4] === 'function') {
                        // Named function, e.g. function f(){} /42/
                        const check = this.values[this.curly - 5];
                        regex = check ? !this.beforeFunctionExpression(check) : true;
                    }
                    break;
                default:
                    break;
            }
            return regex;
        }
        push(token) {
            if (token.type === 7 /* Punctuator */ || token.type === 4 /* Keyword */) {
                if (token.value === '{') {
                    this.curly = this.values.length;
                }
                else if (token.value === '(') {
                    this.paren = this.values.length;
                }
                this.values.push(token.value);
            }
            else {
                this.values.push(null);
            }
        }
    }
    class Tokenizer {
        constructor(code, config) {
            this.errorHandler = new error_handler_1.ErrorHandler();
            this.errorHandler.tolerant = config ? (typeof config.tolerant === 'boolean' && config.tolerant) : false;
            this.scanner = new scanner_1.Scanner(code, this.errorHandler);
            this.scanner.trackComment = config ? (typeof config.comment === 'boolean' && config.comment) : false;
            this.trackRange = config ? (typeof config.range === 'boolean' && config.range) : false;
            this.trackLoc = config ? (typeof config.loc === 'boolean' && config.loc) : false;
            this.buffer = [];
            this.reader = new Reader();
        }
        errors() {
            return this.errorHandler.errors;
        }
        getNextToken() {
            if (this.buffer.length === 0) {
                const comments = this.scanner.scanComments();
                if (this.scanner.trackComment) {
                    for (let i = 0; i < comments.length; ++i) {
                        const e = comments[i];
                        const value = this.scanner.source.slice(e.slice[0], e.slice[1]);
                        const comment = {
                            type: e.multiLine ? 'BlockComment' : 'LineComment',
                            value: value
                        };
                        if (this.trackRange) {
                            comment.range = e.range;
                        }
                        if (this.trackLoc) {
                            comment.loc = e.loc;
                        }
                        this.buffer.push(comment);
                    }
                }
                if (!this.scanner.eof()) {
                    let loc;
                    if (this.trackLoc) {
                        loc = {
                            start: {
                                line: this.scanner.lineNumber,
                                column: this.scanner.index - this.scanner.lineStart
                            },
                            end: {}
                        };
                    }
                    const maybeRegex = (this.scanner.source[this.scanner.index] === '/') && this.reader.isRegexStart();
                    let token;
                    if (maybeRegex) {
                        const state = this.scanner.saveState();
                        try {
                            token = this.scanner.scanRegExp();
                        }
                        catch (e) {
                            this.scanner.restoreState(state);
                            token = this.scanner.lex();
                        }
                    }
                    else {
                        token = this.scanner.lex();
                    }
                    this.reader.push(token);
                    const entry = {
                        type: token_1.TokenName[token.type],
                        value: this.scanner.source.slice(token.start, token.end)
                    };
                    if (this.trackRange) {
                        entry.range = [token.start, token.end];
                    }
                    if (this.trackLoc) {
                        loc.end = {
                            line: this.scanner.lineNumber,
                            column: this.scanner.index - this.scanner.lineStart
                        };
                        entry.loc = loc;
                    }
                    if (token.type === 9 /* RegularExpression */) {
                        const pattern = token.pattern;
                        const flags = token.flags;
                        entry.regex = { pattern, flags };
                    }
                    this.buffer.push(entry);
                }
            }
            return this.buffer.shift();
        }
    }
    exports.Tokenizer = Tokenizer;


/***/ }
/******/ ])
});
