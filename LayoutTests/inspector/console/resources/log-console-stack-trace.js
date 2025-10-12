TestPage.registerInitializer(() => {
    window.logConsoleMessageStackTrace = function(consoleMessage) {
        let stackTrace = consoleMessage.stackTrace;
        let foundAsyncBoundary = false;
        let callFrameIndex = 0;

        function logCallFrame(callFrame, isAsyncBoundary) {
            let label = callFrame.functionName || "(anonymous function)";
            if (isAsyncBoundary)
                InspectorTest.log(`${callFrameIndex}: --- ${label} ---`);
            else {
                let code = callFrame.nativeCode ? "N" : (callFrame.programCode ? "P" : "F");
                InspectorTest.log(`${callFrameIndex}: [${code}] ${label}`);
            }
            callFrameIndex++;
        }

        InspectorTest.log("CALL STACK:");

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
                callFrameIndex = 0;
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
    }
});
