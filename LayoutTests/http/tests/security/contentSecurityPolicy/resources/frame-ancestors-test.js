var SAME_ORIGIN = true;
var CROSS_ORIGIN = false;

var EXPECT_BLOCK = true;
var EXPECT_LOAD = false;

var SAMEORIGIN_ORIGIN = "http://127.0.0.1:8000";
var CROSSORIGIN_ORIGIN = "http://localhost:8080";

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.waitUntilDone();
}

function done() {
    if (window.testRunner)
        testRunner.notifyDone();
}

window.addEventListener("message", function (e) {
    if (window.parent != window) {
        window.parent.postMessage(e.data, "*");
        return;
    }
    done();
});

function injectNestedIframe(policy, parent, child, expectation) {
    var iframe = document.createElement("iframe");

    var url = "/security/contentSecurityPolicy/resources/frame-in-frame.pl?"
              + "policy=" + policy
              + "&parent=" + parent
              + "&child=" + child
              + "&expectation=" + expectation;
    url = (parent == "same" ? SAMEORIGIN_ORIGIN : CROSSORIGIN_ORIGIN) + url;

    iframe.src = url;
    document.body.appendChild(iframe);
}

function injectIFrame(policy, sameOrigin) {
    var iframe = document.createElement("iframe");
    iframe.addEventListener("load", handleFrameEvent);
    iframe.addEventListener("error", handleFrameEvent);

    var url = "/security/contentSecurityPolicy/resources/frame-ancestors.pl?policy=" + policy;
    if (!sameOrigin)
        url = CROSSORIGIN_ORIGIN + url;

    iframe.src = url;
    document.body.appendChild(iframe);
}

function handleFrameEvent(event) {
    if (window.parent != window) {
        window.parent.postMessage(null, '*');
        return;
    }
    done();
}

function crossOriginFrameShouldBeBlocked(policy) {
    window.onload = function () {
        injectIFrame(policy, CROSS_ORIGIN, EXPECT_BLOCK);
    };
}

function crossOriginFrameShouldBeAllowed(policy) {
    window.onload = function () {
        injectIFrame(policy, CROSS_ORIGIN, EXPECT_LOAD);
    };
}

function sameOriginFrameShouldBeBlocked(policy) {
    window.onload = function () {
        injectIFrame(policy, SAME_ORIGIN, EXPECT_BLOCK);
    };
}

function sameOriginFrameShouldBeAllowed(policy) {
    window.onload = function () {
        injectIFrame(policy, SAME_ORIGIN, EXPECT_LOAD);
    };
}

function testNestedIFrame(policy, parent, child, expectation) {
    window.onload = function () {
        injectNestedIframe(policy, parent == SAME_ORIGIN ? "same" : "cross", child == SAME_ORIGIN ? "same" : "cross", expectation == EXPECT_LOAD ? "Allowed" : "Blocked");
    };
}
