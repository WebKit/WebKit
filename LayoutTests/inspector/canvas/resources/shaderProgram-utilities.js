let context;
let program;

function createProgram(contextType) {
    context = document.createElement("canvas").getContext(contextType);
    program = context.createProgram();
}

function deleteProgram() {
    context.deleteProgram(program);
    program = null;
}

function deleteContext() {
    context = null;
    // Force GC to make sure the canvas element is destroyed, otherwise the frontend
    // does not receive WI.CanvasManager.Event.CanvasWasRemoved events.
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
                    WI.canvasManager.awaitEvent(WI.CanvasManager.Event.CanvasWasRemoved)
                    .then(reject)
                ])
                .catch(() => { InspectorTest.fail("Removed Canvas before ShaderProgram."); });

                InspectorTest.evaluateInPage(`createProgram("${contextType}")`);
                InspectorTest.evaluateInPage(`deleteContext()`);
            }
        });
    }
});
