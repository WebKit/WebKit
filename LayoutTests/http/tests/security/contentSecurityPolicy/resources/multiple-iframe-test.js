if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
}

function testPreescapedPolicy() {
    testImpl(true);
}

function test() {
    testImpl(false);
}

function testImpl(preescapedPolicy) {
    if (tests.length === 0)
        return finishTesting();

    var baseURL = "http://127.0.0.1:8000/security/contentSecurityPolicy/";
    var current = tests.shift();
    var iframe = document.createElement("iframe");

    var policy = current[1];
    if (!preescapedPolicy)
        policy = encodeURIComponent(policy);

    var scriptToLoad = baseURL + encodeURIComponent(current[2]);
    if (current[2].match(/^data:/))
        scriptToLoad = encodeURIComponent(current[2]);

    iframe.src = baseURL + "resources/echo-script-src.pl?" +
                 "should_run=" + encodeURIComponent(current[0]) +
                 "&csp=" + policy + "&q=" + scriptToLoad;
    if (current[3])
      iframe.src += "&nonce=" + encodeURIComponent(current[3]);

    iframe.onload = test;
    document.body.appendChild(iframe);
}

function finishTesting() {
    if (window.testRunner) {
        setTimeout("testRunner.notifyDone()", 0);
    }
    return true;
}
