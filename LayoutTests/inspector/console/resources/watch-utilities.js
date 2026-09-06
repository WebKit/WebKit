TestPage.registerInitializer(() => {
    InspectorTest.WatchObject = {};

    InspectorTest.WatchObject.evaluateWithCommandLineAPI = function(expression) {
        return promisify((callback) => {
            WI.runtimeManager.evaluateInInspectedWindow(expression, {objectGroup: "test", includeCommandLineAPI: true}, callback);
        });
    };

    InspectorTest.WatchObject.watchObject = async function(expression) {
        let [remoteObject, wasThrown] = await InspectorTest.WatchObject.evaluateWithCommandLineAPI(`watch(${expression})`);
        if (wasThrown) {
            InspectorTest.fail(`Watching ${expression} should not throw.`);
            InspectorTest.log(remoteObject.description);
            throw new Error("Unable to continue without watching the object.");
        }
    };

    InspectorTest.WatchObject.unwatchObject = async function(expression) {
        let [remoteObject, wasThrown] = await InspectorTest.WatchObject.evaluateWithCommandLineAPI(`unwatch(${expression})`);
        if (wasThrown) {
            InspectorTest.fail(`Unwatching ${expression} should not throw.`);
            InspectorTest.log(remoteObject.description);
            throw new Error("Unable to continue without unwatching the object.");
        }
    };

    InspectorTest.WatchObject.ensureDFGCompiled = async function(functionExpression, invocationExpression) {
        await InspectorTest.evaluateInPage(`testRunner.neverInlineFunction(${functionExpression})`);

        for (let i = 0; i < 100; ++i) {
            let numberOfDFGCompiles = await InspectorTest.evaluateInPage(`(() => {
                for (let j = 0; j < 1000; ++j)
                    ${invocationExpression};
                return testRunner.numberOfDFGCompiles(${functionExpression});
            })()`);
            if (numberOfDFGCompiles)
                return numberOfDFGCompiles;
            await new Promise((resolve) => setTimeout(resolve, 10));
        }

        return 0;
    };

    InspectorTest.WatchObject.expectPause = async function({expression, beforeExpression, whilePausedExpression, afterExpression, description, includeCommandLineAPI}) {
        let pausedListener;
        let pausePromise = new Promise((resolve) => {
            pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, resolve);
        });

        let evaluationPromise = includeCommandLineAPI
            ? InspectorTest.WatchObject.evaluateWithCommandLineAPI(expression)
            : InspectorTest.evaluateInPage(expression);
        let didPause = await Promise.race([
            pausePromise.then(() => true),
            evaluationPromise.then(() => false),
        ]);

        WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);

        if (!didPause) {
            InspectorTest.fail(`Should pause before ${description}.`);
            return;
        }

        let callFrame = WI.debuggerManager.activeCallFrame;
        let targetData = WI.debuggerManager.dataForTarget(callFrame.target);
        InspectorTest.expectEqual(targetData.pauseReason, WI.DebuggerManager.PauseReason.WatchedObject, "Pause reason should be WatchedObject.");
        let {result, wasThrown} = await DebuggerAgent.evaluateOnCallFrame.invoke({
            callFrameId: callFrame.id,
            expression: beforeExpression,
            objectGroup: "test",
            doNotPauseOnExceptionsAndMuteConsole: true,
            returnByValue: true,
        });
        InspectorTest.expectTrue(!wasThrown && result.value, `Should pause before ${description}.`);

        if (whilePausedExpression) {
            let {wasThrown} = await DebuggerAgent.evaluateOnCallFrame.invoke({
                callFrameId: callFrame.id,
                expression: whilePausedExpression,
                objectGroup: "test",
                includeCommandLineAPI: true,
                doNotPauseOnExceptionsAndMuteConsole: true,
                returnByValue: true,
            });
            InspectorTest.expectFalse(wasThrown, "Should run the requested expression while paused.");
        }

        let additionalPauseHandlers = [];
        let additionalPausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, () => {
            additionalPauseHandlers.push(WI.debuggerManager.resume());
        });

        try {
            await WI.debuggerManager.resume();
            await evaluationPromise;
            await Promise.all(additionalPauseHandlers);
        } finally {
            WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, additionalPausedListener);
        }

        if (!await InspectorTest.evaluateInPage(afterExpression))
            InspectorTest.fail(`Should complete ${description} after resuming.`);
    };

    InspectorTest.WatchObject.expectNoPause = async function(expression, description) {
        let paused = false;
        let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, () => {
            paused = true;
            WI.debuggerManager.resume();
        });

        await InspectorTest.evaluateInPage(expression);

        WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
        InspectorTest.expectFalse(paused, description);
    };
});
