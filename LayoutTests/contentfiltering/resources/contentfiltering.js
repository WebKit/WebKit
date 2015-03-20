function _doTest(decisionPoint, decision)
{
    var settings = window.internals.mockContentFilterSettings;
    settings.enabled = true;
    settings.decisionPoint = decisionPoint;
    settings.decision = decision;
    
    var blockedStringText = (decision === settings.DECISION_ALLOW ? "FAIL" : "PASS");
    settings.blockedString = "<!DOCTYPE html><body>" + blockedStringText;

    var iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    iframe.addEventListener("load", function(event) {
        window.testRunner.notifyDone();
    }, false);
    iframe.src = "data:text/html,<!DOCTYPE html><body>" + (blockedStringText === "FAIL" ? "PASS" : "FAIL");
}

function testContentFiltering(decisionPoint, decision)
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
    window.addEventListener("load", function(event) {
        _doTest(decisionPoint, decision);
    }, false);
}