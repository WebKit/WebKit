let symbolicBreakpointPropertyReadSymbol = Symbol("watchedSymbol");
let symbolicBreakpointPropertyReadTargets = {
    dot: {watchedDot: "dot-value"},
    computed: {watchedComputed: "computed-value"},
    array: [],
    getter: {
        get watchedGetter() {
            ++symbolicBreakpointPropertyReadGetterReadCount;
            return "getter-value";
        },
    },
    proxy: new Proxy({}, {
        get(target, propertyName) {
            if (propertyName === "watchedProxy") {
                ++symbolicBreakpointPropertyReadProxyReadCount;
                return "proxy-value";
            }
        },
    }),
    readModifyWrite: {watchedReadModifyWrite: 5},
    destructuring: {watchedDestructuring: "destructuring-value"},
    symbol: {[symbolicBreakpointPropertyReadSymbol]: "symbol-value"},
};
symbolicBreakpointPropertyReadTargets.array[937463] = "index-value";

let symbolicBreakpointPropertyReadComputedKeyConversionCount = 0;
let symbolicBreakpointPropertyReadGetterReadCount = 0;
let symbolicBreakpointPropertyReadProxyReadCount = 0;
let symbolicBreakpointPropertyReadSuperGetterReadCount = 0;
let symbolicBreakpointPropertyReadLastResult = "unread";
let symbolicBreakpointPropertyReadComputedKey = {
    [Symbol.toPrimitive]() {
        ++symbolicBreakpointPropertyReadComputedKeyConversionCount;
        return "watchedComputed";
    },
};

class SymbolicBreakpointPropertyReadBase {
    get watchedSuper() {
        ++symbolicBreakpointPropertyReadSuperGetterReadCount;
        return "super-value";
    }
}

class SymbolicBreakpointPropertyReadDerived extends SymbolicBreakpointPropertyReadBase {
    read() {
        symbolicBreakpointPropertyReadLastResult = super.watchedSuper;
    }
}

let symbolicBreakpointPropertyReadDerived = new SymbolicBreakpointPropertyReadDerived;

function prepareSymbolicBreakpointPropertyRead(operation)
{
    symbolicBreakpointPropertyReadLastResult = "unread";

    switch (operation) {
    case "computed":
        symbolicBreakpointPropertyReadComputedKeyConversionCount = 0;
        break;
    case "getter":
        symbolicBreakpointPropertyReadGetterReadCount = 0;
        break;
    case "proxy":
        symbolicBreakpointPropertyReadProxyReadCount = 0;
        break;
    case "super":
        symbolicBreakpointPropertyReadSuperGetterReadCount = 0;
        break;
    case "read-modify-write":
        symbolicBreakpointPropertyReadTargets.readModifyWrite.watchedReadModifyWrite = 5;
        break;
    }
}

function triggerSymbolicBreakpointPropertyReadArgumentsLength()
{
    symbolicBreakpointPropertyReadLastResult = arguments.length;
}

function triggerSymbolicBreakpointPropertyRead(operation)
{
    switch (operation) {
    case "dot":
        symbolicBreakpointPropertyReadLastResult = symbolicBreakpointPropertyReadTargets.dot.watchedDot;
        break;
    case "computed":
        symbolicBreakpointPropertyReadLastResult = symbolicBreakpointPropertyReadTargets.computed[symbolicBreakpointPropertyReadComputedKey];
        break;
    case "index":
        symbolicBreakpointPropertyReadLastResult = symbolicBreakpointPropertyReadTargets.array[937463];
        break;
    case "getter":
        symbolicBreakpointPropertyReadLastResult = symbolicBreakpointPropertyReadTargets.getter.watchedGetter;
        break;
    case "proxy":
        symbolicBreakpointPropertyReadLastResult = symbolicBreakpointPropertyReadTargets.proxy.watchedProxy;
        break;
    case "super":
        symbolicBreakpointPropertyReadDerived.read();
        break;
    case "read-modify-write":
        symbolicBreakpointPropertyReadLastResult = symbolicBreakpointPropertyReadTargets.readModifyWrite.watchedReadModifyWrite += 2;
        break;
    case "destructuring":
        ({watchedDestructuring: symbolicBreakpointPropertyReadLastResult} = symbolicBreakpointPropertyReadTargets.destructuring);
        break;
    case "symbol":
        symbolicBreakpointPropertyReadLastResult = symbolicBreakpointPropertyReadTargets.symbol[symbolicBreakpointPropertyReadSymbol];
        break;
    case "arguments-length":
        triggerSymbolicBreakpointPropertyReadArgumentsLength(1, 2, 3);
        break;
    default:
        throw new Error(`Unknown property-read operation '${operation}'.`);
    }
}

