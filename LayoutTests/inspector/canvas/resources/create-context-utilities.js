window.contexts = [];

function createAttachedCanvas(contextType) {
    let canvas = document.body.appendChild(document.createElement("canvas"));
    let context = canvas.getContext(contextType);
    if (!context)
        TestPage.addResult("FAIL: missing context for type " + contextType);
    window.contexts.push(context);
}

function createDetachedCanvas(contextType) {
    let context = document.createElement("canvas").getContext(contextType);
    if (!context)
        TestPage.addResult("FAIL: missing context for type " + contextType);
    window.contexts.push(context);
}

function createCSSCanvas(contextType, canvasName) {
    let context = document.getCSSCanvasContext(contextType, canvasName, 10, 10);
    if (!context)
        TestPage.addResult("FAIL: missing context for type " + contextType);
    window.contexts.push();
}

function destroyCanvases() {
    for (let context of window.contexts) {
        if (!context)
            continue;

        let canvasElement = context.canvas;
        if (canvasElement && canvasElement.parentNode)
            canvasElement.remove();
    }

    window.contexts = [];

    // Force GC to make sure the canvas element is destroyed, otherwise the frontend
    // does not receive WI.CanvasManager.Event.CanvasRemoved events.
    setTimeout(() => { GCController.collect(); }, 0);
}

TestPage.registerInitializer(() => {
    let suite = null;

    function awaitCanvasAdded(contextType) {
        return WI.canvasManager.awaitEvent(WI.CanvasManager.Event.CanvasAdded)
        .then((event) => {
            let canvas = event.data.canvas;
            let contextDisplayName = WI.Canvas.displayNameForContextType(contextType);
            InspectorTest.expectEqual(canvas.contextType, contextType, `Canvas context should be ${contextDisplayName}.`);

            for (let i = 0; i < canvas.backtrace.length; ++i) {
                let callFrame = canvas.backtrace[i];
                let traceText = `  ${i}: `;
                traceText += callFrame.functionName || "(anonymous function)";

                if (callFrame.nativeCode)
                    traceText += " - [native code]";
                else if (callFrame.programCode)
                    traceText += " - [program code]";
                else if (callFrame.sourceCodeLocation) {
                    let location = callFrame.sourceCodeLocation;
                    traceText += " - " + sanitizeURL(location.sourceCode.url) + `:${location.lineNumber}:${location.columnNumber}`;
                }

                InspectorTest.log(traceText);
            }

            InspectorTest.log("");

            return canvas;
        });
    }

    function awaitCanvasRemoved(canvasIdentifier) {
        return WI.canvasManager.awaitEvent(WI.CanvasManager.Event.CanvasRemoved)
        .then((event) => {
            let canvas = event.data.canvas;
            InspectorTest.expectEqual(canvas.identifier, canvasIdentifier, "Removed canvas has expected ID.");
        });
    }

    InspectorTest.CreateContextUtilities = {};

    InspectorTest.CreateContextUtilities.initializeTestSuite = function(name) {
        suite = InspectorTest.createAsyncSuite(name);

        suite.addTestCase({
            name: `${suite.name}.NoCanvases`,
            description: "Check that the CanvasManager has no canvases initially.",
            test(resolve, reject) {
                InspectorTest.expectEqual(WI.canvasManager.canvases.length, 0, "CanvasManager should have no canvases.");
                resolve();
            }
        });

        return suite;
    };

    InspectorTest.CreateContextUtilities.addSimpleTestCase = function({name, description, expression, contextType}) {
        suite.addTestCase({
            name: suite.name + "." + name,
            description,
            test(resolve, reject) {
                awaitCanvasAdded(contextType)
                .then((canvas) => {
                    if (canvas.cssCanvasName) {
                        InspectorTest.log("CSS canvas will not be destroyed");
                        resolve();
                        return;
                    }

                    let promise = awaitCanvasRemoved(canvas.identifier);
                    InspectorTest.evaluateInPage(`destroyCanvases()`);
                    return promise;
                })
                .then(resolve, reject);

                InspectorTest.evaluateInPage(expression);
            },
        });
    };

    let previousCSSCanvasContextType = null;
    InspectorTest.CreateContextUtilities.addCSSCanvasTestCase = function(contextType) {
        InspectorTest.assert(!previousCSSCanvasContextType || previousCSSCanvasContextType === contextType, "addCSSCanvasTestCase cannot be called more than once with different context types.");
        if (!previousCSSCanvasContextType)
            previousCSSCanvasContextType = contextType;

        suite.addTestCase({
            name: `${suite.name}.CSSCanvas`,
            description: "Check that CSS canvases have the correct name and type.",
            test(resolve, reject) {
                awaitCanvasAdded(contextType)
                .then((canvas) => {
                    InspectorTest.expectEqual(canvas.cssCanvasName, "css-canvas", "Canvas name should equal the identifier passed to -webkit-canvas.");
                })
                .then(resolve, reject);

                let contextId = null;
                if (contextType === WI.Canvas.ContextType.Canvas2D)
                    contextId = "2d";
                else if (contextType === WI.Canvas.ContextType.WebGPU)
                    contextId = "gpu";
                else
                    contextId = contextType;

                InspectorTest.log(`Create CSS canvas from -webkit-canvas(css-canvas).`);
                InspectorTest.evaluateInPage(`createCSSCanvas("${contextId}", "css-canvas")`);
            },
        });
    };
});
