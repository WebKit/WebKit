// Wait for the load event, run post layout tasks, run the specified function,
// and notify the test runner that the test is done.

var NotifyDone = true;
var DoNotNotifyDone = false;

function runAfterPluginLoad(func, notifyDone, node) {
    if (window.testRunner)
        testRunner.waitUntilDone();

    window.addEventListener('load', function() {
        if (window.internals)
            internals.updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(node);

        if (func)
            func();

        if (notifyDone && window.testRunner)
            testRunner.notifyDone();
    }, false);
}
