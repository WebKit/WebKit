TestPage.registerInitializer(() => {
    InspectorTest.EventBreakpoint = {};

    InspectorTest.EventBreakpoint.addBreakpointOptionsTestCases = function(suite, type, eventName) {
        async function triggerBreakpoint() {
            InspectorTest.log("Triggering breakpoint...");
            return Promise.all([
                InspectorTest.awaitEvent("TestPage-" + eventName),
                InspectorTest.evaluateInPage(`trigger_${eventName}()`),
            ]);
        }

        suite.addTestCase({
            name: suite.name + ".Options.Condition",
            description: "Check that the debugger will not pause unless the breakpoint has a truthy breakpoint condition.",
            async test() {
                let pauseCount = 0;

                let listener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                let breakpoint = await InspectorTest.EventBreakpoint.createBreakpoint(type, {eventName});

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

                    await triggerBreakpoint();

                    if (i <= 2)
                        InspectorTest.expectEqual(pauseCount, 0, "Should not pause.");
                    else
                        InspectorTest.expectEqual(pauseCount, i - 2, "Should pause.");
                }

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, listener);
            },
            teardown: InspectorTest.EventBreakpoint.teardown,
        });

        suite.addTestCase({
            name: suite.name + ".Options.IgnoreCount",
            description: "Check that the debugger will not pause unless the breakpoint is hit at least as many times as it's `ignoreCount`.",
            async test() {
                let pauseCount = 0;

                let listener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                let breakpoint = await InspectorTest.EventBreakpoint.createBreakpoint(type, {eventName});

                InspectorTest.newline();

                InspectorTest.log("Setting ignoreCount to '2'...");
                breakpoint.ignoreCount = 2;

                for (let i = 1; i <=4; ++i) {
                    InspectorTest.newline();

                    await triggerBreakpoint();

                    if (i <= 2)
                        InspectorTest.expectEqual(pauseCount, 0, "Should not pause.");
                    else
                        InspectorTest.expectEqual(pauseCount, i - 2, "Should pause.");
                }

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, listener);
            },
            teardown: InspectorTest.EventBreakpoint.teardown,
        });

        suite.addTestCase({
            name: suite.name + ".Options.Action.Log",
            description: "Check that log breakpoint actions execute when the breakpoint is hit.",
            async test() {
                let pauseCount = 0;

                let listener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                let breakpoint = await InspectorTest.EventBreakpoint.createBreakpoint(type, {eventName});

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

                    let [messageAddedEvent] = await Promise.all([
                        WI.consoleManager.awaitEvent(WI.ConsoleManager.Event.MessageAdded),
                        triggerBreakpoint(),
                    ]);

                    InspectorTest.expectEqual(messageAddedEvent.data.message.messageText, action.data, "Should execute breakpoint action.");

                    if (i <= 2)
                        InspectorTest.expectEqual(pauseCount, i, "Should pause.");
                    else
                        InspectorTest.expectEqual(pauseCount, 2, "Should not pause.");
                }

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, listener);
            },
            teardown: InspectorTest.EventBreakpoint.teardown,
        });

        suite.addTestCase({
            name: suite.name + ".Options.Actions.Evaluate",
            description: "Check that evaluate breakpoint actions execute when the breakpoint is hit.",
            async test() {
                let pauseCount = 0;

                let listener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                    ++pauseCount;
                    WI.debuggerManager.resume();
                });

                let breakpoint = await InspectorTest.EventBreakpoint.createBreakpoint(type, {eventName});

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

                    await triggerBreakpoint();

                    let breakpointActionEvaluateResult = await InspectorTest.evaluateInPage(`window.BREAKPOINT_ACTION_EVALUATE`);
                    InspectorTest.expectEqual(breakpointActionEvaluateResult, i, "Should execute breakpoint action.");

                    if (i <= 2)
                        InspectorTest.expectEqual(pauseCount, i, "Should pause.");
                    else
                        InspectorTest.expectEqual(pauseCount, 2, "Should not pause.");
                }

                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, listener);
            },
            teardown: InspectorTest.EventBreakpoint.teardown,
        });
    };

    InspectorTest.EventBreakpoint.teardown = function(resolve, reject) {
        WI.domDebuggerManager.allAnimationFramesBreakpoint?.remove();
        WI.domDebuggerManager.allIntervalsBreakpoint?.remove();
        WI.domDebuggerManager.allListenersBreakpoint?.remove();
        WI.domDebuggerManager.allTimeoutsBreakpoint?.remove();

        for (let breakpoint of WI.domDebuggerManager.listenerBreakpoints)
            breakpoint.remove();

        resolve();
    };

    InspectorTest.EventBreakpoint.failOnPause = function(resolve, reject, pauseReason, eventName, message) {
        let paused = false;

        let listener = WI.debuggerManager.singleFireEventListener(WI.DebuggerManager.Event.Paused, (event) => {
            paused = true;

            let targetData = WI.debuggerManager.dataForTarget(WI.debuggerManager.activeCallFrame.target);
            InspectorTest.assert(targetData.pauseReason === pauseReason, `Pause reason should be "${pauseReason}".`);
            if (targetData.pauseData.eventName)
                InspectorTest.assert(targetData.pauseData.eventName === eventName, `Pause data eventName should be "${eventName}".`);

            InspectorTest.fail(message);
            logActiveStackTrace();

            WI.debuggerManager.resume()
            .catch(reject);
        });

        InspectorTest.singleFireEventListener("TestPage-" + eventName, (event) => {
            if (!paused) {
                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, listener);

                InspectorTest.pass(message);
            }

            resolve();
        });
    };

    InspectorTest.EventBreakpoint.createBreakpoint = function(type, {eventName} = {}) {
        if (type !== WI.EventBreakpoint.Type.Listener)
            eventName = null;

        InspectorTest.log(`Creating "${eventName || type}" Event Breakpoint...`);
        return InspectorTest.EventBreakpoint.addBreakpoint(new WI.EventBreakpoint(type, {eventName}));
    };

    InspectorTest.EventBreakpoint.addBreakpoint = function(breakpoint) {
        InspectorTest.log(`Adding "${breakpoint.type + (breakpoint.eventName ? ":" + breakpoint.eventName : "")}" Event Breakpoint...`);

        breakpoint.disabled = false;
        return new Promise((resolve, reject) => {
            WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.EventBreakpointAdded)
            .then((event) => {
                InspectorTest.assert(event.data.breakpoint === breakpoint, "Added Breakpoint should be expected object.");
                InspectorTest.assert(!event.data.breakpoint.disabled, "Breakpoint should not be disabled initially.");
                resolve(breakpoint);
            });

            WI.domDebuggerManager.addEventBreakpoint(breakpoint);
        });
    };

    InspectorTest.EventBreakpoint.removeBreakpoint = function(breakpoint) {
        InspectorTest.log(`Removing "${breakpoint.type + (breakpoint.eventName ? ":" + breakpoint.eventName : "")}" Event Breakpoint...`);

        return new Promise((resolve, reject) => {
            WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.EventBreakpointRemoved)
            .then((event) => {
                InspectorTest.assert(event.data.breakpoint === breakpoint, "Removed Breakpoint should be expected object.");
                InspectorTest.assert(!WI.domDebuggerManager.listenerBreakpoints.includes(breakpoint), "Breakpoint should not be in the list of breakpoints.");
                resolve(breakpoint);
            });

            breakpoint.remove();
        });
    };

    InspectorTest.EventBreakpoint.disableBreakpoint = function(breakpoint) {
        InspectorTest.log(`Disabling "${breakpoint.type + (breakpoint.eventName ? ":" + breakpoint.eventName : "")}" Event Breakpoint...`);

        breakpoint.disabled = true;
        return breakpoint;
    };

    InspectorTest.EventBreakpoint.awaitEvent = function(context, eventName) {
        return function() {
            InspectorTest.log(`Firing "${eventName}" on ${context}...`);
            return InspectorTest.evaluateInPage(`trigger_${eventName}()`);
        };
    };
});
