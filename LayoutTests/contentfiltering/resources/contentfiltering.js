function _doTest(decisionPoint, decision, decideAfterUnblockRequest)
{
    var settings = window.internals.mockContentFilterSettings;
    settings.enabled = true;
    settings.decisionPoint = decisionPoint;
    settings.decision = (decideAfterUnblockRequest ? settings.DECISION_BLOCK : decision);
    
    var blockedStringText;
    if (decisionPoint === settings.DECISION_POINT_NEVER || decision === settings.DECISION_ALLOW)
        blockedStringText = "FAIL";
    else
        blockedStringText =  "PASS";

    if (decideAfterUnblockRequest) {
        settings.unblockRequestDecision = decision;
        settings.blockedString = "<!DOCTYPE html><script>function unblockRequestDenied() { window.top.postMessage('unblockrequestdenied', '*'); }</script><body>" + blockedStringText;
    } else
        settings.blockedString = "<!DOCTYPE html><body>" + blockedStringText;

    var isUnblocking = false;
    var iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    iframe.addEventListener("load", function(event) {
        if (isUnblocking || !decideAfterUnblockRequest) {
            window.testRunner.notifyDone();
            return;
        }

        isUnblocking = true;
        window.addEventListener("message", function(event) {
            if (event.data === "unblockrequestdenied")
                window.testRunner.notifyDone();
        }, false);
        iframe.contentDocument.location = settings.unblockRequestURL;
    }, false);

    if (blockedStringText === "FAIL")
        iframe.src = "resources/pass.html";
    else
        iframe.src = "resources/fail.html";
}

function testContentFiltering(decisionPoint, decision, decideAfterUnblockRequest)
{
    if (!window.internals) {
        console.log("This test requires window.internals");
        return;
    }

    if (!window.testRunner) {
        console.log("This test requires window.testRunner");
        return;
    }

    window.testRunner.waitUntilDone();
    window.testRunner.dumpAsText();
    window.testRunner.dumpChildFramesAsText();
    window.testRunner.dumpBackForwardList();
    window.testRunner.dumpFrameLoadCallbacks();
    window.addEventListener("load", function(event) {
        _doTest(decisionPoint, decision, decideAfterUnblockRequest);
    }, false);
}
