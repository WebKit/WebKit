let propertyWriteSymbol = Symbol("watchedSymbol");
let propertyWriteTargets = {
    assignment: {watchedAssignment: "before-assignment"},
    computed: {watchedComputed: "before-computed"},
    array: [],
    deletion: {watchedDeletion: "before-deletion"},
    symbol: {[propertyWriteSymbol]: "before-symbol"},
};
propertyWriteTargets.array[937463] = "before-index";

let propertyWriteComputedKeyConversionCount = 0;
let propertyWriteComputedKey = {
    [Symbol.toPrimitive]() {
        ++propertyWriteComputedKeyConversionCount;
        return "watchedComputed";
    },
};

function triggerSymbolicBreakpointPropertyWrite(operation)
{
    switch (operation) {
    case "dot":
        propertyWriteTargets.assignment.watchedAssignment = "after-assignment";
        break;
    case "computed":
        propertyWriteTargets.computed[propertyWriteComputedKey] = "after-computed";
        break;
    case "index":
        propertyWriteTargets.array[937463] = "after-index";
        break;
    case "delete":
        delete propertyWriteTargets.deletion.watchedDeletion;
        break;
    case "symbol":
        propertyWriteTargets.symbol[propertyWriteSymbol] = "after-symbol";
        break;
    default:
        throw new Error(`Unknown property-write operation '${operation}'.`);
    }

    TestPage.dispatchEventToFrontend("TestPage-SymbolicBreakpointPropertyWrite");
}

