wasPostTestScriptParsed = true;

if (window.layoutTestController)
    layoutTestController.waitUntilDone();
if (window.wasFinishJSTestCalled)
    finishJSTest();
