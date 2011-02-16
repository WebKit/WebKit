wasPostTestScriptParsed = true;

if (window.jsTestIsAsync) {
    if (window.layoutTestController)
        layoutTestController.waitUntilDone();
    if (window.wasFinishJSTestCalled)
        finishJSTest();
} else
    finishJSTest();
