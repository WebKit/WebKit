/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.ScriptSyntaxTree = function(sourceText, script)
{
    console.assert(script && script instanceof WebInspector.Script, script);

    WebInspector.Object.call(this);
    this._script = script;

    try {
        var esprimaSyntaxTree = WebInspector.Esprima.parse(sourceText, {range: true});
        this._syntaxTree = this._createInternalSyntaxTree(esprimaSyntaxTree);
        this._parsedSuccessfully = true;
    } catch (error) {
        this._parsedSuccessfully = false;
        this._syntaxTree = null;
        console.error("Couldn't parse JavaScript File: " + script.url, error);
    }
};

// This should be kept in sync with an enum in JavaSciptCore/runtime/TypeProfiler.h
WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor = {
    NormalExpression: 1,
    FunctionReturn: 2
};

WebInspector.ScriptSyntaxTree.NodeType = {
    AssignmentExpression: "assignment-expression",
    ArrayExpression: "expression",
    BlockStatement: "block-statement",
    BinaryExpression: "binary-expression",
    BreakStatement: "break-statement",
    CallExpression: "call-expression",
    CatchClause: "catch-clause",
    ConditionalExpression: "conditional-expression",
    ContinueStatement: "continue-statement",
    DoWhileStatement: "do-while-statement",
    DebuggerStatement: "debugger-statement",
    EmptyStatement: "empty-statement",
    ExpressionStatement: "expression-statement",
    ForStatement: "for-statement",
    ForInStatement: "for-in-statement",
    FunctionDeclaration: "function-declaration",
    FunctionExpression: "function-expression",
    Identifier: "identifier",
    IfStatement: "if-statement",
    Literal: "literal",
    LabeledStatement: "labeled-statement",
    LogicalExpression: "logical-expression",
    MemberExpression: "member-expression",
    NewExpression: "new-expression",
    ObjectExpression: "objectExpression",
    Program: "program",
    Property: "property",
    ReturnStatement: "return-statement",
    SequenceExpression: "sequence-expression",
    SwitchStatement: "switch-statement",
    SwitchCase: "switch-case",
    ThisExpression: "this-expression",
    ThrowStatement: "throw-statement",
    TryStatement: "try-statement",
    UnaryExpression: "unary-expression",
    UpdateExpression: "update-expression",
    VariableDeclaration: "variable-declaration",
    VariableDeclarator: "variable-declarator",
    WhileStatement: "while-statement",
    WithStatement: "with-statement"
};

