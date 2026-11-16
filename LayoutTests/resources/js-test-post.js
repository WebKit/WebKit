// DEPRECATED: Do not include this file in new tests. Use js-test.js instead,
// which combines the functionality of js-test-pre.js and js-test-post.js.

wasPostTestScriptParsed = true;

if (window.jsTestIsAsync) {
    if (window.testRunner)
        testRunner.waitUntilDone();
    if (window.wasFinishJSTestCalled)
        finishJSTest();
} else
    finishJSTest();
