let context;
let program;

function createProgram(contextType) {
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
    // does not receive WI.CanvasManager.Event.CanvasRemoved events.
    setTimeout(() => { GCController.collect(); }, 0);
}

TestPage.registerInitializer(() => {
    let suite = null;

    function awaitProgramAdded() {
        return WI.canvasManager.awaitEvent(WI.CanvasManager.Event.ShaderProgramAdded)
        .then((event) => {
            let program = event.data.program;
            InspectorTest.expectThat(program instanceof WI.ShaderProgram, "Added ShaderProgram.");
            InspectorTest.expectThat(program.canvas instanceof WI.Canvas, "ShaderProgram should have a parent Canvas.");
            return program;
        });
    }

    function awaitProgramRemoved() {
        return WI.canvasManager.awaitEvent(WI.CanvasManager.Event.ShaderProgramRemoved)
        .then((event) => {
            let program = event.data.program;
            InspectorTest.expectThat(program instanceof WI.ShaderProgram, "Removed ShaderProgram.");
            InspectorTest.expectThat(program.canvas instanceof WI.Canvas, "ShaderProgram should have a parent Canvas.");
            return program;
        });
    }

    window.initializeTestSuite = function(contextType) {
        suite = InspectorTest.createAsyncSuite(`Canvas.ShaderProgram.${contextType}`);

        suite.addTestCase({
            name: `${suite.name}.reloadPage`,
            description: "Check that ShaderProgramAdded is sent for a program created before CanvasAgent is enabled.",
            test(resolve, reject) {
                awaitProgramAdded().then(resolve, reject);

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
                awaitProgramAdded()
                .then((addedProgram) => {
                    awaitProgramRemoved()
                    .then((removedProgram) => {
                        InspectorTest.expectEqual(removedProgram, addedProgram, "Removed the previously added ShaderProgram.");
                    })
                    .then(resolve, reject);

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
                Promise.race([
                    awaitProgramRemoved()
                    .then(() => {
                        InspectorTest.pass("Removed ShaderProgram before Canvas.");
                        resolve();
                    }),
                    WI.canvasManager.awaitEvent(WI.CanvasManager.Event.CanvasRemoved)
                    .then(reject)
                ])
                .catch(() => { InspectorTest.fail("Removed Canvas before ShaderProgram."); });

                InspectorTest.evaluateInPage(`createProgram("${contextType}")`);
                InspectorTest.evaluateInPage(`deleteContext()`);
            }
        });
    }
});
