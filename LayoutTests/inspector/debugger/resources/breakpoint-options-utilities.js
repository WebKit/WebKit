TestPage.registerInitializer(() => {
    InspectorTest.BreakpointOptions = {};

    InspectorTest.BreakpointOptions.addTestCases = function(suite, {testCaseNamePrefix, createBreakpoint, triggerBreakpoint}) {
        testCaseNamePrefix ??= "";

        function removeBreakpoint(breakpoint) {
            if (breakpoint.removable)
                breakpoint.remove();
            else {
                breakpoint.disabled = true;
                breakpoint.reset();
            }
        }

        suite.addTestCase({
            name: suite.name + "." + testCaseNamePrefix + "Options.Condition",
            description: "Check that the debugger will not pause unless the breakpoint has a truthy breakpoint condition.",
            async test() {
                let pauseCount = 0;

                let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                let breakpoint = await createBreakpoint();

                InspectorTest.newline();

                InspectorTest.log("Setting condition to 'false'...");
                breakpoint.condition = "false";

                for (let i = 1; i <= 4; ++i) {
                    if (i === 3) {
                        InspectorTest.newline();

                        InspectorTest.log("Setting condition to 'true'...");
                        breakpoint.condition = "true";
                    }

                    InspectorTest.newline();

                    InspectorTest.log("Triggering breakpoint...");
                    await triggerBreakpoint();

                    if (i <= 2)
                        InspectorTest.expectEqual(pauseCount, 0, "Should not pause.");
                    else
                        InspectorTest.expectEqual(pauseCount, i - 2, "Should pause.");
                }

                removeBreakpoint(breakpoint);

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
            },
        });

        suite.addTestCase({
            name: suite.name + "." + testCaseNamePrefix + "Options.IgnoreCount",
            description: "Check that the debugger will not pause unless the breakpoint is hit at least as many times as it's `ignoreCount`.",
            async test() {
                let pauseCount = 0;

                let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                let breakpoint = await createBreakpoint();

                InspectorTest.newline();

                InspectorTest.log("Setting ignoreCount to '2'...");
                breakpoint.ignoreCount = 2;

                for (let i = 1; i <=4; ++i) {
                    InspectorTest.newline();

                    InspectorTest.log("Triggering breakpoint...");
                    await triggerBreakpoint();

                    if (i <= 2)
                        InspectorTest.expectEqual(pauseCount, 0, "Should not pause.");
                    else
                        InspectorTest.expectEqual(pauseCount, i - 2, "Should pause.");
                }

                removeBreakpoint(breakpoint);

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
            },
        });

        suite.addTestCase({
            name: suite.name + "." + testCaseNamePrefix + "Options.Action.Log",
            description: "Check that log breakpoint actions execute when the breakpoint is hit.",
            async test() {
                let pauseCount = 0;

                let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                let breakpoint = await createBreakpoint();

                InspectorTest.newline();

                InspectorTest.log("Adding log action...");
                let action = new WI.BreakpointAction(WI.BreakpointAction.Type.Log, {data: "BREAKPOINT ACTION LOG 1"});
                breakpoint.addAction(action);

                for (let i = 1; i <= 4; ++i) {
                    if (i > 1) {
                        InspectorTest.newline();

                        InspectorTest.log("Editing log action...");
                        action.data = `BREAKPOINT ACTION LOG ${i}`;

                        if (i === 3) {
                            InspectorTest.log("Enabling auto-continue...");
                            breakpoint.autoContinue = true;
                        }
                    }

                    InspectorTest.newline();

                    let messages = [];

                    let messageAddedListener = WI.consoleManager.addEventListener(WI.ConsoleManager.Event.MessageAdded, (event) => {
                        messages.push(event.data.message);
                    });

                    InspectorTest.log("Triggering breakpoint...");
                    await triggerBreakpoint();

                    WI.consoleManager.removeEventListener(WI.ConsoleManager.Event.MessageAdded, messageAddedListener);

                    InspectorTest.expectThat(messages.some((message) => message.messageText === action.data), "Should execute breakpoint action.");

                    if (i <= 2)
                        InspectorTest.expectEqual(pauseCount, i, "Should pause.");
                    else
                        InspectorTest.expectEqual(pauseCount, 2, "Should not pause.");
                }

                removeBreakpoint(breakpoint);

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
            },
        });

        suite.addTestCase({
            name: suite.name + "." + testCaseNamePrefix + "Options.Actions.Evaluate",
            description: "Check that evaluate breakpoint actions execute when the breakpoint is hit.",
            async test() {
                let pauseCount = 0;

                let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                let breakpoint = await createBreakpoint();

                InspectorTest.newline();

                InspectorTest.log("Adding evaluate action...");
                let action = new WI.BreakpointAction(WI.BreakpointAction.Type.Evaluate, {data: "window.BREAKPOINT_ACTION_EVALUATE = 1;"});
                breakpoint.addAction(action);

                for (let i = 1; i <= 4; ++i) {
                    if (i > 1) {
                        InspectorTest.newline();

                        InspectorTest.log("Editing evaluate action...");
                        action.data = `window.BREAKPOINT_ACTION_EVALUATE = ${i};`;

                        if (i === 3) {
                            InspectorTest.log("Enabling auto-continue...");
                            breakpoint.autoContinue = true;
                        }
                    }

                    InspectorTest.newline();

                    InspectorTest.log("Triggering breakpoint...");
                    await triggerBreakpoint();

                    let breakpointActionEvaluateResult = await InspectorTest.evaluateInPage(`window.BREAKPOINT_ACTION_EVALUATE`);
                    InspectorTest.expectEqual(breakpointActionEvaluateResult, i, "Should execute breakpoint action.");

                    if (i <= 2)
                        InspectorTest.expectEqual(pauseCount, i, "Should pause.");
                    else
                        InspectorTest.expectEqual(pauseCount, 2, "Should not pause.");
                }

                removeBreakpoint(breakpoint);

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
            },
        });
    };
});
