TestPage.registerInitializer(function() {

// Poll until the expression evaluates to a truthy value in the target's RuntimeAgent.
// Useful for waiting on page content (scripts, DOM state) to become available.
window.waitForExpressionInTarget = async function waitForExpressionInTarget(target, expression, maxAttempts = 20) {
    for (let i = 0; i < maxAttempts; i++) {
        try {
            let response = await target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test", returnByValue: true});
            if (response.result.value)
                return response.result.value;
        } catch (e) {
            // Only suppress protocol errors that may indicate the backend
            // connection isn't fully established. Re-throw programming errors.
            if (e instanceof TypeError || e instanceof SyntaxError || e instanceof ReferenceError)
                throw e;
        }
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
    return undefined;
};

// Evaluate document.location.href in each frame target and return a Map of {url => target}.
// Useful for identifying which frame target corresponds to a particular nested frame.
window.fetchDocumentURLsForTargets = async function fetchDocumentURLsForTargets(targets) {
    let entries = await Promise.all(targets.map(async function (target) {
        let { result } = await target.RuntimeAgent.evaluate.invoke({
            expression: "document.location.href",
            objectGroup: "test",
            returnByValue: true,
        });
        return [result.value, target];
    }));
    return new Map(entries);
};

});
