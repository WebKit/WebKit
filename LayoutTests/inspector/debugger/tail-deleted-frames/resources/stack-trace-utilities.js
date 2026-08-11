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

    window.logAsyncStackTrace = async function(options = {}) {
        let foundAsyncBoundary = false;
        let callFrameIndex = 0;

        async function logThisObject(callFrame) {
            if (callFrame.thisObject.canLoadPreview()) {
                await new Promise((resolve) => { callFrame.thisObject.updatePreview(resolve); });
                let preview = callFrame.thisObject.preview.propertyPreviews[0];
                InspectorTest.log(`  this: ${preview.name} - ${preview.value}`);
            } else
                InspectorTest.log(`  this: undefined`);
        }

        async function logScope(callFrame, scopeChainIndex) {
            InspectorTest.log(`  ----Scope----`);
            return new Promise((resolve, reject) => {
                let scope = callFrame.scopeChain[scopeChainIndex];
                if (!scope) {
                    resolve();
                    return;
                }
                scope.objects[0].getPropertyDescriptors((properties) => {
                    for (let propertyDescriptor of properties)
                        InspectorTest.log(`  ${propertyDescriptor.name}: ${propertyDescriptor.value.description}`);
                    if (!properties.length)
                        InspectorTest.log(`  --  empty  --`);
                    InspectorTest.log(`  -------------`);
                    resolve();
                });
            });
        }

        async function logCallFrame(callFrame, isAsyncBoundary) {
            let label = callFrame.functionName;
            if (isAsyncBoundary)
                InspectorTest.log(`${callFrameIndex}: --- ${label} ---`);
            else {
                let code = callFrame.nativeCode ? "N" : (callFrame.programCode ? "P" : "F");
                let icon = callFrame.isTailDeleted ? `-${code}-` : `[${code}]`;
                InspectorTest.log(`${callFrameIndex}: ${icon} ${label}`);
                if ("includeThisObject" in options)
                    await logThisObject(callFrame);
                if ("includeScope" in options)
                    await logScope(callFrame, options.includeScope);
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
                await logCallFrame(callFrames[0] || "(async)", true);
            }

            for (let i = topCallFrameIsBoundary ? 1 : 0; i < callFrames.length; ++i) {
                let callFrame = callFrames[i];
                await logCallFrame(callFrame);

                // Skip call frames after the test harness entry point.
                if (callFrame.programCode)
                    break;
            }

            if (truncated)
                InspectorTest.log("(remaining call frames truncated)");
        }
    };
});
