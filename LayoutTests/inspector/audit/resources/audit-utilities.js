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

        InspectorTest.assert(array.every((item) => typeof item === "string"), name + " should only contain strings.");
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
                        if (result.resolvedDOMNodes) {
                            InspectorTest.assert(result.resolvedDOMNodes.every((domNode) => domNode instanceof WI.DOMNode), "domNodes should only contain WI.DOMNode.");
                            logArray("domNodes", result.resolvedDOMNodes.map((domNode) => domNode.displayName));

                            for (let i = 0; i < result.resolvedDOMNodes.length; ++i)
                                InspectorTest.assert(WI.cssPath(result.resolvedDOMNodes[i], {full: true}) === data.domNodes[i], "The resolved DOM node should match the saved CSS path.");
                        } else
                            logArray("domNodes", data.domNodes);
                    }
                    if (data.domAttributes)
                        logArray("domAttributes", data.domAttributes);
                    if (data.errors)
                        logArray("errors", data.errors);
                });

                InspectorTest.log("Testing" + (logs.beforeStart || "") + "...");

                WI.auditManager.start([audit])
                .then(resolve, reject);
            },
        });
    };

    InspectorTest.Audit.addFunctionlessTest = function(name, test, level, options = {}) {
        InspectorTest.Audit.addTest(name, (options.async ? "async " : "") + `function() { return ${test} }`, level, {
            beforeStart: ` value \`${test}\``,
        });
    };

    InspectorTest.Audit.addStringTest = function(name, test, level, options = {}) {
        InspectorTest.Audit.addFunctionlessTest(name, `"${test}"`, level, options);
    };

    InspectorTest.Audit.addObjectTest = function(name, test, level, options = {}) {
        InspectorTest.Audit.addFunctionlessTest(name, JSON.stringify(test), level, options);
    };

    InspectorTest.Audit.addPromiseTest = function(name, test, level, options = {}) {
        InspectorTest.Audit.addFunctionlessTest(name, `new Promise((resolve, reject) => ${test})`, level, options);
    };

    InspectorTest.Audit.addDOMSelectorTest = function(name, test, level) {
        InspectorTest.Audit.addTest(name, querySelectorTest.format(test), level, {
            beforeStart: ` selector \`${test}\``,
        });
    };
});
