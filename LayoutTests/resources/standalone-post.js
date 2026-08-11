wasPostTestScriptParsed = true;

if (this.jsTestIsAsync) {
    if (this.wasFinishJSTestCalled)
        finishJSTest();
} else
    finishJSTest();
