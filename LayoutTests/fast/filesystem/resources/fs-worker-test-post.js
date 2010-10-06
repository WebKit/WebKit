wasPostTestScriptParsed = true;

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    window.jsTestIsAsync = true;
}
if (window.wasFinishJSTestCalled)
    finishJSTest();
