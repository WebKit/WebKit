/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

WebInspector.ScriptSyntaxTree = class ScriptSyntaxTree extends WebInspector.Object
{
    constructor(sourceText, script)
    {
        super();

        console.assert(script && script instanceof WebInspector.Script, script);

        this._script = script;

        try {
            let sourceType = this._script.sourceType === WebInspector.Script.SourceType.Module ? "module" : "script";
            let esprimaSyntaxTree = esprima.parse(sourceText, {range: true, sourceType});
            this._syntaxTree = this._createInternalSyntaxTree(esprimaSyntaxTree);
            this._parsedSuccessfully = true;
        } catch (error) {
            this._parsedSuccessfully = false;
            this._syntaxTree = null;
            console.error("Couldn't parse JavaScript File: " + script.url, error);
        }
    }

    // Public

    get parsedSuccessfully()
    {
        return this._parsedSuccessfully;
    }

    forEachNode(callback)
    {
        console.assert(this._parsedSuccessfully);
        if (!this._parsedSuccessfully)
            return;

        this._recurse(this._syntaxTree, callback, this._defaultParserState());
    }

    filter(predicate, startNode)
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
    }

    containersOfOffset(offset)
    {
        console.assert(this._parsedSuccessfully);
        if (!this._parsedSuccessfully)
            return [];

        let allNodes = [];
        const start = 0;
        const end = 1;

        this.forEachNode((node, state) => {
            if (node.range[end] < offset)
                state.skipChildNodes = true;
            if (node.range[start] > offset)
                state.shouldStopEarly = true;
            if (node.range[start] <= offset && node.range[end] >= offset)
                allNodes.push(node);
        });

        return allNodes;
    }

    filterByRange(startOffset, endOffset)
    {
        console.assert(this._parsedSuccessfully);
        if (!this._parsedSuccessfully)
            return [];

        var allNodes = [];
        var start = 0;
        var end = 1;
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
    }

    containsNonEmptyReturnStatement(startNode)
    {
        console.assert(startNode && this._parsedSuccessfully);
        if (!this._parsedSuccessfully)
            return false;

        if (startNode.attachments._hasNonEmptyReturnStatement !== undefined)
            return startNode.attachments._hasNonEmptyReturnStatement;

        function removeFunctionsFilter(node)
        {
            return node.type !== WebInspector.ScriptSyntaxTree.NodeType.FunctionExpression
                && node.type !== WebInspector.ScriptSyntaxTree.NodeType.FunctionDeclaration
                && node.type !== WebInspector.ScriptSyntaxTree.NodeType.ArrowFunctionExpression;
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
    }

    static functionReturnDivot(node)
    {
        console.assert(node.type === WebInspector.ScriptSyntaxTree.NodeType.FunctionDeclaration || node.type === WebInspector.ScriptSyntaxTree.NodeType.FunctionExpression || node.type === WebInspector.ScriptSyntaxTree.NodeType.MethodDefinition || node.type === WebInspector.ScriptSyntaxTree.NodeType.ArrowFunctionExpression);

        // COMPATIBILITY (iOS 9): Legacy Backends view the return type as being the opening "{" of the function body.
        // After iOS 9, this is to move to the start of the function statement/expression. See below:
        // FIXME: Need a better way to determine backend versions. Using DOM.pseudoElement because that was added after iOS 9.
        if (!DOMAgent.hasEvent("pseudoElementAdded"))
            return node.body.range[0];

        // "f" in "function". "s" in "set". "g" in "get". First letter in any method name for classes and object literals.
        // The "[" for computed methods in classes and object literals.
        return node.typeProfilingReturnDivot;
    }

    updateTypes(nodesToUpdate, callback)
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
            case WebInspector.ScriptSyntaxTree.NodeType.ArrowFunctionExpression:
                for (var param of node.params) {
                    for (var identifier of this._gatherIdentifiersInDeclaration(param)) {
                        allRequests.push({
                            typeInformationDescriptor: WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor.NormalExpression,
                            sourceID,
                            divot: identifier.range[0]
                        });
                        allRequestNodes.push(identifier);
                    }
                }

                allRequests.push({
                    typeInformationDescriptor: WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor.FunctionReturn,
                    sourceID,
                    divot: WebInspector.ScriptSyntaxTree.functionReturnDivot(node)
                });
                allRequestNodes.push(node);
                break;
            case WebInspector.ScriptSyntaxTree.NodeType.VariableDeclarator:
                for (var identifier of this._gatherIdentifiersInDeclaration(node.id)) {
                    allRequests.push({
                        typeInformationDescriptor: WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor.NormalExpression,
                        sourceID,
                        divot: identifier.range[0]
                    });
                    allRequestNodes.push(identifier);
                }
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
                var typeInformation = WebInspector.TypeDescription.fromPayload(typeInformationArray[i]);
                if (allRequests[i].typeInformationDescriptor === WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor.FunctionReturn)
                    node.attachments.returnTypes = typeInformation;
                else
                    node.attachments.types = typeInformation;
            }

            callback(allRequestNodes);
        }

        this._script.target.RuntimeAgent.getRuntimeTypesForVariablesAtOffsets(allRequests, handleTypes);
    }

    // Private

    _gatherIdentifiersInDeclaration(node)
    {
        function gatherIdentifiers(node)
        {
            switch (node.type) {
                case WebInspector.ScriptSyntaxTree.NodeType.Identifier:
                    return [node];
                case WebInspector.ScriptSyntaxTree.NodeType.Property:
                    return gatherIdentifiers(node.value);
                case WebInspector.ScriptSyntaxTree.NodeType.ObjectPattern:
                    var identifiers = [];
                    for (var property of node.properties) {
                        for (var identifier of gatherIdentifiers(property))
                            identifiers.push(identifier);
                    }
                    return identifiers;
                case WebInspector.ScriptSyntaxTree.NodeType.ArrayPattern:
                    var identifiers = [];
                    for (var element of node.elements) {
                        for (var identifier of gatherIdentifiers(element))
                            identifiers.push(identifier);
                    }
                    return identifiers;
                case WebInspector.ScriptSyntaxTree.NodeType.AssignmentPattern:
                    return gatherIdentifiers(node.left);
                case WebInspector.ScriptSyntaxTree.NodeType.RestElement:
                case WebInspector.ScriptSyntaxTree.NodeType.RestProperty:
                    return gatherIdentifiers(node.argument);
                default:
                    console.assert(false, "Unexpected node type in variable declarator: " + node.type);
                    return [];
            }
        }

        console.assert(node.type === WebInspector.ScriptSyntaxTree.NodeType.Identifier || node.type === WebInspector.ScriptSyntaxTree.NodeType.ObjectPattern || node.type === WebInspector.ScriptSyntaxTree.NodeType.ArrayPattern || node.type === WebInspector.ScriptSyntaxTree.NodeType.RestElement || node.type === WebInspector.ScriptSyntaxTree.NodeType.RestProperty);

        return gatherIdentifiers(node);
    }

    _defaultParserState()
    {
        return {
            shouldStopEarly: false,
            skipChildNodes: false
        };
    }

    _recurse(node, callback, state)
    {
        if (!node)
            return;

        if (state.shouldStopEarly || state.skipChildNodes)
            return;

        callback(node, state);

        switch (node.type) {
        case WebInspector.ScriptSyntaxTree.NodeType.AssignmentExpression:
            this._recurse(node.left, callback, state);
            this._recurse(node.right, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ArrayExpression:
        case WebInspector.ScriptSyntaxTree.NodeType.ArrayPattern:
            this._recurseArray(node.elements, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.AssignmentPattern:
            this._recurse(node.left, callback, state);
            this._recurse(node.right, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.AwaitExpression:
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.BlockStatement:
            this._recurseArray(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.BinaryExpression:
            this._recurse(node.left, callback, state);
            this._recurse(node.right, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.BreakStatement:
            this._recurse(node.label, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.CatchClause:
            this._recurse(node.param, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.CallExpression:
            this._recurse(node.callee, callback, state);
            this._recurseArray(node.arguments, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ClassBody:
            this._recurseArray(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ClassDeclaration:
        case WebInspector.ScriptSyntaxTree.NodeType.ClassExpression:
            this._recurse(node.id, callback, state);
            this._recurse(node.superClass, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ContinueStatement:
            this._recurse(node.label, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.DoWhileStatement:
            this._recurse(node.body, callback, state);
            this._recurse(node.test, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ExpressionStatement:
            this._recurse(node.expression, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ForStatement:
            this._recurse(node.init, callback, state);
            this._recurse(node.test, callback, state);
            this._recurse(node.update, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ForInStatement:
        case WebInspector.ScriptSyntaxTree.NodeType.ForOfStatement:
            this._recurse(node.left, callback, state);
            this._recurse(node.right, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.FunctionDeclaration:
        case WebInspector.ScriptSyntaxTree.NodeType.FunctionExpression:
        case WebInspector.ScriptSyntaxTree.NodeType.ArrowFunctionExpression:
            this._recurse(node.id, callback, state);
            this._recurseArray(node.params, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.IfStatement:
            this._recurse(node.test, callback, state);
            this._recurse(node.consequent, callback, state);
            this._recurse(node.alternate, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.LabeledStatement:
            this._recurse(node.label, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.LogicalExpression:
            this._recurse(node.left, callback, state);
            this._recurse(node.right, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.MemberExpression:
            this._recurse(node.object, callback, state);
            this._recurse(node.property, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.MethodDefinition:
            this._recurse(node.key, callback, state);
            this._recurse(node.value, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.NewExpression:
            this._recurse(node.callee, callback, state);
            this._recurseArray(node.arguments, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ObjectExpression:
        case WebInspector.ScriptSyntaxTree.NodeType.ObjectPattern:
            this._recurseArray(node.properties, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.Program:
            this._recurseArray(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.Property:
            this._recurse(node.key, callback, state);
            this._recurse(node.value, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.RestElement:
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.RestProperty:
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ReturnStatement:
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.SequenceExpression:
            this._recurseArray(node.expressions, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.SpreadElement:
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.SpreadProperty:
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.SwitchStatement:
            this._recurse(node.discriminant, callback, state);
            this._recurseArray(node.cases, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.SwitchCase:
            this._recurse(node.test, callback, state);
            this._recurseArray(node.consequent, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ConditionalExpression:
            this._recurse(node.test, callback, state);
            this._recurse(node.consequent, callback, state);
            this._recurse(node.alternate, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.TaggedTemplateExpression:
            this._recurse(node.tag, callback, state);
            this._recurse(node.quasi, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.TemplateLiteral:
            this._recurseArray(node.quasis, callback, state);
            this._recurseArray(node.expressions, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ThrowStatement:
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.TryStatement:
            this._recurse(node.block, callback, state);
            this._recurse(node.handler, callback, state);
            this._recurse(node.finalizer, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.UnaryExpression:
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.UpdateExpression:
            this._recurse(node.argument, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.VariableDeclaration:
            this._recurseArray(node.declarations, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.VariableDeclarator:
            this._recurse(node.id, callback, state);
            this._recurse(node.init, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.WhileStatement:
            this._recurse(node.test, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.WithStatement:
            this._recurse(node.object, callback, state);
            this._recurse(node.body, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.YieldExpression:
            this._recurse(node.argument, callback, state);
            break;

        // Modules.

        case WebInspector.ScriptSyntaxTree.NodeType.ExportAllDeclaration:
            this._recurse(node.source, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ExportNamedDeclaration:
            this._recurse(node.declaration, callback, state);
            this._recurseArray(node.specifiers, callback, state);
            this._recurse(node.source, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ExportDefaultDeclaration:
            this._recurse(node.declaration, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ExportSpecifier:
            this._recurse(node.local, callback, state);
            this._recurse(node.exported, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ImportDeclaration:
            this._recurseArray(node.specifiers, callback, state);
            this._recurse(node.source, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ImportDefaultSpecifier:
            this._recurse(node.local, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ImportNamespaceSpecifier:
            this._recurse(node.local, callback, state);
            break;
        case WebInspector.ScriptSyntaxTree.NodeType.ImportSpecifier:
            this._recurse(node.imported, callback, state);
            this._recurse(node.local, callback, state);
            break;

        // All the leaf nodes go here.
        case WebInspector.ScriptSyntaxTree.NodeType.DebuggerStatement:
        case WebInspector.ScriptSyntaxTree.NodeType.EmptyStatement:
        case WebInspector.ScriptSyntaxTree.NodeType.Identifier:
        case WebInspector.ScriptSyntaxTree.NodeType.Import:
        case WebInspector.ScriptSyntaxTree.NodeType.Literal:
        case WebInspector.ScriptSyntaxTree.NodeType.MetaProperty:
        case WebInspector.ScriptSyntaxTree.NodeType.Super:
        case WebInspector.ScriptSyntaxTree.NodeType.ThisExpression:
        case WebInspector.ScriptSyntaxTree.NodeType.TemplateElement:
            break;
        }

        state.skipChildNodes = false;
    }

    _recurseArray(array, callback, state)
    {
        for (var node of array)
            this._recurse(node, callback, state);
    }

    // This function translates from esprima's Abstract Syntax Tree to ours.
    // Mostly, this is just the identity function. We've added an extra typeProfilingReturnDivot property for functions/methods.
    // Our AST complies with the Mozilla parser API:
    // https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/Parser_API
    _createInternalSyntaxTree(node)
    {
        if (!node)
            return null;

        var result = null;
        switch (node.type) {
        case "ArrayExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ArrayExpression,
                elements: node.elements.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "ArrayPattern":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ArrayPattern,
                elements: node.elements.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "ArrowFunctionExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ArrowFunctionExpression,
                id: this._createInternalSyntaxTree(node.id),
                params: node.params.map(this._createInternalSyntaxTree, this),
                body: this._createInternalSyntaxTree(node.body),
                generator: node.generator,
                expression: node.expression, // Boolean indicating if the body a single expression or a block statement.
                async: node.async,
                typeProfilingReturnDivot: node.range[0]
            };
            break;
        case "AssignmentExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.AssignmentExpression,
                operator: node.operator,
                left: this._createInternalSyntaxTree(node.left),
                right: this._createInternalSyntaxTree(node.right)
            };
            break;
        case "AssignmentPattern":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.AssignmentPattern,
                left: this._createInternalSyntaxTree(node.left),
                right: this._createInternalSyntaxTree(node.right),
            };
            break;
        case "AwaitExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.AwaitExpression,
                argument: this._createInternalSyntaxTree(node.argument),
            };
            break;
        case "BlockStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.BlockStatement,
                body: node.body.map(this._createInternalSyntaxTree, this)
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
                arguments: node.arguments.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "CatchClause":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.CatchClause,
                param: this._createInternalSyntaxTree(node.param),
                body: this._createInternalSyntaxTree(node.body)
            };
            break;
        case "ClassBody":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ClassBody,
                body: node.body.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "ClassDeclaration":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ClassDeclaration,
                id: this._createInternalSyntaxTree(node.id),
                superClass: this._createInternalSyntaxTree(node.superClass),
                body: this._createInternalSyntaxTree(node.body),
            };
            break;
        case "ClassExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ClassExpression,
                id: this._createInternalSyntaxTree(node.id),
                superClass: this._createInternalSyntaxTree(node.superClass),
                body: this._createInternalSyntaxTree(node.body),
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
        case "ForOfStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ForOfStatement,
                left: this._createInternalSyntaxTree(node.left),
                right: this._createInternalSyntaxTree(node.right),
                body: this._createInternalSyntaxTree(node.body)
            };
            break;
        case "FunctionDeclaration":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.FunctionDeclaration,
                id: this._createInternalSyntaxTree(node.id),
                params: node.params.map(this._createInternalSyntaxTree, this),
                body: this._createInternalSyntaxTree(node.body),
                generator: node.generator,
                async: node.async,
                typeProfilingReturnDivot: node.range[0]
            };
            break;
        case "FunctionExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.FunctionExpression,
                id: this._createInternalSyntaxTree(node.id),
                params: node.params.map(this._createInternalSyntaxTree, this),
                body: this._createInternalSyntaxTree(node.body),
                generator: node.generator,
                async: node.async,
                typeProfilingReturnDivot: node.range[0] // This may be overridden in the Property AST node.
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
        case "MetaProperty":
            // i.e: new.target produces {meta: "new", property: "target"}
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.MetaProperty,
                meta: this._createInternalSyntaxTree(node.meta),
                property: this._createInternalSyntaxTree(node.property),
            };
            break;
        case "MethodDefinition":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.MethodDefinition,
                key: this._createInternalSyntaxTree(node.key),
                value: this._createInternalSyntaxTree(node.value),
                computed: node.computed,
                kind: node.kind,
                static: node.static
            };
            result.value.typeProfilingReturnDivot = node.range[0]; // "g" in "get" or "s" in "set" or "[" in "['computed']" or "m" in "methodName".
            break;
        case "NewExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.NewExpression,
                callee: this._createInternalSyntaxTree(node.callee),
                arguments: node.arguments.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "ObjectExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ObjectExpression,
                properties: node.properties.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "ObjectPattern":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ObjectPattern,
                properties: node.properties.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "Program":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.Program,
                sourceType: node.sourceType,
                body: node.body.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "Property":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.Property,
                key: this._createInternalSyntaxTree(node.key),
                value: this._createInternalSyntaxTree(node.value),
                kind: node.kind,
                method: node.method,
                computed: node.computed
            };
            if (result.kind === "get" || result.kind === "set" || result.method)
                result.value.typeProfilingReturnDivot = node.range[0];  // "g" in "get" or "s" in "set" or "[" in "['computed']" method or "m" in "methodName".
            break;
        case "RestElement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.RestElement,
                argument: this._createInternalSyntaxTree(node.argument)
            };
            break;
        case "RestProperty":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.RestProperty,
                argument: this._createInternalSyntaxTree(node.argument),
            };
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
                expressions: node.expressions.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "SpreadElement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.SpreadElement,
                argument: this._createInternalSyntaxTree(node.argument),
            };
            break;
        case "SpreadProperty":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.SpreadProperty,
                argument: this._createInternalSyntaxTree(node.argument),
            };
            break;
        case "Super":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.Super
            };
            break;
        case "SwitchStatement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.SwitchStatement,
                discriminant: this._createInternalSyntaxTree(node.discriminant),
                cases: node.cases.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "SwitchCase":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.SwitchCase,
                test: this._createInternalSyntaxTree(node.test),
                consequent: node.consequent.map(this._createInternalSyntaxTree, this)
            };
            break;
        case "TaggedTemplateExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.TaggedTemplateExpression,
                tag: this._createInternalSyntaxTree(node.tag),
                quasi: this._createInternalSyntaxTree(node.quasi)
            };
            break;
        case "TemplateElement":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.TemplateElement,
                value: node.value,
                tail: node.tail
            };
            break;
        case "TemplateLiteral":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.TemplateLiteral,
                quasis: node.quasis.map(this._createInternalSyntaxTree, this),
                expressions: node.expressions.map(this._createInternalSyntaxTree, this)
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
                handler: this._createInternalSyntaxTree(node.handler),
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
                declarations: node.declarations.map(this._createInternalSyntaxTree, this),
                kind: node.kind
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
        case "YieldExpression":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.YieldExpression,
                argument: this._createInternalSyntaxTree(node.argument),
                delegate: node.delegate
            };
            break;

        // Modules.

        case "ExportAllDeclaration":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ExportAllDeclaration,
                source: this._createInternalSyntaxTree(node.source),
            };
            break;
        case "ExportNamedDeclaration":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ExportNamedDeclaration,
                declaration: this._createInternalSyntaxTree(node.declaration),
                specifiers: node.specifiers.map(this._createInternalSyntaxTree, this),
                source: this._createInternalSyntaxTree(node.source),
            };
            break;
        case "ExportDefaultDeclaration":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ExportDefaultDeclaration,
                declaration: this._createInternalSyntaxTree(node.declaration),
            };
            break;
        case "ExportSpecifier":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ExportSpecifier,
                local: this._createInternalSyntaxTree(node.local),
                exported: this._createInternalSyntaxTree(node.exported),
            };
            break;
        case "Import":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.Import,
            };
            break;
        case "ImportDeclaration":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ImportDeclaration,
                specifiers: node.specifiers.map(this._createInternalSyntaxTree, this),
                source: this._createInternalSyntaxTree(node.source),
            };
            break;
        case "ImportDefaultSpecifier":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ImportDefaultSpecifier,
                local: this._createInternalSyntaxTree(node.local),
            };
            break;
        case "ImportNamespaceSpecifier":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ImportNamespaceSpecifier,
                local: this._createInternalSyntaxTree(node.local),
            };
            break;
        case "ImportSpecifier":
            result = {
                type: WebInspector.ScriptSyntaxTree.NodeType.ImportSpecifier,
                imported: this._createInternalSyntaxTree(node.imported),
                local: this._createInternalSyntaxTree(node.local),
            };
            break;

        default:
            console.error("Unsupported Syntax Tree Node: " + node.type, node);
            return null;
        }

        result.range = node.range;
        // This is an object for which you can add fields to an AST node without worrying about polluting the syntax-related fields of the node.
        result.attachments = {};

        return result;
    }
};

// This should be kept in sync with an enum in JavaSciptCore/runtime/TypeProfiler.h
WebInspector.ScriptSyntaxTree.TypeProfilerSearchDescriptor = {
    NormalExpression: 1,
    FunctionReturn: 2
};

WebInspector.ScriptSyntaxTree.NodeType = {
    ArrayExpression: Symbol("array-expression"),
    ArrayPattern: Symbol("array-pattern"),
    ArrowFunctionExpression: Symbol("arrow-function-expression"),
    AssignmentExpression: Symbol("assignment-expression"),
    AssignmentPattern: Symbol("assignment-pattern"),
    AwaitExpression: Symbol("await-expression"),
    BinaryExpression: Symbol("binary-expression"),
    BlockStatement: Symbol("block-statement"),
    BreakStatement: Symbol("break-statement"),
    CallExpression: Symbol("call-expression"),
    CatchClause: Symbol("catch-clause"),
    ClassBody: Symbol("class-body"),
    ClassDeclaration: Symbol("class-declaration"),
    ClassExpression: Symbol("class-expression"),
    ConditionalExpression: Symbol("conditional-expression"),
    ContinueStatement: Symbol("continue-statement"),
    DebuggerStatement: Symbol("debugger-statement"),
    DoWhileStatement: Symbol("do-while-statement"),
    EmptyStatement: Symbol("empty-statement"),
    ExportAllDeclaration: Symbol("export-all-declaration"),
    ExportDefaultDeclaration: Symbol("export-default-declaration"),
    ExportNamedDeclaration: Symbol("export-named-declaration"),
    ExportSpecifier: Symbol("export-specifier"),
    ExpressionStatement: Symbol("expression-statement"),
    ForInStatement: Symbol("for-in-statement"),
    ForOfStatement: Symbol("for-of-statement"),
    ForStatement: Symbol("for-statement"),
    FunctionDeclaration: Symbol("function-declaration"),
    FunctionExpression: Symbol("function-expression"),
    Identifier: Symbol("identifier"),
    IfStatement: Symbol("if-statement"),
    Import: Symbol("import"),
    ImportDeclaration: Symbol("import-declaration"),
    ImportDefaultSpecifier: Symbol("import-default-specifier"),
    ImportNamespaceSpecifier: Symbol("import-namespace-specifier"),
    ImportSpecifier: Symbol("import-specifier"),
    LabeledStatement: Symbol("labeled-statement"),
    Literal: Symbol("literal"),
    LogicalExpression: Symbol("logical-expression"),
    MemberExpression: Symbol("member-expression"),
    MetaProperty: Symbol("meta-property"),
    MethodDefinition: Symbol("method-definition"),
    NewExpression: Symbol("new-expression"),
    ObjectExpression: Symbol("object-expression"),
    ObjectPattern: Symbol("object-pattern"),
    Program: Symbol("program"),
    Property: Symbol("property"),
    RestElement: Symbol("rest-element"),
    RestProperty: Symbol("rest-property"),
    ReturnStatement: Symbol("return-statement"),
    SequenceExpression: Symbol("sequence-expression"),
    SpreadElement: Symbol("spread-element"),
    SpreadProperty: Symbol("spread-property"),
    Super: Symbol("super"),
    SwitchCase: Symbol("switch-case"),
    SwitchStatement: Symbol("switch-statement"),
    TaggedTemplateExpression: Symbol("tagged-template-expression"),
    TemplateElement: Symbol("template-element"),
    TemplateLiteral: Symbol("template-literal"),
    ThisExpression: Symbol("this-expression"),
    ThrowStatement: Symbol("throw-statement"),
    TryStatement: Symbol("try-statement"),
    UnaryExpression: Symbol("unary-expression"),
    UpdateExpression: Symbol("update-expression"),
    VariableDeclaration: Symbol("variable-declaration"),
    VariableDeclarator: Symbol("variable-declarator"),
    WhileStatement: Symbol("while-statement"),
    WithStatement: Symbol("with-statement"),
    YieldExpression: Symbol("yield-expression"),
};
