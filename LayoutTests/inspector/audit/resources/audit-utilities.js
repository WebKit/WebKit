TestPage.registerInitializer(() => {
    const querySelectorTest = `function() {
    let domNodes = Array.from(document.querySelectorAll("%s"));
    return {
        level: domNodes.length ? "fail" : "pass",
        domNodes,
    };
}`;

    let suite = null;

    function logArray(name, array) {
        if (!array.length)
            return;

        InspectorTest.assert(array.every((item) => typeof item === "string"), name + "should only contain strings.");
        InspectorTest.log("  " + name + ":");
        for (let item of array)
            InspectorTest.log("   - " + item);
    }

    InspectorTest.Audit = {};

    InspectorTest.Audit.createSuite = function(name) {
        suite = InspectorTest.createAsyncSuite(name);
        return suite;
    }

    InspectorTest.Audit.addTest = function(name, test, level, logs = {}) {
        suite.addTestCase({
            name,
            test(resolve, reject) {
                let audit = new WI.AuditTestCase(name, test);

                WI.auditManager.awaitEvent(WI.AuditManager.Event.TestCompleted).then((event) => {
                    let results = WI.auditManager.results.lastValue;
                    InspectorTest.assert(results.length === 1, "There should be 1 result.");

                    let result = results[0];
                    InspectorTest.assert(result instanceof WI.AuditTestCaseResult, "Result should be a WI.AuditTestCaseResult.");
                    if (!result)
                        return;

                    InspectorTest.expectEqual(result.level, level, `Result should be "${level}".`);

                    let data = result.data;
                    if (data.domNodes) {
                        InspectorTest.assert(data.domNodes.every((domNode) => domNode instanceof WI.DOMNode), "domNodes should only contain WI.DOMNode.");
                        logArray("domNodes", data.domNodes.map((domNode) => domNode.displayName));
                    }
                    if (data.domAttributes)
                        logArray("domAttributes", data.domAttributes);
                    if (data.errors)
                        logArray("errors", data.errors);
                })
                .then(resolve, reject);

                InspectorTest.log("Testing" + (logs.beforeStart || "") + "...");

                WI.auditManager.start([audit])
                .then(resolve, reject);
            },
        });
    };

    InspectorTest.Audit.addFunctionlessTest = function(name, test, level) {
        InspectorTest.Audit.addTest(name, `function() { return ${test} }`, level, {
            beforeStart: ` value \`${test}\``,
        });
    };

    InspectorTest.Audit.addStringTest = function(name, test, level) {
        InspectorTest.Audit.addFunctionlessTest(name, `"${test}"`, level);
    };

    InspectorTest.Audit.addObjectTest = function(name, test, level) {
        InspectorTest.Audit.addFunctionlessTest(name, JSON.stringify(test), level);
    };

    InspectorTest.Audit.addDOMSelectorTest = function(name, test, level) {
        InspectorTest.Audit.addTest(name, querySelectorTest.format(test), level, {
            beforeStart: ` selector \`${test}\``,
        });
    };
});
