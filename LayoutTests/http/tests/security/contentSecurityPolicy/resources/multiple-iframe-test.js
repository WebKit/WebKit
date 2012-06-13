if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
}

function test() {
    if (tests.length === 0)
        return finishTesting();

    var baseURL = "http://127.0.0.1:8000/security/contentSecurityPolicy/";
    var current = tests.shift();
    var iframe = document.createElement("iframe");
    iframe.src = baseURL + "resources/echo-script-src.pl?" +
                 "should_run=" + escape(current[0]) +
                 "&csp=" + escape(current[1]) +
                 "&q=" + baseURL + escape(current[2]);
    iframe.onload = test;
    document.body.appendChild(iframe);
}

function finishTesting() {
    if (window.testRunner) {
        setTimeout("testRunner.notifyDone()", 0);
    }
    return true;
}
