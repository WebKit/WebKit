/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WI.RuntimeManager = class RuntimeManager extends WI.Object
{
    constructor()
    {
        super();

        this._activeExecutionContext = null;

        WI.settings.consoleSavedResultAlias.addEventListener(WI.Setting.Event.Changed, function(event) {
            for (let target of WI.targets) {
                // COMPATIBILITY (iOS 13.0): Runtime.setSavedResultAlias did not exist.
                if (target.hasCommand("Runtime.setSavedResultAlias"))
                    target.RuntimeAgent.setSavedResultAlias(WI.settings.consoleSavedResultAlias.value);
            }
        }, this);
    }

    // Static

    static preferredSavedResultPrefix()
    {
        // COMPATIBILITY (iOS 13.0): Runtime.setSavedResultAlias did not exist.
        if (!InspectorBackend.hasCommand("Runtime.setSavedResultAlias"))
            return "$";
        return WI.settings.consoleSavedResultAlias.value || "$";
    }

    // Target

    initializeTarget(target)
    {
        target.RuntimeAgent.enable();

        if (WI.settings.showJavaScriptTypeInformation.value)
            target.RuntimeAgent.enableTypeProfiler();

        if (WI.settings.enableControlFlowProfiler.value)
            target.RuntimeAgent.enableControlFlowProfiler();

        // COMPATIBILITY (iOS 13.0): Runtime.setSavedResultAlias did not exist.
        if (target.hasCommand("Runtime.setSavedResultAlias") && WI.settings.consoleSavedResultAlias.value)
            target.RuntimeAgent.setSavedResultAlias(WI.settings.consoleSavedResultAlias.value);
    }

    // Public

    get activeExecutionContext()
    {
        return this._activeExecutionContext;
    }

    set activeExecutionContext(executionContext)
    {
        if (this._activeExecutionContext === executionContext)
            return;

        this._activeExecutionContext = executionContext;

        this.dispatchEventToListeners(WI.RuntimeManager.Event.ActiveExecutionContextChanged);
    }

    evaluateInInspectedWindow(expression, options, callback)
    {
        if (!this._activeExecutionContext) {
            callback(null, false);
            return;
        }

        let {objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, returnByValue, generatePreview, saveResult, emulateUserGesture, sourceURLAppender} = options;

        includeCommandLineAPI = includeCommandLineAPI || false;
        doNotPauseOnExceptionsAndMuteConsole = doNotPauseOnExceptionsAndMuteConsole || false;
        returnByValue = returnByValue || false;
        generatePreview = generatePreview || false;
        saveResult = saveResult || false;
        emulateUserGesture = emulateUserGesture || false;
        sourceURLAppender = sourceURLAppender || appendWebInspectorSourceURL;

        console.assert(objectGroup, "RuntimeManager.evaluateInInspectedWindow should always be called with an objectGroup");
        console.assert(typeof sourceURLAppender === "function");

        if (!expression) {
            // There is no expression, so the completion should happen against global properties.
            expression = "this";
        } else if (/^\s*\{/.test(expression) && /\}\s*$/.test(expression)) {
            // Transform {a:1} to ({a:1}) so it is treated like an object literal instead of a block with a label.
            expression = "(" + expression + ")";
        } else if (/\bawait\b/.test(expression)) {
            // Transform `await <expr>` into an async function assignment.
            expression = this._tryApplyAwaitConvenience(expression);
        }

        expression = sourceURLAppender(expression);

        let target = this._activeExecutionContext.target;
        let executionContextId = this._activeExecutionContext.id;

        if (WI.debuggerManager.activeCallFrame) {
            target = WI.debuggerManager.activeCallFrame.target;
            executionContextId = target.executionContext.id;
        }

        function evalCallback(error, result, wasThrown, savedResultIndex)
        {
            this.dispatchEventToListeners(WI.RuntimeManager.Event.DidEvaluate, {objectGroup});

            if (error) {
                console.error(error);
                callback(null, false);
                return;
            }

            if (returnByValue)
                callback(null, wasThrown, wasThrown ? null : result, savedResultIndex);
            else
                callback(WI.RemoteObject.fromPayload(result, target), wasThrown, savedResultIndex);
        }

        if (WI.debuggerManager.activeCallFrame) {
            target.DebuggerAgent.evaluateOnCallFrame.invoke({
                callFrameId: WI.debuggerManager.activeCallFrame.id,
                expression,
                objectGroup,
                includeCommandLineAPI,
                doNotPauseOnExceptionsAndMuteConsole,
                returnByValue,
                generatePreview,
                saveResult,
                emulateUserGesture, // COMPATIBILITY (iOS 13): "emulateUserGesture" did not exist yet.
            }, evalCallback.bind(this));
            return;
        }

        target.RuntimeAgent.evaluate.invoke({
            expression,
            objectGroup,
            includeCommandLineAPI,
            doNotPauseOnExceptionsAndMuteConsole,
            contextId: executionContextId,
            returnByValue,
            generatePreview,
            saveResult,
            emulateUserGesture,
        }, evalCallback.bind(this));
    }

    saveResult(remoteObject, callback)
    {
        console.assert(remoteObject instanceof WI.RemoteObject);

        let target = this._activeExecutionContext.target;
        let executionContextId = this._activeExecutionContext.id;

        function mycallback(error, savedResultIndex)
        {
            callback(savedResultIndex);
        }

        if (remoteObject.objectId)
            target.RuntimeAgent.saveResult(remoteObject.asCallArgument(), mycallback);
        else
            target.RuntimeAgent.saveResult(remoteObject.asCallArgument(), executionContextId, mycallback);
    }

    // Private

    _tryApplyAwaitConvenience(originalExpression)
    {
        function containsTopLevelAwait(node) {
            if (!node)
                return false;

            switch (node.type) {
            case "AssignmentExpression":
            case "BinaryExpression":
                return containsTopLevelAwait(node.left)
                    || containsTopLevelAwait(node.right);
            case "CallExpression":
                return (node.callee.type === "Identifier" && node.callee.name === "await")
                    || containsTopLevelAwait(node.callee)
                    || node.arguments.some(containsTopLevelAwait);
            case "ExpressionStatement":
                return containsTopLevelAwait(node.expression);
            case "VariableDeclaration":
                if (node.declarations.length === 1) {
                    let declaration = node.declarations[0];
                    return declaration.id.type === "Identifier"
                        && declaration.init?.type === "CallExpression"
                        && declaration.init.callee.type === "Identifier"
                        && declaration.init.callee.name === "await";
                }
                break;
            }
            return false;
        }

        let esprimaSyntaxTree = null;

        // Do not transform if the original code parses just fine, unless `await(<expr>)` was parsed as a call to an identifier.
        try {
            esprimaSyntaxTree = esprima.parse(originalExpression).body[0];
        } catch { }
        if (esprimaSyntaxTree && !containsTopLevelAwait(esprimaSyntaxTree))
            return originalExpression;

        // Do not transform if the async function version does not parse.
        let wrappedExpression = `(async function(){${originalExpression}})`;
        try {
            esprimaSyntaxTree = esprima.parse(wrappedExpression, {range: true});
        } catch {
            return originalExpression;
        }

        // Assert expected AST produced by our wrapping code.
        console.assert(esprimaSyntaxTree.type === "Program");
        console.assert(esprimaSyntaxTree.body.length === 1);
        console.assert(esprimaSyntaxTree.body[0].type === "ExpressionStatement");
        console.assert(esprimaSyntaxTree.body[0].expression.type === "FunctionExpression");
        console.assert(esprimaSyntaxTree.body[0].expression.async);
        console.assert(esprimaSyntaxTree.body[0].expression.body.type === "BlockStatement");

        // Do not transform if there is more than one statement.
        let asyncFunctionBlock = esprimaSyntaxTree.body[0].expression.body;
        if (asyncFunctionBlock.body.length !== 1)
            return originalExpression;

        let statement = asyncFunctionBlock.body[0];

        function getTopLevelAwait(node) {
            if (!node)
                return null;

            switch (node.type) {
            case "AssignmentExpression":
                if (node.operator === "="
                    && node.left.type === "Identifier"
                    && node.right.type === "AwaitExpression")
                    return {
                        variable: node.left.name,
                        declaration: "var",
                        expression: node.right,
                    };
                if (getTopLevelAwait(node.left) || getTopLevelAwait(node.right))
                    return {};
                break;
            case "BinaryExpression":
                if (getTopLevelAwait(node.left) || getTopLevelAwait(node.right))
                    return {};
                break;
            case "AwaitExpression":
                return {};
            case "CallExpression":
                if (getTopLevelAwait(node.callee) || node.arguments.some(getTopLevelAwait))
                    return {};
                break;
            case "ExpressionStatement":
                return getTopLevelAwait(node.expression);
            case "VariableDeclaration":
                if (node.declarations.length === 1) {
                    let declaration = node.declarations[0];
                    if (declaration.init
                        && declaration.init.type === "AwaitExpression"
                        && declaration.id.type === "Identifier") {
                        return {
                            variable: declaration.id.name,
                            declaration: node.kind === "const" ? "let" : node.kind,
                            expression: declaration.init,
                        };
                    }
                }
                break;
            }
            return null;
        }
        let found = getTopLevelAwait(statement);
        if (!found)
            return originalExpression;

        let expressionNode = found.expression || statement.expression;
        let expressionText = wrappedExpression.substring(expressionNode.range[0], expressionNode.range[1]);

        let external = "";
        let internal = "";
        if (found.variable) {
            console.assert(found.declaration);
            external = `${found.declaration} ${found.variable}; `;
            internal = `${found.variable} = ${expressionText}; console.info("%o", ${found.variable})`;
        } else {
            // Nothing to do externally as it's an anonymous expression.
            // Preserve evaluation order without introducing a binding.
            internal = `(function(result){ console.info("%o", result); })(${expressionText})`;
        }
        console.assert(internal);
        return `${external}(async function() { try { ${internal}; } catch (e) { console.error(e); } })(); undefined`;
    }
};

WI.RuntimeManager.ConsoleObjectGroup = "console";
WI.RuntimeManager.TopLevelExecutionContextIdentifier = undefined;

WI.RuntimeManager.Event = {
    DidEvaluate: "runtime-manager-did-evaluate",
    DefaultExecutionContextChanged: "runtime-manager-default-execution-context-changed",
    ActiveExecutionContextChanged: "runtime-manager-active-execution-context-changed",
};