TestPage.registerInitializer(() => {
    InspectorTest.SymbolicBreakpointPropertyRead = {};

    const operations = {
        "dot": {
            displayName: "Dot",
            propertyName: "watchedDot",
            beforeExpression: "symbolicBreakpointPropertyReadLastResult",
            beforeValue: "unread",
            afterExpression: "symbolicBreakpointPropertyReadLastResult",
            afterValue: "dot-value",
        },
        "computed": {
            displayName: "Computed",
            propertyName: "watchedComputed",
            beforeExpression: "JSON.stringify([symbolicBreakpointPropertyReadLastResult, symbolicBreakpointPropertyReadComputedKeyConversionCount])",
            beforeValue: '["unread",1]',
            afterExpression: "JSON.stringify([symbolicBreakpointPropertyReadLastResult, symbolicBreakpointPropertyReadComputedKeyConversionCount])",
            afterValue: '["computed-value",1]',
        },
        "index": {
            displayName: "Index",
            propertyName: "937463",
            beforeExpression: "symbolicBreakpointPropertyReadLastResult",
            beforeValue: "unread",
            afterExpression: "symbolicBreakpointPropertyReadLastResult",
            afterValue: "index-value",
        },
        "getter": {
            displayName: "Getter",
            propertyName: "watchedGetter",
            beforeExpression: "symbolicBreakpointPropertyReadGetterReadCount",
            beforeValue: 0,
            afterExpression: "JSON.stringify([symbolicBreakpointPropertyReadLastResult, symbolicBreakpointPropertyReadGetterReadCount])",
            afterValue: '["getter-value",1]',
        },
        "proxy": {
            displayName: "Proxy",
            propertyName: "watchedProxy",
            beforeExpression: "symbolicBreakpointPropertyReadProxyReadCount",
            beforeValue: 0,
            afterExpression: "JSON.stringify([symbolicBreakpointPropertyReadLastResult, symbolicBreakpointPropertyReadProxyReadCount])",
            afterValue: '["proxy-value",1]',
        },
        "super": {
            displayName: "Super",
            propertyName: "watchedSuper",
            beforeExpression: "symbolicBreakpointPropertyReadSuperGetterReadCount",
            beforeValue: 0,
            afterExpression: "JSON.stringify([symbolicBreakpointPropertyReadLastResult, symbolicBreakpointPropertyReadSuperGetterReadCount])",
            afterValue: '["super-value",1]',
        },
        "read-modify-write": {
            displayName: "ReadModifyWrite",
            propertyName: "watchedReadModifyWrite",
            beforeExpression: "symbolicBreakpointPropertyReadTargets.readModifyWrite.watchedReadModifyWrite",
            beforeValue: 5,
            afterExpression: "symbolicBreakpointPropertyReadLastResult",
            afterValue: 7,
        },
        "destructuring": {
            displayName: "Destructuring",
            propertyName: "watchedDestructuring",
            beforeExpression: "symbolicBreakpointPropertyReadLastResult",
            beforeValue: "unread",
            afterExpression: "symbolicBreakpointPropertyReadLastResult",
            afterValue: "destructuring-value",
        },
        "symbol": {
            displayName: "Symbol",
            propertyName: "watchedSymbol",
            beforeExpression: "symbolicBreakpointPropertyReadLastResult",
            beforeValue: "unread",
            afterExpression: "symbolicBreakpointPropertyReadLastResult",
            afterValue: "symbol-value",
        },
        "arguments-length": {
            displayName: "ArgumentsLength",
            propertyName: "length",
            beforeExpression: "symbolicBreakpointPropertyReadLastResult",
            beforeValue: "unread",
            afterExpression: "symbolicBreakpointPropertyReadLastResult",
            afterValue: 3,
        },
    };

    function alternatingCase(text)
    {
        let shouldUppercase = true;
        return text.replace(/[a-z]/gi, (character) => {
            let result = shouldUppercase ? character.toUpperCase() : character.toLowerCase();
            shouldUppercase = !shouldUppercase;
            return result;
        });
    }

    function escapeRegularExpression(text)
    {
        return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    }

    function matchingSymbol(propertyName, {caseSensitive, isRegex})
    {
        if (!isRegex)
            return caseSensitive ? propertyName : alternatingCase(propertyName);

        let pattern = "^" + escapeRegularExpression(propertyName.substring(0, propertyName.length - 1)) + ".$";
        return caseSensitive ? pattern : alternatingCase(pattern);
    }

    function nonMatchingSymbol(propertyName, {caseSensitive, isRegex})
    {
        let symbol;
        if (caseSensitive && /[a-z]/i.test(propertyName))
            symbol = alternatingCase(propertyName);
        else
            symbol = propertyName + "DoesNotExist";

        if (isRegex)
            return "^" + escapeRegularExpression(symbol) + "$";
        return symbol;
    }

    async function evaluateOnActiveCallFrame(expression)
    {
        let {result, wasThrown} = await DebuggerAgent.evaluateOnCallFrame.invoke({
            callFrameId: WI.debuggerManager.activeCallFrame.id,
            expression,
            objectGroup: "test",
            doNotPauseOnExceptionsAndMuteConsole: true,
            returnByValue: true,
        });
        InspectorTest.expectFalse(wasThrown, "Evaluating state while paused should not throw.");
        return result.value;
    }

    InspectorTest.SymbolicBreakpointPropertyRead.runTest = function({operation: operationName, caseSensitive, isRegex}) {
        let operation = operations[operationName];
        console.assert(operation, operationName);
        console.assert(typeof caseSensitive === "boolean", caseSensitive);
        console.assert(typeof isRegex === "boolean", isRegex);

        let matcherName = (isRegex ? "Regex" : "Exact") + "." + (caseSensitive ? "CaseSensitive" : "CaseInsensitive");
        let testCaseNamePrefix = `PropertyRead.${operation.displayName}.${matcherName}`;
        let suite = InspectorTest.createAsyncSuite("SymbolicBreakpoint");

        function prepareOperation()
        {
            return InspectorTest.evaluateInPage(`prepareSymbolicBreakpointPropertyRead(${JSON.stringify(operationName)})`);
        }

        function triggerBreakpoint()
        {
            return InspectorTest.evaluateInPage(`triggerSymbolicBreakpointPropertyRead(${JSON.stringify(operationName)})`);
        }

        function addBreakpoint({matches = true, disabled = false} = {})
        {
            let symbol = matches ? matchingSymbol(operation.propertyName, {caseSensitive, isRegex}) : nonMatchingSymbol(operation.propertyName, {caseSensitive, isRegex});
            let breakpoint = new WI.SymbolicBreakpoint(symbol, {
                caseSensitive,
                isRegex,
                disabled,
                matchFunctionCalls: false,
                matchPropertyReads: true,
            });
            WI.debuggerManager.addSymbolicBreakpoint(breakpoint);
            return breakpoint;
        }

        suite.addTestCase({
            name: `SymbolicBreakpoint.${testCaseNamePrefix}.Hit`,
            async test() {
                await prepareOperation();

                InspectorTest.log("Adding breakpoint...");
                let breakpoint = addBreakpoint();
                InspectorTest.assert(WI.debuggerManager.symbolicBreakpointsForSymbol(operation.propertyName, {matchPropertyReads: true})[0] === breakpoint, "Should match breakpoint.");

                InspectorTest.log("Triggering breakpoint...");
                let pausedPromise = WI.debuggerManager.awaitEvent(WI.DebuggerManager.Event.Paused);
                let evaluationPromise = triggerBreakpoint();
                await pausedPromise;
                InspectorTest.pass("Should pause.");

                let targetData = WI.debuggerManager.dataForTarget(WI.debuggerManager.activeCallFrame.target);
                InspectorTest.expectEqual(targetData.pauseReason, WI.DebuggerManager.PauseReason.PropertyRead, "Pause reason should be PropertyRead.");
                InspectorTest.expectEqual(targetData.pauseData.name, operation.propertyName, "Pause data should contain the matching property name.");
                InspectorTest.expectEqual(await evaluateOnActiveCallFrame(operation.beforeExpression), operation.beforeValue, "Should expose the state from before the read.");

                await WI.debuggerManager.resume();
                await evaluationPromise;

                InspectorTest.expectEqual(await InspectorTest.evaluateInPage(operation.afterExpression), operation.afterValue, "Should perform the read after resuming.");

                InspectorTest.newline();

                InspectorTest.log("Removing breakpoint...");
                breakpoint.remove();

                let pauseCount = 0;
                let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, () => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                InspectorTest.log("Triggering breakpoint...");
                await triggerBreakpoint();
                InspectorTest.expectEqual(pauseCount, 0, "Should not pause.");

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
            },
        });

        InspectorTest.BreakpointOptions.addTestCases(suite, {
            testCaseNamePrefix: testCaseNamePrefix + ".Hit.",
            async createBreakpoint() {
                await prepareOperation();
                return addBreakpoint();
            },
            triggerBreakpoint,
        });

        suite.addTestCase({
            name: `SymbolicBreakpoint.${testCaseNamePrefix}.Miss`,
            async test() {
                await prepareOperation();

                let pauseCount = 0;
                let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, () => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                InspectorTest.log("Adding breakpoint...");
                let breakpoint = addBreakpoint({matches: false});
                InspectorTest.assert(!WI.debuggerManager.symbolicBreakpointsForSymbol(operation.propertyName, {matchPropertyReads: true}).length, "Should not match breakpoint.");

                InspectorTest.log("Triggering breakpoint...");
                await triggerBreakpoint();
                InspectorTest.expectEqual(pauseCount, 0, "Should not pause.");

                InspectorTest.newline();

                InspectorTest.log("Removing breakpoint...");
                breakpoint.remove();

                InspectorTest.log("Triggering breakpoint...");
                await triggerBreakpoint();
                InspectorTest.expectEqual(pauseCount, 0, "Should not pause.");

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
            },
        });

        suite.addTestCase({
            name: `SymbolicBreakpoint.${testCaseNamePrefix}.Disabled`,
            async test() {
                await prepareOperation();

                let pauseCount = 0;
                let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, () => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                InspectorTest.log("Adding breakpoint...");
                let breakpoint = addBreakpoint({disabled: true});
                InspectorTest.assert(!WI.debuggerManager.symbolicBreakpointsForSymbol(operation.propertyName, {matchPropertyReads: true}).length, "Should not match breakpoint.");

                InspectorTest.log("Triggering breakpoint...");
                await triggerBreakpoint();
                InspectorTest.expectEqual(pauseCount, 0, "Should not pause.");

                InspectorTest.newline();

                InspectorTest.log("Removing breakpoint...");
                breakpoint.remove();

                InspectorTest.log("Triggering breakpoint...");
                await triggerBreakpoint();
                InspectorTest.expectEqual(pauseCount, 0, "Should not pause.");

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
            },
        });

        suite.runTestCasesAndFinish();
    };
});