WebInspector.ScriptSyntaxTree.prototype = {
    constructor: WebInspector.ScriptSyntaxTree,
    __proto__: WebInspector.Object.prototype,

    // Public
    
    get parsedSuccessfully()
    {
        return this._parsedSuccessfully;
    },

    forEachNode: function(callback)
    {
        console.assert(this._parsedSuccessfully);
        if (!this._parsedSuccessfully)
            return;

        this._recurse(this._syntaxTree, callback, this._defaultParserState());
    },

    filter: function(predicate, startNode) 
    {
        console.assert(startNode && this._parsedSuccessfully);
        if (!this._parsedSuccessfully)
            return [];

        var nodes = [];
        function filter(node, state)
        {
            if (predicate(node))
                nodes.push(node);
            else
                state.skipChildNodes = true; 
        }

        this._recurse(startNode, filter, this._defaultParserState());

        return nodes;
    },

    filterByRange: function(startOffset, endOffset)
    {
        console.assert(this._parsedSuccessfully);
        if (!this._parsedSuccessfully)
            return [];
        
        var allNodes = [];
        const start = 0;
        const end = 1;
        function filterForNodesInRange(node, state)
        {
            // program start        range            program end
            // [                 [         ]               ]
            //            [ ]  [   [        ] ]  [ ]

            // If a node's range ends before the range we're interested in starts, we don't need to search any of its
            // enclosing ranges, because, by definition, those enclosing ranges are contained within this node's range.
            if (node.range[end] < startOffset)
                state.skipChildNodes = true;

            // We are only interested in nodes whose start position is within our range.
            if (startOffset <= node.range[start] && node.range[start] <= endOffset)
                allNodes.push(node);

            // Once we see nodes that start beyond our range, we can quit traversing the AST. We can do this safely
            // because we know the AST is traversed using depth first search, so it will traverse into enclosing ranges
            // before it traverses into adjacent ranges.
            if (node.range[start] > endOffset)
                state.shouldStopEarly = true;
        }

        this.forEachNode(filterForNodesInRange);

        return allNodes;
    },

    containsNonEmptyReturnStatement: function(startNode)
    {
        console.assert(startNode && this._parsedSuccessfully);
        if (!this._parsedSuccessfully)
            return false;

        if (startNode.attachments._hasNonEmptyReturnStatement !== undefined)
            return startNode.attachments._hasNonEmptyReturnStatement;

        function removeFunctionsFilter(node)
        {
            return node.type !== WebInspector.ScriptSyntaxTree.NodeType.FunctionExpression 
                && node.type !== WebInspector.ScriptSyntaxTree.NodeType.FunctionDeclaration;
        }

        var nodes = this.filter(removeFunctionsFilter, startNode);
        var hasNonEmptyReturnStatement = false;
        var returnStatementType = WebInspector.ScriptSyntaxTree.NodeType.ReturnStatement;
        for (var node of nodes) {
            if (node.type === returnStatementType && node.argument) {
                hasNonEmptyReturnStatement = true;
                break;
            }
        }
         
        startNode.attachments._hasNonEmptyReturnStatement = hasNonEmptyReturnStatement;

        return hasNonEmptyReturnStatement;
    },

    updateTypes: function(nodesToUpdate, callback)
    {
        console.assert(RuntimeAgent.getRuntimeTypesForVariablesAtOffsets);
        console.assert(Array.isArray(nodesToUpdate) && this._parsedSuccessfully);

        if (!this._parsedSuccessfully)
            return;

        var allRequests = [];
        var allRequestNodes = [];
        var sourceID = this._script.id;

        for (var node of nodesToUpdate) {
            switch (node.type) {
            case WebInspector.ScriptSyntaxTree.NodeType.FunctionDeclaration:
            case WebInspector.ScriptSyntaxTree.NodeType.FunctionExpression:
                for (var param of node.params) {
                    allRequests.push({
                        typeInformationDescriptor: WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor.NormalExpression,
                        sourceID: sourceID,
                        divot: param.range[0]
                    });
                    allRequestNodes.push(param);
                }

                allRequests.push({
                    typeInformationDescriptor: WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor.FunctionReturn,
                    sourceID: sourceID,
                    divot: node.body.range[0]
                });
                allRequestNodes.push(node);
                break;
            case WebInspector.ScriptSyntaxTree.NodeType.VariableDeclarator:
                allRequests.push({
                    typeInformationDescriptor: WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor.NormalExpression,
                    sourceID: sourceID,
                    divot: node.id.range[0]
                });
                allRequestNodes.push(node);
                break;
            }
        }

        console.assert(allRequests.length === allRequestNodes.length);

        function handleTypes(error, typeInformationArray)
        {
            if (error)
                return;

            console.assert(typeInformationArray.length === allRequests.length);

            for (var i = 0; i < typeInformationArray.length; i++) {
                var node = allRequestNodes[i];
                var typeInformation = typeInformationArray[i];
                if (allRequests[i].typeInformationDescriptor === WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor.FunctionReturn)
                    node.attachments.returnTypes = typeInformation;
                else
                    node.attachments.types = typeInformation;
            }

            callback();
        }

        RuntimeAgent.getRuntimeTypesForVariablesAtOffsets(allRequests, handleTypes);
    },

    // Private

    _defaultParserState: function() 
    {
        return {
            shouldStopEarly: false,
            skipChildNodes: false
        };
    },

    _recurse: function(node, callback, state) 
    {
        if (!node)
            return;

        if (state.shouldStopEarly || state.skipChildNodes)
            return;

        switch (node.type) {
        case WebInspector.ScriptSyntaxTree.NodeType.AssignmentExpression:
            callback(node, state);
            this._recurse(node.left, callback, state);
            this._recurse(node.right, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ArrayExpression:
            callback(node, state);
            this._recurseArray(node.elements, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.BlockStatement:
            callback(node, state);
            this._recurseArray(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.BinaryExpression:
            callback(node, state);
            this._recurse(node.left, callback, state);
            this._recurse(node.right, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.BreakStatement:
            callback(node, state);
            this._recurse(node.label, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.CatchClause:
            callback(node, state);
            this._recurse(node.param, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.CallExpression:
            callback(node, state);
            this._recurse(node.callee, callback, state);
            this._recurseArray(node.arguments, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ContinueStatement:
            callback(node, state);
            this._recurse(node.label, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.DoWhileStatement:
            callback(node, state);
            this._recurse(node.body, callback, state);
            this._recurse(node.test, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.DebuggerStatement:
            callback(node, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.EmptyStatement:
            callback(node, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ExpressionStatement:
            callback(node, state);
            this._recurse(node.expression, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ForStatement:
            callback(node, state);
            this._recurse(node.init, callback, state);
            this._recurse(node.test, callback, state);
            this._recurse(node.update, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ForInStatement:
            callback(node, state);
            this._recurse(node.left, callback, state);
            this._recurse(node.right, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.FunctionDeclaration:
            callback(node, state);
            this._recurse(node.id, callback, state);
            this._recurseArray(node.params, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.FunctionExpression:
            callback(node, state);
            this._recurse(node.id, callback, state);
            this._recurseArray(node.params, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.Identifier:
            callback(node, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.IfStatement:
            callback(node, state);
            this._recurse(node.test, callback, state);
            this._recurse(node.consequent, callback, state);
            this._recurse(node.alternate, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.Literal:
            callback(node, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.LabeledStatement:
            callback(node, state);
            this._recurse(node.label, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.LogicalExpression:
            callback(node, state);
            this._recurse(node.left, callback, state);
            this._recurse(node.right, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.MemberExpression:
            callback(node, state);
            this._recurse(node.object, callback, state);
            this._recurse(node.property, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.NewExpression:
            callback(node, state);
            this._recurse(node.callee, callback, state);
            this._recurseArray(node.arguments, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ObjectExpression:
            callback(node, state);
            this._recurseArray(node.properties, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.Program:
            callback(node, state);
            this._recurseArray(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.Property:
            callback(node, state);
            this._recurse(node.key, callback, state);
            this._recurse(node.value, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ReturnStatement:
            callback(node, state);
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.SequenceExpression:
            callback(node, state);
            this._recurseArray(node.expressions, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.SwitchStatement:
            callback(node, state);
            this._recurse(node.discriminant, callback, state);
            this._recurseArray(node.cases, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.SwitchCase:
            callback(node, state);
            this._recurse(node.test, callback, state);
            this._recurseArray(node.consequent, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ConditionalExpression:
            callback(node, state);
            this._recurse(node.test, callback, state);
            this._recurse(node.consequent, callback, state);
            this._recurse(node.alternate, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ThisExpression:
            callback(node, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ThrowStatement:
            callback(node, state);
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.TryStatement:
            callback(node, state);
            this._recurse(node.block, callback, state);
            this._recurseArray(node.guardedHandlers, callback, state);
            this._recurseArray(node.handlers, callback, state);
            this._recurse(node.finalizer, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.UnaryExpression:
            callback(node, state);
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.UpdateExpression:
            callback(node, state);
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.VariableDeclaration:
            callback(node, state);
            this._recurseArray(node.declarations, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.VariableDeclarator:
            callback(node, state);
            this._recurse(node.id, callback, state);
            this._recurse(node.init, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.WhileStatement:
            callback(node, state);
            this._recurse(node.test, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.WithStatement:
            callback(node, state);
            this._recurse(node.object, callback, state);
            this._recurse(node.body, callback, state);
            break;
        }

        state.skipChildNodes = false;
    },

    _recurseArray: function(array, callback, state) 
    {
        for (var node of array)
            this._recurse(node, callback, state);
    },
    
    // This function translates from esprima's Abstract Syntax Tree to ours. 
    // Mostly, this is just the identity function. We've added an extra isGetterOrSetter property for functions.
    // Our AST complies with the Mozilla parser API:
    // https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/Parser_API
    _createInternalSyntaxTree: function(node) 
    {
        if (!node)
            return null;

        var result = null;
        switch (node.type) {
        case "AssignmentExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.AssignmentExpression,
                operator: node.operator,
                left: this._createInternalSyntaxTree(node.left),
                right: this._createInternalSyntaxTree(node.right)
            };
            break;
        case "ArrayExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ArrayExpression,
                elements: node.elements.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "BlockStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.BlockStatement,
                body: node.body.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "BinaryExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.BinaryExpression,
                operator: node.operator,
                left: this._createInternalSyntaxTree(node.left),
                right: this._createInternalSyntaxTree(node.right)
            };
            break;
        case "BreakStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.BreakStatement,
                label: this._createInternalSyntaxTree(node.label)
            };
            break;
        case "CallExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.CallExpression,
                callee: this._createInternalSyntaxTree(node.callee),
                arguments: node.arguments.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "CatchClause":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.CatchClause,
                param: this._createInternalSyntaxTree(node.param),
                body: this._createInternalSyntaxTree(node.body)
            };
            break;
        case "ConditionalExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ConditionalExpression,
                test: this._createInternalSyntaxTree(node.test),
                consequent: this._createInternalSyntaxTree(node.consequent),
                alternate: this._createInternalSyntaxTree(node.alternate)
            };
            break;
        case "ContinueStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ContinueStatement,
                label: this._createInternalSyntaxTree(node.label)
            };
            break;
        case "DoWhileStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.DoWhileStatement,
                body: this._createInternalSyntaxTree(node.body),
                test: this._createInternalSyntaxTree(node.test)
            };
            break;
        case "DebuggerStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.DebuggerStatement
            };
            break;
        case "EmptyStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.EmptyStatement
            };
            break;
        case "ExpressionStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ExpressionStatement,
                expression: this._createInternalSyntaxTree(node.expression)
            };
            break;
        case "ForStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ForStatement,
                init: this._createInternalSyntaxTree(node.init),
                test: this._createInternalSyntaxTree(node.test),
                update: this._createInternalSyntaxTree(node.update),
                body: this._createInternalSyntaxTree(node.body)
            };
            break;
        case "ForInStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ForInStatement,
                left: this._createInternalSyntaxTree(node.left),
                right: this._createInternalSyntaxTree(node.right),
                body: this._createInternalSyntaxTree(node.body)
            };
            break;
        case "FunctionDeclaration":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.FunctionDeclaration,
                id: this._createInternalSyntaxTree(node.id),
                params: node.params.map(this._createInternalSyntaxTree.bind(this)),
                body: this._createInternalSyntaxTree(node.body),
                isGetterOrSetter: false // This is obvious, but is convenient none the less b/c Declarations and Expressions are often intertwined.
            };
            break;
        case "FunctionExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.FunctionExpression,
                id: this._createInternalSyntaxTree(node.id),
                params: node.params.map(this._createInternalSyntaxTree.bind(this)),
                body: this._createInternalSyntaxTree(node.body),
                isGetterOrSetter: false // If true, it is set in the Property AST node.
            };
            break;
        case "Identifier":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.Identifier,
                name: node.name
            };
            break;
        case "IfStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.IfStatement,
                test: this._createInternalSyntaxTree(node.test),
                consequent: this._createInternalSyntaxTree(node.consequent),
                alternate: this._createInternalSyntaxTree(node.alternate)
            };
            break;
        case "Literal":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.Literal,
                value: node.value,
                raw: node.raw
            };
            break;
        case "LabeledStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.LabeledStatement,
                label: this._createInternalSyntaxTree(node.label),
                body: this._createInternalSyntaxTree(node.body)
            };
            break;
        case "LogicalExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.LogicalExpression,
                left: this._createInternalSyntaxTree(node.left),
                right: this._createInternalSyntaxTree(node.right),
                operator: node.operator
            };
            break;
        case "MemberExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.MemberExpression,
                object: this._createInternalSyntaxTree(node.object),
                property: this._createInternalSyntaxTree(node.property),
                computed: node.computed
            };
            break;
        case "NewExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.NewExpression,
                callee: this._createInternalSyntaxTree(node.callee),
                arguments: node.arguments.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "ObjectExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ObjectExpression,
                properties: node.properties.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "Program":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.Program,
                body: node.body.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "Property":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.Property,
                key: this._createInternalSyntaxTree(node.key),
                value: this._createInternalSyntaxTree(node.value),
                kind: node.kind
            };
            if (result.kind === "get" || result.kind === "set") {
                result.value.isGetterOrSetter = true;
                result.value.getterOrSetterRange = result.key.range;
            }
            break;
        case "ReturnStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ReturnStatement,
                argument: this._createInternalSyntaxTree(node.argument)
            };
            break;
        case "SequenceExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.SequenceExpression,
                expressions: node.expressions.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "SwitchStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.SwitchStatement,
                discriminant: this._createInternalSyntaxTree(node.discriminant),
                cases: node.cases.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "SwitchCase":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.SwitchCase,
                test: this._createInternalSyntaxTree(node.test),
                consequent: node.consequent.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "ThisExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ThisExpression
            };
            break;
        case "ThrowStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ThrowStatement,
                argument: this._createInternalSyntaxTree(node.argument)
            };
            break;
        case "TryStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.TryStatement,
                block: this._createInternalSyntaxTree(node.block),
                // FIXME: What are guarded handlers?
                guardedHandlers: node.guardedHandlers.map(this._createInternalSyntaxTree.bind(this)),
                handlers: node.handlers.map(this._createInternalSyntaxTree.bind(this)),
                finalizer: this._createInternalSyntaxTree(node.finalizer)
            };
            break;
        case "UnaryExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.UnaryExpression,
                operator: node.operator,
                argument: this._createInternalSyntaxTree(node.argument)
            };
            break;
        case "UpdateExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.UpdateExpression,
                operator: node.operator,
                prefix: node.prefix,
                argument: this._createInternalSyntaxTree(node.argument)
            };
            break;
        case "VariableDeclaration":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.VariableDeclaration,
                declarations: node.declarations.map(this._createInternalSyntaxTree.bind(this))
            };
            break;
        case "VariableDeclarator":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.VariableDeclarator,
                id: this._createInternalSyntaxTree(node.id),
                init: this._createInternalSyntaxTree(node.init)
            };
            break;
        case "WhileStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.WhileStatement,
                test: this._createInternalSyntaxTree(node.test),
                body: this._createInternalSyntaxTree(node.body)
            };
            break;
        case "WithStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.WithStatement,
                object: this._createInternalSyntaxTree(node.object),
                body: this._createInternalSyntaxTree(node.body)
            };
            break;
        default:
            console.error("Unsupported Syntax Tree Node: " + node.type, node);
        }
        
        result.range = node.range;
        // This is an object for which you can add fields to an AST node without worrying about polluting the syntax-related fields of the node.
        result.attachments = {};

        return result;
    }
};
