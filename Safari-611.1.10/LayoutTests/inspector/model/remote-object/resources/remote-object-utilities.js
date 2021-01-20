function runInBrowserTest() {
    if (window.testRunner)
        return;
 
    test(); // Populate window.steps.

    for (let step of steps) {
        console.info("EXPRESSION", step.expression);
        try {
            console.log(eval(step.expression));
        } catch (e) {
            console.log("EXCEPTION: " + e);
        }
    }
}

TestPage.registerInitializer(() => {
    function remoteObjectJSONFilter(filter, key, value) {
        if (key === "_target" || key === "_hasChildren" || key === "_listeners")
            return undefined;
        if (key === "_objectId" || key === "_stackTrace")
            return "<filtered>";
        if (typeof value === "bigint")
            return "<filtered " + String(value) + "n>";
        if (filter && filter(key, value))
            return "<filtered>";
        return value;
    }

    window.runSteps = function(steps) {
        let currentStepIndex = 0;

        function checkComplete() {
            if (++currentStepIndex >= steps.length)
                InspectorTest.completeTest();
        }

        for (let {expression, browserOnly, filter} of steps) {
            if (browserOnly) {
                checkComplete();
                continue;
            }

            filter = remoteObjectJSONFilter.bind(null, filter);

            WI.runtimeManager.evaluateInInspectedWindow(expression, {objectGroup: "test", doNotPauseOnExceptionsAndMuteConsole: true, generatePreview: true}, (remoteObject, wasThrown) => {
                InspectorTest.log("");
                InspectorTest.log("-----------------------------------------------------");
                InspectorTest.log("EXPRESSION: " + expression);
                InspectorTest.assert(remoteObject instanceof WI.RemoteObject);
                InspectorTest.log(JSON.stringify(remoteObject, filter, 2));
                checkComplete();
            });
        }
    };
});
