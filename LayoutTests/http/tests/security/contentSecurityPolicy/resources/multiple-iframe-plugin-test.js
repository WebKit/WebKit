if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
}

function runTests()
{
    runNextTest();
}

function runNextTest()
{
    var currentTest = tests.shift();
    if (!currentTest) {
        if (window.testRunner)
            setTimeout("testRunner.notifyDone()", 0);
        return;
    }

    var iframe = document.createElement("iframe");
    iframe.onload = function() {
        if (window.internals)
            internals.updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(iframe);
        runNextTest();
    };
    var url = "/security/contentSecurityPolicy/resources/echo-object-data.pl?csp=" + encodeURIComponent(currentTest[1]);
    url += "&log=" + (currentTest[0] ? "PASS." : "FAIL.");
    url += "&plugin=" + (currentTest[2] ? encodeURIComponent(currentTest[2]) : "data:application/x-webkit-test-netscape,logifloaded");
    url += "&type=" + (currentTest[3] !== undefined ? encodeURIComponent(currentTest[3]) : "application/x-webkit-test-netscape");
    iframe.src = url;
    document.body.appendChild(iframe);
}
