async function a() { return "a"; }
async function b() { return "b"; }
async function c() { return "c"; }

async function testStatements() {
    debugger;
    let x = await 1;
    let y = await 2;
    TestPage.dispatchEventToFrontend("done");
}

async function testFunctions() {
    debugger;
    let before = await 1;
    await a();
    let after = await 2;
    TestPage.dispatchEventToFrontend("done");
}

async function testEval() {
    debugger;
    let before = await 1;
    await eval("1 + 1");
    let after = await 2;
    TestPage.dispatchEventToFrontend("done");
}

async function testAnonymousFunction() {
    await (async function() {
        debugger;
        let inner = await 1;
    })();
    let outer = await 2;
    TestPage.dispatchEventToFrontend("done");
}

async function testCommas() {
    debugger;
    let x = await 1,
        y = await 2,
        z = await 3;
    await a(), await b(), await c();
    await true && (await a(), await b(), await c()) && await true;
    TestPage.dispatchEventToFrontend("done");
}

async function testChainedExpressions() {
    debugger;
    await a() && await b() && await c();
    TestPage.dispatchEventToFrontend("done");
}

async function testDeclarations() {
    debugger;
    let x = await a(),
        y = await b(),
        z = await c();
    TestPage.dispatchEventToFrontend("done");
}

async function testInnerFunction() {
    async function alpha() {
        await beta();
    }
    async function beta() {
        debugger;
    }
    await alpha();
    TestPage.dispatchEventToFrontend("done");
}

async function testFor() {
    debugger;
    for await (let item of [a(), b()]) {
        c();
    }
    TestPage.dispatchEventToFrontend("done");
}

async function testRepeatedInvocation() {
    async function wrap(state) {
        if (state === 2)
            debugger;
        if (state === 1)
            await a(); // should not pause on this line
        await b();
        if (state === 1)
            await c(); // should not pause on this line
        if (state === 2)
            TestPage.dispatchEventToFrontend("done");
    }

    wrap(1);
    wrap(2);
}

TestPage.registerInitializer(() => {
    InspectorTest.SteppingAsync = {};

    InspectorTest.SteppingAsync.run = function(name) {
        let suite = InspectorTest.createAsyncSuite(name);

        function addTestCase({name, expression}) {
            suite.addTestCase({
                name,
                test(resolve, reject) {
                    let done = false;
                    let paused = false;

                    let pausedListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                        InspectorTest.log(`PAUSED (${WI.debuggerManager.dataForTarget(WI.debuggerManager.activeCallFrame.target).pauseReason})`);
                        paused = true;
                    });

                    let resumeListener = WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Resumed, (event) => {
                        InspectorTest.log("RESUMED");
                        paused = false;

                        if (done) {
                            WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
                            WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Resumed, resumeListener);
                            resolve();
                        }
                    });

                    InspectorTest.singleFireEventListener("done", (event) => {
                        done = true;

                        if (!paused) {
                            WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, pausedListener);
                            WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Resumed, resumeListener);
                            resolve();
                        }
                    });

                    InspectorTest.evaluateInPage(expression).catch(reject);
                },
            });
        }

        addTestCase({
            name: name + ".statements",
            expression: "setTimeout(testStatements)",
        });

        addTestCase({
            name: name + ".functions",
            expression: "setTimeout(testFunctions)",
        });

        addTestCase({
            name: name + ".eval",
            expression: "setTimeout(testEval)",
        });

        addTestCase({
            name: name + ".anonymousFunction",
            expression: "setTimeout(testAnonymousFunction)",
        });

        addTestCase({
            name: name + ".commas",
            expression: "setTimeout(testCommas)",
        });

        addTestCase({
            name: name + ".chainedExpressions",
            expression: "setTimeout(testChainedExpressions)",
        });

        addTestCase({
            name: name + ".declarations",
            expression: "setTimeout(testDeclarations)",
        });

        addTestCase({
            name: name + ".innerFunction",
            expression: "setTimeout(testInnerFunction)",
        });

        addTestCase({
            name: name + ".testFor",
            expression: "setTimeout(testFor)",
        });

        addTestCase({
            name: name + ".testRepeatedInvocation",
            expression: "setTimeout(testRepeatedInvocation)",
        });

        loadLinesFromSourceCode(findScript(/\/inspector\/debugger\/resources\/stepping-async\.js$/)).then(() => {
            suite.runTestCasesAndFinish();
        });
    };
});
