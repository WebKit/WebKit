let context;
let program;

function createProgram(contextType, {offscreen} = {}) {
    if (offscreen) {
        if (window.OffscreenCanvas)
            context = (new OffscreenCanvas(300, 150)).getContext(contextType);
    } else
        context = document.createElement("canvas").getContext(contextType);

    program = context.createProgram();
}

function linkProgram(vertexShaderIdentifier, fragmentShaderIdentifier) {
    function typeForScript(script) {
        if (script.type === "x-shader/x-vertex")
            return context.VERTEX_SHADER;
        if (script.type === "x-shader/x-fragment")
            return context.FRAGMENT_SHADER;

        console.assert(false, "Unexpected script x-shader type: " + script.type);
    }

    function createShaderFromScript(scriptIdentifier) {
        let script = document.getElementById(scriptIdentifier);
        let shader = context.createShader(typeForScript(script));
        context.attachShader(program, shader);
        context.shaderSource(shader, script.text);
        context.compileShader(shader);
    }

    createShaderFromScript(vertexShaderIdentifier);
    createShaderFromScript(fragmentShaderIdentifier);
    context.linkProgram(program);
}

function deleteProgram() {
    context.deleteProgram(program);
    program = null;
}

function deleteContext() {
    context = null;
    // Force GC to make sure the canvas element is destroyed, otherwise the frontend
    // does not receive Canvas.canvasRemoved events.
    setTimeout(() => { GCController.collect(); }, 0);
}

TestPage.registerInitializer(() => {
    let suite = null;

    function whenProgramAdded(callback) {
        let canvases = Array.from(WI.canvasManager.canvasCollection);
        InspectorTest.assert(canvases.length === 1, "There should only be one canvas.");
        canvases[0].shaderProgramCollection.singleFireEventListener(WI.Collection.Event.ItemAdded, (event) => {
            let program = event.data.item;
            InspectorTest.expectThat(program instanceof WI.ShaderProgram, "Added ShaderProgram.");
            InspectorTest.expectThat(program.canvas instanceof WI.Canvas, "ShaderProgram should have a parent Canvas.");
            callback(program);
        });
    }

    function whenProgramRemoved(callback) {
        let canvases = Array.from(WI.canvasManager.canvasCollection);
        InspectorTest.assert(canvases.length === 1, "There should only be one canvas.");
        canvases[0].shaderProgramCollection.singleFireEventListener(WI.Collection.Event.ItemRemoved, (event) => {
            let program = event.data.item;
            InspectorTest.expectThat(program instanceof WI.ShaderProgram, "Removed ShaderProgram.");
            InspectorTest.expectThat(program.canvas instanceof WI.Canvas, "ShaderProgram should have a parent Canvas.");
            callback(program);
        });
    }

    window.initializeTestSuite = function(contextType) {
        suite = InspectorTest.createAsyncSuite(`Canvas.ShaderProgram.${contextType}`);

        suite.addTestCase({
            name: `${suite.name}.reloadPage`,
            description: "Check that ShaderProgramAdded is sent for a program created before CanvasAgent is enabled.",
            test(resolve, reject) {
                // This can't use `awaitEvent` since the promise resolution happens on the next tick.
                WI.canvasManager.canvasCollection.singleFireEventListener(WI.Collection.Event.ItemAdded, (event) => {
                    whenProgramAdded((program) => {
                        resolve();
                    });
                });

                InspectorTest.reloadPage();
            }
        });

        return suite;
    }

     window.addSimpleTestCase = function(contextType) {
        suite.addTestCase({
            name: `${suite.name}.ShaderProgramAdded`,
            description: "Check that added/removed events are sent.",
            test(resolve, reject) {
                whenProgramAdded((addedProgram) => {
                    whenProgramRemoved((removedProgram) => {
                        InspectorTest.expectEqual(removedProgram, addedProgram, "Removed the previously added ShaderProgram.");
                        resolve();
                    });

                    InspectorTest.evaluateInPage(`deleteProgram()`);
                });

                InspectorTest.evaluateInPage(`createProgram("${contextType}")`);
            }
        });
    }

    window.addParentCanvasRemovedTestCase = function(contextType) {
        suite.addTestCase({
            name: `${suite.name}.ParentCanvasRemoved`,
            description: "Check that the ShaderProgram is removed before it's parent Canvas.",
            test(resolve, reject) {
                let canvasRemoved = false;

                WI.canvasManager.canvasCollection.singleFireEventListener(WI.Collection.Event.ItemRemoved, (event) => {
                    canvasRemoved = true;
                });

                whenProgramRemoved((program) => {
                    InspectorTest.expectFalse(canvasRemoved, "Removed ShaderProgram before Canvas.");
                    resolve();
                });

                InspectorTest.evaluateInPage(`createProgram("${contextType}")`);
                InspectorTest.evaluateInPage(`deleteContext()`);
            }
        });
    }
});
