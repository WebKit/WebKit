wasPostTestScriptParsed = true;

if (window.jsTestIsAsync) {
    if (window.testRunner)
        testRunner.waitUntilDone();
    if (window.wasFinishJSTestCalled)
        finishJSTest();
} else
    finishJSTest();
