window.contexts = [];

// Set by the frontend. Because the GC is conservative, any one canvas can stay alive after it is
// no longer referenced, so tests create many canvases and wait for any of them to be destroyed.
window.canvasCountPerTestCase = 1;

// Creating more WebGL contexts than the per-page limit loses the oldest ones, which would otherwise
// log a console message for each one.
window.internals?.settings.setWebGLErrorsToConsoleEnabled(false);

function createAttachedCanvas(contextType) {
    for (let i = 0; i < window.canvasCountPerTestCase; ++i) {
        let canvas = document.body.appendChild(document.createElement("canvas"));
        let context = canvas.getContext(contextType);
        if (!context) {
            TestPage.addResult("FAIL: missing context for type " + contextType);
            return;
        }
        window.contexts.push(context);
    }
}

function createDetachedCanvas(contextType) {
    for (let i = 0; i < window.canvasCountPerTestCase; ++i) {
        let context = document.createElement("canvas").getContext(contextType);
        if (!context) {
            TestPage.addResult("FAIL: missing context for type " + contextType);
            return;
        }
        window.contexts.push(context);
    }
}

function createCSSCanvas(contextType, canvasName) {
    let context = document.getCSSCanvasContext(contextType, canvasName, 10, 10);
    if (!context) {
        TestPage.addResult("FAIL: missing context for type " + contextType);
        return;
    }
    window.contexts.push(context);
}

function createOffscreenCanvas(contextType, canvasName) {
    if (!window.OffscreenCanvas) {
        TestPage.addResult("FAIL: missing OffscreenCanvas implementation");
        return;
    }
    for (let i = 0; i < window.canvasCountPerTestCase; ++i) {
        let canvas = new OffscreenCanvas(10, 10);
        let context = canvas.getContext(contextType);

        if (!context) {
            TestPage.addResult("FAIL: missing offscreen context for type " + contextType);
            return;
        }
        window.contexts.push(context);
    }
}

let destroyCanvasesInterval = null;

function destroyCanvases() {
    for (let context of window.contexts) {
        let canvasElement = context.canvas;
        if (canvasElement && canvasElement.parentNode)
            canvasElement.remove();
    }

    for (let i = 0; i < window.contexts.length; ++i)
        window.contexts[i] = null;
    window.contexts = [];

    window.worker?.postMessage({name: `destroyContexts`, args: []});

    // Force GC to make sure the canvas element is destroyed, otherwise the frontend
    // does not receive Canvas.canvasRemoved events.
    destroyCanvasesInterval = setInterval(() => { GCController.collect(); }, 0);
}

function stopDestroyingCanvases()
{
    clearInterval(destroyCanvasesInterval);
}

TestPage.registerInitializer(() => {
    let suite = null;

    const canvasCountPerTestCase = 100;

    function awaitCanvasesAdded(contextType, count) {
        return new Promise((resolve, reject) => {
            let canvases = [];
            let listener = WI.canvasManager.canvasCollection.addEventListener(WI.Collection.Event.ItemAdded, (event) => {
                canvases.push(event.data.item);
                if (canvases.length < count)
                    return;
                WI.canvasManager.canvasCollection.removeEventListener(WI.Collection.Event.ItemAdded, listener);
                resolve(canvases);
            });
        })
        .then((canvases) => {
            let contextDisplayName = WI.Canvas.displayNameForContextType(contextType);
            InspectorTest.expectThat(canvases.every((canvas) => canvas.contextType === contextType), `Canvas context should be ${contextDisplayName}.`);

            let canvas = canvases[0];

            let traceText = "";
            let callFrames = canvas?.stackTrace.callFrames ?? [];
            for (let i = 0; i < callFrames.length; ++i) {
                let callFrame = callFrames[i];
                traceText += `  ${i}: ` + (callFrame.functionName || "(anonymous function)");
                if (callFrame.nativeCode)
                    traceText += " - [native code]";
                else if (callFrame.programCode)
                    traceText += " - [program code]";
                else if (callFrame.sourceCodeLocation) {
                    let location = callFrame.sourceCodeLocation;
                    traceText += " - " + sanitizeURL(location.sourceCode.url) + `:${location.lineNumber}:${location.columnNumber}`;
                }
                traceText += "\n";
            }
            InspectorTest.log(traceText);

            return canvases;
        });
    }

    function awaitAnyCanvasRemoved(canvases) {
        let identifiers = new Set(canvases.map((canvas) => canvas.identifier));
        return new Promise((resolve, reject) => {
            let listener = WI.canvasManager.canvasCollection.addEventListener(WI.Collection.Event.ItemRemoved, (event) => {
                if (!identifiers.has(event.data.item.identifier))
                    return;
                WI.canvasManager.canvasCollection.removeEventListener(WI.Collection.Event.ItemRemoved, listener);
                InspectorTest.pass("Removed canvas has expected ID.");
                resolve();
            });
        });
    }

    InspectorTest.CreateContextUtilities = {};

    InspectorTest.CreateContextUtilities.initializeTestSuite = function(name) {
        suite = InspectorTest.createAsyncSuite(name);

        InspectorTest.evaluateInPage(`window.canvasCountPerTestCase = ${canvasCountPerTestCase}`);

        suite.addTestCase({
            name: `${suite.name}.NoCanvases`,
            description: "Check that the CanvasManager has no canvases initially.",
            test(resolve, reject) {
                InspectorTest.expectEqual(WI.canvasManager.canvasCollection.size, 0, "CanvasManager should have no canvases.");
                resolve();
            }
        });

        return suite;
    };

    InspectorTest.CreateContextUtilities.addSimpleTestCase = function({name, description, expression, contextType, canvasCount, skipDestroy}) {
        suite.addTestCase({
            name: suite.name + "." + name,
            description,
            test(resolve, reject) {
                awaitCanvasesAdded(contextType, canvasCount ?? canvasCountPerTestCase)
                .then((canvases) => {
                    if (skipDestroy) {
                        resolve();
                        return;
                    }

                    let promise = awaitAnyCanvasRemoved(canvases).then(() => { InspectorTest.evaluateInPage(`stopDestroyingCanvases()`); });
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
                awaitCanvasesAdded(contextType, 1)
                .then(([canvas]) => {
                    InspectorTest.expectShallowEqual(canvas.cssCanvasNames, ["css-canvas"], "Canvas name should equal the identifier passed to -webkit-canvas.");
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
