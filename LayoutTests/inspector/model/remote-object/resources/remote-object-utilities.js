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
        function runStep(i) {
            if (i >= steps.length) {
                InspectorTest.completeTest();
                return;
            }

            let {expression, browserOnly, filter, deep} = steps[i];

            if (browserOnly) {
                runStep(i + 1);
                return;
            }

            filter = remoteObjectJSONFilter.bind(null, filter);

            WI.runtimeManager.evaluateInInspectedWindow(expression, {objectGroup: "test", doNotPauseOnExceptionsAndMuteConsole: true, generatePreview: true}, (remoteObject, wasThrown) => {
                InspectorTest.log("");
                InspectorTest.log("-----------------------------------------------------");
                InspectorTest.log("EXPRESSION: " + expression);
                InspectorTest.assert(remoteObject instanceof WI.RemoteObject);
                InspectorTest.log(JSON.stringify(remoteObject, filter, 2));

                if (deep) {
                    remoteObject.getPropertyDescriptors((properties) => {
                        for (let property of properties) {
                            if (deep.includes(property.name))
                                InspectorTest.log(JSON.stringify(property, filter, 2));
                        }

                        runStep(i + 1);
                    }, {generatePreview: true});
                    return;
                }

                runStep(i + 1);
            });
        }
        runStep(0);
    };
});
