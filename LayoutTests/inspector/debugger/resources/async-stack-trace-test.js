TestPage.registerInitializer(() => {
    window.getAsyncStackTrace = function() {
        InspectorTest.assert(WI.debuggerManager.activeCallFrame, "Active call frame should exist.");
        if (!WI.debuggerManager.activeCallFrame)
            return null;

        let targetData = WI.debuggerManager.dataForTarget(WI.debuggerManager.activeCallFrame.target);
        InspectorTest.assert(targetData, "Data for active call frame target should exist.");
        if (!targetData)
            return null;

        return targetData.stackTrace;
    };

    window.logAsyncStackTrace = function() {
        let foundAsyncBoundary = false;
        let callFrameIndex = 0;

        function logCallFrame(callFrame, isAsyncBoundary) {
            let label = callFrame.functionName;
            if (isAsyncBoundary)
                InspectorTest.log(`${callFrameIndex}: --- ${label} ---`);
            else {
                let code = callFrame.nativeCode ? "N" : (callFrame.programCode ? "P" : "F");
                InspectorTest.log(`${callFrameIndex}: [${code}] ${label}`);
            }
            callFrameIndex++;
        }

        InspectorTest.log("CALL STACK:");

        let stackTrace = getAsyncStackTrace();
        while (stackTrace) {
            let callFrames = stackTrace.callFrames;
            let topCallFrameIsBoundary = stackTrace.topCallFrameIsBoundary;
            let truncated = stackTrace.truncated;
            stackTrace = stackTrace.parentStackTrace;
            if (!callFrames) {
                InspectorTest.log("EMPTY CALL STACK:");
                continue;
            }

            if (topCallFrameIsBoundary) {
                if (!foundAsyncBoundary) {
                    InspectorTest.log("ASYNC CALL STACK:");
                    foundAsyncBoundary = true;
                }
                logCallFrame(callFrames[0] || "(async)", true);
            }

            for (let i = topCallFrameIsBoundary ? 1 : 0; i < callFrames.length; ++i) {
                let callFrame = callFrames[i];
                logCallFrame(callFrame);

                // Skip call frames after the test harness entry point.
                if (callFrame.programCode)
                    break;
            }

            if (truncated)
                InspectorTest.log("(remaining call frames truncated)");
        }
    };

    window.addAsyncStackTraceTestCase = function(suite, {name, description, expression, pausedHandler, setup, teardown}) {
        suite.addTestCase({
            name,
            description,
            setup,
            teardown,
            test(resolve, reject) {
                WI.debuggerManager.singleFireEventListener(WI.DebuggerManager.Event.Paused, () => {
                    InspectorTest.log("PAUSED");
                    logAsyncStackTrace();

                    if (pausedHandler)
                        pausedHandler(getAsyncStackTrace());

                    WI.debuggerManager.singleFireEventListener(WI.DebuggerManager.Event.Resumed, () => {
                        resolve();
                    });

                    WI.debuggerManager.resume();
                });

                InspectorTest.evaluateInPage(expression);
            }
        });
    };
});