TestPage.registerInitializer(() => {
    const propertyWriteOperations = {
        "dot": {
            testName: "Dot",
            propertyName: "watchedAssignment",
            beforeExpression: "propertyWriteTargets.assignment.watchedAssignment",
            beforeValue: "before-assignment",
            afterExpression: "propertyWriteTargets.assignment.watchedAssignment",
            afterValue: "after-assignment",
        },
        "computed": {
            testName: "Computed",
            propertyName: "watchedComputed",
            beforeExpression: "JSON.stringify([propertyWriteTargets.computed.watchedComputed, propertyWriteComputedKeyConversionCount])",
            beforeValue: "[\"before-computed\",1]",
            afterExpression: "JSON.stringify([propertyWriteTargets.computed.watchedComputed, propertyWriteComputedKeyConversionCount])",
            afterValue: "[\"after-computed\",1]",
        },
        "index": {
            testName: "Index",
            propertyName: "937463",
            beforeExpression: "propertyWriteTargets.array[937463]",
            beforeValue: "before-index",
            afterExpression: "propertyWriteTargets.array[937463]",
            afterValue: "after-index",
        },
        "delete": {
            testName: "Delete",
            propertyName: "watchedDeletion",
            beforeExpression: "Object.hasOwn(propertyWriteTargets.deletion, \"watchedDeletion\")",
            beforeValue: true,
            afterExpression: "Object.hasOwn(propertyWriteTargets.deletion, \"watchedDeletion\")",
            afterValue: false,
        },
        "symbol": {
            testName: "Symbol",
            propertyName: "watchedSymbol",
            beforeExpression: "propertyWriteTargets.symbol[propertyWriteSymbol]",
            beforeValue: "before-symbol",
            afterExpression: "propertyWriteTargets.symbol[propertyWriteSymbol]",
            afterValue: "after-symbol",
        },
    };

    function alternateCase(string)
    {
        let shouldUppercase = true;
        return Array.from(string, (character) => {
            if (!/[a-z]/i.test(character))
                return character;
            let result = shouldUppercase ? character.toUpperCase() : character.toLowerCase();
            shouldUppercase = !shouldUppercase;
            return result;
        }).join("");
    }

    function escapeForRegularExpression(string)
    {
        return string.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    }

    InspectorTest.runSymbolicBreakpointPropertyWriteTest = function({operation, isRegex, caseSensitive})
    {
        let operationConfiguration = propertyWriteOperations[operation];
        InspectorTest.assert(operationConfiguration, `Unknown property-write operation '${operation}'.`);

        let matcherTestName = isRegex ? "Regex" : "Exact";
        let sensitivityTestName = caseSensitive ? "CaseSensitive" : "CaseInsensitive";
        let testCasePrefix = `PropertyWrite.${operationConfiguration.testName}.${matcherTestName}.${sensitivityTestName}`;

        function breakpointSymbol({miss = false} = {})
        {
            let symbol = operationConfiguration.propertyName;
            if (miss) {
                if (operationConfiguration.propertyName === "937463")
                    symbol = "937464";
                else if (caseSensitive)
                    symbol = alternateCase(symbol);
                else
                    symbol += "DoesNotExist";
            } else if (!caseSensitive)
                symbol = alternateCase(symbol);
            if (isRegex)
                symbol = `^${escapeForRegularExpression(symbol)}$`;
            return symbol;
        }

        function createBreakpoint({miss = false, disabled = false} = {})
        {
            return new WI.SymbolicBreakpoint(breakpointSymbol({miss}), {
                caseSensitive,
                isRegex,
                disabled,
                matchFunctionCalls: false,
                matchPropertyWrites: true,
            });
        }

        function triggerBreakpoint()
        {
            return Promise.all([
                InspectorTest.awaitEvent("TestPage-SymbolicBreakpointPropertyWrite"),
                InspectorTest.evaluateInPage(`triggerSymbolicBreakpointPropertyWrite(${JSON.stringify(operation)})`),
            ]);
        }

        async function triggerAndExpectNoPause(message)
        {
            let pauseCount = 0;
            let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, () => {
                ++pauseCount;
                WI.debuggerManager.resume();
            });

            await triggerBreakpoint();

            WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
            InspectorTest.expectEqual(pauseCount, 0, message);
        }

        let suite = InspectorTest.createAsyncSuite("SymbolicBreakpoint");

        suite.addTestCase({
            name: `SymbolicBreakpoint.${testCasePrefix}.Hit`,
            async test() {
                let breakpoint = createBreakpoint();
                WI.debuggerManager.addSymbolicBreakpoint(breakpoint);
                InspectorTest.assert(WI.debuggerManager.symbolicBreakpointsForSymbol(operationConfiguration.propertyName, {matchPropertyWrites: true})[0] === breakpoint, "Should match breakpoint.");

                let pausedPromise = WI.debuggerManager.awaitEvent(WI.DebuggerManager.Event.Paused);
                let evaluationPromise = triggerBreakpoint();
                await pausedPromise;

                let targetData = WI.debuggerManager.dataForTarget(WI.debuggerManager.activeCallFrame.target);
                InspectorTest.expectEqual(targetData.pauseReason, WI.DebuggerManager.PauseReason.PropertyWrite, "Pause reason should be property-write.");
                InspectorTest.expectEqual(targetData.pauseData.name, operationConfiguration.propertyName, "Pause data should contain the matching property name.");

                let {result, wasThrown} = await DebuggerAgent.evaluateOnCallFrame.invoke({
                    callFrameId: WI.debuggerManager.activeCallFrame.id,
                    expression: operationConfiguration.beforeExpression,
                    objectGroup: "test",
                    doNotPauseOnExceptionsAndMuteConsole: true,
                    returnByValue: true,
                });
                InspectorTest.expectFalse(wasThrown, "Evaluating state while paused should not throw.");
                InspectorTest.expectEqual(result.value, operationConfiguration.beforeValue, "Should expose the state from before the write.");

                await WI.debuggerManager.resume();
                await evaluationPromise;

                InspectorTest.expectEqual(await InspectorTest.evaluateInPage(operationConfiguration.afterExpression), operationConfiguration.afterValue, "Should perform the write after resuming.");

                breakpoint.remove();
                await triggerAndExpectNoPause("Should not pause after removing the breakpoint.");
            },
        });

        InspectorTest.BreakpointOptions.addTestCases(suite, {
            testCaseNamePrefix: testCasePrefix + ".Hit.",
            createBreakpoint() {
                let breakpoint = createBreakpoint();
                WI.debuggerManager.addSymbolicBreakpoint(breakpoint);
                return breakpoint;
            },
            triggerBreakpoint,
        });

        suite.addTestCase({
            name: `SymbolicBreakpoint.${testCasePrefix}.Miss`,
            async test() {
                let breakpoint = createBreakpoint({miss: true});
                WI.debuggerManager.addSymbolicBreakpoint(breakpoint);
                InspectorTest.assert(!WI.debuggerManager.symbolicBreakpointsForSymbol(operationConfiguration.propertyName, {matchPropertyWrites: true}).length, "Should not match breakpoint.");

                await triggerAndExpectNoPause("Should not pause for a nonmatching breakpoint.");
                breakpoint.remove();
                await triggerAndExpectNoPause("Should not pause after removing the nonmatching breakpoint.");
            },
        });

        suite.addTestCase({
            name: `SymbolicBreakpoint.${testCasePrefix}.Disabled`,
            async test() {
                let breakpoint = createBreakpoint({disabled: true});
                WI.debuggerManager.addSymbolicBreakpoint(breakpoint);
                InspectorTest.assert(!WI.debuggerManager.symbolicBreakpointsForSymbol(operationConfiguration.propertyName, {matchPropertyWrites: true}).length, "Should not match a disabled breakpoint.");

                await triggerAndExpectNoPause("Should not pause while the breakpoint is disabled.");
                breakpoint.remove();
                await triggerAndExpectNoPause("Should not pause after removing the disabled breakpoint.");
            },
        });

        suite.runTestCasesAndFinish();
    };
});

