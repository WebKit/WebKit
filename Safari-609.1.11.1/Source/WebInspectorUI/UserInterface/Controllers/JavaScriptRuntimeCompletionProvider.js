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

Object.defineProperty(WI, "javaScriptRuntimeCompletionProvider",
{
    get: function()
    {
        if (!WI.JavaScriptRuntimeCompletionProvider._instance)
            WI.JavaScriptRuntimeCompletionProvider._instance = new WI.JavaScriptRuntimeCompletionProvider;
        return WI.JavaScriptRuntimeCompletionProvider._instance;
    }
});

WI.JavaScriptRuntimeCompletionProvider = class JavaScriptRuntimeCompletionProvider extends WI.Object
{
    constructor()
    {
        super();

        console.assert(!WI.JavaScriptRuntimeCompletionProvider._instance);

        this._ongoingCompletionRequests = 0;

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ActiveCallFrameDidChange, this._clearLastProperties, this);
    }

    // Static

    static get _commandLineAPIKeys()
    {
        if (!JavaScriptRuntimeCompletionProvider.__cachedCommandLineAPIKeys) {
            JavaScriptRuntimeCompletionProvider.__cachedCommandLineAPIKeys = [
                "$_",
                "assert",
                "clear",
                "count",
                "countReset",
                "debug",
                "dir",
                "dirxml",
                "error",
                "group",
                "groupCollapsed",
                "groupEnd",
                "info",
                "inspect",
                "keys",
                "log",
                "profile",
                "profileEnd",
                "queryHolders",
                "queryInstances",
                "queryObjects",
                "record",
                "recordEnd",
                "screenshot",
                "table",
                "takeHeapSnapshot",
                "time",
                "timeEnd",
                "timeLog",
                "timeStamp",
                "trace",
                "values",
                "warn",
            ];
        }
        return JavaScriptRuntimeCompletionProvider.__cachedCommandLineAPIKeys;
    }

    // Protected

    completionControllerCompletionsNeeded(completionController, defaultCompletions, base, prefix, suffix, forced)
    {
        // Don't allow non-forced empty prefix completions unless the base is that start of property access.
        if (!forced && !prefix && !/[.[]$/.test(base)) {
            completionController.updateCompletions(null);
            return;
        }

        // If the base ends with an open parentheses or open curly bracket then treat it like there is
        // no base so we get global object completions.
        if (/[({]$/.test(base))
            base = "";

        var lastBaseIndex = base.length - 1;
        var dotNotation = base[lastBaseIndex] === ".";
        var bracketNotation = base[lastBaseIndex] === "[";

        if (dotNotation || bracketNotation) {
            base = base.substring(0, lastBaseIndex);

            // Don't suggest anything for an empty base that is using dot notation.
            // Bracket notation with an empty base will be treated as an array.
            if (!base && dotNotation) {
                completionController.updateCompletions(defaultCompletions);
                return;
            }

            // Don't allow non-forced empty prefix completions if the user is entering a number, since it might be a float.
            // But allow number completions if the base already has a decimal, so "10.0." will suggest Number properties.
            if (!forced && !prefix && dotNotation && base.indexOf(".") === -1 && parseInt(base, 10) == base) {
                completionController.updateCompletions(null);
                return;
            }

            // An empty base with bracket notation is not property access, it is an array.
            // Clear the bracketNotation flag so completions are not quoted.
            if (!base && bracketNotation)
                bracketNotation = false;
        }

        // Start an completion request. We must now decrement before calling completionController.updateCompletions.
        this._incrementOngoingCompletionRequests();

        // If the base is the same as the last time, we can reuse the property names we have already gathered.
        // Doing this eliminates delay caused by the async nature of the code below and it only calls getters
        // and functions once instead of repetitively. Sure, there can be difference each time the base is evaluated,
        // but this optimization gives us more of a win. We clear the cache after 30 seconds or when stepping in the
        // debugger to make sure we don't use stale properties in most cases.
        if (this._lastBase === base && this._lastPropertyNames) {
            receivedPropertyNames.call(this, this._lastPropertyNames);
            return;
        }

        this._lastBase = base;
        this._lastPropertyNames = null;

        var activeCallFrame = WI.debuggerManager.activeCallFrame;
        if (!base && activeCallFrame && !this._alwaysEvaluateInWindowContext)
            activeCallFrame.collectScopeChainVariableNames(receivedPropertyNames.bind(this));
        else {
            let options = {objectGroup: "completion", includeCommandLineAPI: true, doNotPauseOnExceptionsAndMuteConsole: true, returnByValue: false, generatePreview: false, saveResult: false};
            WI.runtimeManager.evaluateInInspectedWindow(base, options, evaluated.bind(this));
        }

        function updateLastPropertyNames(propertyNames)
        {
            if (this._clearLastPropertiesTimeout)
                clearTimeout(this._clearLastPropertiesTimeout);
            this._clearLastPropertiesTimeout = setTimeout(this._clearLastProperties.bind(this), WI.JavaScriptLogViewController.CachedPropertiesDuration);

            this._lastPropertyNames = propertyNames || [];
        }

        function evaluated(result, wasThrown)
        {
            if (wasThrown || !result || result.type === "undefined" || (result.type === "object" && result.subtype === "null")) {
                this._decrementOngoingCompletionRequests();

                updateLastPropertyNames.call(this, []);
                completionController.updateCompletions(defaultCompletions);

                return;
            }

            function inspectedPage_evalResult_getArrayCompletions(primitiveType)
            {
                var array = this;
                var arrayLength;

                var resultSet = {};
                for (var o = array; o; o = o.__proto__) {
                    try {
                        if (o === array && o.length) {
                            // If the array type has a length, don't include a list of all the indexes.
                            // Include it at the end and the frontend can build the list.
                            arrayLength = o.length;
                        } else {
                            var names = Object.getOwnPropertyNames(o);
                            for (var i = 0; i < names.length; ++i)
                                resultSet[names[i]] = true;
                        }
                    } catch { }
                }

                if (arrayLength)
                    resultSet["length"] = arrayLength;

                return resultSet;
            }

            function inspectedPage_evalResult_getCompletions(primitiveType)
            {
                var object;
                if (primitiveType === "string")
                    object = new String("");
                else if (primitiveType === "number")
                    object = new Number(0);
                else if (primitiveType === "boolean")
                    object = new Boolean(false);
                else if (primitiveType === "symbol")
                    object = Symbol();
                else
                    object = this;

                var resultSet = {};
                for (var o = object; o; o = o.__proto__) {
                    try {
                        var names = Object.getOwnPropertyNames(o);
                        for (var i = 0; i < names.length; ++i)
                            resultSet[names[i]] = true;
                    } catch (e) { }
                }

                return resultSet;
            }

            if (result.subtype === "array")
                result.callFunctionJSON(inspectedPage_evalResult_getArrayCompletions, undefined, receivedArrayPropertyNames.bind(this));
            else if (result.type === "object" || result.type === "function")
                result.callFunctionJSON(inspectedPage_evalResult_getCompletions, undefined, receivedObjectPropertyNames.bind(this));
            else if (result.type === "string" || result.type === "number" || result.type === "boolean" || result.type === "symbol") {
                let options = {objectGroup: "completion", includeCommandLineAPI: false, doNotPauseOnExceptionsAndMuteConsole: true, returnByValue: true, generatePreview: false, saveResult: false};
                WI.runtimeManager.evaluateInInspectedWindow("(" + inspectedPage_evalResult_getCompletions + ")(\"" + result.type + "\")", options, receivedPropertyNamesFromEvaluate.bind(this));
            } else
                console.error("Unknown result type: " + result.type);
        }

        function receivedPropertyNamesFromEvaluate(object, wasThrown, result)
        {
            receivedPropertyNames.call(this, result && !wasThrown ? Object.keys(result.value) : null);
        }

        function receivedObjectPropertyNames(propertyNames)
        {
            receivedPropertyNames.call(this, Object.keys(propertyNames));
        }

        function receivedArrayPropertyNames(propertyNames)
        {
            if (propertyNames && typeof propertyNames.length === "number") {
                // FIXME <https://webkit.org/b/201909> Web Inspector: autocompletion of array indexes can't handle large arrays in a performant way
                var max = Math.min(propertyNames.length, 1000);
                for (var i = 0; i < max; ++i)
                    propertyNames[i] = true;
            }

            receivedObjectPropertyNames.call(this, propertyNames);
        }

        function receivedPropertyNames(propertyNames)
        {
            console.assert(!propertyNames || Array.isArray(propertyNames));
            propertyNames = propertyNames || [];

            updateLastPropertyNames.call(this, propertyNames);

            this._decrementOngoingCompletionRequests();

            if (!base) {
                propertyNames.pushAll(JavaScriptRuntimeCompletionProvider._commandLineAPIKeys);

                let savedResultAlias = WI.settings.consoleSavedResultAlias.value;
                if (savedResultAlias)
                    propertyNames.push(savedResultAlias + "_");

                let target = WI.runtimeManager.activeExecutionContext.target;

                if (WI.debuggerManager.paused) {
                    let targetData = WI.debuggerManager.dataForTarget(target);
                    if (targetData.pauseReason === WI.DebuggerManager.PauseReason.Listener || targetData.pauseReason === WI.DebuggerManager.PauseReason.EventListener) {
                        propertyNames.push("$event");
                        if (savedResultAlias)
                            propertyNames.push(savedResultAlias + "event");
                    } else if (targetData.pauseReason === WI.DebuggerManager.PauseReason.Exception) {
                        propertyNames.push("$exception");
                        if (savedResultAlias)
                            propertyNames.push(savedResultAlias + "exception");
                    }
                }

                switch (target.type) {
                case WI.TargetType.Page:
                    propertyNames.push("$");
                    propertyNames.push("$$");
                    propertyNames.push("$0");
                    if (savedResultAlias)
                        propertyNames.push(savedResultAlias + "0");
                    propertyNames.push("$x");
                    // fallthrough
                case WI.TargetType.ServiceWorker:
                case WI.TargetType.Worker:
                    propertyNames.push("copy");
                    propertyNames.push("getEventListeners");
                    propertyNames.push("monitorEvents");
                    propertyNames.push("unmonitorEvents");
                    break;
                }

                // FIXME: Due to caching, sometimes old $n values show up as completion results even though they are not available. We should clear that proactively.
                for (var i = 1; i <= WI.ConsoleCommandResultMessage.maximumSavedResultIndex; ++i) {
                    propertyNames.push("$" + i);
                    if (savedResultAlias)
                        propertyNames.push(savedResultAlias + i);
                }
            }

            var implicitSuffix = "";
            if (bracketNotation) {
                var quoteUsed = prefix[0] === "'" ? "'" : "\"";
                if (suffix !== "]" && suffix !== quoteUsed)
                    implicitSuffix = "]";
            }

            var completions = defaultCompletions;
            let knownCompletions = new Set(completions);

            for (var i = 0; i < propertyNames.length; ++i) {
                var property = propertyNames[i];

                if (dotNotation && !/^[a-zA-Z_$][a-zA-Z0-9_$]*$/.test(property))
                    continue;

                if (bracketNotation) {
                    if (parseInt(property) != property)
                        property = quoteUsed + property.escapeCharacters(quoteUsed + "\\") + (suffix !== quoteUsed ? quoteUsed : "");
                }

                if (!property.startsWith(prefix) || knownCompletions.has(property))
                    continue;

                completions.push(property);
                knownCompletions.add(property);
            }

            function compare(a, b)
            {
                // Try to sort in numerical order first.
                let numericCompareResult = a - b;
                if (!isNaN(numericCompareResult))
                    return numericCompareResult;

                // Sort __defineGetter__, __lookupGetter__, and friends last.
                let aRareProperty = a.startsWith("__") && a.endsWith("__");
                let bRareProperty = b.startsWith("__") && b.endsWith("__");
                if (aRareProperty && !bRareProperty)
                    return 1;
                if (!aRareProperty && bRareProperty)
                    return -1;

                // Not numbers, sort as strings.
                return a.extendedLocaleCompare(b);
            }

            completions.sort(compare);

            completionController.updateCompletions(completions, implicitSuffix);
        }
    }

    // Private

    _incrementOngoingCompletionRequests()
    {
        this._ongoingCompletionRequests++;

        console.assert(this._ongoingCompletionRequests <= 50, "Ongoing requests probably should not get this high. We may be missing a balancing decrement.");
    }

    _decrementOngoingCompletionRequests()
    {
        this._ongoingCompletionRequests--;

        console.assert(this._ongoingCompletionRequests >= 0, "Unbalanced increments / decrements.");

        if (this._ongoingCompletionRequests <= 0)
            WI.runtimeManager.activeExecutionContext.target.RuntimeAgent.releaseObjectGroup("completion");
    }

    _clearLastProperties()
    {
        if (this._clearLastPropertiesTimeout) {
            clearTimeout(this._clearLastPropertiesTimeout);
            delete this._clearLastPropertiesTimeout;
        }

        // Clear the cache of property names so any changes while stepping or sitting idle get picked up if the same
        // expression is evaluated again.
        this._lastPropertyNames = null;
    }
};
