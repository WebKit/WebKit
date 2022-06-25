if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
}

function testPreescapedPolicy() {
    testImpl(false, true);
}

function testExperimentalPolicy() {
    testImpl(true, false);
}

function test() {
    testImpl(false, false);
}

function testImpl(experimental, preescapedPolicy) {
    if (tests.length === 0)
        return finishTesting();

    var baseURL = "/security/contentSecurityPolicy/";
    var current = tests.shift();
    var iframe = document.createElement("iframe");

    var queries = {
        should_run: encodeURIComponent(current[0])
    };

    var policy = current[1];
    if (!preescapedPolicy)
        policy = encodeURIComponent(policy);
    queries["csp"] = policy;

    var scriptToLoad = baseURL + encodeURIComponent(current[2]);
    if (current[2].match(/^data:/) || current[2].match(/^https?:/))
        scriptToLoad = encodeURIComponent(current[2]);
    queries["q"] = scriptToLoad;
    queries["experimental"] = experimental ? "true" : "false";
    if (current[3] !== undefined)
        queries["nonce"] = encodeURIComponent(current[3]);

    iframe.src = `${baseURL}resources/echo-module-script-src.pl?` + Object.getOwnPropertyNames(queries).map((key) => key + '=' + queries[key]).join('&');

    iframe.onload = function() { testImpl(experimental, preescapedPolicy); };
    document.body.appendChild(iframe);
}

function finishTesting() {
    if (window.testRunner) {
        setTimeout("testRunner.notifyDone()", 0);
    }
    return true;
}
